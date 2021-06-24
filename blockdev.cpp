/****************************************************************************
 * Block device enumeration for the installer.
 ****************************************************************************
 *   Copyright (C) 2019 by AK-47
 *   Licensed under the Apache License, Version 2.0 (the "License");
 *   you may not use this file except in compliance with the License.
 *   You may obtain a copy of the License at
 *
 *       http://www.apache.org/licenses/LICENSE-2.0
 *
 *   Unless required by applicable law or agreed to in writing, software
 *   distributed under the License is distributed on an "AS IS" BASIS,
 *   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *   See the License for the specific language governing permissions and
 *   limitations under the License.
 *
 * This file is part of the gazelle-installer.
****************************************************************************/

#include <QDebug>
#include <QFile>
#include <QSettings>
#include <QRegularExpression>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include "blockdev.h"

// static function to split a device name into its drive and partition
QStringList BlockDeviceInfo::split(const QString &devname)
{
    const QRegularExpression rxdev1("^(?:\\/dev\\/)*(mmcblk.*|nvme.*)p([0-9]*)$");
    const QRegularExpression rxdev2("^(?:\\/dev\\/)*([a-z]*)([0-9]*)$");
    QRegularExpressionMatch rxmatch(rxdev1.match(devname));
    if (!rxmatch.hasMatch()) rxmatch = rxdev2.match(devname);
    QStringList list(rxmatch.capturedTexts());
    if (!list.isEmpty()) list.removeFirst();
    return list;
}

QString BlockDeviceInfo::join(const QString &drive, int partnum)
{
    QString name = drive;
    if (name.startsWith("nvme") || name.startsWith("mmcblk")) name += 'p';
    return (name + QString::number(partnum));
}

// return block device info that is suitable for a combo box
void BlockDeviceInfo::addToCombo(QComboBox *combo, bool warnNasty) const
{
    QString strout(QLocale::system().formattedDataSize(size, 1, QLocale::DataSizeTraditionalFormat));
    if (!fs.isEmpty()) strout += " " + fs;
    if (!label.isEmpty()) strout += " - " + label;
    if (!model.isEmpty()) strout += (label.isEmpty() ? " - " : "; ") + model;
    QString stricon;
    if (isFuture) stricon = ":/appointment-soon";
    else if (isNasty && warnNasty) stricon = ":/dialog-warning";
    combo->addItem(QIcon(stricon), name + " (" + strout + ")", name);
}

//////////////////////////////////////////////////////////////////////////////

