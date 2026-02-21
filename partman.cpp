/***************************************************************************
 * Basic partition manager for the installer.
 *
 *   Copyright (C) 2019-2023 by AK-47
 *   Transplanted code, marked with comments further down this file:
 *    - Copyright (C) 2003-2010 by Warren Woodford
 *    - Heavily edited, with permision, by anticapitalista for antiX 2011-2014.
 *    - Heavily revised by dolphin oracle, adrian, and anticaptialista 2018.
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
 ***************************************************************************/

#include <algorithm>
#include <iterator>
#include <utility>
#include <sys/stat.h>
#include <QDebug>
#include <QLocale>
#include <QFileInfo>
#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QProcessEnvironment>
#include <QRegularExpression>
#include <QMessageBox>
#include <QPainter>
#include <QMenu>
#include <QSpinBox>
#include <QComboBox>
#include <QLineEdit>
#include <QFormLayout>
#include <QDialogButtonBox>
#include <QStandardPaths>
#include <QList>

#include "core.h"
#include "mprocess.h"
#include "msettings.h"
#include "crypto.h"
#include "autopart.h"
#include "partman.h"

using namespace Qt::Literals::StringLiterals;

PartMan::PartMan(MProcess &mproc, Core &mcore, Ui::MeInstall &ui, Crypto &cman,
    const MIni &appConf, const QCommandLineParser &appArgs, QObject *parent)
    : QAbstractItemModel(parent), proc(mproc), core(mcore), gui(ui), crypto(cman)
{
    root = new Device(*this);
    const QString &projShort = appConf.getString(u"ShortName"_s);
    volSpecs[u"BIOS-GRUB"_s] = {u"BIOS GRUB"_s};
    volSpecs[u"/boot"_s] = {u"boot"_s};
    volSpecs[u"/boot/efi"_s] = volSpecs[u"ESP"_s] = {u"EFI-SYSTEM"_s};
    const bool archLive = QFileInfo::exists(u"/run/archiso/airootfs"_s);
    if (archLive) {
        volSpecs[u"/"_s] = {u"rootMXarch"_s};
    } else {
        volSpecs[u"/"_s] = {"root"_L1 + projShort + appConf.getString(u"Version"_s)};
    }
    volSpecs[u"/home"_s] = {"home"_L1 + projShort};
    volSpecs[u"SWAP"_s] = {"swap"_L1 + projShort};
    brave = appArgs.isSet(u"brave"_s);

    gui.treePartitions->setModel(this);
    gui.treePartitions->setItemDelegate(new ItemDelegate);
    gui.treePartitions->header()->setMinimumSectionSize(5);
    gui.treePartitions->setContextMenuPolicy(Qt::CustomContextMenu);
    showAdvancedFields(gui.pushAdvancedFields->isChecked());
    connect(gui.treePartitions, &QTreeView::customContextMenuRequested, this, &PartMan::treeMenu);
    connect(gui.treePartitions->selectionModel(), &QItemSelectionModel::selectionChanged, this, &PartMan::treeSelChange);
    connect(gui.pushAdvancedFields, &QToolButton::toggled, this, &PartMan::showAdvancedFields);
    connect(gui.pushPartClear, &QToolButton::clicked, this, &PartMan::partClearClick);
    connect(gui.pushPartAdd, &QToolButton::clicked, this, &PartMan::partAddClick);
    connect(gui.pushPartRemove, &QToolButton::clicked, this, &PartMan::partRemoveClick);
    connect(gui.pushPartReload, &QToolButton::clicked, this, &PartMan::partReloadClick);
    connect(gui.pushPartManRun, &QToolButton::clicked, this, &PartMan::partManRunClick);
    connect(this, &PartMan::dataChanged, this, &PartMan::treeItemChange);
    gui.pushPartAdd->setEnabled(false);
    gui.pushPartRemove->setEnabled(false);
    gui.pushPartClear->setEnabled(false);

    gui.pushGrid->setChecked(gui.treePartitions->grid());
    connect(gui.pushGrid, &QToolButton::toggled, gui.treePartitions, &MTreeView::setGrid);

    // Hide encryption options if cryptsetup not present.
    if (!crypto.supported()) {
        gui.treePartitions->setColumnHidden(COL_ENCRYPT, true);
    }

    // UUID of the device that the live system is booted from.
    const MIni livecfg(u"/live/config/initrd.out"_s, MIni::ReadOnly);
    bootUUID = livecfg.getString(u"BOOT_UUID"_s);
}
PartMan::~PartMan()
{
    if (root) delete root;
}

void PartMan::scan(Device *drvstart)
{   
	//check for ventoy
	QString ventoycheck;
	QString ventoydevice;
    proc.exec(u"dmsetup"_s,{u"ls"_s},nullptr, true);
    ventoycheck = proc.readOut(true).toUtf8();
    //qDebug() << "ventoycheck " << ventoycheck;
    if ( ventoycheck != "No devices found" ) {
        proc.exec(u"dmsetup"_s,{u"deps"_s,u"/dev/mapper/ventoy"_s,u"-o"_s,u"blkdevname"_s},nullptr, true);
        ventoydevice = proc.readOut(true).toUtf8();
        //qDebug() << "ventoydevice " << ventoydevice;
        ventoydevice = ventoydevice.remove("\t").remove("\n").section(":",1,1).trimmed().remove("(").remove(")");
        //qDebug() << "ventoydevice " << ventoydevice;
        
	}

    QStringList cargs({u"-T"_s, u"-bJo"_s,
        u"TYPE,NAME,PATH,UUID,ROTA,DISC-GRAN,SIZE,PHY-SEC,PTTYPE,PARTTYPENAME,FSTYPE,FSVER,LABEL,MODEL,PARTFLAGS,PARTUUID,PARTLABEL"_s});
    if (drvstart) cargs.append(drvstart->path);
    proc.exec(u"lsblk"_s, cargs, nullptr, true);
    const QJsonObject &jsonObjBD = QJsonDocument::fromJson(proc.readOut(true).toUtf8()).object();
    const QJsonArray &jsonBD = jsonObjBD[u"blockdevices"_s].toArray();

    // Partitions listed in order of their physical locations.
    QStringList order;
    QString curdev;
    proc.exec(u"parted"_s, {u"-lm"_s}, nullptr, true);
    for(const QStringList &lines = proc.readOutLines(); const QString &line : lines) {
        const QString &sect = line.section(':', 0, 0);
        const int part = sect.toInt();
        if (part) order.append(joinName(curdev, part));
        else if (sect.startsWith("/dev/"_L1)) curdev = sect.mid(5);
    }

    if (!drvstart) root->clear();
    for (const QJsonValue &jsonDrive : jsonBD) {
        const QString &jdName = jsonDrive[u"name"_s].toString();
        const QString &jdPath = jsonDrive[u"path"_s].toString();
        if (jsonDrive[u"type"_s] != "disk") continue;
        if (jdName.startsWith("zram"_L1)) continue;
        Device *drive = drvstart;
        if (!drvstart) drive = new Device(Device::DRIVE, root);
        else if (jdPath != drvstart->path) continue;

        drive->clear();
        drive->flags.curEmpty = true;
        drive->flags.oldLayout = true;
        drive->name = jdName;
        drive->path = jdPath;
        const QString &ptType = jsonDrive[u"pttype"_s].toString();
        drive->format = drive->curFormat = ptType.toUpper();
        drive->flags.rotational = jsonDrive[u"rota"_s].toBool();
        drive->discgran = jsonDrive[u"disc-gran"_s].toInt();
        drive->size = jsonDrive[u"size"_s].toVariant().toLongLong();
        drive->physec = jsonDrive[u"phy-sec"_s].toInt();
        drive->uuid = jsonDrive[u"uuid"_s].toString();
        drive->partuuid = jsonDrive[u"partuuid"_s].toString();
        drive->partlabel = jsonDrive[u"partlabel"_s].toString();
        drive->curLabel = jsonDrive[u"label"_s].toString();
        drive->model = jsonDrive[u"model"_s].toString();
        const QJsonArray &jsonParts = jsonDrive[u"children"_s].toArray();
        for (const QJsonValue &jsonPart : jsonParts) {
            const QString &partTypeName = jsonPart[u"parttypename"_s].toString();
            if (partTypeName == "Extended"_L1 || partTypeName == "W95 Ext'd (LBA)"_L1
                || partTypeName == "Linux extended"_L1) continue;
            Device *part = new Device(Device::PARTITION, drive);
            drive->flags.curEmpty = false;
            part->flags.oldLayout = true;
            part->name = jsonPart[u"name"_s].toString();
            part->path = jsonPart[u"path"_s].toString();
            //qDebug() << "part path is " << part->path;
            part->uuid = jsonPart[u"uuid"_s].toString();
            part->partuuid = jsonPart[u"partuuid"_s].toString();
            part->partlabel = jsonPart[u"partlabel"_s].toString();
            part->order = order.indexOf(part->name);
            part->size = jsonPart[u"size"_s].toVariant().toLongLong();
            part->physec = jsonPart[u"phy-sec"_s].toInt();
            part->curLabel = jsonPart[u"label"_s].toString();
            part->model = jsonPart[u"model"_s].toString();
            part->flags.rotational = jsonPart[u"rota"_s].toBool();
            part->discgran = jsonPart[u"disc-gran"_s].toInt();
            const int partflags = jsonPart[u"partflags"_s].toString().toUInt(nullptr, 0);
            if ((partflags & 0x80) || (partflags & 0x04)) part->setActive(true);
            part->mapCount = jsonPart[u"children"_s].toArray().count();
            part->flags.sysEFI = part->flags.curESP = partTypeName.startsWith("EFI "_L1); // "System"/"(FAT-12/16/32)"
            part->flags.bootRoot = (!bootUUID.isEmpty() && part->uuid == bootUUID);
            part->curFormat = jsonPart[u"fstype"_s].toString();
            if (part->curFormat == "vfat"_L1) part->curFormat = jsonPart[u"fsver"_s].toString();
            if (partTypeName == "BIOS boot"_L1) part->curFormat = "BIOS-GRUB"_L1;
            // Touching Microsoft LDM may brick the system.
            if (partTypeName.startsWith("Microsoft LDM"_L1)) part->flags.nasty = true;
            // ventoy device should be avoided
            //qDebug() << "part path " << part->path << "ventoy device" << "/dev/" +ventoydevice;
            if (! ventoydevice.isEmpty() ){
                if (part->path == "/dev/" + ventoydevice){
                    part->flags.bootRoot = true;
                    //qDebug() << "Ventoyflags boot" << part->flags.bootRoot;
                }
            }
            // Propagate the boot and nasty flags up to the drive.
            if (part->flags.bootRoot) drive->flags.bootRoot = true;
            if (part->flags.nasty) drive->flags.nasty = true;
        }
        for (Device *part : drive->children) {
            // Propagate the boot flag across the entire drive.
            if (drive->flags.bootRoot) part->flags.bootRoot = true;
        }
        drive->sortChildren();
        notifyChange(drive);
        // Hide the live boot media and its partitions by default.
        if (!brave && drive->flags.bootRoot) {
            gui.treePartitions->setRowHidden(drive->row(), QModelIndex(), true);
        }
    }

    if (!drvstart) scanVirtualDevices(false);
    gui.treePartitions->expandAll();
    resizeColumnsToFit();
    treeSelChange();
}

void PartMan::scanVirtualDevices(bool rescan)
{
    Device *virtdevs = nullptr;
    std::map<QString, Device *> listed;
    if (rescan) {
        for (Device *drive : root->children) {
            if (drive->type == Device::VIRTUAL_DEVICES) {
                virtdevs = drive;
                for (Device *device : virtdevs->children) {
                    listed[device->path] = device;
                }
                break;
            }
        }
    }
    // /dev/mapper/control is not a block device; silence its warning.
    proc.shell(u"lsblk -T -bJo TYPE,NAME,PATH,UUID,ROTA,DISC-GRAN,SIZE,PHY-SEC,FSTYPE,LABEL /dev/mapper/* 2>/dev/null || true"_s, nullptr, true);
    const QString &bdRaw = proc.readOut(true);
    const QJsonObject &jsonObjBD = QJsonDocument::fromJson(bdRaw.toUtf8()).object();
    const QJsonArray &jsonBD = jsonObjBD[u"blockdevices"_s].toArray();
    if (jsonBD.empty()) {
        if (virtdevs) delete virtdevs;
        return;
    } else if (!virtdevs) {
        virtdevs = new Device(Device::VIRTUAL_DEVICES, root);
        virtdevs->name = tr("Virtual Devices");
        virtdevs->flags.oldLayout = true;
        gui.treePartitions->setFirstColumnSpanned(virtdevs->row(), QModelIndex(), true);
    }
    assert(virtdevs != nullptr);
    for (const QJsonValue &jsonDev : jsonBD) {
        const QString &path = jsonDev[u"path"_s].toString();
        const bool rota = jsonDev[u"rota"_s].toBool();
        const int discgran = jsonDev[u"disc-gran"_s].toInt();
        const long long size = jsonDev[u"size"_s].toVariant().toLongLong();
        const int physec = jsonDev[u"phy-sec"_s].toInt();
        const QString &label = jsonDev[u"label"_s].toString();
        const bool crypto = (jsonDev[u"type"_s].toString() == "crypt"_L1);
        // Check if the device is already in the list.
        Device *device = nullptr;
        const auto fit = listed.find(path);
        if (fit != listed.cend()) {
            device = fit->second;
            if (rota != device->flags.rotational || discgran != device->discgran
                || size != device->size || physec != device->physec
                || label != device->curLabel || crypto != device->flags.volCrypto) {
                // List entry is different to the device, so refresh it.
                delete device;
                device = nullptr;
            }
            listed.erase(fit);
        }
        // Create a new list entry if needed.
        if (!device) {
            device = new Device(Device::VIRTUAL, virtdevs);
            device->name = jsonDev[u"name"_s].toString();
            device->path = path;
            device->uuid = jsonDev[u"uuid"_s].toString();
            device->flags.rotational = rota;
            device->discgran = discgran;
            device->size = size;
            device->physec = physec;
            device->flags.bootRoot = (!bootUUID.isEmpty() && device->uuid == bootUUID);
            device->curLabel = label;
            device->curFormat = jsonDev[u"fstype"_s].toString();
            device->flags.volCrypto = crypto;
            device->flags.oldLayout = true;
        }
    }
    for (const auto &it : listed) delete it.second;
    virtdevs->sortChildren();
    for(Device *virt : virtdevs->children) {
        const QString &name = virt->name;
        virt->origin = nullptr; // Clear stale origin pointer.
        // Find the associated partition and decrement its reference count if found.
        proc.exec(u"cryptsetup"_s, {u"status"_s, virt->path}, nullptr, true);
        for (const QStringList &lines = proc.readOutLines(); const QString &line : lines) {
            const QString &trline = line.trimmed();
            if (!trline.startsWith("device:"_L1)) continue;
            virt->origin = findByPath(trline.mid(8).trimmed());
            if (virt->origin) {
                virt->origin->map = name;
                notifyChange(virt->origin);
                break;
            }
        }
    }
    gui.treePartitions->expand(index(virtdevs));
}

