// Block device enumeration for the installer.
//
//   Copyright (C) 2019 by AK-47
//   Licensed under the Apache License, Version 2.0 (the "License");
//   you may not use this file except in compliance with the License.
//   You may obtain a copy of the License at
//
//       http://www.apache.org/licenses/LICENSE-2.0
//
//   Unless required by applicable law or agreed to in writing, software
//   distributed under the License is distributed on an "AS IS" BASIS,
//   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//   See the License for the specific language governing permissions and
//   limitations under the License.
//
// This file is part of the gazelle-installer.

#include <QDebug>
#include <QFile>
#include <QSettings>
#include <QRegularExpression>
#include <QUrl>
#include "blockdev.h"

// static function to split a device name into its drive and partition
QStringList BlockDeviceInfo::split(const QString &devname)
{
    const QRegularExpression rxdev1("^(?:/dev/)+(mmcblk.*|nvme.*)p([0-9]*)$");
    const QRegularExpression rxdev2("^(?:/dev/)+([a-z]*)([0-9]*)$");
    QRegularExpressionMatch rxmatch(rxdev1.match(devname));
    if (!rxmatch.hasMatch()) rxmatch = rxdev2.match(devname);
    QStringList list(rxmatch.capturedTexts());
    if (!list.isEmpty()) list.removeFirst();
    return list;
}

// return block device info that is suitable for a combo box
void BlockDeviceInfo::addToCombo(QComboBox *combo, bool warnNasty) const
{
    const char *suffixes[] = {"B", "KB", "MB", "GB", "TB", "PB", "EB"};
    unsigned int isuffix = 0;
    qlonglong scalesize = size;
    while (scalesize >= 1024 && isuffix < sizeof(suffixes)) {
        ++isuffix;
        scalesize /= 1024;
    }
    QString strout(name + " (" + QString::number(scalesize) + suffixes[isuffix]);
    if (!fs.isEmpty()) strout += " " + fs;
    if (!label.isEmpty()) strout += " - " + label;
    if (!model.isEmpty()) strout += (label.isEmpty() ? " - " : "; ") + model;
    QString stricon;
    if (isFuture) stricon = "appointment-soon-symbolic";
    else if (isNasty && warnNasty) stricon = "dialog-warning-symbolic";
    combo->addItem(QIcon::fromTheme(stricon), strout + ")", name);
}

//////////////////////////////////////////////////////////////////////////////