void BlockDeviceList::build(MProcess &proc)
{
    proc.log(__PRETTY_FUNCTION__);

    proc.exec("partprobe -s", true);
    proc.exec("blkid -c /dev/null", true);

    // expressions for matching various partition types
    const QRegularExpression rxESP("^(c12a7328-f81f-11d2-ba4b-00a0c93ec93b|0xef)$");
    const QRegularExpression rxNative("^(0x83|0fc63daf-8483-4772-8e79-3d69d8477de4" // Linux data
                                      "|0x82|0657fd6d-a4ab-43c4-84e5-0933c84b4f4f" // Linux swap
                                      "|44479540-f297-41b2-9af7-d131d5f0458a" // Linux /root x86
                                      "|4f68bce3-e8cd-4db1-96e7-fbcaf984b709" // Linux /root x86-64
                                      "|933ac7e1-2eb4-4f13-b844-0e14e2aef915)$"); // Linux /home
    const QRegularExpression rxWinLDM("^(0x42|5808c8aa-7e8f-42e0-85d2-e1e90434cfb3"
                                      "|af9b60a0-1431-4f62-bc68-3311714a69ad)$"); // Windows LDM
    const QRegularExpression rxNativeFS("^(btrfs|ext2|ext3|ext4|jfs|nilfs2|reiserfs|ufs|xfs)$");

    QString bootUUID;
    if (QFile::exists("/live/config/initrd.out")) {
        QSettings livecfg("/live/config/initrd.out", QSettings::NativeFormat);
        bootUUID = livecfg.value("BOOT_UUID").toString();
    }

    // Backup ESP detection. Populated when needed.
    QStringList backup_list;
    bool backup_checked = false;
    // Collect information and populate the block device list.
    const QString &bdRaw = proc.execOut("lsblk -T -bJo"
        " TYPE,NAME,UUID,SIZE,PTTYPE,PARTTYPE,FSTYPE,LABEL,MODEL", true);
    const QJsonObject &jsonObjBD = QJsonDocument::fromJson(bdRaw.toUtf8()).object();

    clear();
    const QJsonArray &jsonBD = jsonObjBD["blockdevices"].toArray();
    for (const QJsonValue &jsonDrive : jsonBD) {
        int driveIndex = count(); // For propagating the nasty flag to the drive.
        if (jsonDrive["type"] != "disk") continue;
        BlockDeviceInfo bdinfo;
        bdinfo.name = jsonDrive["name"].toString();
        bdinfo.isDrive = true;
        bdinfo.isGPT = (jsonDrive["pttype"]=="gpt");
        bdinfo.size = jsonDrive["size"].toVariant().toLongLong();
        bdinfo.label = jsonDrive["label"].toString();
        bdinfo.model = jsonDrive["model"].toString();
        append(bdinfo);

        const QJsonArray &jsonParts = jsonDrive["children"].toArray();
        for (const QJsonValue &jsonPart : jsonParts) {
            bdinfo.isDrive = false;
            bdinfo.name = jsonPart["name"].toString();
            bdinfo.size = jsonPart["size"].toVariant().toLongLong();
            bdinfo.label = jsonPart["label"].toString();
            bdinfo.model = jsonPart["model"].toString();
            bdinfo.mapCount = jsonPart["children"].toArray().count();

            const QString &partType = jsonPart["parttype"].toString();
            if (partType.isEmpty()) bdinfo.isESP = bdinfo.isNative = false;
            else {
                bdinfo.isESP = (partType.count(rxESP) >= 1);
                bdinfo.isNative = (partType.count(rxNative) >= 1);
                if (!bdinfo.isNasty) bdinfo.isNasty = (partType.count(rxWinLDM) >= 1);
            }
            if (!bdinfo.isESP) {
                // Backup detection for drives that don't have UUID for ESP.
                if (!backup_checked) {
                    backup_list = proc.execOutLines("fdisk -l -o DEVICE,TYPE"
                        " | grep 'EFI System' |cut -d\\  -f1 | cut -d/ -f3");
                    backup_checked = true;
                }
                bdinfo.isESP = backup_list.contains(bdinfo.name);
            }

            bdinfo.isBoot = (!bootUUID.isEmpty() && jsonPart["uuid"]==bootUUID);
            bdinfo.fs = jsonPart["fstype"].toString();
            if (bdinfo.fs.count(rxNativeFS) >= 1) bdinfo.isNative = true;
            append(bdinfo);
            // Propagate the boot and nasty flags up to the drive.
            if (bdinfo.isBoot) operator[](driveIndex).isBoot = true;
            if (bdinfo.isNasty) operator[](driveIndex).isNasty = true;
        }
    }
    // propagate the boot flag across the entire drive
    bool driveBoot = false;
    for (BlockDeviceInfo &bdinfo : *this) {
        if (bdinfo.isDrive) driveBoot = bdinfo.isBoot;
        bdinfo.isBoot = driveBoot;
    }

    // debug
    qDebug() << "Name Size Model FS | isDisk isGPT isBoot isESP isNative isNasty";
    for (const BlockDeviceInfo &bdi : *this) {
        qDebug() << bdi.name << bdi.size << bdi.model << bdi.fs << "|"
                 << bdi.isDrive << bdi.isGPT << bdi.isBoot << bdi.isESP
                 << bdi.isNative << bdi.isNasty;
    }
}

int BlockDeviceList::findDevice(const QString &devname) const
{
    const int cnt = count();
    for (int ixi = 0; ixi < cnt; ++ixi) {
        const BlockDeviceInfo &bdinfo = at(ixi);
        if (bdinfo.name == devname) return ixi;
    }
    return -1;
}