void PartMan::scanSubvolumes(Device *part, const QString &scratch)
{
    while (part->children.size()) {
        delete part->child(0);
    }
    QString defline;
    QStringList lines;
    const QString &scratchpath = scratch.isEmpty() ? u"/mnt/scratch"_s : scratch;
    if (scratch.isEmpty()) {
        if (!proc.exec(u"mount"_s, {u"--mkdir"_s, u"-o"_s, u"subvolid=5,ro"_s,
            part->mappedDevice(), scratchpath})) return;
    }
    proc.exec(u"btrfs"_s, {u"subvolume"_s, u"get-default"_s, scratchpath}, nullptr, true);
    defline = proc.readOut();
    proc.exec(u"btrfs"_s, {u"subvolume"_s, u"list"_s, scratchpath}, nullptr, true);
    lines = proc.readOutLines();
    if (scratch.isEmpty()) {
        proc.exec(u"umount"_s, {scratchpath});
    }
    for (const QString &line : std::as_const(lines)) {
        const int start = line.indexOf("path"_L1) + 5;
        if (line.length() <= start) return;
        Device *subvol = new Device(Device::SUBVOLUME, part);
        subvol->flags.oldLayout = true;
        subvol->label = subvol->curLabel = line.mid(start);
        subvol->format = "PRESERVE"_L1;
        if (line == defline) {
            subvol->setActive(true);
        }
        notifyChange(subvol);
    }
}

bool PartMan::loadConfig(MSettings &config) noexcept
{
    config.setSection(u"Storage"_s, gui.treePartitions);
    for (Device *const drive : root->children) {
        // Check if the drive is to be cleared and formatted.
        size_t partCount = drive->children.size();
        bool drvPreserve = drive->flags.oldLayout;
        config.beginGroup(drive->name);
        if (config.contains(u"Format"_s)) {
            drive->format = config.getString(u"Format"_s);
            drvPreserve = false;
            drive->clear();
            partCount = config.getInteger(u"Partitions"_s);
            if (partCount > PARTMAN_MAX_PARTS) return false;
        }
        // Partition configuration.
        const long long sizeMax = drive->size - PARTMAN_SAFETY;
        long long sizeTotal = 0;
        for (size_t ixPart = 0; ixPart < partCount; ++ixPart) {
            Device *part = nullptr;
            if (drvPreserve) {
                part = drive->child(ixPart);
                part->flags.oldLayout = true;
            } else {
                part = new Device(Device::PARTITION, drive);
                part->name = joinName(drive->name, ixPart+1);
            }
            // Configuration management, accounting for automatic control correction order.
            config.beginGroup("Partition"_L1 + QString::number(ixPart+1));
            if (!drvPreserve && config.contains(u"Size"_s)) {
                part->size = config.getInteger(u"Size"_s);
                sizeTotal += part->size;
                if (sizeTotal > sizeMax) return false;
                if (config.getBoolean(u"Active"_s)) part->setActive(true);
            }
            part->flags.sysEFI = config.getBoolean(u"ESP"_s, part->flags.sysEFI);
            part->usefor = config.getString(u"UseFor"_s, part->usefor);
            part->format = config.getString(u"Format"_s, part->format);
            part->chkbadblk = config.getBoolean(u"CheckBadBlocks"_s, part->chkbadblk);
            part->encrypt = config.getBoolean(u"Encrypt"_s, part->encrypt);
            part->addToCrypttab = config.getBoolean(u"AddToCrypttab"_s, part->addToCrypttab);
            part->label = config.getString(u"Label"_s, part->label);
            part->options = config.getString(u"Options"_s, part->options);
            part->dump = config.getBoolean(u"Dump"_s, part->dump);
            part->pass = config.getInteger(u"Pass"_s, part->pass);
            size_t subvolCount = 0;
            if (part->format == "btrfs"_L1) {
                subvolCount = config.getInteger(u"Subvolumes"_s);
            }
            // Btrfs subvolume configuration.
            for (size_t ixSV=0; ixSV<subvolCount; ++ixSV) {
                Device *subvol = new Device(Device::SUBVOLUME, part);
                assert(subvol != nullptr);
                config.beginGroup("Subvolume"_L1 + QString::number(ixSV+1));
                // if (config.getBoolean(u"Default"_s)) subvol->setActive(true);
                subvol->usefor = config.getString(u"UseFor"_s);
                subvol->label = config.getString(u"Label"_s);
                subvol->options = config.getString(u"Options"_s);
                subvol->dump = config.getBoolean(u"Dump"_s);
                subvol->pass = config.getInteger(u"Pass"_s);
                config.endGroup(); // Subvolume#
            }
            config.endGroup(); // Partition#
            gui.treePartitions->expand(index(part));
        }
        config.endGroup(); // (drive name)
    }
    treeSelChange();
    return true;
}
void PartMan::saveConfig(MSettings &config) const noexcept
{
    config.setSection(u"Storage"_s, gui.treePartitions);
    for (const Device *const drive : root->children) {
        // Check if the drive is to be cleared and formatted.
        const size_t partCount = drive->children.size();
        config.beginGroup(drive->name);
        if (!drive->flags.oldLayout) {
            config.setString(u"Format"_s, drive->format);
            config.setInteger(u"Partitions"_s, partCount);
        }
        // Partition configuration.
        for (size_t ixPart = 0; ixPart < partCount; ++ixPart) {
            const Device *const part = drive->child(ixPart);
            // Configuration management, accounting for automatic control correction order.
            config.beginGroup(u"Partition"_s + QString::number(ixPart+1));
            config.setInteger(u"Size"_s, part->size);
            if (part->isActive()) config.setBoolean(u"Active"_s, true);
            if (part->flags.sysEFI) config.setBoolean(u"ESP"_s, true);
            if (part->addToCrypttab) config.setBoolean(u"AddToCrypttab"_s, true);
            if (!part->usefor.isEmpty()) config.setString(u"UseFor"_s, part->usefor);
            if (!part->format.isEmpty()) config.setString(u"Format"_s, part->format);
            config.setBoolean(u"Encrypt"_s, part->encrypt);
            if (!part->label.isEmpty()) config.setString(u"Label"_s, part->label);
            if (!part->options.isEmpty()) config.setString(u"Options"_s, part->options);
            config.setBoolean(u"CheckBadBlocks"_s, part->chkbadblk);
            config.setBoolean(u"Dump"_s, part->dump);
            config.setInteger(u"Pass"_s, part->pass);
            size_t subvolCount = 0;
            if (part->format == "btrfs"_L1) {
                subvolCount = part->children.size();
                config.setInteger(u"Subvolumes"_s, subvolCount);
            }
            // Btrfs subvolume configuration.
            for (size_t ixSV=0; ixSV<subvolCount; ++ixSV) {
                const Device *const subvol = part->child(ixSV);
                assert(subvol != nullptr);
                config.beginGroup("Subvolume"_L1 + QString::number(ixSV+1));
                //if (subvol->isActive()) config.setBoolean(u"Default"_s, true);
                if (!subvol->usefor.isEmpty()) config.setString(u"UseFor"_s, subvol->usefor);
                if (!subvol->label.isEmpty()) config.setString(u"Label"_s, subvol->label);
                if (!subvol->options.isEmpty()) config.setString(u"Options"_s, subvol->options);
                config.setBoolean(u"Dump"_s, subvol->dump);
                config.setInteger(u"Pass"_s, subvol->pass);
                config.endGroup(); // Subvolume#
            }
            config.endGroup(); // Partition#
        }
        config.endGroup(); // (drive name)
    }
}

void PartMan::resizeColumnsToFit() noexcept
{
    for (int ixi = TREE_COLUMNS - 1; ixi >= 0; --ixi) {
        if (!gui.treePartitions->isColumnHidden(ixi)) {
            gui.treePartitions->resizeColumnToContents(ixi);
        } else {
            gui.treePartitions->showColumn(ixi);
            gui.treePartitions->resizeColumnToContents(ixi);
            gui.treePartitions->hideColumn(ixi);
        }
    }
}

void PartMan::treeItemChange() noexcept
{
    // Encryption and bad blocks controls
    treeSelChange();
}

void PartMan::treeSelChange() noexcept
{
    const QModelIndexList &indexes = gui.treePartitions->selectionModel()->selectedIndexes();
    Device *seldev = (indexes.size() > 0) ? item(indexes.at(0)) : nullptr;
    if (seldev && seldev->type != Device::SUBVOLUME) {
        const bool isdrive = (seldev->type == Device::DRIVE);
        bool isold = seldev->flags.oldLayout;
        const bool islocked = seldev->isLocked();
        if (isdrive && seldev->flags.curEmpty) isold = false;
        gui.pushPartClear->setEnabled(isdrive && !islocked);
        gui.pushPartRemove->setEnabled(!isold && !isdrive);
        // Only allow adding partitions if there is enough space.
        Device *drive = seldev->parent();
        if (!drive) drive = seldev;
        if (!islocked && isold && isdrive) gui.pushPartAdd->setEnabled(false);
        else {
            const size_t maxparts = (drive->format == "DOS"_L1) ? 4 : PARTMAN_MAX_PARTS;
            gui.pushPartAdd->setEnabled(seldev->driveFreeSpace(true) >= 1*MB
                && !isold && drive->children.size() < maxparts);
        }
    } else {
        gui.pushPartClear->setEnabled(false);
        gui.pushPartAdd->setEnabled(seldev != nullptr);
        gui.pushPartRemove->setEnabled(seldev != nullptr && !seldev->flags.oldLayout);
    }
}

void PartMan::treeMenu(const QPoint &)
{
    const QModelIndexList &ixlist = gui.treePartitions->selectionModel()->selectedIndexes();
    if (ixlist.count() < 1) return;
    const QModelIndex &selIndex = ixlist.at(0);
    if (!selIndex.isValid()) return;
    Device *seldev = item(selIndex);
    if (seldev->type == Device::VIRTUAL_DEVICES) return;
    QMenu menu(gui.treePartitions);
    if (seldev->isVolume()) {
        QAction *actAdd = menu.addAction(tr("&Add partition"));
        actAdd->setEnabled(gui.pushPartAdd->isEnabled());
        QAction *actRemove = menu.addAction(tr("&Remove partition"));
        QAction *actLock = nullptr;
        QAction *actUnlock = nullptr;
        QAction *actAddCrypttab = nullptr;
        QAction *actNewSubvolume = nullptr, *actScanSubvols = nullptr;
        actRemove->setEnabled(gui.pushPartRemove->isEnabled());
        menu.addSeparator();
        if (seldev->type == Device::VIRTUAL) {
            actRemove->setEnabled(false);
            if (seldev->flags.volCrypto) actLock = menu.addAction(tr("&Lock"));
        } else {
            bool allowCryptTab = seldev->encrypt;
            if (seldev->flags.oldLayout && seldev->curFormat == "crypto_LUKS"_L1) {
                actUnlock = menu.addAction(tr("&Unlock"));
                allowCryptTab = true;
            }
            if (allowCryptTab) {
                actAddCrypttab = menu.addAction(tr("Add to crypttab"));
                actAddCrypttab->setCheckable(true);
                actAddCrypttab->setChecked(seldev->addToCrypttab);
            }
        }
        if (seldev->finalFormat() == "btrfs"_L1) {
            menu.addSeparator();
            actNewSubvolume = menu.addAction(tr("New subvolume"));
            actScanSubvols = menu.addAction(tr("Scan subvolumes"));
            actScanSubvols->setDisabled(seldev->willFormat());
        }
        if (seldev->mapCount && actUnlock) actUnlock->setEnabled(false);
        QAction *action = menu.exec(QCursor::pos());
        if (!action) return;
        else if (action == actAdd) partAddClick(true);
        else if (action == actRemove) partRemoveClick(true);
        else if (action == actUnlock) partMenuUnlock(seldev);
        else if (action == actLock) partMenuLock(seldev);
        else if (action == actAddCrypttab) seldev->addToCrypttab = action->isChecked();
        else if (action == actNewSubvolume) {
            Device *subvol = new Device(Device::SUBVOLUME, seldev);
            subvol->autoFill();
            gui.treePartitions->expand(selIndex);
        } else if (action == actScanSubvols) {
            qApp->setOverrideCursor(Qt::WaitCursor);
            gui.boxMain->setEnabled(false);
            scanSubvolumes(seldev);
            gui.treePartitions->expand(selIndex);
            gui.boxMain->setEnabled(true);
            qApp->restoreOverrideCursor();
        }
    } else if (seldev->type == Device::DRIVE) {
        QAction *actAdd = menu.addAction(tr("&Add partition"));
        actAdd->setEnabled(gui.pushPartAdd->isEnabled());
        menu.addSeparator();
        QAction *actClear = menu.addAction(tr("New &layout"));
        QAction *actReset = menu.addAction(tr("&Reset layout"));
        menu.addSeparator();
        QAction *actBuilder = menu.addAction(tr("Layout &Builder..."));

        const bool locked = seldev->isLocked();
        actClear->setDisabled(locked);
        actReset->setDisabled(locked);

        long long minSpace = 0;
        if (!brave && autopart) {
            QStringList excludes;
            minSpace = autopart->layoutHead(seldev, false, false, &excludes);
            minSpace += volSpecTotal(u"/"_s, excludes).minimum;
        }
        actBuilder->setEnabled(!locked && autopart && seldev->size >= minSpace);

        QAction *action = menu.exec(QCursor::pos());
        if (action == actAdd) partAddClick(true);
        else if (action == actClear) partClearClick(true);
        else if (action == actBuilder) {
            if (autopart) autopart->builderGUI(seldev);
            treeSelChange();
        } else if (action == actReset) {
            gui.boxMain->setEnabled(false);
            scan(seldev);
            gui.boxMain->setEnabled(true);
        }
    } else if (seldev->type == Device::SUBVOLUME) {
        // QAction *actDefault = menu.addAction(tr("Default subvolume"));
        QAction *actRemSubvolume = menu.addAction(tr("Remove subvolume"));
        // actDefault->setCheckable(true);
        // actDefault->setChecked(seldev->isActive());
        actRemSubvolume->setDisabled(seldev->flags.oldLayout);

        QAction *action = menu.exec(QCursor::pos());
        if (action == actRemSubvolume) {
            delete seldev;
        }
        // else if (action == actDefault) {
        //     seldev->setActive(action->isChecked());
        // }
    }
}