void BlockDeviceList::build(MProcess &proc)
{
    proc.exec("/sbin/partprobe", true);
    proc.exec("blkid -c /dev/null", true);

    // expressions for matching various partition types
    const QRegularExpression rxESP("^(c12a7328-f81f-11d2-ba4b-00a0c93ec93b|0xef)$");
    const QRegularExpression rxSwap("^(0x82|0657fd6d-a4ab-43c4-84e5-0933c84b4f4f)$");
    const QRegularExpression rxNative("^(0x83|0fc63daf-8483-4772-8e79-3d69d8477de4" // Linux data
                                      "|0x82|0657fd6d-a4ab-43c4-84e5-0933c84b4f4f" // Linux swap
                                      "|44479540-f297-41b2-9af7-d131d5f0458a" // Linux /root x86
                                      "|4f68bce3-e8cd-4db1-96e7-fbcaf984b709" // Linux /root x86-64
                                      "|933ac7e1-2eb4-4f13-b844-0e14e2aef915)$"); // Linux /home
    const QRegularExpression rxWinLDM("^(0x42|5808c8aa-7e8f-42e0-85d2-e1e90434cfb3"
                                      "|e3c9e316-0b5c-4db8-817d-f92df00215ae)$"); // Windows LDM
    const QRegularExpression rxNativeFS("^(btrfs|ext2|ext3|ext4|jfs|nilfs2|reiser4|reiserfs|ufs|xfs)$");

    QString bootUUID;
    if (QFile::exists("/live/config/initrd.out")) {
        QSettings livecfg("/live/config/initrd.out", QSettings::NativeFormat);
        bootUUID = livecfg.value("BOOT_UUID").toString();
    }

    // backup detection for drives that don't have UUID for ESP
    const QStringList backup_list = proc.execOutLines("fdisk -l -o DEVICE,TYPE |grep 'EFI System' |cut -d\\  -f1 | cut -d/ -f3");

    // populate the block device list
    clear();
    bool gpt = false; // propagates to all partitions within the drive
    int driveIndex = 0; // for propagating the nasty flag to the drive
    const QStringList &blkdevs = proc.execOutLines("lsblk -brno TYPE,NAME,UUID,SIZE,PARTTYPE,FSTYPE,LABEL,MODEL"
                                            " | grep -E '^(disk|part)'");
    for (const QString &blkdev : blkdevs) {
        const QStringList &bdsegs = blkdev.split(' ');
        const int segsize = bdsegs.size();
        if (segsize < 3) continue;

        BlockDeviceInfo bdinfo;
        bdinfo.isFuture = false;
        bdinfo.isDrive = (bdsegs.at(0) == "disk");
        bdinfo.name = bdsegs.at(1);

        if (bdinfo.isDrive) {
            driveIndex = count();
            const QString cmd("blkid /dev/%1 | grep -q PTTYPE=\\\"gpt\\\"");
            gpt = proc.exec(cmd.arg(bdinfo.name), false);
        }
        bdinfo.isGPT = gpt;

        const QString &uuid = bdsegs.at(2);
        if (!bootUUID.isEmpty() && uuid == bootUUID) {
            bdinfo.isBoot = true;
            // propagate the boot flag across the entire drive
            operator[](driveIndex).isBoot = true;
            for (int ixi = driveIndex + 1; ixi < count(); ++ixi) {
                BlockDeviceInfo &bdi = operator[](ixi);
                if (bdi.isDrive) break;
                bdi.isBoot = true;
            }
        }

        bdinfo.size = bdsegs.at(3).toLongLong();
        if (segsize > 4) {
            const QString &seg4 = bdsegs.at(4);
            bdinfo.isESP = (seg4.count(rxESP) >= 1);
            bdinfo.isSwap = (seg4.count(rxSwap) >= 1);
            bdinfo.isNative = (seg4.count(rxNative) >= 1);
            if (!bdinfo.isNasty) bdinfo.isNasty = (seg4.count(rxWinLDM) >= 1);
        } else {
            bdinfo.isESP = bdinfo.isSwap = bdinfo.isNative = false;
        }

        if (!bdinfo.isDrive && !bdinfo.isESP) {
            // check the backup ESP detection list
            bdinfo.isESP = backup_list.contains(bdinfo.name);
        }
        if (segsize > 5) {
            bdinfo.fs = bdsegs.at(5);
            if(bdinfo.fs.count(rxNativeFS) >= 1) bdinfo.isNative = true;
        }
        if (segsize > 6) {
            const QByteArray seg(bdsegs.at(6).toUtf8().replace('%', "\\x25").replace("\\x", "%"));
            bdinfo.label = QUrl::fromPercentEncoding(seg).trimmed();
        }
        if (segsize > 7) {
            const QByteArray seg(bdsegs.at(7).toUtf8().replace('%', "\\x25").replace("\\x", "%"));
            bdinfo.model = QUrl::fromPercentEncoding(seg).trimmed();
        }
        append(bdinfo);
        // propagate the nasty flag up to the drive
        if (bdinfo.isNasty) operator[](driveIndex).isNasty = true;
    }

    // debug
    qDebug() << "Name Size Model FS | isDisk isGPT isBoot isESP isNative isSwap";
    for (const BlockDeviceInfo &bdi : *this) {
        qDebug() << bdi.name << bdi.size << bdi.model << bdi.fs << "|"
                 << bdi.isDrive << bdi.isGPT << bdi.isBoot << bdi.isESP
                 << bdi.isNative << bdi.isSwap;
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