void PartMan::showAdvancedFields(bool show) noexcept
{
    for (int ixi = 0; ixi < TREE_COLUMNS; ++ixi) {
        if (colprops[ixi].advanced) {
            if (show) {
                gui.treePartitions->showColumn(ixi);
            } else {
                gui.treePartitions->hideColumn(ixi);
            }
        }
    }
}

// Partition manager list buttons
void PartMan::partClearClick(bool)
{
    const QModelIndexList &indexes = gui.treePartitions->selectionModel()->selectedIndexes();
    Device *seldev = (indexes.size() > 0) ? item(indexes.at(0)) : nullptr;
    if (!seldev || seldev->type != Device::DRIVE) return;
    seldev->clear();
    treeSelChange();
}

void PartMan::partAddClick(bool) noexcept
{
    const QModelIndexList &indexes = gui.treePartitions->selectionModel()->selectedIndexes();
    Device *selitem = (indexes.size() > 0) ? item(indexes.at(0)) : nullptr;
    if (!selitem) return;
    Device *volume = selitem->parent();
    if (!volume) volume = selitem;

    Device::DeviceType newtype = Device::PARTITION;
    if (selitem->type == Device::SUBVOLUME) newtype = Device::SUBVOLUME;

    Device *newitem = new Device(newtype, volume, selitem);
    if (newtype == Device::PARTITION) {
        volume->labelParts();
        volume->flags.oldLayout = false;
        notifyChange(volume);
    } else {
        newitem->autoFill();
    }
    gui.treePartitions->selectionModel()->select(index(newitem),
        QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
}

void PartMan::partRemoveClick(bool) noexcept
{
    const QModelIndexList &indexes = gui.treePartitions->selectionModel()->selectedIndexes();
    Device *seldev = (indexes.size() > 0) ? item(indexes.at(0)) : nullptr;
    if (!seldev) return;
    Device *parent = seldev->parent();
    if (!parent) return;
    const bool notSub = (seldev->type != Device::SUBVOLUME);
    delete seldev;
    if (notSub) {
        parent->labelParts();
        treeSelChange();
    }
}
void PartMan::partReloadClick()
{
    QGuiApplication::setOverrideCursor(Qt::WaitCursor);
    gui.boxMain->setEnabled(false);
    scan();
    gui.boxMain->setEnabled(true);
    QGuiApplication::restoreOverrideCursor();
}
void PartMan::partManRunClick()
{
    gui.boxMain->setEnabled(false);
    // Restore the user's locale so the partition editor opens in their language.
    QProcessEnvironment env = proc.processEnvironment();
    env.remove(u"LC_ALL"_s);
    proc.setProcessEnvironment(env);
    if (QFile::exists(u"/usr/bin/gparted"_s)) {
        proc.exec(u"/usr/bin/gparted"_s);
    } else if (QFile::exists(u"/usr/sbin/gparted"_s)) {
        proc.exec(u"/usr/sbin/gparted"_s);
    } else {
        proc.exec(u"partitionmanager"_s);
    }
    env.insert(u"LC_ALL"_s, u"C.UTF-8"_s);
    proc.setProcessEnvironment(env);
    QGuiApplication::setOverrideCursor(Qt::WaitCursor);
    scan();
    gui.boxMain->setEnabled(true);
    QGuiApplication::restoreOverrideCursor();
}

// Partition menu items
void PartMan::partMenuUnlock(Device *part)
{
    QDialog dialog(gui.boxMain);
    QFormLayout layout(&dialog);
    dialog.setWindowTitle(tr("Unlock Drive"));

    QLineEdit *editPass = new QLineEdit(&dialog);
    editPass->setEchoMode(QLineEdit::Password);
    layout.addRow(tr("Password:"), editPass);

    QCheckBox *checkCrypttab = new QCheckBox(tr("Add to crypttab"), &dialog);
    checkCrypttab->setChecked(true);
    layout.addRow(checkCrypttab);

    QDialogButtonBox buttons(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, Qt::Horizontal, &dialog);
    layout.addRow(&buttons);
    connect(&buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(&buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    // Do not allow empty text to be accepted.
    QPushButton *pushOK = buttons.button(QDialogButtonBox::Ok);
    assert(pushOK != nullptr);
    pushOK->setDisabled(true);
    connect(editPass, &QLineEdit::textChanged, this, [pushOK](const QString &text) {
        assert(pushOK != nullptr);
        pushOK->setDisabled(text.isEmpty());
    });

    if (dialog.exec() == QDialog::Accepted) {
        qApp->setOverrideCursor(Qt::WaitCursor);
        gui.boxMain->setEnabled(false);
        if (crypto.open(part, editPass->text().toUtf8())) {
            part->usefor.clear();
            part->addToCrypttab = checkCrypttab->isChecked();
            notifyChange(part);
            scanVirtualDevices(true);
            treeSelChange();
        } else {
            QMessageBox msgbox(gui.boxMain);
            msgbox.setIcon(QMessageBox::Warning);
            msgbox.setText(tr("Could not unlock device."));
            msgbox.setInformativeText(tr("Possible incorrect password."));
            msgbox.exec();
        }
        gui.boxMain->setEnabled(true);
        qApp->restoreOverrideCursor();
    }
}

void PartMan::partMenuLock(Device *volume)
{
    qApp->setOverrideCursor(QCursor(Qt::WaitCursor));
    gui.boxMain->setEnabled(false);
    bool ok = false;
    // Find the associated partition and decrement its reference count if found.
    Device *origin = volume->origin;
    if (crypto.close(volume)) {
        // Notify that the former parent may not have any unlocked children.
        if (origin) notifyChange(origin);
    }
    // Refresh virtual devices list.
    scanVirtualDevices(true);
    treeSelChange();

    gui.boxMain->setEnabled(true);
    qApp->restoreOverrideCursor();
    // If not OK then a command failed, or trying to close a non-LUKS device.
    if (!ok) {
        QMessageBox msgbox(gui.boxMain);
        msgbox.setIcon(QMessageBox::Critical);
        msgbox.setText(tr("Failed to close %1").arg(volume->name));
        msgbox.exec();
    }
}

bool PartMan::validate(bool automatic, QTreeWidgetItem *confroot) const noexcept
{
    QMessageBox msgbox(gui.boxMain);
    msgbox.setIcon(QMessageBox::Critical);
    if (!confroot) confroot = gui.treeConfirm->invisibleRootItem();
    std::map<QString, Device *> mounts;
    // Partition use and other validation
    Device *rootdev = nullptr;
    for (Iterator it(*this); Device *volume = *it; it.next()) {
        if (volume->type == Device::DRIVE || volume->type == Device::VIRTUAL_DEVICES) continue;
        if (volume->type == Device::SUBVOLUME) {
            // Ensure the subvolume label entry is valid.
            bool ok = true;
            const QString &cmptext = volume->label.trimmed();
            static const QRegularExpression labeltest(u"[^A-Za-z0-9\\/\\@\\.\\-\\_]|\\/\\/"_s);
            if (cmptext.isEmpty()) ok = false;
            if (cmptext.count(labeltest)) ok = false;
            if (cmptext.startsWith('/') || cmptext.endsWith('/')) ok = false;
            if (!ok) {
                msgbox.setText(tr("Invalid subvolume label"));
                msgbox.exec();
                return false;
            }
            // Check for duplicate subvolume label entries.
            Device *pit = volume->parentItem;
            assert(pit != nullptr);
            for (Device *sdevice : pit->children) {
                if (sdevice == volume) continue;
                if (sdevice->label.trimmed().compare(cmptext, Qt::CaseInsensitive) == 0) {
                    msgbox.setText(tr("Duplicate subvolume label"));
                    msgbox.exec();
                    return false;
                }
            }
        }
        const QString &mount = volume->mountPoint();
        if (mount.isEmpty()) continue;
        if (mount == "/"_L1) {
            rootdev = volume;
        } else if (!mount.startsWith("/"_L1) && !volume->allowedUsesFor().contains(volume->usefor)) {
            msgbox.setText(tr("Invalid use for %1: %2").arg(volume->shownDevice(), mount));
            msgbox.exec();
            return false;
        }

        // The mount can only be selected once.
        const auto fit = mounts.find(mount);
        if (fit != mounts.cend()) {
            msgbox.setText(tr("%1 is already selected for: %2")
                .arg(fit->second->shownDevice(), fit->second->usefor));
            msgbox.exec();
            return false;
        } else if(volume->canMount(true)) {
            mounts[mount] = volume;
        }
    }

    if (rootdev == nullptr) {
        const long long rootMin = volSpecTotal(u"/"_s).minimum;
        const QString &tMinRoot = QLocale::system().formattedDataSize(rootMin,
            1, QLocale::DataSizeTraditionalFormat);
        msgbox.setText(tr("A root partition of at least %1 is required.").arg(tMinRoot));
        msgbox.exec();
        return false;
    }

    if (!rootdev->willFormat() && mounts.count(u"/home"_s) > 0) {
        bool allowSharedSubvol = false;
        const auto homeIt = mounts.find(u"/home"_s);
        if (homeIt != mounts.cend()) {
            const Device *homeDev = homeIt->second;
            if (homeDev && homeDev->type == Device::SUBVOLUME && rootdev->type == Device::SUBVOLUME) {
                const Device *homeParent = homeDev->parent();
                const Device *rootParent = rootdev->parent();
                if (homeParent && rootParent && homeParent == rootParent) {
                    allowSharedSubvol = true;
                }
            }
        }
        if (!allowSharedSubvol) {
            msgbox.setText(tr("Cannot preserve /home inside root (/)"
                " if a separate /home partition is also mounted."));
            msgbox.exec();
            return false;
        }
    }

    // Confirmation page action list
    for (const Device *drive : std::as_const(root->children)) {
        QTreeWidgetItem *twdrive = new QTreeWidgetItem(confroot);
        if (drive->type == Device::VIRTUAL_DEVICES) {
            twdrive->setText(0, tr("Virtual Devices"));
        } else if (drive->flags.oldLayout) {
            twdrive->setText(0, tr("Re-use partition table on %1").arg(drive->friendlyName()));
        } else {
            twdrive->setText(0, tr("Prepare %1 partition table on %2").arg(drive->format, drive->friendlyName()));
        }

        for (const Device *part : std::as_const(drive->children)) {
            QString actmsg;
            if (drive->flags.oldLayout || drive->type == Device::VIRTUAL_DEVICES) {
                if (part->usefor.isEmpty()) {
                    if (part->children.size() > 0) actmsg = tr("Reuse (no reformat) %1");
                    else continue;
                } else {
                    if (part->usefor == "FORMAT"_L1) actmsg = tr("Format %1");
                    else if (part->willFormat()) actmsg = tr("Format %1 to use for %2");
                    else if (part->usefor != "/") actmsg = tr("Reuse (no reformat) %1 as %2");
                    else actmsg = tr("Delete the data on %1 except for /home, to use for %2");
                }
            } else {
                if (part->usefor.isEmpty()) actmsg = tr("Create %1 without formatting");
                else actmsg = tr("Create %1, format to use for %2");
            }
            // QString::arg() emits warnings if a marker is not in the string.
            QTreeWidgetItem *twpart = new QTreeWidgetItem(twdrive);
            twpart->setText(0, actmsg.replace("%1"_L1, part->shownDevice()).replace("%2"_L1, part->usefor));
            if (part->origin) {
                twpart->setToolTip(0, part->origin->friendlyName());
            } else {
                twpart->setToolTip(0, part->friendlyName());
            }

            for (const Device *subvol : std::as_const(part->children)) {
                const bool svnouse = subvol->usefor.isEmpty();
                if (subvol->format == "PRESERVE"_L1) {
                    if (svnouse) continue;
                    else actmsg = tr("Reuse subvolume %1 as %2");
                } else if (subvol->format == "DELETE"_L1) {
                    actmsg = tr("Delete subvolume %1");
                } else if (subvol->format == "CREATE"_L1) {
                    if (subvol->flags.oldLayout) {
                        if (svnouse) actmsg = tr("Overwrite subvolume %1");
                        else actmsg = tr("Overwrite subvolume %1 to use for %2");
                    } else {
                        if (svnouse) actmsg = tr("Create subvolume %1");
                        else actmsg = tr("Create subvolume %1 to use for %2");
                    }
                }
                // QString::arg() emits warnings if a marker is not in the string.
                QTreeWidgetItem *twsubvol = new QTreeWidgetItem(twpart);
                twsubvol->setText(0, actmsg.replace("%1"_L1, subvol->label).replace("%2"_L1, subvol->usefor));
            }
        }
        if (twdrive->childCount() < 1) delete twdrive;
    }
    if (!automatic) {
        // Warning messages
        if (rootdev->willEncrypt() && mounts.count(u"/boot"_s)==0) {
            msgbox.setIcon(QMessageBox::Warning);
            msgbox.setStandardButtons(QMessageBox::Yes|QMessageBox::No);
            msgbox.setDefaultButton(QMessageBox::No);
            msgbox.setText(tr("You must choose a separate boot partition when encrypting root."));
            if (msgbox.exec() != QMessageBox::Yes) return false;
        }
        if (!confirmBootable()) return false;
        if (!confirmSpace()) return false;
    }

    return true;
}
bool PartMan::confirmSpace() const noexcept
{
    QMessageBox msgbox(gui.boxMain);
    msgbox.setIcon(QMessageBox::Warning);
    msgbox.setStandardButtons(QMessageBox::Yes|QMessageBox::No);
    msgbox.setDefaultButton(QMessageBox::No);

    // Isolate used points from each other in total calculations
    QStringList excludes;
    for (Iterator it(*this); *it; it.next()) {
        const QString &use = (*it)->usefor;
        if (!use.isEmpty()) excludes.append(use);
    }

    QStringList toosmall;
    for (const Device *drive : std::as_const(root->children)) {
        for (const Device *part : std::as_const(drive->children)) {
            bool isused = !part->usefor.isEmpty();
            long long minsize = isused ? volSpecTotal(part->usefor, excludes).minimum : 0;

            // First pass = get the total minimum required for all subvolumes.
            for (const Device *subvol : std::as_const(part->children)) {
                if(!subvol->usefor.isEmpty()) {
                    minsize += volSpecTotal(subvol->usefor, excludes).minimum;
                    isused = true;
                }
            }
            // If this volume is too small, add to the warning list.
            if (isused && part->size < minsize) {
                const QString &msgsz = tr("%1 (%2) requires %3");
                toosmall << msgsz.arg(part->usefor, part->shownDevice(),
                    QLocale::system().formattedDataSize(minsize, 1, QLocale::DataSizeTraditionalFormat));

                // Add all subvolumes (sorted) to the warning list as one string.
                QStringList svsmall;
                for (const Device *subvol : std::as_const(part->children)) {
                    if (subvol->usefor.isEmpty()) continue;

                    const long long svmin = volSpecTotal(subvol->usefor, excludes).minimum;
                    if (svmin > 0) {
                        svsmall << msgsz.arg(subvol->usefor, subvol->shownDevice(),
                            QLocale::system().formattedDataSize(svmin, 1, QLocale::DataSizeTraditionalFormat));
                    }
                }
                if (!svsmall.isEmpty()) {
                    svsmall.sort();
                    toosmall.last() += "<ul style='margin:0; list-style-type:circle'><li>"_L1
                        + svsmall.join(u"</li><li>"_s) + "</li></ul>"_L1;
                }
            }
        }
    }

    if (!toosmall.isEmpty()) {
        toosmall.sort();
        msgbox.setText(tr("The installation may fail because the following volumes are too small:")
            + "<br/><ul style='margin:0'><li>"_L1 + toosmall.join(u"</li><li>"_s) + "</li></ul><br/>"_L1
            + tr("Are you sure you want to continue?"));
        if (msgbox.exec() != QMessageBox::Yes) return false;
    }
    return true;
}
bool PartMan::confirmBootable() const noexcept
{
    QMessageBox msgbox(gui.boxMain);
    msgbox.setIcon(QMessageBox::Warning);
    msgbox.setStandardButtons(QMessageBox::Yes|QMessageBox::No);
    msgbox.setDefaultButton(QMessageBox::No);
    msgbox.setText(tr("This setup may produce an unbootable system. Do you want to continue?"));

    if (core.detectEFI()) {
        const char *msgtext = nullptr;
        const PartMan::Device *efidev = findByMount(u"/boot/efi"_s);
        if (efidev == nullptr) {
            msgtext = QT_TR_NOOP("This system uses EFI, but no valid"
                " EFI system partition was assigned to /boot/efi separately.");
        } else if (!efidev->flags.sysEFI) {
            msgtext = QT_TR_NOOP("The volume assigned to /boot/efi"
                " is not a valid EFI system partition.");
        }
        if (msgtext) {
            msgbox.setInformativeText(msgtext);
            if (msgbox.exec() != QMessageBox::Yes) return false;
        }
        return true;
    }

    // BIOS with GPT
    QString biosgpt;
    for (const Device *drive : std::as_const(root->children)) {
        bool hasBiosGrub = false;
        for (const Device *part : std::as_const(drive->children)) {
            if (part->finalFormat() == "BIOS-GRUB"_L1) {
                hasBiosGrub = true;
                break;
            }
        }
        // Potentially unbootable GPT when on a BIOS-based PC.
        const bool hasBoot = (drive->active != nullptr);
        if (!core.detectEFI() && drive->format=="GPT"_L1 && hasBoot && !hasBiosGrub) {
            biosgpt += ' ' + drive->name;
        }
    }
    if (!biosgpt.isEmpty()) {
        biosgpt.prepend(tr("The following drives are, or will be, setup with GPT,"
            " but do not have a BIOS-GRUB partition:") + "\n\n"_L1);
        biosgpt += "\n\n"_L1 + tr("This system may not boot from GPT drives without a BIOS-GRUB partition.");
        msgbox.setInformativeText(biosgpt);
        if (msgbox.exec() != QMessageBox::Yes) return false;
    }
    return true;
}

// Checks SMART status of the selected drives.
// Returns false if it detects errors and user chooses to abort.
bool PartMan::checkTargetDrivesOK() const
{
    MProcess::Section sect(proc, nullptr); // No exception on execution error for this block.
    QString smartFail, smartWarn;
    for (Device *drive : root->children) {
        if (drive->type == Device::VIRTUAL_DEVICES) continue;
        QStringList purposes;
        for (Iterator it(drive); const Device *device = *it; it.next()) {
            if (!device->usefor.isEmpty()) purposes << device->usefor;
        }
        // If any partitions are selected run the SMART tests.
        if (!purposes.isEmpty()) {
            QString smartMsg = drive->name + " ("_L1 + purposes.join(u", "_s) + ")"_L1;
            proc.exec(u"smartctl"_s, {u"-H"_s, drive->path});
            if (proc.exitStatus() == MProcess::NormalExit && proc.exitCode() & 8) {
                // See smartctl(8) manual: EXIT STATUS (Bit 3)
                smartFail += " - "_L1 + smartMsg + "\n"_L1;
            } else {
                proc.shell("smartctl -A "_L1 + drive->path + "| grep -E \"^  5|^196|^197|^198\""
                    " | awk '{ if ( $10 != 0 ) { print $1,$2,$10} }'"_L1, nullptr, true);
                const QString &out = proc.readOut();
                if (!out.isEmpty()) {
                    smartWarn += " ---- "_L1 + smartMsg + " ---\n"_L1 + out + "\n\n"_L1;
                }
            }
        }
    }

    QString msg;
    if (!smartFail.isEmpty()) {
        msg = tr("The disks with the partitions you selected for installation are failing:")
            + "\n\n"_L1 + smartFail + "\n"_L1;
    }
    if (!smartWarn.isEmpty()) {
        msg += tr("Smartmon tool output:") + "\n\n"_L1 + smartWarn
            + tr("The disks with the partitions you selected for installation pass the SMART monitor test (smartctl),"
                " but the tests indicate it will have a higher than average failure rate in the near future.");
    }
    if (!msg.isEmpty()) {
        QMessageBox msgbox(gui.boxMain);
        msgbox.setStandardButtons(QMessageBox::Yes|QMessageBox::No);
        msgbox.setDefaultButton(QMessageBox::Yes);
        msg += tr("If unsure, please exit the Installer and run GSmartControl for more information.") + "\n\n"_L1;
        if (!smartFail.isEmpty()) {
            msgbox.setIcon(QMessageBox::Critical);
            msgbox.setEscapeButton(QMessageBox::Yes);
            msg += tr("Do you want to abort the installation?");
            msgbox.setText(msg);
            if (msgbox.exec() == QMessageBox::Yes) return false;
        } else {
            msgbox.setIcon(QMessageBox::Warning);
            msgbox.setEscapeButton(QMessageBox::No);
            msg += tr("Do you want to continue?");
            msgbox.setText(msg);
            if (msgbox.exec() != QMessageBox::Yes) return false;
        }
    }
    return true;
}

int PartMan::countPrepSteps() noexcept
{
    int nstep = 0;
    for (Iterator it(*this); Device *device = *it; it.next()) {
        if (device->type == Device::DRIVE) {
            if (!device->flags.oldLayout) ++nstep; // New partition table
        } else if (device->isVolume()) {
            // Preparation
            if (!device->flags.oldLayout) ++nstep; // New partition
            else if (!device->usefor.isEmpty()) ++nstep; // Existing partition
            // Formatting
            if (device->encrypt) nstep += 2; // LUKS Format
            if (device->willFormat()) ++nstep; // New file system
            // Mounting
            if (device->usefor.startsWith('/')) ++nstep;
        } else if (device->type == Device::SUBVOLUME) {
            ++nstep; // Create a new subvolume.
        }
    }
    return nstep;
}
void PartMan::prepStorage()
{
    // For debugging
    qDebug() << "Mount points:";
    for (Iterator it(*this); Device *dev = *it; it.next()) {
        if (dev->canMount(false)) {
            qDebug() << " -" << dev->mountPoint()
                << '-' << dev->shownDevice() << dev->map << dev->path;
        }
    }

    // Prepare and mount storage
    preparePartitions();
    crypto.formatAll(*this);
    formatPartitions();
    mountPartitions();
    // Refresh the UUIDs
    proc.exec(u"lsblk"_s, {u"--list"_s, u"-bJo"_s, u"PATH,UUID"_s}, nullptr, true);
    const QJsonObject &jsonObjBD = QJsonDocument::fromJson(proc.readOut(true).toUtf8()).object();
    const QJsonArray &jsonBD = jsonObjBD[u"blockdevices"_s].toArray();
    std::map<QString, Device *> alldevs;
    for (Iterator it(*this); Device *device = *it; it.next()) {
        alldevs[device->path] = device;
    }
    for (const QJsonValue &jsonDev : jsonBD) {
        const auto fit = alldevs.find(jsonDev[u"path"_s].toString());
        if (fit != alldevs.cend()) fit->second->uuid = jsonDev[u"uuid"_s].toString();
    }
}
bool PartMan::installTabs() noexcept
{
    proc.log(u"Install tabs"_s);
    if (!makeFstab()) return false;
    if (!crypto.makeCrypttab(*this)) return false;
    return true;
}

void PartMan::preparePartitions()
{
    proc.log(__PRETTY_FUNCTION__, MProcess::LOG_MARKER);
    MProcess::Section sect(proc, QT_TR_NOOP("Failed to prepare required partitions."));

    // Detach all existing partitions.
    QStringList listToUnmount;
    for (const Device *drive : std::as_const(root->children)) {
        if (drive->type == Device::VIRTUAL_DEVICES) continue;
        if (drive->flags.oldLayout) {
            // Using the existing layout, so only mark used partitions for unmounting.
            for (const Device *part : std::as_const(drive->children)) {
                if (!part->usefor.isEmpty()) {
                    listToUnmount << part->path;
                    const QString mapped = part->mappedDevice();
                    if (!mapped.isEmpty() && mapped != part->path) listToUnmount << mapped;
                }
            }
        } else {
            // Clearing the drive, so mark all partitions on the drive for unmounting.
            proc.exec(u"lsblk"_s, {u"-nro"_s, u"path"_s, drive->path}, nullptr, true);
            listToUnmount << proc.readOutLines();
        }
    }

    // Clean up any leftovers.
    {
        MProcess::Section sect2(proc, nullptr);
        if (QFileInfo::exists(u"/mnt/antiX"_s)) {
            proc.exec(u"umount"_s, {u"-qR"_s, u"/mnt/antiX"_s});
            QDir dir(u"/mnt/antiX"_s);
            dir.removeRecursively();
        }
        // Detach swap and file systems of targets which may have been auto-mounted.
        proc.exec(u"swapon"_s, {u"--show=NAME"_s, u"--noheadings"_s}, nullptr, true);
        const QStringList swaps = proc.readOutLines();
        proc.exec(u"findmnt"_s, {u"--raw"_s, u"-o"_s, u"SOURCE"_s}, nullptr, true);
        const QStringList fsmounts = proc.readOutLines();
        for (const QString &devpath : std::as_const(listToUnmount)) {
            if (swaps.contains(devpath)) proc.exec(u"swapoff"_s, {devpath});
            if (fsmounts.contains(devpath)) proc.exec(u"umount"_s, {u"-q"_s, devpath});
        }
    }

    // Prepare partition tables on devices which will have a new layout.
    for (const Device *drive : std::as_const(root->children)) {
        if (drive->flags.oldLayout || drive->type == Device::VIRTUAL_DEVICES) continue;
        proc.status(tr("Preparing partition tables"));

        // Wipe the first and last 4MB to clear the partition tables, turbo-nuke style.
        const int gran = std::max(drive->discgran, drive->physec);
        QString opts = drive->discgran ? u"-fv"_s : u"-fvz"_s;
        if (core.detectVirtualBox()) {
            opts = u"-fvz"_s; // VirtualBox incorrectly reports TRIM support.
        }
        // First 17KB = primary partition table (accounts for both MBR and GPT disks).
        // First 17KB, from 32KB = sneaky iso-hybrid partition table (maybe USB with an ISO burned onto it).
        const long long length = (4*MB + (gran - 1)) / gran; // ceiling
        sect.setExceptionStrict(false);
        if (proc.exec(u"blkdiscard"_s, {opts, u"-l"_s, QString::number(length*gran), drive->path})) {
            sect.setExceptionStrict(true);
        } else {
            sect.setExceptionStrict(true);
            opts = u"-fvz"_s; // use zero instead of failing - but reported - trim support
            proc.exec(u"blkdiscard"_s, {opts, u"-l"_s, QString::number(length*gran), drive->path});
        }
        // Last 17KB = secondary partition table (for GPT disks).
        const long long offset = (drive->size - 4*MB) / gran; // floor
        proc.exec(u"blkdiscard"_s, {opts, u"-o"_s, QString::number(offset*gran), drive->path});

        proc.exec(u"parted"_s, {u"-s"_s, drive->path, u"mklabel"_s,
            (drive->format=="GPT"_L1 ? u"gpt"_s : u"msdos"_s)});
    }

    // Prepare partition tables, creating tables and partitions when needed.
    proc.status(tr("Preparing required partitions"));
    for (const Device *drive : std::as_const(root->children)) {
        bool partupdate = false;
        if (drive->type == Device::VIRTUAL_DEVICES) continue;
        const int devCount = drive->children.size();
        const bool useGPT = (drive->finalFormat() == "GPT"_L1);
        if (drive->flags.oldLayout) {
            // Using existing partitions.
            QString cmd; // command to set the partition type
            if (useGPT) cmd = "sgdisk /dev/%1 -q --typecode=%2:%3"_L1;
            else cmd = "sfdisk /dev/%1 -q --part-type %2 %3"_L1;
            // Set the type for partitions that will be used in this installation.
            for (int ixdev = 0; ixdev < devCount; ++ixdev) {
                Device *part = drive->child(ixdev);
                assert(part != nullptr);
                if (part->usefor.isEmpty()) continue;
                const char *ptype = useGPT ? "8303" : "83";
                if (part->flags.sysEFI) ptype = useGPT ? "ef00" : "ef";
                const NameParts &devsplit = splitName(part->name);
                proc.shell(cmd.arg(devsplit.drive, devsplit.partition, ptype));
                partupdate = true;
                proc.status();
            }
        } else {
            // Creating new partitions.
            long long start = 1; // start with 1 MB to aid alignment
            for (int ixdev = 0; ixdev<devCount; ++ixdev) {
                Device *part = drive->child(ixdev);
                const long long end = start + part->size / MB;
                proc.exec(u"parted"_s, {u"-s"_s, u"--align"_s, u"optimal"_s, drive->path,
                    u"mkpart"_s , u"primary"_s, QString::number(start) + "MiB"_L1, QString::number(end) + "MiB"_L1});
                partupdate = true;
                start = end;
                proc.status();
            }
        }
        // Partition flags.
        for (int ixdev=0; ixdev<devCount; ++ixdev) {
            Device *part = drive->child(ixdev);
            if (part->usefor.isEmpty()) continue;
            if (part->isActive()) {
                const NameParts &devsplit = splitName(part->name);
                QStringList cargs({u"-s"_s, "/dev/"_L1 + devsplit.drive, u"set"_s, devsplit.partition});
                cargs.append(useGPT ? u"legacy_boot"_s : u"boot"_s);
                cargs.append(u"on"_s);
                proc.exec(u"parted"_s, cargs);
                partupdate = true;
            }
            if (part->flags.sysEFI != part->flags.curESP) {
                const NameParts &devsplit = splitName(part->name);
                proc.exec(u"parted"_s, {u"-s"_s, "/dev/"_L1 + devsplit.drive, u"set"_s, devsplit.partition,
                    u"esp"_s, part->flags.sysEFI ? u"on"_s : u"off"_s});
                partupdate = true;
            }
            proc.status();
        }
        // Update kernel partition records.
        if (partupdate) proc.exec(u"partx"_s, {u"-u"_s, drive->path});
    }
}

void PartMan::formatPartitions()
{
    proc.log(__PRETTY_FUNCTION__, MProcess::LOG_MARKER);
    MProcess::Section sect(proc, QT_TR_NOOP("Failed to format partition."));

    // Format partitions.
    for (Iterator it(*this); Device *volume = *it; it.next()) {
        if (volume->type != Device::PARTITION && volume->type != Device::VIRTUAL) continue;
        if (!volume->willFormat()) continue;
        const QString &dev = volume->mappedDevice();
        const QString &fmtstatus = tr("Formatting: %1");
        if (volume->usefor == "FORMAT"_L1) proc.status(fmtstatus.arg(volume->name));
        else proc.status(fmtstatus.arg(volume->usefor));
        if (volume->usefor == "BIOS-GRUB"_L1) {
            if (core.detectVirtualBox()) {
                proc.exec(u"blkdiscard"_s, {u"-fvz"_s, dev}); // VirtualBox incorrectly reports TRIM support.
            } else {
                sect.setExceptionStrict(false);
                if (! proc.exec(u"blkdiscard"_s, {volume->discgran ? u"-fv"_s : u"-fvz"_s, dev})) {
                    sect.setExceptionStrict(true);
                    proc.exec(u"blkdiscard"_s, {u"-fvz"_s, dev});
                }
                sect.setExceptionStrict(true);
            }
            const NameParts &devsplit = splitName(dev);
            proc.exec(u"parted"_s, {u"-s"_s, "/dev/"_L1 + devsplit.drive,
                u"set"_s, devsplit.partition, u"bios_grub"_s, u"on"_s});
        } else if (volume->usefor == "SWAP"_L1) {
            QStringList cargs({u"-q"_s, dev});
            if (!volume->label.isEmpty()) cargs << u"-L"_s << volume->label;
            proc.exec(u"mkswap"_s, cargs);
        } else if (volume->format.left(3) == "FAT"_L1) {
            QStringList cargs({u"-F"_s, volume->format.mid(3)});
            if (volume->chkbadblk) cargs.append(u"-c"_s);
            if (!volume->label.isEmpty()) cargs << u"-n"_s << volume->label.trimmed().left(11);
            cargs.append(dev);
            proc.exec(u"mkfs.msdos"_s, cargs);
        } else {
            // Transplanted from minstall.cpp and modified to suit.
            const QString &format = volume->format;
            QStringList cargs;
            if (format == "btrfs"_L1) {
                cargs.append(u"-f"_s);
                const QString fsckAuto =
                    QFileInfo(u"/usr/bin/fsck.auto"_s).exists() ? u"/usr/bin/fsck.auto"_s : u"/usr/sbin/fsck.auto"_s;
                proc.exec(u"cp"_s, {u"-fp"_s, u"/usr/bin/true"_s, fsckAuto});
                if (volume->size < 6000000000) {
                    cargs << u"-M"_s << u"-O"_s << u"skinny-metadata"_s; // Mixed mode (-M)
                }
            } else if (format == "xfs"_L1 || format == "f2fs"_L1) {
                cargs.append(u"-f"_s);
            } else { // jfs, ext2, ext3, ext4
                if (format == "jfs"_L1) cargs.append(u"-q"_s);
                else cargs.append(u"-F"_s);
                if (volume->chkbadblk) cargs.append(u"-c"_s);
            }
            cargs.append(dev);
            if (!volume->label.isEmpty()) {
                if (format == "f2fs"_L1) cargs.append(u"-l"_s);
                else cargs.append(u"-L"_s);
                cargs.append(volume->label);
            }
            proc.exec("mkfs."_L1 + format, cargs);
            if (format.startsWith("ext"_L1)) {
                proc.exec(u"tune2fs"_s, {u"-c0"_s, u"-C0"_s, u"-i1m"_s, dev}); // ext4 tuning
            }
        }
    }
    // Prepare subvolumes on all that (are to) contain them.
    for (Iterator it(*this); Device *volume = *it; it.next()) {
        if (!volume->isVolume()) continue;
        if (volume->finalFormat() != "btrfs"_L1) continue;
        prepareSubvolumes(volume);
        for (Device *subvol : volume->children) {
            if (!subvol) continue;
            if (subvol->usefor.isEmpty()) continue;
            if (!subvol->usefor.startsWith('/')) continue;
            changeBegin(subvol);
            subvol->format = "PRESERVE"_L1;
            changeEnd(false, false);
        }
    }
}

void PartMan::prepareSubvolumes(Device *part)
{
    const int subvolcount = part->children.size();
    if (subvolcount <= 0) return;
    MProcess::Section sect(proc, QT_TR_NOOP("Failed to prepare subvolumes."));
    proc.status(tr("Preparing subvolumes"));
    const QString &scratchpath = u"/mnt/scratch"_s;

    proc.exec(u"mount"_s, {u"--mkdir"_s, u"-o"_s, u"subvolid=5,noatime"_s, part->mappedDevice(), scratchpath});
    const char *errmsg = nullptr;
    try {
        // Current default subvolume, which could be on the volume itself, but not in the device tree.
        proc.exec(u"btrfs"_s, {u"subvolume"_s, u"get-default"_s, scratchpath}, nullptr, true);
        QString subdefault = proc.readOut(false);
        const qsizetype start = subdefault.indexOf("path "_L1);
        if (start != -1 && subdefault.size() >= 6) {
            subdefault.slice(start + 5);
        } else {
            subdefault.clear();
        }

        for (int ixsv = 0; ixsv < subvolcount; ++ixsv) {
            Device *subvol = part->child(ixsv);
            assert(subvol != nullptr);
            const QString &svpath = scratchpath + '/' + subvol->label;
            if (subvol->format != "PRESERVE"_L1 && QFileInfo::exists(svpath)) {
                const QString &svstart = subvol->label + '/';
                // Since the default subvolume cannot be deleted, set it to the top-level before deleting.
                if (subdefault == subvol->label || subdefault.startsWith(svstart)) {
                    proc.exec(u"btrfs"_s, {u"subvolume"_s, u"set-default"_s, u"5"_s, scratchpath});
                    subdefault.clear();
                }
                // Remove this subvolume and all nested subvolumes within.
                proc.exec(u"btrfs"_s, {u"-q"_s, u"subvolume"_s, u"delete"_s, u"--recursive"_s, svpath});
            }
            if (subvol->format == "CREATE"_L1) {
                proc.exec(u"btrfs"_s, {u"-q"_s, u"subvolume"_s, u"create"_s, svpath});
            }
            proc.status();
        }
        // // Set the default subvolume if one was chosen.
        // if (part->active) {
        //     proc.exec(u"btrfs"_s, {u"-q"_s, u"subvolume"_s, u"set-default"_s,
        //         scratchpath + '/' + part->active->label});
        // }
    } catch(const char *msg) {
        errmsg = msg;
    }
    proc.exec(u"umount"_s, {scratchpath});
    if (errmsg) throw errmsg;
}

// create /etc/fstab file
bool PartMan::makeFstab() noexcept
{
    QFile file(u"/mnt/antiX/etc/fstab"_s);
    if (!file.open(QIODevice::WriteOnly)) return false;
    QTextStream out(&file);
    out << "# Pluggable devices are handled by uDev, they are not in fstab\n";
    // Use std::map to arrange mounts in alphabetical (thus, hierarchical) order.
    std::map<QString, Device *> mounts;
    for (Iterator it(*this); Device *dev = *it; it.next()) {
        const QString &mount = dev->mountPoint();
        if (!mount.isEmpty()) {
            mounts[mount] = dev;
        }
    }
    // File systems and swap space.
    for (auto &it : mounts) {
        const Device *volume = it.second;
        const QString &dev = volume->mappedDevice();
        qDebug() << "Creating fstab entry for:" << it.first << dev;
        // Device ID or UUID
        if (volume->willMap()){
            out << dev;
        } else {
            QString UUID = volume->assocUUID();
            //fallback UUID
            //some btrfs systems show incorrect UUID for volume, and so parent UUID never found
            if (UUID.isEmpty()){
                MProcess::Section sect(proc, nullptr);
                if (!proc.exec(u"blkid"_s, {u"-p"_s, u"-s"_s,u"UUID"_s, dev}, nullptr, true)) {
                    return false; // TODO: review noexcept status of this function.
                }
                UUID = proc.readOut().section('=', 1, 1).section('\"',1,1);
                qDebug() << "UUID:" << UUID;
            }
            out << "UUID=" << UUID;
        }
        // Mount point
        out << ' ' << it.first;
        // File system
        QString fsfmt = volume->finalFormat();
        if (volume->willMap() && fsfmt == "crypto_LUKS"_L1) {
            const QString mappedPath = volume->mappedDevice();
            if (!mappedPath.isEmpty()) {
                Device *mappedDev = findByPath(mappedPath);
                if (mappedDev) {
                    const QString mappedFmt = mappedDev->finalFormat();
                    if (!mappedFmt.isEmpty() && mappedFmt != "crypto_LUKS"_L1) {
                        fsfmt = mappedFmt;
                    } else if (!mappedDev->curFormat.isEmpty()) {
                        fsfmt = mappedDev->curFormat;
                    }
                }
            }
        }
        if (fsfmt.startsWith("FAT"_L1)) out << " vfat";
        else out << ' ' << fsfmt;
        // Options
        const QString &mountopts = volume->options;
        if (volume->type == Device::SUBVOLUME) {
            //set format information if fsfmt is empty
            if (fsfmt.isEmpty()){
                out << " btrfs";
            }
            out << " subvol=" << volume->label;
            if (!mountopts.isEmpty()) out << ',' << mountopts;
        } else {
            if (mountopts.isEmpty()) out << " defaults";
            else out << ' ' << mountopts;
        }
        // Dump, pass
        out << ' ' << (volume->dump ? 1 : 0) << ' ' << volume->pass << '\n';
    }
    file.close();
    return true;
}

void PartMan::mountPartitions()
{
    proc.log(__PRETTY_FUNCTION__, MProcess::LOG_MARKER);
    MProcess::Section sect(proc, QT_TR_NOOP("Failed to mount partition."));

    // Use std::map to arrange mounts in alphabetical (thus, hierarchical) order.
    std::map<QString, Device *> mounts;
    for (Iterator it(*this); Device *dev = *it; it.next()) {
        const QString &mount = dev->mountPoint();
        if (!mount.isEmpty() && mount.at(0) == '/') {
            mounts[mount] = dev;
        }
    }
    for (auto &it : mounts) {
        const QString point("/mnt/antiX"_L1 + it.first);
        const QString &dev = it.second->mappedDevice();
        proc.status(tr("Mounting: %1").arg(it.first));
        QStringList opts;
        if (it.second->type == Device::SUBVOLUME) {
            opts.append("subvol="_L1 + it.second->label);
        }
        // Use noatime to speed up the installation.
        opts.append(it.second->options.split(','));
        opts.removeAll("ro");
        opts.removeAll("defaults");
        opts.removeAll("atime");
        opts.removeAll("relatime");
        if (it.second->finalFormat() == "ext4"_L1 && !opts.contains(u"noinit_itable"_s)) {
            opts.append(u"noinit_itable"_s);
        }
        if (!opts.contains(u"async"_s)) opts.append(u"async"_s);
        if (!opts.contains(u"noiversion"_s)) opts.append(u"noiversion"_s);
        if (!opts.contains(u"noatime"_s)) opts.append(u"noatime"_s);
        if (!opts.contains(u"lazytime"_s)) opts.append(u"lazytime"_s);
        proc.exec(u"mount"_s, {u"--mkdir"_s, u"-o"_s, opts.join(','), dev, point});
        if (proc.exec(u"findmnt"_s, {u"-no"_s, u"OPTIONS"_s, point}, nullptr, true)) {
            const QString options = proc.readOut(true);
            if (options.contains(u"ro"_s)) {
                proc.exec(u"mount"_s, {u"-o"_s, u"remount,rw"_s, dev, point});
            }
        }
    }
}

void PartMan::clearWorkArea()
{
    // Close swap files that may have been opened (should not have swap opened in the first place)
    proc.exec(u"swapon"_s, {u"--show=NAME"_s, u"--noheadings"_s}, nullptr, true);
    QStringList swaps = proc.readOutLines();
    for (Iterator it(*this); *it; it.next()) {
        const QString &dev = (*it)->mappedDevice();
        if (swaps.contains(dev)) proc.exec(u"swapoff"_s, {dev});
    }
    // Unmount everything in /mnt/antiX which is only to be for working on the target system.
    if (QFileInfo::exists(u"/mnt/antiX"_s)) proc.exec(u"umount"_s, {u"-qR"_s, u"/mnt/antiX"_s});
    // Clean up empty mount directories
    QDir antiXDir(u"/mnt/antiX"_s);
    if (antiXDir.exists() && antiXDir.entryList(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden).count() == 0) {
        antiXDir.removeRecursively();
    }
    QDir scratchDir(u"/mnt/scratch"_s);
    if (scratchDir.exists() && scratchDir.entryList(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden).count() == 0) {
        scratchDir.removeRecursively();
    }
    // Close encrypted containers that were opened by the installer.
    QStringList closedMaps;
    for (Iterator it(*this); Device *device = *it; it.next()) {
        if (!device->map.isEmpty() && device->type != Device::VIRTUAL) {
            if (closedMaps.contains(device->map)) continue;
            const QString mapperPath = u"/dev/mapper/"_s + device->map;
            if (!QFile::exists(mapperPath)) continue;
            if (crypto.close(device)) {
                closedMaps.append(device->map);
                device->mapCount = 0;
                device->addToCrypttab = false;
                notifyChange(device);
            }
        }
    }
}

PartMan::Device *PartMan::findByPath(const QString &devpath) const noexcept
{
    for (Iterator it(*this); *it; it.next()) {
        if ((*it)->path == devpath) return *it;
    }
    return nullptr;
}
PartMan::Device *PartMan::findByMount(const QString &mount) const noexcept
{
    for (Iterator it(*this); *it; it.next()) {
        if ((*it)->mountPoint() == mount) return *it;
    }
    return nullptr;
}
PartMan::Device *PartMan::findHostDev(const QString &path) const noexcept
{
    QString spath = path;
    Device *device = nullptr;
    do {
        spath = QDir::cleanPath(QFileInfo(spath).path());
        device = findByMount(spath);
    } while(!device && !spath.isEmpty() && spath != '/' && spath != '.');
    return device;
}

struct PartMan::VolumeSpec PartMan::volSpecTotal(const QString &path, const QStringList &excludes) const noexcept
{
    struct VolumeSpec vspec = {0};
    const auto fit = volSpecs.find(path);
    if (fit != volSpecs.cend()) vspec = fit->second;
    const QString &spath = (path!='/' ? path+'/' : path);
    for (const auto &it : volSpecs) {
        if (it.first.size() > 1 && it.first.startsWith(spath) && !excludes.contains(it.first)) {
            vspec.image += it.second.image;
            vspec.minimum += it.second.minimum;
            vspec.preferred += it.second.preferred;
        }
    }
    return vspec;
}
struct PartMan::VolumeSpec PartMan::volSpecTotal(const QString &path, bool excludeChildMounts) const noexcept
{
    QStringList excludes;
    if (excludeChildMounts) {
        for (Iterator it(*this); *it; it.next()) {
            if ((*it)->canMount()) {
                excludes.append((*it)->mountPoint());
            }
        }
    }
    return volSpecTotal(path, excludes);
}

/*************************\
 * Model View Controller *
\*************************/

QVariant PartMan::data(const QModelIndex &index, int role) const noexcept
{
    Device *device = static_cast<Device*>(index.internalPointer());
    const bool isDriveOrVD = (device->type == Device::DRIVE || device->type == Device::VIRTUAL_DEVICES);
    if (role == Qt::EditRole) {
        switch (index.column())
        {
        case COL_DEVICE: return device->name; break;
        case COL_SIZE: return device->size; break;
        case COL_USEFOR: return device->usefor; break;
        case COL_LABEL: return device->label; break;
        case COL_FORMAT: return device->format; break;
        case COL_OPTIONS: return device->options; break;
        case COL_PASS: return device->pass; break;
        }
    } else if (role == Qt::CheckStateRole && !isDriveOrVD
        && index.flags() & Qt::ItemIsUserCheckable) {
        switch (index.column())
        {
        case COL_FLAG_ACTIVE:
            assert(device->parentItem != nullptr);
            return (device->parentItem->active == device) ? Qt::Checked : Qt::Unchecked;
            break;
        case COL_FLAG_ESP: return device->flags.sysEFI ? Qt::Checked : Qt::Unchecked; break;
        case COL_ENCRYPT: return device->encrypt ? Qt::Checked : Qt::Unchecked; break;
        case COL_CHECK: return device->chkbadblk ? Qt::Checked : Qt::Unchecked; break;
        case COL_DUMP: return device->dump ? Qt::Checked : Qt::Unchecked; break;
        }
    } else if (role == Qt::DisplayRole) {
        switch (index.column()) {
        case COL_DEVICE: return device->name; break;
        case COL_SIZE:
            if (device->type == Device::SUBVOLUME) {
                return device->isActive() ? "++++" : "----";
            } else {
                return QLocale::system().formattedDataSize(device->size,
                    1, QLocale::DataSizeTraditionalFormat);
            }
            break;
        case COL_USEFOR: return device->usefor; break;
        case COL_LABEL:
            if (index.flags() & Qt::ItemIsEditable) return device->label;
            else return device->curLabel;
            break;
        case COL_FORMAT:
            if (device->type == Device::DRIVE || device->type == Device::SUBVOLUME) {
                return device->shownFormat(device->format);
            } else {
                if (device->usefor.isEmpty()) return device->curFormat;
                else return device->shownFormat(device->format);
            }
            break;
        case COL_OPTIONS:
            if (device->canMount(false)) return device->options;
            break;
        case COL_PASS:
            if (device->canMount()) return device->pass;
            break;
        }
    } else if (role == Qt::ToolTipRole) {
        switch (index.column()) {
        case COL_DEVICE:
            if (!device->model.isEmpty()) return tr("Model: %1").arg(device->model);
            else if (!device->origin) return device->path;
            else return device->path + "\n("_L1 + device->origin->name + ')';
            break;
        case COL_SIZE:
            if (device->type == Device::DRIVE) {
                long long fs = device->driveFreeSpace();
                if (fs <= 0) fs = 0;
                return tr("Free space: %1").arg(QLocale::system().formattedDataSize(fs,
                    1, QLocale::DataSizeTraditionalFormat));
            }
            break;
        }
        return device->shownDevice();
    } else if (role == Qt::DecorationRole && index.column() == COL_DEVICE) {
        if ((device->type == Device::DRIVE || device->type == Device::SUBVOLUME) && !device->flags.oldLayout) {
            return QIcon(":/appointment-soon");
        } else if (device->type == Device::VIRTUAL && device->flags.volCrypto) {
            return QIcon::fromTheme(u"unlock"_s);
        } else if (device->mapCount) {
            return QIcon::fromTheme(u"lock"_s);
        }
    }
    return QVariant();
}
bool PartMan::setData(const QModelIndex &index, const QVariant &value, int role) noexcept
{
    if (role == Qt::CheckStateRole) {
        Device *device = static_cast<Device *>(index.internalPointer());
        changeBegin(device);
        switch (index.column())
        {
        case COL_FLAG_ACTIVE: device->setActive(value == Qt::Checked); break;
        case COL_FLAG_ESP: device->flags.sysEFI = (value == Qt::Checked); break;
        case COL_ENCRYPT: device->encrypt = (value == Qt::Checked); break;
        case COL_CHECK: device->chkbadblk = (value == Qt::Checked); break;
        case COL_DUMP: device->dump = (value == Qt::Checked); break;
        }
        if (!changeEnd(true, true)) {
            emit dataChanged(index, index);
        }
    }
    return true;
}
Qt::ItemFlags PartMan::flags(const QModelIndex &index) const noexcept
{
    Device *device = static_cast<Device *>(index.internalPointer());
    if (device->type == Device::VIRTUAL_DEVICES) return Qt::ItemIsEnabled;
    Qt::ItemFlags flagsOut({Qt::ItemIsSelectable, Qt::ItemIsEnabled});
    if (device->mapCount) return flagsOut;
    switch (index.column())
    {
    case COL_DEVICE: break;
    case COL_SIZE:
        if (device->type == Device::PARTITION && !device->flags.oldLayout) {
            flagsOut |= Qt::ItemIsEditable;
        }
        break;
    case COL_FLAG_ACTIVE:
        if (device->type == Device::PARTITION) {
            flagsOut |= Qt::ItemIsUserCheckable;
        }
        break;
    case COL_FLAG_ESP:
        if (device->type == Device::PARTITION && device->parent() && device->parent()->finalFormat() == "GPT"_L1) {
            flagsOut |= Qt::ItemIsUserCheckable;
        }
        break;
    case COL_USEFOR:
        if (device->allowedUsesFor().count() >= 1 && device->format != "DELETE"_L1) {
            flagsOut |= Qt::ItemIsEditable;
        }
        break;
    case COL_LABEL:
        if (device->format != "PRESERVE"_L1 && device->format != "DELETE"_L1) {
            if (device->type == Device::SUBVOLUME) flagsOut |= Qt::ItemIsEditable;
            else if (!device->usefor.isEmpty()) flagsOut |= Qt::ItemIsEditable;
        }
        break;
    case COL_ENCRYPT:
        if (device->canEncrypt()) flagsOut |= Qt::ItemIsUserCheckable;
        break;
    case COL_FORMAT:
        if (device->allowedFormats().count() > 1) flagsOut |= Qt::ItemIsEditable;
        break;
    case COL_CHECK:
        if (device->format.startsWith("ext"_L1) || device->format == "jfs"_L1
            || device->format.startsWith("FAT"_L1, Qt::CaseInsensitive)) {
            flagsOut |= Qt::ItemIsUserCheckable;
        }
        break;
    case COL_OPTIONS:
        if (device->canMount(false)) flagsOut |= Qt::ItemIsEditable;
        break;
    case COL_DUMP:
        if (device->canMount()) flagsOut |= Qt::ItemIsUserCheckable;
        break;
    case COL_PASS:
        if (device->canMount()) flagsOut |= Qt::ItemIsEditable;
        break;
    }
    return flagsOut;
}
QVariant PartMan::headerData(int section, [[maybe_unused]] Qt::Orientation orientation, int role) const noexcept
{
    assert(orientation == Qt::Horizontal);
    if (role == Qt::DisplayRole) {
        static constexpr const char *text[TREE_COLUMNS] = {
            QT_TR_NOOP("Device"),   // COL_DEVICE
            QT_TR_NOOP("Size"),     // COL_SIZE
            QT_TR_NOOP("Active"),   // COL_FLAG_ACTIVE
            QT_TR_NOOP("ESP"),      // COL_FLAG_ESP
            QT_TR_NOOP("Use For"),  // COL_USEFOR
            QT_TR_NOOP("Label"),    // COL_LABEL
            QT_TR_NOOP("Encrypt"),  // COL_ENCRYPT
            QT_TR_NOOP("Format"),   // COL_FORMAT
            QT_TR_NOOP("Check"),    // COL_CHECK
            QT_TR_NOOP("Options"),  // COL_OPTIONS
            QT_TR_NOOP("Dump"),     // COL_DUMP
            QT_TR_NOOP("Pass")      // COL_PASS
        };
        return tr(text[section]);
    } else if (role == Qt::ToolTipRole) {
        switch (section)
        {
        case COL_DEVICE: break;
        case COL_SIZE: break;
        case COL_FLAG_ACTIVE: return tr("Active partition"); break;
        case COL_FLAG_ESP: return tr("EFI System Partition"); break;
        case COL_USEFOR: break;
        case COL_LABEL: break;
        case COL_ENCRYPT: break;
        case COL_FORMAT: break;
        case COL_CHECK: break;
        case COL_OPTIONS: break;
        case COL_DUMP: break;
        case COL_PASS: break;
        }
    } else if (role == Qt::FontRole) {
        QFont font;
        if (colprops[section].checkbox) {
            font.setPointSizeF(font.pointSizeF() * 0.5);
        }
        font.setItalic(colprops[section].advanced);
        return font;
    }
    return QVariant();
}
QModelIndex PartMan::index(Device *device) const noexcept
{
    if (device == root) return QModelIndex();
    return createIndex(device->row(), 0, device);
}
QModelIndex PartMan::index(int row, int column, const QModelIndex &parent) const
{
    if (!hasIndex(row, column, parent)) return QModelIndex();
    const Device *pdevice = parent.isValid() ? static_cast<Device *>(parent.internalPointer()) : root;
    Device *cdevice = pdevice->child(row);
    if (cdevice) return createIndex(row, column, cdevice);
    return QModelIndex();
}
QModelIndex PartMan::parent(const QModelIndex &index) const noexcept
{
    if (!index.isValid()) return QModelIndex();
    Device *cdevice = static_cast<Device *>(index.internalPointer());
    Device *pdevice = cdevice->parentItem;
    if (!pdevice || pdevice == root) return QModelIndex();
    return createIndex(pdevice->row(), 0, pdevice);
}
inline PartMan::Device *PartMan::item(const QModelIndex &index) const noexcept
{
    return static_cast<Device *>(index.internalPointer());
}
int PartMan::rowCount(const QModelIndex &parent) const noexcept
{
    if (parent.column() > 0) return 0;
    if (parent.isValid()) {
        return static_cast<Device *>(parent.internalPointer())->children.size();
    }
    return root->children.size();
}

bool PartMan::changeBegin(Device *device) noexcept
{
    if (changing) return false;
    root->flags = device->flags;
    root->size = device->size;
    root->usefor = device->usefor;
    root->label = device->label;
    root->encrypt = device->encrypt;
    root->format = device->format;
    root->options = device->options;
    root->dump = device->dump;
    root->pass = device->pass;
    changing = device;
    return true;
}
int PartMan::changeEnd(bool autofill, bool notify) noexcept
{
    if (!changing) return false;
    int changed = 0;
    if (changing->size != root->size) {
        if (!changing->usefor.startsWith('/') && !changing->allowedUsesFor().contains(changing->usefor)) {
            changing->usefor.clear();
        }
        if (!changing->canEncrypt()) changing->encrypt = false;
        changed |= (1 << COL_SIZE);
    }
    if (changing->usefor != root->usefor) {
        if (changing->usefor.isEmpty()) changing->format.clear();
        else {
            const QStringList &allowed = changing->allowedFormats();
            if (!allowed.contains(changing->format)) changing->format = allowed.at(0);
        }
        changed |= (1 << COL_USEFOR);
    }
    if (changing->encrypt != root->encrypt) {
        const QStringList &allowed = changing->allowedFormats();
        if (!allowed.contains(changing->format)) changing->format = allowed.at(0);
        changed |= (1 << COL_ENCRYPT);
    }
    if (changing->type != Device::DRIVE && (changing->format != root->format || changing->usefor != root->usefor)) {
        changing->dump = false;
        changing->pass = 2;
        if (changing->usefor.isEmpty() || changing->usefor == "FORMAT"_L1) {
            changing->options.clear();
        }
        if (changing->format != "btrfs"_L1) {
            // Clear all subvolumes if not supported.
            while (changing->children.size()) delete changing->child(0);
        } else {
            // Remove preserve option from all subvolumes.
            for (Device *cdevice : changing->children) {
                cdevice->format = "CREATE"_L1;
                notifyChange(cdevice);
            }
        }
    }
    if (changing->format != root->format) {
        changed |= (1 << COL_FORMAT);
    }
    if (changing->flags.sysEFI != root->flags.sysEFI) {
        changed |= (1 << COL_FLAG_ESP);
    }

    if (autofill) {
        changing->autoFill(changed);
    }
    if (changed && notify) {
        notifyChange(changing);
    }
    changing = nullptr;
    return changed;
}
void PartMan::notifyChange(class Device *device, int first, int last) noexcept
{
    if (first < 0) first = 0;
    if (last < 0) last = TREE_COLUMNS - 1;
    const int row = device->row();
    emit dataChanged(createIndex(row, first, device), createIndex(row, last, device));
}

/* Model element */

PartMan::Device::Device(enum DeviceType type, Device *parent, Device *preceding) noexcept
    : type(type), parentItem(parent), partman(parent->partman)
{
    assert(parent != nullptr);
    flags.rotational = parent->flags.rotational;
    discgran = parent->discgran;
    if (type == SUBVOLUME) size = parent->size;
    else if (type == PARTITION) size = 1*MB;
    physec = parent->physec;
    if (parent->children.size() == 0 && parent->format.isEmpty()) {
        // Starting with a completely blank layout.
        parent->flags.oldLayout = false;
        parent->format = parent->allowedFormats().value(0);
    }
    const int i = preceding ? (parentItem->indexOfChild(preceding) + 1) : parentItem->children.size();
    partman.beginInsertRows(partman.index(parent), i, i);
    parent->children.insert(std::next(parent->children.begin(), i), this);
    partman.endInsertRows();
}
PartMan::Device::~Device()
{
    if (parentItem) {
        const int r = parentItem->indexOfChild(this);
        partman.beginRemoveRows(partman.index(parentItem), r, r);
    }
    for (Device *cdevice : children) {
        cdevice->parentItem = nullptr; // Stop unnecessary signals and double deletes.
        delete cdevice;
    }
    children.clear();
    if (parentItem) {
        if (parentItem->active == this) parentItem->active = nullptr;
        for (auto it = parentItem->children.begin(); it != parentItem->children.end();) {
            if (*it == this) it = parentItem->children.erase(it);
            else ++it;
        }
        partman.endRemoveRows();
        partman.notifyChange(parentItem);
    }
}
void PartMan::Device::clear() noexcept
{
    const int chcount = children.size();
    if (chcount > 0) partman.beginRemoveRows(partman.index(this), 0, chcount - 1);
    for (Device *cdevice : children) {
        cdevice->parentItem = nullptr; // Stop unnecessary signals and double deletes.
        delete cdevice;
    }
    children.clear();
    active = nullptr;
    flags.oldLayout = false;
    if (chcount > 0) partman.endRemoveRows();
}
inline int PartMan::Device::row() const noexcept
{
    return parentItem ? parentItem->indexOfChild(this) : 0;
}
PartMan::Device *PartMan::Device::parent() const noexcept
{
    if (parentItem && !parentItem->parentItem) return nullptr; // Invisible root
    return parentItem;
}
inline PartMan::Device *PartMan::Device::child(int row) const noexcept
{
    if (row < 0 || row >= static_cast<int>(children.size())) return nullptr;
    return children[row];
}
inline int PartMan::Device::indexOfChild(const Device *device) const noexcept
{
    for (size_t ixi = 0; ixi < children.size(); ++ixi) {
        if (children[ixi] == device) return ixi;
    }
    return -1;
}
void PartMan::Device::sortChildren() noexcept
{
    auto cmp = [](Device *l, Device *r) {
        if (l->order != r->order) return l->order < r->order;
        return l->name < r->name;
    };
    std::sort(children.begin(), children.end(), cmp);
    for (Device *c : std::as_const(children)) {
        partman.notifyChange(c);
    }
}
/* Helpers */
void PartMan::Device::setActive(bool on) noexcept
{
    if (!parentItem) return;
    if (parentItem->active != this) {
        if (parentItem->active) partman.notifyChange(parentItem->active);
    }
    parentItem->active = on ? this : nullptr;
    partman.notifyChange(this);
}
bool PartMan::Device::isActive() const noexcept
{
    if (!parentItem) return false;
    return (parentItem->active == this);
}
bool PartMan::Device::isLocked() const noexcept
{
    for (const Device *cdevice : std::as_const(children)) {
        if (cdevice->isLocked()) return true;
    }
    return (mapCount != 0); // In use by at least one virtual device.
}
bool PartMan::Device::willFormat() const noexcept
{
    return format != "PRESERVE"_L1 && !usefor.isEmpty();
}
bool PartMan::Device::canEncrypt() const noexcept
{
    if (type != PARTITION) return false;
    return !(flags.sysEFI || usefor.isEmpty() || usefor == "BIOS-GRUB"_L1 || usefor == "/boot"_L1);
}
bool PartMan::Device::willEncrypt() const noexcept
{
    if (type == SUBVOLUME) return parentItem->encrypt;
    return encrypt;
}
QString PartMan::Device::assocUUID() const noexcept
{
    if (type == SUBVOLUME) return parentItem->uuid;
    return uuid;
}
QString PartMan::Device::mappedDevice() const noexcept
{
    const Device *device = this;
    if (device->type == SUBVOLUME) device = device->parentItem;
    if (device->type == PARTITION) {
        const QString &d = device->map;
        if (!d.isEmpty()) return "/dev/mapper/"_L1 + d;
    }
    return device->path;
}
inline bool PartMan::Device::willMap() const noexcept
{
    if (type == DRIVE || type == VIRTUAL_DEVICES) return false;
    else if (type == SUBVOLUME) return !parentItem->map.isEmpty();
    return !map.isEmpty();
}
QString PartMan::Device::shownDevice() const noexcept
{
    if (type == SUBVOLUME) return parentItem->name + '[' + label + ']';
    return name;
}
QStringList PartMan::Device::allowedUsesFor(bool all) const noexcept
{
    if (!isVolume() && type != SUBVOLUME) return QStringList();
    QStringList list;
    auto checkAndAdd = [&](const QString &use) {
        const auto fit = partman.volSpecs.find(usefor);
        if (all || fit == partman.volSpecs.end() || size >= fit->second.minimum) {
            list.append(use);
        }
    };

    if (type != SUBVOLUME) {
        checkAndAdd(u"FORMAT"_s);
        if (type != VIRTUAL) {
            if ((all || size <= 1*MB) && parentItem && parentItem->format == "GPT"_L1) {
                checkAndAdd(u"BIOS-GRUB"_s);
            }
            if (all || size <= 8*GB) {
                if (size < (2*TB - 512)) {
                    if (all || flags.sysEFI || size <= 1024*MB) checkAndAdd(u"ESP"_s);
                }
                checkAndAdd(u"/boot"_s); // static files of the boot loader
            }
        }
    }
    if (!flags.sysEFI) {
        // Debian 12 installer order: / /boot /home /tmp /usr /var /srv /opt /usr/local
        checkAndAdd(u"/"_s); // the root file system
        checkAndAdd(u"/home"_s); // user home directories
        checkAndAdd(u"/usr"_s); // static data
        // checkAndAdd(u"/usr/local"_s); // local hierarchy
        checkAndAdd(u"/var"_s); // variable data
        // checkAndAdd(u"/tmp"_s); // temporary files
        // checkAndAdd(u"/srv"_s); // data for services provided by this system
        // checkAndAdd(u"/opt"_s); // add-on application software packages
        if (type != SUBVOLUME) checkAndAdd(u"SWAP"_s);
        else checkAndAdd(u"/swap"_s);
    }
    return list;
}
QStringList PartMan::Device::allowedFormats() const noexcept
{
    QStringList list;
    if (type == DRIVE && !flags.oldLayout) {
        list.append(u"GPT"_s);
        if (size < 2*TB && children.size() <= 4) {
            if (partman.core.detectEFI()) {
                list.append(u"DOS"_s);
            } else {
                list.prepend(u"DOS"_s);
            }
        }
        return list;
    }

    bool allowPreserve = false;
    if (isVolume()) {
        if (usefor.isEmpty()) return QStringList();
        else if (usefor == "BIOS-GRUB"_L1) {
            list.append(u"BIOS-GRUB"_s);
        } else if (usefor == "SWAP"_L1) {
            list.append(u"swap"_s);
            allowPreserve = list.contains(curFormat, Qt::CaseInsensitive);
        } else {
            if (!flags.sysEFI) {
                list << u"ext4"_s;
                if (usefor != u"/boot"_s) {
                    list << u"ext3"_s << u"ext2"_s;
                    if (!QStandardPaths::findExecutable(u"mkfs.f2fs"_s).isEmpty()) list << u"f2fs"_s;
                    if (!QStandardPaths::findExecutable(u"mkfs.jfs"_s).isEmpty()) list << u"jfs"_s;
                    if (!QStandardPaths::findExecutable(u"mkfs.xfs"_s).isEmpty()) list << u"xfs"_s;
                    if (!QStandardPaths::findExecutable(u"mkfs.btrfs"_s).isEmpty()) list << u"btrfs"_s;
                }
            }
            if (size <= (2*TB - 64*KB)) list.append(u"FAT32"_s);
            if (size <= (4*GB - 64*KB)) list.append(u"FAT16"_s);
            if (size <= (32*MB - 512)) list.append(u"FAT12"_s);

        if (usefor != "FORMAT"_L1) {
            allowPreserve = list.contains(curFormat, Qt::CaseInsensitive);
            if (!allowPreserve && curFormat == "crypto_LUKS"_L1) allowPreserve = true;
            if (!allowPreserve && flags.oldLayout && !curFormat.isEmpty()) allowPreserve = true;
        }
        }
    } else if (type == SUBVOLUME) {
        list.append(u"CREATE"_s);
        if (flags.oldLayout) {
            list.append(u"DELETE"_s);
            allowPreserve = true;
        }
    }

    if (encrypt) allowPreserve = false;
    if (allowPreserve) {
        // People often share SWAP partitions between distros and need to keep the same UUIDs.
        if (flags.sysEFI || usefor == "/home"_L1 || !allowedUsesFor().contains(usefor) || usefor == "SWAP"_L1) {
            list.prepend(u"PRESERVE"_s); // Preserve ESP, SWAP, custom mounts and /home by default.
        } else {
            list.append(u"PRESERVE"_s);
        }
    }
    return list;
}
QString PartMan::Device::finalFormat() const noexcept
{
    if (type == SUBVOLUME) return parentItem->format;
    return (type != DRIVE && (usefor.isEmpty() || format == "PRESERVE"_L1)) ? curFormat : format;
}
QString PartMan::Device::finalLabel() const noexcept
{
    if (type == SUBVOLUME) return parentItem->label;
    return (type != DRIVE && (usefor.isEmpty() || format == "PRESERVE"_L1)) ? curLabel : label;
}
QString PartMan::Device::shownFormat(const QString &fmt) const noexcept
{
    if (fmt == "CREATE"_L1) return flags.oldLayout ? tr("Overwrite") : tr("Create");
    else if (fmt == "DELETE"_L1) return tr("Delete");
    else if (fmt != "PRESERVE"_L1) return fmt;
    else {
        if (type == SUBVOLUME) return tr("Preserve");
        else if (usefor != "/"_L1) return tr("Preserve (%1)").arg(curFormat);
        else return tr("Preserve /home (%1)").arg(curFormat);
    }
}
QString PartMan::Device::mountPoint() const noexcept
{
    if (!canMount(false)) return QString();
    else if (usefor == "ESP"_L1) return u"/boot/efi"_s;
    else if (usefor == "SWAP"_L1) return u"swap"_s;
    return usefor;
}
bool PartMan::Device::canMount(bool fsonly) const noexcept
{
    return !usefor.isEmpty() && usefor != "FORMAT"_L1 && usefor != "BIOS-GRUB"_L1
        && (!fsonly || usefor != "SWAP"_L1);
}
long long PartMan::Device::driveFreeSpace(bool inclusive) const noexcept
{
    const Device *drive = parent();
    if (!drive) drive = this;
    long long free = drive->size - PARTMAN_SAFETY;
    for (const Device *cdevice : std::as_const(drive->children)) {
        if (inclusive || cdevice != this) free -= cdevice->size;
    }
    return free;
}
/* Convenience */
PartMan::Device *PartMan::Device::addPart(long long defaultSize, const QString &defaultUse, bool crypto) noexcept
{
    PartMan::Device *part = new PartMan::Device(PartMan::Device::PARTITION, this);
    if (!defaultUse.isEmpty()) part->usefor = defaultUse;
    part->size = defaultSize;
    if (part->canEncrypt()) part->encrypt = crypto;
    part->autoFill();
    part->addToCrypttab = part->encrypt;
    partman.notifyChange(part);
    return part;
}
void PartMan::Device::driveAutoSetActive() noexcept
{
    if (active) return;
    if (partman.core.detectEFI() && format=="GPT"_L1) return;

    for (const char *pref : {"/boot", "/"}) {
        for (PartMan::Iterator it(this); Device *device = *it; it.next()) {
            if (device->usefor == pref) {
                while (device && device->type != PARTITION) device = device->parentItem;
                if (device) device->setActive(true);
                return;
            }
        }
    }
}
void PartMan::Device::autoFill(unsigned int changed) noexcept
{
//    if ((changed & (1 << PartMan::COL_FLAG_ESP)) && usefor.isEmpty()) {
//        usefor = "ESP";
//        changed |= (1 << PartMan::COL_USEFOR);
//    }
    if (changed & (1 << PartMan::COL_USEFOR)) {
        // Default labels
        if (type == SUBVOLUME) {
            QStringList chklist;
            for (Device *cdevice : std::as_const(parentItem->children)) {
                if (cdevice == this) continue;
                chklist << cdevice->label;
            }
            QString newLabel;
            if (usefor.startsWith('/')) {
                const QString base = usefor.mid(1).replace('/','.');
                newLabel = '@' + base;
                for (int ixi = 2; chklist.contains(newLabel, Qt::CaseInsensitive); ++ixi) {
                    newLabel = QString::number(ixi) + '@' + base;
                }
            } else if (!usefor.isEmpty()) {
                newLabel = usefor;
                for (int ixi = 2; chklist.contains(newLabel, Qt::CaseInsensitive); ++ixi) {
                    newLabel = usefor + QString::number(ixi);
                }
            }
            label = newLabel;
        } else {
            const auto fit = partman.volSpecs.find(usefor);
            if (fit == partman.volSpecs.cend()) label.clear();
            else label = fit->second.defaultLabel;
        }
        // Automatic default boot device selection
        if ((type != VIRTUAL) && (usefor == "/boot"_L1 || usefor == "/"_L1)) {
            Device *drive = this;
            while (drive && drive->type != DRIVE) drive = drive->parentItem;
            if (drive) drive->driveAutoSetActive();
        }
        // if (type == SUBVOLUME && usefor == "/"_L1) setActive(true);
        if (usefor == "ESP"_L1 || usefor == "/boot/efi"_L1) flags.sysEFI = true;

        if (encrypt & !canEncrypt()) {
            encrypt = false;
            changed |= (1 << PartMan::COL_ENCRYPT);
        }
    }
    const QStringList &af = allowedFormats();
    if (format.isEmpty() || !af.contains(format)) {
        if (af.isEmpty()) format.clear();
        else format = af.at(0);
        changed |= (1 << PartMan::COL_FORMAT);
    }
    if ((changed & ((1 << PartMan::COL_USEFOR) | (1 << PartMan::COL_FORMAT))) && canMount(false)) {
        // Default options, dump and pass
        if (usefor == "SWAP"_L1) {
            options = discgran ? "discard=once"_L1 : "defaults"_L1;
        } else if (finalFormat().startsWith("FAT"_L1)) {
            options = "noatime,dmask=0002,fmask=0113"_L1;
            pass = 0;
            dump = false;
        } else {
            if (usefor == "/boot"_L1 || usefor == "/"_L1) {
                pass = (format == "btrfs"_L1) ? 0 : 1;
            }
            options.clear();
            const bool btrfs = (format == "btrfs"_L1 || type == SUBVOLUME);
            if (!flags.rotational && btrfs) options += "ssd,"_L1;
            options += "noatime"_L1;
            if (btrfs && usefor != "/swap"_L1) options += ",compress=zstd:1"_L1;
            dump = true;
        }
    }
    if (changed & (1 << PartMan::COL_ENCRYPT)) addToCrypttab = encrypt;
}
void PartMan::Device::labelParts() noexcept
{
    const size_t nchildren = children.size();
    for (size_t ixi = 0; ixi < nchildren; ++ixi) {
        Device *cdevice = children[ixi];
        cdevice->name = PartMan::joinName(name, ixi + 1);
        cdevice->path = "/dev/"_L1 + cdevice->name;
        partman.notifyChange(cdevice, PartMan::COL_DEVICE, PartMan::COL_DEVICE);
    }
    partman.resizeColumnsToFit();
}

// Return block device info that is suitable for a combo box.
QString PartMan::Device::friendlyName(bool mappedFormat) const noexcept
{
    QString strout(QLocale::system().formattedDataSize(size, 1, QLocale::DataSizeTraditionalFormat));
    if (mappedFormat) {
        Device *mdevice = partman.findByPath(mappedDevice());
        if (mdevice) {
            strout += ' ' + mdevice->finalFormat();
        }
    } else {
        strout += ' ' + finalFormat();
    }
    const QString &flabel = finalLabel();
    if (!flabel.isEmpty()) strout += " - "_L1 + flabel;
    if (!model.isEmpty()) strout += (flabel.isEmpty() ? " - "_L1 : "; "_L1) + model;
    return name + " ("_L1 + strout + ")"_L1;
}
void PartMan::Device::addToCombo(QComboBox *combo, bool warnNasty) const noexcept
{
    QString stricon;
    if (!flags.oldLayout || !usefor.isEmpty()) stricon = ":/appointment-soon"_L1;
    else if (flags.nasty && warnNasty) stricon = ":/dialog-warning"_L1;
    combo->addItem(QIcon(stricon), friendlyName(), name);
}
// Split a device name into its drive and partition.
PartMan::NameParts PartMan::splitName(const QString &devname) noexcept
{
    static const QRegularExpression rxdev1(u"^(?:\\/dev\\/)*(mmcblk.*|nvme.*)p([0-9]*)$"_s);
    static const QRegularExpression rxdev2(u"^(?:\\/dev\\/)*([a-z]*)([0-9]*)$"_s);
    QRegularExpressionMatch rxmatch(rxdev1.match(devname));
    if (!rxmatch.hasMatch()) rxmatch = rxdev2.match(devname);
    return {rxmatch.captured(1), rxmatch.captured(2)};
}
QString PartMan::joinName(const QString &drive, int partnum) noexcept
{
    QString name = drive;
    if (name.startsWith("nvme"_L1) || name.startsWith("mmcblk"_L1)) name += 'p';
    return (name + QString::number(partnum));
}

/* A very slimmed down and non-standard one-way tree iterator. */
PartMan::Iterator::Iterator(const PartMan &partman) noexcept
{
    if (partman.root->children.size() < 1) return;
    ixParents.push(0);
    start = partman.root;
    pos = partman.root->child(0);
}
void PartMan::Iterator::next() noexcept
{
    if (!pos) return;
    if (pos->children.size()) {
        ixParents.push(ixPos);
        ixPos = 0;
        pos = pos->child(0);
    } else {
        Device *parent = pos->parentItem;
        if (!parent) {
            pos = nullptr;
            return;
        }
        Device *chnext = parent->child(ixPos+1);
        while (!chnext && parent && parent != start) {
            parent = parent->parentItem;
            if (!parent) break;
            ixPos = ixParents.top();
            ixParents.pop();
            chnext = parent->child(ixPos+1);
        }
        if (chnext) ++ixPos;
        pos = chnext;
    }
    // If back to the start then stop the search.
    if (pos == start) {
        pos = nullptr;
    }
}

/* Delegate */

void PartMan::ItemDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    QStyledItemDelegate::paint(painter, option, index);
    // Frame to indicate editable cells
    if (index.flags() & Qt::ItemIsEditable) {
        painter->save();
        QPen pen = painter->pen();
        pen.setColor(option.palette.color(QPalette::Active, QPalette::Text));
        pen.setWidth(2);
        painter->setPen(pen);
        painter->translate(pen.widthF() / 2, pen.widthF() / 2);
        const QRect &rect = option.rect.adjusted(0, 0, -pen.width(), -pen.width());
        painter->drawRect(rect);
        // Arrow to indicate a drop-down list
        if(colprops[index.column()].dropdown) {
            const int arrowEdgeX = 4, arrowEdgeY = 6, arrowWidth = 8;
            pen.setWidthF(1.5);
            painter->setPen(pen);
            QBrush brush = pen.brush();
            brush.setStyle(Qt::Dense4Pattern);
            painter->setBrush(brush);
            pen.setColor(option.palette.color(QPalette::Active, QPalette::Text));
            const int adjright = rect.right() - arrowEdgeX;
            QPoint arrow[] = {
                {adjright, rect.top() + arrowEdgeY},
                {adjright - arrowWidth/2, rect.bottom() - arrowEdgeY},
                {adjright - arrowWidth, rect.top() + arrowEdgeY}
            };
            painter->drawConvexPolygon(arrow, 3);
        }
        painter->restore();
    }
}
QSize PartMan::ItemDelegate::sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    PartMan::Device *device = static_cast<PartMan::Device*>(index.internalPointer());
    int width = QStyledItemDelegate::sizeHint(option, index).width();
    // In the case of Format and Use For, the cell should accommodate all options in the list.
    const int residue = 10 + width - option.fontMetrics.boundingRect(index.data(Qt::DisplayRole).toString()).width();
    switch(index.column()) {
    case PartMan::COL_FORMAT:
        for (const QStringList &fmts = device->allowedFormats(); const QString &fmt : fmts) {
            const int fw = option.fontMetrics.boundingRect(device->shownFormat(fmt)).width() + residue;
            if (fw > width) width = fw;
        }
        break;
    case PartMan::COL_USEFOR:
        for (const QStringList &uses = device->allowedUsesFor(false); const QString &use : uses) {
            const int uw = option.fontMetrics.boundingRect(use).width() + residue;
            if (uw > width) width = uw;
        }
        break;
    }
    return QSize(width + 4, option.fontMetrics.height() + 4);
}

QWidget *PartMan::ItemDelegate::createEditor(QWidget *parent, const QStyleOptionViewItem &, const QModelIndex &index) const
{
    QWidget *widget = nullptr;
    switch (index.column())
    {
    case PartMan::COL_SIZE:
        {
            QSpinBox *spin = new QSpinBox(parent);
            spin->setSuffix(u"MB"_s);
            spin->setStepType(QSpinBox::AdaptiveDecimalStepType);
            spin->setAccelerated(true);
            spin->setSpecialValueText(u"MAX"_s);
            widget = spin;
        }
        break;
    case PartMan::COL_USEFOR:
        {
            QComboBox *combo = new QComboBox(parent);
            combo->setEditable(true);
            combo->setInsertPolicy(QComboBox::NoInsert);
            combo->lineEdit()->setPlaceholderText(u"----"_s);
            widget = combo;
        }
        break;
    case PartMan::COL_FORMAT: widget = new QComboBox(parent); break;
    case PartMan::COL_PASS: widget = new QSpinBox(parent); break;
    case PartMan::COL_OPTIONS:
        {
            QLineEdit *edit = new QLineEdit(parent);
            edit->setProperty("row", QVariant::fromValue<void *>(index.internalPointer()));
            edit->setContextMenuPolicy(Qt::CustomContextMenu);
            connect(edit, &QLineEdit::customContextMenuRequested, this, &ItemDelegate::partOptionsMenu);
            widget = edit;
        }
        break;
    default: widget = new QLineEdit(parent);
    }
    assert(widget != nullptr);
    widget->setAutoFillBackground(true);
    widget->setFocusPolicy(Qt::StrongFocus);
    return widget;
}
void PartMan::ItemDelegate::setEditorData(QWidget *editor, const QModelIndex &index) const
{
    PartMan::Device *device = static_cast<PartMan::Device*>(index.internalPointer());
    editor->blockSignals(true);
    switch (index.column())
    {
    case PartMan::COL_SIZE:
        {
            QSpinBox *spin = qobject_cast<QSpinBox *>(editor);
            spin->setRange(0, static_cast<int>(device->driveFreeSpace() / MB));
            spin->setValue(device->size / MB);
        }
        break;
    case PartMan::COL_USEFOR:
        {
            QComboBox *combo = qobject_cast<QComboBox *>(editor);
            combo->clear();
            combo->addItem(QString());
            combo->addItems(device->allowedUsesFor(false));
            combo->setCurrentText(device->usefor);
        }
        break;
    case PartMan::COL_LABEL:
        qobject_cast<QLineEdit *>(editor)->setText(device->label);
        break;
    case PartMan::COL_FORMAT:
        {
            QComboBox *combo = qobject_cast<QComboBox *>(editor);
            combo->clear();
            const QStringList &formats = device->allowedFormats();
            assert(!formats.isEmpty());
            for (const QString &fmt : formats) {
                if (fmt != "PRESERVE") combo->addItem(device->shownFormat(fmt), fmt);
                else {
                    // Add an item at the start to allow preserving the existing format.
                    combo->insertItem(0, device->shownFormat(u"PRESERVE"_s), "PRESERVE");
                    combo->insertSeparator(1);
                }
            }
            const int ixfmt = combo->findData(device->format);
            if (ixfmt >= 0) combo->setCurrentIndex(ixfmt);
        }
        break;
    case PartMan::COL_PASS:
        qobject_cast<QSpinBox *>(editor)->setValue(device->pass);
        break;
    case PartMan::COL_OPTIONS:
        qobject_cast<QLineEdit *>(editor)->setText(device->options);
        break;
    }
    editor->blockSignals(false);
}
void PartMan::ItemDelegate::setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const
{
    PartMan::Device *device = static_cast<PartMan::Device*>(index.internalPointer());
    PartMan *partman = qobject_cast<PartMan *>(model);
    partman->changeBegin(device);
    switch (index.column())
    {
    case PartMan::COL_SIZE:
        device->size = qobject_cast<QSpinBox *>(editor)->value();
        device->size *= 1048576; // Separate step to prevent int overflow.
        if (device->size == 0) device->size = device->driveFreeSpace();
        // Set subvolume sizes to keep allowed uses accurate.
        for (PartMan::Device *subvol : device->children) {
            subvol->size = device->size;
        }
        break;
    case PartMan::COL_USEFOR:
        device->usefor = qobject_cast<QComboBox *>(editor)->currentText().trimmed();
        // Convert user-friendly entries to real mounts.
        if (!device->usefor.startsWith('/')) device->usefor = device->usefor.toUpper();
        break;
    case PartMan::COL_LABEL:
        device->label = qobject_cast<QLineEdit *>(editor)->text().trimmed();
        break;
    case PartMan::COL_FORMAT:
        device->format = qobject_cast<QComboBox *>(editor)->currentData().toString();
        break;
    case PartMan::COL_PASS:
        device->pass = qobject_cast<QSpinBox *>(editor)->value();
        break;
    case PartMan::COL_OPTIONS:
        device->options = qobject_cast<QLineEdit *>(editor)->text().trimmed();
        break;
    }
    partman->changeEnd(true, true);
}

void PartMan::ItemDelegate::partOptionsMenu() noexcept
{
    QLineEdit *edit = static_cast<QLineEdit *>(sender());
    if (!edit) return;
    PartMan::Device *part = static_cast<PartMan::Device *>(edit->property("row").value<void *>());
    if (!part) return;
    QMenu *menu = edit->createStandardContextMenu();
    menu->addSeparator();
    QMenu *menuTemplates = menu->addMenu(tr("&Templates"));
    QString selFS = part->format;
    if (selFS == "PRESERVE"_L1) selFS = part->curFormat;
    if ((part->type == PartMan::Device::PARTITION && selFS == "btrfs"_L1) || part->type == PartMan::Device::SUBVOLUME) {
        QString tcommon;
        if (!part->flags.rotational) tcommon = "ssd,"_L1;
        tcommon += "noatime"_L1;
        QAction *action = menuTemplates->addAction(tr("Compression (Z&STD)"));
        action->setData(tcommon + ",compress=zstd"_L1);
        action = menuTemplates->addAction(tr("Compression (&LZO)"));
        action->setData(tcommon + ",compress=lzo"_L1);
        action = menuTemplates->addAction(tr("Compression (&ZLIB)"));
        action->setData(tcommon + ",compress=zlib"_L1);
    }
    menuTemplates->setDisabled(menuTemplates->isEmpty());
    QAction *action = menu->exec(QCursor::pos());
    if (menuTemplates->actions().contains(action)) edit->setText(action->data().toString());
    delete menu;
}
