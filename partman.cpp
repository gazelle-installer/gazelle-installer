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
#include <QRegularExpression>
#include <QMessageBox>
#include <QPainter>
#include <QMenu>
#include <QSpinBox>
#include <QComboBox>
#include <QLineEdit>
#include <QFormLayout>
#include <QDialogButtonBox>

#include "mprocess.h"
#include "msettings.h"
#include "autopart.h"
#include "partman.h"

PartMan::PartMan(MProcess &mproc, Ui::MeInstall &ui, const QSettings &appConf, const QCommandLineParser &appArgs)
    : QAbstractItemModel(ui.boxMain), proc(mproc), gui(ui)
{
    root = new Device(*this);
    const QString &projShort = appConf.value("PROJECT_SHORTNAME").toString();
    volSpecs["BIOS-GRUB"] = {"BIOS GRUB"};
    volSpecs["/boot"] = {"boot"};
    volSpecs["/boot/efi"] = volSpecs["ESP"] = {"EFI-SYSTEM"};
    volSpecs["/"] = {"root" + projShort + appConf.value("VERSION").toString()};
    volSpecs["/home"] = {"home" + projShort};
    volSpecs["SWAP"] = {"swap" + projShort};
    brave = appArgs.isSet("brave");

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
    QFileInfo cryptsetup("/usr/sbin/cryptsetup");
    QFileInfo crypsetupinitramfs("/usr/share/initramfs-tools/conf-hooks.d/cryptsetup");
    if (!cryptsetup.exists() || !cryptsetup.isExecutable() || !crypsetupinitramfs.exists()) {
        gui.treePartitions->setColumnHidden(COL_ENCRYPT, true);
    }

    // UUID of the device that the live system is booted from.
    if (QFile::exists("/live/config/initrd.out")) {
        QSettings livecfg("/live/config/initrd.out", QSettings::NativeFormat);
        bootUUID = livecfg.value("BOOT_UUID").toString();
    }
}
PartMan::~PartMan()
{
    if (root) delete root;
}

void PartMan::scan(Device *drvstart)
{
    QStringList cargs({"-T", "-bJo",
        "TYPE,NAME,PATH,UUID,ROTA,DISC-GRAN,SIZE,PHY-SEC,PTTYPE,PARTTYPENAME,FSTYPE,FSVER,LABEL,MODEL,PARTFLAGS"});
    if (drvstart) cargs.append(drvstart->path);
    proc.exec("lsblk", cargs, nullptr, true);
    const QJsonObject &jsonObjBD = QJsonDocument::fromJson(proc.readOut(true).toUtf8()).object();
    const QJsonArray &jsonBD = jsonObjBD["blockdevices"].toArray();

    // Partitions listed in order of their physical locations.
    QStringList order;
    QString curdev;
    proc.exec("parted", {"-lm"}, nullptr, true);
    for(const QString &line : proc.readOutLines()) {
        const QString &sect = line.section(':', 0, 0);
        const int part = sect.toInt();
        if (part) order.append(joinName(curdev, part));
        else if (sect.startsWith("/dev/")) curdev = sect.mid(5);
    }

    if (!drvstart) root->clear();
    for (const QJsonValue &jsonDrive : jsonBD) {
        const QString &jdName = jsonDrive["name"].toString();
        const QString &jdPath = jsonDrive["path"].toString();
        if (jsonDrive["type"] != "disk") continue;
        if (jdName.startsWith("zram")) continue;
        Device *drive = drvstart;
        if (!drvstart) drive = new Device(Device::DRIVE, root);
        else if (jdPath != drvstart->path) continue;

        drive->clear();
        drive->flags.curEmpty = true;
        drive->flags.oldLayout = true;
        drive->name = jdName;
        drive->path = jdPath;
        const QString &ptType = jsonDrive["pttype"].toString();
        drive->format = drive->curFormat = ptType.toUpper();
        drive->flags.rotational = jsonDrive["rota"].toBool();
        drive->discgran = jsonDrive["disc-gran"].toInt();
        drive->size = jsonDrive["size"].toVariant().toLongLong();
        drive->physec = jsonDrive["phy-sec"].toInt();
        drive->curLabel = jsonDrive["label"].toString();
        drive->model = jsonDrive["model"].toString();
        const QJsonArray &jsonParts = jsonDrive["children"].toArray();
        for (const QJsonValue &jsonPart : jsonParts) {
            const QString &partTypeName = jsonPart["parttypename"].toString();
            if (partTypeName == "Extended" || partTypeName == "W95 Ext'd (LBA)"
                || partTypeName == "Linux extended") continue;
            Device *part = new Device(Device::PARTITION, drive);
            drive->flags.curEmpty = false;
            part->flags.oldLayout = true;
            part->name = jsonPart["name"].toString();
            part->path = jsonPart["path"].toString();
            part->uuid = jsonPart["uuid"].toString();
            part->order = order.indexOf(part->name);
            part->size = jsonPart["size"].toVariant().toLongLong();
            part->physec = jsonPart["phy-sec"].toInt();
            part->curLabel = jsonPart["label"].toString();
            part->model = jsonPart["model"].toString();
            part->flags.rotational = jsonPart["rota"].toBool();
            part->discgran = jsonPart["disc-gran"].toInt();
            const int partflags = jsonPart["partflags"].toString().toUInt(nullptr, 0);
            if ((partflags & 0x80) || (partflags & 0x04)) part->setActive(true);
            part->mapCount = jsonPart["children"].toArray().count();
            part->flags.sysEFI = part->flags.curESP = partTypeName.startsWith("EFI "); // "System"/"(FAT-12/16/32)"
            part->flags.bootRoot = (!bootUUID.isEmpty() && part->uuid == bootUUID);
            part->curFormat = jsonPart["fstype"].toString();
            if (part->curFormat == "vfat") part->curFormat = jsonPart["fsver"].toString();
            if (partTypeName == "BIOS boot") part->curFormat = "BIOS-GRUB";
            // Touching Microsoft LDM may brick the system.
            if (partTypeName.startsWith("Microsoft LDM")) part->flags.nasty = true;
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
                for (Device *device : virtdevs->children) listed[device->path] = device;
                break;
            }
        }
    }
    proc.shell("lsblk -T -bJo TYPE,NAME,PATH,UUID,ROTA,DISC-GRAN,SIZE,PHY-SEC,FSTYPE,LABEL /dev/mapper/*", nullptr, true);
    const QString &bdRaw = proc.readOut(true);
    const QJsonObject &jsonObjBD = QJsonDocument::fromJson(bdRaw.toUtf8()).object();
    const QJsonArray &jsonBD = jsonObjBD["blockdevices"].toArray();
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
        const QString &path = jsonDev["path"].toString();
        const bool rota = jsonDev["rota"].toBool();
        const int discgran = jsonDev["disc-gran"].toInt();
        const long long size = jsonDev["size"].toVariant().toLongLong();
        const int physec = jsonDev["phy-sec"].toInt();
        const QString &label = jsonDev["label"].toString();
        const bool crypto = (jsonDev["type"].toString() == "crypt");
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
            device->name = jsonDev["name"].toString();
            device->path = path;
            device->uuid = jsonDev["uuid"].toString();
            device->flags.rotational = rota;
            device->discgran = discgran;
            device->size = size;
            device->physec = physec;
            device->flags.bootRoot = (!bootUUID.isEmpty() && device->uuid == bootUUID);
            device->curLabel = label;
            device->curFormat = jsonDev["fstype"].toString();
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
        proc.exec("cryptsetup", {"status", virt->path}, nullptr, true);
        for (const QString &line : proc.readOutLines()) {
            const QString &trline = line.trimmed();
            if (!trline.startsWith("device:")) continue;
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

bool PartMan::manageConfig(MSettings &config, bool save) noexcept
{
    for (Device *drive : root->children) {
        // Check if the drive is to be cleared and formatted.
        size_t partCount = drive->children.size();
        bool drvPreserve = drive->flags.oldLayout;
        config.beginGroup("Storage." + drive->name);
        if (save) {
            if (!drvPreserve) {
                config.setValue("Format", drive->format);
                config.setValue("Partitions", static_cast<uint>(partCount));
            }
        } else if (config.contains("Format")) {
            drive->format = config.value("Format").toString();
            drvPreserve = false;
            drive->clear();
            partCount = config.value("Partitions").toUInt();
            if (partCount > PARTMAN_MAX_PARTS) return false;
        }
        config.endGroup();
        // Partition configuration.
        const long long sizeMax = drive->size - PARTMAN_SAFETY;
        long long sizeTotal = 0;
        for (size_t ixPart = 0; ixPart < partCount; ++ixPart) {
            Device *part = nullptr;
            if (save || drvPreserve) {
                part = drive->child(ixPart);
                if (!save) part->flags.oldLayout = true;
            } else {
                part = new Device(Device::PARTITION, drive);
                part->name = joinName(drive->name, ixPart+1);
            }
            // Configuration management, accounting for automatic control correction order.
            const QString &groupPart = "Storage." + drive->name + ".Partition" + QString::number(ixPart+1);
            config.beginGroup(groupPart);
            if (save) {
                config.setValue("Size", part->size);
                if (part->isActive()) config.setValue("Active", true);
                if (part->flags.sysEFI) config.setValue("ESP", true);
                if (part->addToCrypttab) config.setValue("AddToCrypttab", true);
                if (!part->usefor.isEmpty()) config.setValue("UseFor", part->usefor);
                if (!part->format.isEmpty()) config.setValue("Format", part->format);
                config.setValue("Encrypt", part->encrypt);
                if (!part->label.isEmpty()) config.setValue("Label", part->label);
                if (!part->options.isEmpty()) config.setValue("Options", part->options);
                config.setValue("CheckBadBlocks", part->chkbadblk);
                config.setValue("Dump", part->dump);
                config.setValue("Pass", part->pass);
            } else {
                if (!drvPreserve && config.contains("Size")) {
                    part->size = config.value("Size").toLongLong();
                    sizeTotal += part->size;
                    if (sizeTotal > sizeMax) return false;
                    if (config.value("Active").toBool()) part->setActive(true);
                }
                part->flags.sysEFI = config.value("ESP", part->flags.sysEFI).toBool();
                part->usefor = config.value("UseFor", part->usefor).toString();
                part->format = config.value("Format", part->format).toString();
                part->chkbadblk = config.value("CheckBadBlocks", part->chkbadblk).toBool();
                part->encrypt = config.value("Encrypt", part->encrypt).toBool();
                part->addToCrypttab = config.value("AddToCrypttab", part->encrypt).toBool();
                part->label = config.value("Label", part->label).toString();
                part->options = config.value("Options", part->options).toString();
                part->dump = config.value("Dump", part->dump).toBool();
                part->pass = config.value("Pass", part->pass).toInt();
            }
            size_t subvolCount = 0;
            if (part->format == "btrfs") {
                if (!save) subvolCount = config.value("Subvolumes").toUInt();
                else {
                    subvolCount = part->children.size();
                    config.setValue("Subvolumes", static_cast<uint>(subvolCount));
                }
            }
            config.endGroup();
            // Btrfs subvolume configuration.
            for (size_t ixSV=0; ixSV<subvolCount; ++ixSV) {
                Device *subvol = nullptr;
                if (save) subvol = part->child(ixSV);
                else subvol = new Device(Device::SUBVOLUME, part);
                if (!subvol) return false;
                config.beginGroup(groupPart + ".Subvolume" + QString::number(ixSV+1));
                if (save) {
                    if (subvol->isActive()) config.setValue("Default", true);
                    if (!subvol->usefor.isEmpty()) config.setValue("UseFor", subvol->usefor);
                    if (!subvol->label.isEmpty()) config.setValue("Label", subvol->label);
                    if (!subvol->options.isEmpty()) config.setValue("Options", subvol->options);
                    config.setValue("Dump", subvol->dump);
                    config.setValue("Pass", subvol->pass);
                } else {
                    if (config.value("Default").toBool()) subvol->setActive(true);
                    subvol->usefor = config.value("UseFor").toString();
                    subvol->label = config.value("Label").toString();
                    subvol->options = config.value("Options").toString();
                    subvol->dump = config.value("Dump").toBool();
                    subvol->pass = config.value("Pass").toInt();
                }
                config.endGroup();
            }
            if (!save) gui.treePartitions->expand(index(part));
        }
    }
    treeSelChange();
    return true;
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
            const size_t maxparts = (drive->format == "DOS") ? 4 : PARTMAN_MAX_PARTS;
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
            if (seldev->flags.oldLayout && seldev->curFormat == "crypto_LUKS") {
                actUnlock = menu.addAction(tr("&Unlock"));
                allowCryptTab = true;
            }
            if (allowCryptTab) {
                actAddCrypttab = menu.addAction(tr("Add to crypttab"));
                actAddCrypttab->setCheckable(true);
                actAddCrypttab->setChecked(seldev->addToCrypttab);
            }
        }
        if (seldev->finalFormat() == "btrfs") {
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
            scanSubvolumes(seldev);
            gui.treePartitions->expand(selIndex);
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

        const long long minSpace = brave ? 0 : volSpecTotal("/", QStringList()).minimum;
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
        QAction *actDefault = menu.addAction(tr("Default subvolume"));
        QAction *actRemSubvolume = menu.addAction(tr("Remove subvolume"));
        actDefault->setCheckable(true);
        actDefault->setChecked(seldev->isActive());
        actRemSubvolume->setDisabled(seldev->flags.oldLayout);

        QAction *action = menu.exec(QCursor::pos());
        if (action == actDefault) seldev->setActive(action->isChecked());
        else if (action == actRemSubvolume) delete seldev;
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
    if (QFile::exists("/usr/sbin/gparted")) proc.exec("/usr/sbin/gparted");
    else proc.exec("partitionmanager");
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
    QCheckBox *checkCrypttab = new QCheckBox(tr("Add to crypttab"), &dialog);
    editPass->setEchoMode(QLineEdit::Password);
    checkCrypttab->setChecked(true);
    layout.addRow(tr("Password:"), editPass);
    layout.addRow(checkCrypttab);
    QDialogButtonBox buttons(QDialogButtonBox::Ok | QDialogButtonBox::Cancel,
        Qt::Horizontal, &dialog);
    layout.addRow(&buttons);
    connect(&buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(&buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    if (dialog.exec() == QDialog::Accepted) {
        qApp->setOverrideCursor(Qt::WaitCursor);
        gui.boxMain->setEnabled(false);
        try {
            luksOpen(part, editPass->text().toUtf8());
            part->usefor.clear();
            part->addToCrypttab = checkCrypttab->isChecked();
            part->mapCount++;
            notifyChange(part);
            scanVirtualDevices(true);
            treeSelChange();
            gui.boxMain->setEnabled(true);
            qApp->restoreOverrideCursor();
        } catch(...) {
            // This time, failure is not a dealbreaker.
            gui.boxMain->setEnabled(true);
            qApp->restoreOverrideCursor();
            QMessageBox::warning(gui.boxMain, QString(),
                tr("Could not unlock device. Possible incorrect password."));
        }
    }
}

void PartMan::partMenuLock(Device *volume)
{
    qApp->setOverrideCursor(QCursor(Qt::WaitCursor));
    gui.boxMain->setEnabled(false);
    bool ok = false;
    // Find the associated partition and decrement its reference count if found.
    Device *origin = volume->origin;
    if (origin) ok = proc.exec("cryptsetup", {"close", volume->path});
    if (ok) {
        if (origin->mapCount > 0) origin->mapCount--;
        origin->map.clear();
        origin->addToCrypttab = false;
        notifyChange(origin);
    }
    volume->origin = nullptr;
    // Refresh virtual devices list.
    scanVirtualDevices(true);
    treeSelChange();

    gui.boxMain->setEnabled(true);
    qApp->restoreOverrideCursor();
    // If not OK then a command failed, or trying to close a non-LUKS device.
    if (!ok) {
        QMessageBox::critical(gui.boxMain, QString(), tr("Failed to close %1").arg(volume->name));
    }
}

void PartMan::scanSubvolumes(Device *part)
{
    qApp->setOverrideCursor(Qt::WaitCursor);
    gui.boxMain->setEnabled(false);
    while (part->children.size()) delete part->child(0);
    QStringList lines;
    if (!proc.exec("mount", {"--mkdir", "-o", "subvolid=5,ro",
        part->mappedDevice(), "/mnt/btrfs-scratch"})) goto END;
    proc.exec("btrfs", {"subvolume", "list", "/mnt/btrfs-scratch"}, nullptr, true);
    lines = proc.readOutLines();
    proc.exec("umount", {"/mnt/btrfs-scratch"});
    for (const QString &line : std::as_const(lines)) {
        const int start = line.indexOf("path") + 5;
        if (line.length() <= start) goto END;
        Device *subvol = new Device(Device::SUBVOLUME, part);
        subvol->flags.oldLayout = true;
        subvol->label = subvol->curLabel = line.mid(start);
        subvol->format = "PRESERVE";
        notifyChange(subvol);
    }
 END:
    gui.boxMain->setEnabled(true);
    qApp->restoreOverrideCursor();
}

bool PartMan::composeValidate(bool automatic) noexcept
{
    mounts.clear();
    // Partition use and other validation
    int swapnum = 0;
    for (Iterator it(*this); Device *volume = *it; it.next()) {
        if (volume->type == Device::DRIVE || volume->type == Device::VIRTUAL_DEVICES) continue;
        if (volume->type == Device::SUBVOLUME) {
            // Ensure the subvolume label entry is valid.
            bool ok = true;
            const QString &cmptext = volume->label.trimmed();
            if (cmptext.isEmpty()) ok = false;
            if (cmptext.count(QRegularExpression("[^A-Za-z0-9\\/\\@\\.\\-\\_]|\\/\\/"))) ok = false;
            if (cmptext.startsWith('/') || cmptext.endsWith('/')) ok = false;
            if (!ok) {
                QMessageBox::critical(gui.boxMain, QString(), tr("Invalid subvolume label"));
                return false;
            }
            // Check for duplicate subvolume label entries.
            Device *pit = volume->parentItem;
            assert(pit != nullptr);
            for (Device *sdevice : pit->children) {
                if (sdevice == volume) continue;
                if (sdevice->label.trimmed().compare(cmptext, Qt::CaseInsensitive) == 0) {
                    QMessageBox::critical(gui.boxMain, QString(), tr("Duplicate subvolume label"));
                    return false;
                }
            }
        }
        QString mount = volume->usefor;
        if (mount.isEmpty()) continue;
        if (mount == "ESP") mount = "/boot/efi";
        if (!mount.startsWith("/") && !volume->allowedUsesFor().contains(mount)) {
            QMessageBox::critical(gui.boxMain, QString(),
                tr("Invalid use for %1: %2").arg(volume->shownDevice(), mount));
            return false;
        }
        if (mount == "SWAP") {
            ++swapnum;
            if (swapnum > 1) mount+=QString::number(swapnum);
        }

        // The mount can only be selected once.
        const auto fit = mounts.find(mount);
        if (fit != mounts.cend()) {
            QMessageBox::critical(gui.boxMain, QString(), tr("%1 is already"
                " selected for: %2").arg(fit->second->shownDevice(), fit->second->usefor));
            return false;
        } else if(volume->canMount(false)) {
            mounts[mount] = volume;
        }
    }

    const auto fitroot = mounts.find("/");
    if (fitroot == mounts.cend()) {
        const long long rootMin = volSpecTotal("/").minimum;
        const QString &tMinRoot = QLocale::system().formattedDataSize(rootMin,
            1, QLocale::DataSizeTraditionalFormat);
        QMessageBox::critical(gui.boxMain, QString(),
            tr("A root partition of at least %1 is required.").arg(tMinRoot));
        return false;
    }

    if (!fitroot->second->willFormat() && mounts.count("/home")>0) {
        QMessageBox::critical(gui.boxMain, QString(),
            tr("Cannot preserve /home inside root (/) if a separate /home partition is also mounted."));
        return false;
    }

    // Confirmation page action list
    for (const Device *drive : std::as_const(root->children)) {
        if (!drive->flags.oldLayout && drive->type != Device::VIRTUAL_DEVICES) {
            gui.listConfirm->addItem(tr("Prepare %1 partition table on %2").arg(drive->format, drive->name));
        }
        for (const Device *part : std::as_const(drive->children)) {
            QString actmsg;
            if (drive->flags.oldLayout) {
                if (part->usefor.isEmpty()) {
                    if (part->children.size() > 0) actmsg = tr("Reuse (no reformat) %1");
                    else continue;
                } else {
                    if (part->usefor == "FORMAT") actmsg = tr("Format %1");
                    else if (part->willFormat()) actmsg = tr("Format %1 to use for %2");
                    else if (part->usefor != "/") actmsg = tr("Reuse (no reformat) %1 as %2");
                    else actmsg = tr("Delete the data on %1 except for /home, to use for %2");
                }
            } else {
                if (part->usefor.isEmpty()) actmsg = tr("Create %1 without formatting");
                else actmsg = tr("Create %1, format to use for %2");
            }
            // QString::arg() emits warnings if a marker is not in the string.
            gui.listConfirm->addItem(actmsg.replace("%1", part->shownDevice()).replace("%2", part->usefor));

            for (const Device *subvol : std::as_const(part->children)) {
                const bool svnouse = subvol->usefor.isEmpty();
                if (subvol->format == "PRESERVE") {
                    if (svnouse) continue;
                    else actmsg = tr("Reuse subvolume %1 as %2");
                } else if (subvol->format == "DELETE") {
                    actmsg = tr("Delete subvolume %1");
                } else if (subvol->format == "CREATE") {
                    if (subvol->flags.oldLayout) {
                        if (svnouse) actmsg = tr("Overwrite subvolume %1");
                        else actmsg = tr("Overwrite subvolume %1 to use for %2");
                    } else {
                        if (svnouse) actmsg = tr("Create subvolume %1");
                        else actmsg = tr("Create subvolume %1 to use for %2");
                    }
                }
                // QString::arg() emits warnings if a marker is not in the string.
                gui.listConfirm->addItem(" + " + actmsg.replace("%1", subvol->label).replace("%2", subvol->usefor));
            }
        }
    }
    if (!automatic) {
        // Warning messages
        QMessageBox msgbox(gui.boxMain);
        msgbox.setIcon(QMessageBox::Warning);
        msgbox.setStandardButtons(QMessageBox::Yes|QMessageBox::No);
        msgbox.setDefaultButton(QMessageBox::No);

        if (fitroot->second->willEncrypt() && mounts.count("/boot")==0) {
            msgbox.setText(tr("You must choose a separate boot partition when encrypting root.")
                + '\n' + tr("Are you sure you want to continue?"));
            if (msgbox.exec() != QMessageBox::Yes) return false;
        }
        if (!confirmSpace(msgbox)) return false;
        if (!confirmBootable(msgbox)) return false;
    }

    return true;
}
bool PartMan::confirmSpace(QMessageBox &msgbox) noexcept
{
    // Isolate used points from each other in total calculations
    QStringList vols;
    for (Iterator it(*this); *it; it.next()) {
        const QString &use = (*it)->usefor;
        if (!use.isEmpty()) vols.append(use);
    }

    QStringList toosmall;
    for (const Device *drive : std::as_const(root->children)) {
        for (const Device *part : std::as_const(drive->children)) {
            bool isused = !part->usefor.isEmpty();
            long long minsize = isused ? volSpecTotal(part->usefor, vols).minimum : 0;

            // First pass = get the total minimum required for all subvolumes.
            for (const Device *subvol : std::as_const(part->children)) {
                if(!subvol->usefor.isEmpty()) {
                    minsize += volSpecTotal(subvol->usefor, vols).minimum;
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

                    const long long svmin = volSpecTotal(subvol->usefor, vols).minimum;
                    if (svmin > 0) {
                        svsmall << msgsz.arg(subvol->usefor, subvol->shownDevice(),
                            QLocale::system().formattedDataSize(svmin, 1, QLocale::DataSizeTraditionalFormat));
                    }
                }
                if (!svsmall.isEmpty()) {
                    svsmall.sort();
                    toosmall.last() += "<ul style='margin:0; list-style-type:circle'><li>"
                        + svsmall.join("</li><li>") + "</li></ul>";
                }
            }
        }
    }

    if (!toosmall.isEmpty()) {
        toosmall.sort();
        msgbox.setText(tr("The installation may fail because the following volumes are too small:")
            + "<br/><ul style='margin:0'><li>" + toosmall.join("</li><li>") + "</li></ul><br/>"
            + tr("Are you sure you want to continue?"));
        if (msgbox.exec() != QMessageBox::Yes) return false;
    }
    return true;
}
bool PartMan::confirmBootable(QMessageBox &msgbox) noexcept
{
    if (proc.detectEFI()) {
        const char *msgtext = nullptr;
        auto fitefi = mounts.find("/boot/efi");
        if (fitefi == mounts.end()) {
            msgtext = QT_TR_NOOP("This system uses EFI, but no valid"
                " EFI system partition was assigned to /boot/efi separately.");
        } else if (!fitefi->second->flags.sysEFI) {
            msgtext = QT_TR_NOOP("The volume assigned to /boot/efi"
                " is not a valid EFI system partition.");
        }
        if (msgtext) {
            msgbox.setText(tr(msgtext) + '\n' + tr("Are you sure you want to continue?"));
            if (msgbox.exec() != QMessageBox::Yes) return false;
        }
        return true;
    }

    // BIOS with GPT
    QString biosgpt;
    for (const Device *drive : std::as_const(root->children)) {
        bool hasBiosGrub = false;
        for (const Device *part : std::as_const(drive->children)) {
            if (part->finalFormat() == "BIOS-GRUB") {
                hasBiosGrub = true;
                break;
            }
        }
        // Potentially unbootable GPT when on a BIOS-based PC.
        const bool hasBoot = (drive->active != nullptr);
        if (!proc.detectEFI() && drive->format=="GPT" && hasBoot && !hasBiosGrub) {
            biosgpt += ' ' + drive->name;
        }
    }
    if (!biosgpt.isEmpty()) {
        biosgpt.prepend(tr("The following drives are, or will be, setup with GPT,"
            " but do not have a BIOS-GRUB partition:") + "\n\n");
        biosgpt += "\n\n" + tr("This system may not boot from GPT drives without a BIOS-GRUB partition.")
            + '\n' + tr("Are you sure you want to continue?");
        msgbox.setText(biosgpt);
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
            QString smartMsg = drive->name + " (" + purposes.join(", ") + ")";
            proc.exec("smartctl", {"-H", drive->path});
            if (proc.exitStatus() == MProcess::NormalExit && proc.exitCode() & 8) {
                // See smartctl(8) manual: EXIT STATUS (Bit 3)
                smartFail += " - " + smartMsg + "\n";
            } else {
                proc.shell("smartctl -A " + drive->path + "| grep -E \"^  5|^196|^197|^198\""
                    " | awk '{ if ( $10 != 0 ) { print $1,$2,$10} }'", nullptr, true);
                const QString &out = proc.readOut();
                if (!out.isEmpty()) smartWarn += " ---- " + smartMsg + " ---\n" + out + "\n\n";
            }
        }
    }

    QString msg;
    if (!smartFail.isEmpty()) {
        msg = tr("The disks with the partitions you selected for installation are failing:")
            + "\n\n" + smartFail + "\n";
    }
    if (!smartWarn.isEmpty()) {
        msg += tr("Smartmon tool output:") + "\n\n" + smartWarn
            + tr("The disks with the partitions you selected for installation pass the SMART monitor test (smartctl),"
                " but the tests indicate it will have a higher than average failure rate in the near future.");
    }
    if (!msg.isEmpty()) {
        QMessageBox msgbox(gui.boxMain);
        msgbox.setStandardButtons(QMessageBox::Yes|QMessageBox::No);
        msgbox.setDefaultButton(QMessageBox::Yes);
        msg += tr("If unsure, please exit the Installer and run GSmartControl for more information.") + "\n\n";
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

PartMan::Device *PartMan::selectedDriveAuto() noexcept
{
    QString drv(gui.comboDisk->currentData().toString());
    if (!findByPath("/dev/" + drv)) return nullptr;
    for (Device *drive : root->children) {
        if (drive->name == drv) return drive;
    }
    return nullptr;
}

void PartMan::clearAllUses() noexcept
{
    for (Iterator it(*this); Device *device = *it; it.next()) {
        device->usefor.clear();
        if (device->type == Device::PARTITION) device->setActive(false);
        notifyChange(device);
    }
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
    for (const auto &it : mounts) {
        qDebug() << " -" << it.first << '-' << it.second->shownDevice()
            << it.second->map << it.second->path;
    }

    // Prepare and mount storage
    preparePartitions();
    luksFormat();
    formatPartitions();
    mountPartitions();
    // Refresh the UUIDs
    proc.exec("lsblk", {"--list", "-bJo", "PATH,UUID"}, nullptr, true);
    const QJsonObject &jsonObjBD = QJsonDocument::fromJson(proc.readOut(true).toUtf8()).object();
    const QJsonArray &jsonBD = jsonObjBD["blockdevices"].toArray();
    std::map<QString, Device *> alldevs;
    for (Iterator it(*this); Device *device = *it; it.next()) {
        alldevs[device->path] = device;
    }
    for (const QJsonValue &jsonDev : jsonBD) {
        const auto fit = alldevs.find(jsonDev["path"].toString());
        if (fit != alldevs.cend()) fit->second->uuid = jsonDev["uuid"].toString();
    }
}
bool PartMan::installTabs() noexcept
{
    proc.log("Install tabs");
    if (!makeFstab()) return false;
    if (!makeCrypttab()) return false;
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
                if (!part->usefor.isEmpty()) listToUnmount << part->path;
            }
        } else {
            // Clearing the drive, so mark all partitions on the drive for unmounting.
            proc.exec("lsblk", {"-nro", "path", drive->path}, nullptr, true);
            listToUnmount << proc.readOutLines();
        }
    }

    // Clean up any leftovers.
    {
        MProcess::Section sect2(proc, nullptr);
        if (QFileInfo::exists("/mnt/antiX")) {
            proc.exec("umount", {"-qR", "/mnt/antiX"});
            proc.exec("rm", {"-rf", "/mnt/antiX"});
        }
        // Detach swap and file systems of targets which may have been auto-mounted.
        proc.exec("swapon", {"--show=NAME", "--noheadings"}, nullptr, true);
        const QStringList swaps = proc.readOutLines();
        proc.exec("findmnt", {"--raw", "-o", "SOURCE"}, nullptr, true);
        const QStringList fsmounts = proc.readOutLines();
        for (const QString &devpath : std::as_const(listToUnmount)) {
            if (swaps.contains(devpath)) proc.exec("swapoff", {devpath});
            if (fsmounts.contains(devpath)) proc.exec("umount", {"-q", devpath});
        }
    }

    // Prepare partition tables on devices which will have a new layout.
    for (const Device *drive : std::as_const(root->children)) {
        if (drive->flags.oldLayout || drive->type == Device::VIRTUAL_DEVICES) continue;
        proc.status(tr("Preparing partition tables"));

        // Wipe the first and last 4MB to clear the partition tables, turbo-nuke style.
        const int gran = std::max(drive->discgran, drive->physec);
        const char *opts = drive->discgran ? "-fv" : "-fvz";
        // First 17KB = primary partition table (accounts for both MBR and GPT disks).
        // First 17KB, from 32KB = sneaky iso-hybrid partition table (maybe USB with an ISO burned onto it).
        const long long length = (4*MB + (gran - 1)) / gran; // ceiling
        sect.setExceptionMode(false);
        if (proc.shell("lspci -n | grep -qE '80ee:beef|80ee:cafe'")) {
            opts = "-fvz"; // Force zeroing under VirtualBox due to a bug where TRIM fails.
        }
        sect.setExceptionMode(true);
        proc.exec("blkdiscard", {opts, "-l", QString::number(length*gran), drive->path});
        // Last 17KB = secondary partition table (for GPT disks).
        const long long offset = (drive->size - 4*MB) / gran; // floor
        proc.exec("blkdiscard", {opts, "-o", QString::number(offset*gran), drive->path});

        proc.exec("parted", {"-s", drive->path, "mklabel", (drive->format=="GPT" ? "gpt" : "msdos")});
    }

    // Prepare partition tables, creating tables and partitions when needed.
    proc.status(tr("Preparing required partitions"));
    for (const Device *drive : std::as_const(root->children)) {
        bool partupdate = false;
        if (drive->type == Device::VIRTUAL_DEVICES) continue;
        const int devCount = drive->children.size();
        const bool useGPT = (drive->finalFormat() == "GPT");
        if (drive->flags.oldLayout) {
            // Using existing partitions.
            QString cmd; // command to set the partition type
            if (useGPT) cmd = "sgdisk /dev/%1 -q --typecode=%2:%3";
            else cmd = "sfdisk /dev/%1 -q --part-type %2 %3";
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
                proc.exec("parted", {"-s", "--align", "optimal", drive->path,
                    "mkpart" , "primary", QString::number(start) + "MiB", QString::number(end) + "MiB"});
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
                QStringList cargs({"-s", "/dev/" + devsplit.drive, "set", devsplit.partition});
                cargs.append(useGPT ? "legacy_boot" : "boot");
                cargs.append("on");
                proc.exec("parted", cargs);
                partupdate = true;
            }
            if (part->flags.sysEFI != part->flags.curESP) {
                const NameParts &devsplit = splitName(part->name);
                proc.exec("parted", {"-s", "/dev/" + devsplit.drive, "set", devsplit.partition,
                    "esp", part->flags.sysEFI ? "on" : "off"});
                partupdate = true;
            }
            proc.status();
        }
        // Update kernel partition records.
        if (partupdate) proc.exec("partx", {"-u", drive->path});
    }
}

void PartMan::luksFormat()
{
    MProcess::Section sect(proc, QT_TR_NOOP("Failed to format LUKS container."));
    const QByteArray &encPass = gui.textCryptoPass->text().toUtf8();
    for (Iterator it(*this); Device *part = *it; it.next()) {
        if (!part->encrypt || !part->willFormat()) continue;
        proc.status(tr("Creating encrypted volume: %1").arg(part->name));
        proc.exec("cryptsetup", {"--batch-mode", "--key-size=512",
            "--hash=sha512", "luksFormat", part->path}, &encPass);
        proc.status();
        luksOpen(part, encPass);
    }
}
void PartMan::luksOpen(Device *part, const QByteArray &password)
{
    MProcess::Section sect(proc, QT_TR_NOOP("Failed to open LUKS container."));
    if (part->map.isEmpty()) {
        proc.exec("cryptsetup", {"luksUUID", part->path}, nullptr, true);
        part->map = "luks-" + proc.readAll().trimmed();
    }
    QStringList cargs({"open", part->path, part->map, "--type", "luks"});
    if (part->discgran) cargs.prepend("--allow-discards");
    proc.exec("cryptsetup", cargs, &password);
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
        if (volume->usefor == "FORMAT") proc.status(fmtstatus.arg(volume->name));
        else proc.status(fmtstatus.arg(volume->usefor));
        if (volume->usefor == "BIOS-GRUB") {
        sect.setExceptionMode(false);
        if (proc.shell("lspci -n | grep -qE '80ee:beef|80ee:cafe'")) {
            sect.setExceptionMode(true);
            proc.exec("blkdiscard", {"-fvz", dev}); // Force zeroing under VirtualBox due to a bug where TRIM fails.
        } else {
            sect.setExceptionMode(true);
            proc.exec("blkdiscard", {volume->discgran ? "-fv" : "-fvz", dev});
        }
            const NameParts &devsplit = splitName(dev);
            proc.exec("parted", {"-s", "/dev/" + devsplit.drive, "set", devsplit.partition, "bios_grub", "on"});
        } else if (volume->usefor == "SWAP") {
            QStringList cargs({"-q", dev});
            if (!volume->label.isEmpty()) cargs << "-L" << volume->label;
            proc.exec("mkswap", cargs);
        } else if (volume->format.left(3) == "FAT") {
            QStringList cargs({"-F", volume->format.mid(3)});
            if (volume->chkbadblk) cargs.append("-c");
            if (!volume->label.isEmpty()) cargs << "-n" << volume->label.trimmed().left(11);
            cargs.append(dev);
            proc.exec("mkfs.msdos", cargs);
        } else {
            // Transplanted from minstall.cpp and modified to suit.
            const QString &format = volume->format;
            QStringList cargs;
            if (format == "btrfs") {
                cargs.append("-f");
                proc.exec("cp", {"-fp", "/usr/bin/true", "/usr/sbin/fsck.auto"});
                if (volume->size < 6000000000) {
                    cargs << "-M" << "-O" << "skinny-metadata"; // Mixed mode (-M)
                }
            } else if (format == "xfs" || format == "f2fs") {
                cargs.append("-f");
            } else { // jfs, ext2, ext3, ext4
                if (format == "jfs") cargs.append("-q");
                else cargs.append("-F");
                if (volume->chkbadblk) cargs.append("-c");
            }
            cargs.append(dev);
            if (!volume->label.isEmpty()) {
                if (format == "f2fs") cargs.append("-l");
                else cargs.append("-L");
                cargs.append(volume->label);
            }
            proc.exec("mkfs." + format, cargs);
            if (format.startsWith("ext")) {
                proc.exec("tune2fs", {"-c0", "-C0", "-i1m", dev}); // ext4 tuning
            }
        }
    }
    // Prepare subvolumes on all that (are to) contain them.
    for (Iterator it(*this); Device *device = *it; it.next()) {
        if (device->isVolume() && device->finalFormat() == "btrfs") {
            prepareSubvolumes(*it);
        }
    }
}

void PartMan::prepareSubvolumes(Device *part)
{
    const int subvolcount = part->children.size();
    if (subvolcount <= 0) return;
    MProcess::Section sect(proc, QT_TR_NOOP("Failed to prepare subvolumes."));
    proc.status(tr("Preparing subvolumes"));

    proc.exec("mount", {"--mkdir", "-o", "subvolid=5,noatime", part->mappedDevice(), "/mnt/btrfs-scratch"});
    const char *errmsg = nullptr;
    try {
        // Since the default subvolume cannot be deleted, ensure the default is set to the top.
        proc.exec("btrfs", {"-q", "subvolume", "set-default", "5", "/mnt/btrfs-scratch"});
        for (int ixsv = 0; ixsv < subvolcount; ++ixsv) {
            Device *subvol = part->child(ixsv);
            assert(subvol != nullptr);
            const QString &svpath = "/mnt/btrfs-scratch/" + subvol->label;
            if (subvol->format != "PRESERVE" && QFileInfo::exists(svpath)) {
                proc.exec("btrfs", {"-q", "subvolume", "delete", svpath});
            }
            if (subvol->format == "CREATE") {
                proc.exec("btrfs", {"-q", "subvolume", "create", svpath});
            }
            proc.status();
        }
        // Set the default subvolume if one was chosen.
        if (part->active) {
            proc.exec("btrfs", {"-q", "subvolume", "set-default", "/mnt/btrfs-scratch/"+part->active->label});
        }
    } catch(const char *msg) {
        errmsg = msg;
    }
    proc.exec("umount", {"/mnt/btrfs-scratch"});
    if (errmsg) throw errmsg;
}

// write out crypttab if encrypting for auto-opening
bool PartMan::makeCrypttab() noexcept
{
    // Count the number of crypttab entries.
    int cryptcount = 0;
    for (Iterator it(*this); Device *device = *it; it.next()) {
        if (device->addToCrypttab) ++cryptcount;
    }
    // Add devices to crypttab.
    QFile file("/mnt/antiX/etc/crypttab");
    if (!file.open(QIODevice::WriteOnly)) return false;
    QTextStream out(&file);
    for (Iterator it(*this); Device *device = *it; it.next()) {
        if (!device->addToCrypttab) continue;
        out << device->map << " UUID=" << device->uuid << " none luks";
        if (cryptcount > 1) out << ",keyscript=decrypt_keyctl";
        if (!device->flags.rotational) out << ",no-read-workqueue,no-write-workqueue";
        if (device->discgran) out << ",discard";
        out << '\n';
    }
    return true;
}

// create /etc/fstab file
bool PartMan::makeFstab() noexcept
{
    QFile file("/mnt/antiX/etc/fstab");
    if (!file.open(QIODevice::WriteOnly)) return false;
    QTextStream out(&file);
    out << "# Pluggable devices are handled by uDev, they are not in fstab\n";
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
                proc.exec("blkid", {"-p", "-s","UUID", dev}, nullptr, true);
                UUID = proc.readOut().section("=", 1, 1).section("\"",1,1);
                qDebug() << "UUID:" << UUID;
            }
            out << "UUID=" << UUID;
        }
        // Mount point, file system
        QString fsfmt;
        if (volume->type == Device::SUBVOLUME) {
            fsfmt = "btrfs";
        } else {
            fsfmt = volume->finalFormat();
        }

        if (fsfmt.compare("swap", Qt::CaseInsensitive) == 0) {
            out << " swap swap";
        } else {
            out << ' ' << it.first;
            if (fsfmt.startsWith("FAT")) out << " vfat";
            else out << ' ' << fsfmt;
        }
        // Options
        const QString &mountopts = volume->options;
        if (volume->type == Device::SUBVOLUME) {
            out << " subvol=" << volume->label;
            if (!mountopts.isEmpty()) out << ',' << mountopts;
        } else {
            if (mountopts.isEmpty()) out << " defaults";
            else out << ' ' << mountopts;
        }
        if (volume->canMount()) {
            out << ' ' << (volume->dump ? 1 : 0);
            out << ' ' << volume->pass;
        }
        if (fsfmt.compare("swap", Qt::CaseInsensitive) == 0) {
            out <<" 0 0";
        }
        out << '\n';
    }
    file.close();
    return true;
}

void PartMan::mountPartitions()
{
    proc.log(__PRETTY_FUNCTION__, MProcess::LOG_MARKER);
    MProcess::Section sect(proc, QT_TR_NOOP("Failed to mount partition."));
    for (auto &it : mounts) {
        if (it.first.at(0) != '/') continue;
        const QString point("/mnt/antiX" + it.first);
        const QString &dev = it.second->mappedDevice();
        proc.status(tr("Mounting: %1").arg(it.first));
        QStringList opts;
        if (it.second->type == Device::SUBVOLUME) {
            opts.append("subvol=" + it.second->label);
        }
        // Use noatime to speed up the installation.
        opts.append(it.second->options.split(','));
        opts.removeAll("ro");
        opts.removeAll("defaults");
        opts.removeAll("atime");
        opts.removeAll("relatime");
        if (it.second->finalFormat() == "ext4" && !opts.contains("noinit_itable")) {
            opts.append("noinit_itable");
        }
        if (!opts.contains("async")) opts.append("async");
        if (!opts.contains("noiversion")) opts.append("noiversion");
        if (!opts.contains("noatime")) opts.append("noatime");
        if (!opts.contains("lazytime")) opts.append("lazytime");
        proc.exec("mount", {"--mkdir", "-o", opts.join(','), dev, point});
    }
}

void PartMan::clearWorkArea()
{
    // Close swap files that may have been opened (should not have swap opened in the first place)
    proc.exec("swapon", {"--show=NAME", "--noheadings"}, nullptr, true);
    QStringList swaps = proc.readOutLines();
    for (Iterator it(*this); *it; it.next()) {
        const QString &dev = (*it)->mappedDevice();
        if (swaps.contains(dev)) proc.exec("swapoff", {dev});
    }
    // Unmount everything in /mnt/antiX which is only to be for working on the target system.
    if (QFileInfo::exists("/mnt/antiX")) proc.exec("umount", {"-qR", "/mnt/antiX"});
    // Close encrypted containers that were opened by the installer.
    for (Iterator it(*this); Device *device = *it; it.next()) {
        if (device->encrypt && device->type != Device::VIRTUAL && QFile::exists(device->mappedDevice())) {
            proc.exec("cryptsetup", {"close", device->map});
        }
    }
}

// Public properties
int PartMan::swapCount() const noexcept
{
    int count = 0;
    for (const auto &mount : mounts) {
        if (mount.first.startsWith("SWAP")) ++count;
    }
    return count;
}

int PartMan::isEncrypt(const QString &point) const noexcept
{
    int count = 0;
    if (point.isEmpty()) {
        for (const auto &mount : mounts) {
            if (mount.second->willEncrypt()) ++count;
        }
    } else if (point == "SWAP") {
        for (const auto &mount : mounts) {
            if (mount.first.startsWith("SWAP") && mount.second->willEncrypt()) ++count;
        }
    } else {
        const auto fit = mounts.find(point);
        if (fit != mounts.cend() && fit->second->willEncrypt()) ++count;
    }
    return count;
}

PartMan::Device *PartMan::findByPath(const QString &devpath) const noexcept
{
    for (Iterator it(*this); *it; it.next()) {
        if ((*it)->path == devpath) return *it;
    }
    return nullptr;
}
PartMan::Device *PartMan::findHostDev(const QString &path) const noexcept
{
    QString spath = path;
    Device *device = nullptr;
    do {
        spath = QDir::cleanPath(QFileInfo(spath).path());
        const auto fit = mounts.find(spath);
        if (fit != mounts.cend()) device = mounts.at(spath);
    } while(!device && !spath.isEmpty() && spath != '/' && spath != '.');
    return device;
}

struct PartMan::VolumeSpec PartMan::volSpecTotal(const QString &path, const QStringList &vols) const noexcept
{
    struct VolumeSpec vspec = {0};
    const auto fit = volSpecs.find(path);
    if (fit != volSpecs.cend()) vspec = fit->second;
    const QString &spath = (path!='/' ? path+'/' : path);
    for (const auto &it : volSpecs) {
        if (it.first.size() > 1 && it.first.startsWith(spath) && !vols.contains(it.first)) {
            vspec.image += it.second.image;
            vspec.minimum += it.second.minimum;
            vspec.preferred += it.second.preferred;
        }
    }
    return vspec;
}
struct PartMan::VolumeSpec PartMan::volSpecTotal(const QString &path) const noexcept
{
    QStringList vols;
    for (const auto &mount : mounts) vols.append(mount.first);
    return volSpecTotal(path, vols);
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
            else return device->path + "\n(" + device->origin->name + ')';
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
            return QIcon::fromTheme("unlock");
        } else if (device->mapCount) {
            return QIcon::fromTheme("lock");
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
        if (device->type == Device::PARTITION && device->parent() && device->parent()->finalFormat() == "GPT") {
            flagsOut |= Qt::ItemIsUserCheckable;
        }
        break;
    case COL_USEFOR:
        if (device->allowedUsesFor().count() >= 1 && device->format != "DELETE") {
            flagsOut |= Qt::ItemIsEditable;
        }
        break;
    case COL_LABEL:
        if (device->format != "PRESERVE" && device->format != "DELETE") {
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
        if (device->format.startsWith("ext") || device->format == "jfs"
            || device->format.startsWith("FAT", Qt::CaseInsensitive)) {
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
        if (changing->usefor.isEmpty() || changing->usefor == "FORMAT") {
            changing->options.clear();
        }
        if (changing->format != "btrfs") {
            // Clear all subvolumes if not supported.
            while (changing->children.size()) delete changing->child(0);
        } else {
            // Remove preserve option from all subvolumes.
            for (Device *cdevice : changing->children) {
                cdevice->format = "CREATE";
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
inline bool PartMan::Device::isActive() const noexcept
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
    return format != "PRESERVE" && !usefor.isEmpty();
}
bool PartMan::Device::canEncrypt() const noexcept
{
    if (type != PARTITION) return false;
    return !(flags.sysEFI || usefor.isEmpty() || usefor == "BIOS-GRUB" || usefor == "/boot");
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
        if (!d.isEmpty()) return "/dev/mapper/" + d;
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
        checkAndAdd("FORMAT");
        if (type != VIRTUAL) {
            if ((all || size <= 1*MB) && parentItem && parentItem->format == "GPT") {
                checkAndAdd("BIOS-GRUB");
            }
            if (all || size <= 8*GB) {
                if (size < (2*TB - 512)) {
                    if (all || flags.sysEFI || size <= 512*MB) checkAndAdd("ESP");
                }
                checkAndAdd("/boot"); // static files of the boot loader
            }
        }
    }
    if (!flags.sysEFI) {
        // Debian 12 installer order: / /boot /home /tmp /usr /var /srv /opt /usr/local
        checkAndAdd("/"); // the root file system
        checkAndAdd("/home"); // user home directories
        checkAndAdd("/usr"); // static data
        // checkAndAdd("/usr/local"); // local hierarchy
        checkAndAdd("/var"); // variable data
        // checkAndAdd("/tmp"); // temporary files
        // checkAndAdd("/srv"); // data for services provided by this system
        // checkAndAdd("/opt"); // add-on application software packages
        if (type != SUBVOLUME) checkAndAdd("SWAP");
        else checkAndAdd("/swap");
    }
    return list;
}
QStringList PartMan::Device::allowedFormats() const noexcept
{
    QStringList list;
    if (type == DRIVE && !flags.oldLayout) {
        list.append("GPT");
        if (size < 2*TB && children.size() <= 4) {
            if (partman.proc.detectEFI()) {
                list.append("DOS");
            } else {
                list.prepend("DOS");
            }
        }
        return list;
    }

    bool allowPreserve = false;
    if (isVolume()) {
        if (usefor.isEmpty()) return QStringList();
        else if (usefor == "BIOS-GRUB") list.append("BIOS-GRUB");
        else if (usefor == "SWAP") {
            list.append("SWAP");
            allowPreserve = list.contains(curFormat, Qt::CaseInsensitive);
        } else {
            if (!flags.sysEFI) {
                list << "ext4";
                if (usefor != "/boot") {
                    list << "ext3" << "ext2";
                    list << "f2fs" << "jfs" << "xfs" << "btrfs";
                }
            }
            if (size <= (2*TB - 64*KB)) list.append("FAT32");
            if (size <= (4*GB - 64*KB)) list.append("FAT16");
            if (size <= (32*MB - 512)) list.append("FAT12");

            if (usefor != "FORMAT") allowPreserve = list.contains(curFormat, Qt::CaseInsensitive);
        }
    } else if (type == SUBVOLUME) {
        list.append("CREATE");
        if (flags.oldLayout) {
            list.append("DELETE");
            allowPreserve = true;
        }
    }

    if (encrypt) allowPreserve = false;
    if (allowPreserve) {
        // People often share SWAP partitions between distros and need to keep the same UUIDs.
        if (flags.sysEFI || usefor == "/home" || !allowedUsesFor().contains(usefor) || usefor == "SWAP") {
            list.prepend("PRESERVE"); // Preserve ESP, SWAP, custom mounts and /home by default.
        } else {
            list.append("PRESERVE");
        }
    }
    return list;
}
QString PartMan::Device::finalFormat() const noexcept
{
    if (type == SUBVOLUME) return parentItem->format;
    return (type != DRIVE && (usefor.isEmpty() || format == "PRESERVE")) ? curFormat : format;
}
QString PartMan::Device::shownFormat(const QString &fmt) const noexcept
{
    if (fmt == "CREATE") return flags.oldLayout ? tr("Overwrite") : tr("Create");
    else if (fmt == "DELETE") return tr("Delete");
    else if (fmt != "PRESERVE") return fmt;
    else {
        if (type == SUBVOLUME) return tr("Preserve");
        else if (usefor != "/") return tr("Preserve (%1)").arg(curFormat);
        else return tr("Preserve /home (%1)").arg(curFormat);
    }
}
bool PartMan::Device::canMount(bool pointonly) const noexcept
{
    return !usefor.isEmpty() && usefor != "FORMAT" && usefor != "BIOS-GRUB"
        && (!pointonly || usefor != "SWAP");
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
    partman.notifyChange(part);
    return part;
}
void PartMan::Device::driveAutoSetActive() noexcept
{
    if (active) return;
    if (partman.proc.detectEFI() && format=="GPT") return;
    // Cannot use partman.mounts map here as it may not be populated.
    for (const QString &pref : QStringList({"/boot", "/"})) {
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
        if ((type != VIRTUAL) && (usefor == "/boot" || usefor == "/")) {
            Device *drive = this;
            while (drive && drive->type != DRIVE) drive = drive->parentItem;
            if (drive) drive->driveAutoSetActive();
        }
        if (type == SUBVOLUME && usefor == "/") setActive(true);
        if (usefor == "ESP" || usefor == "/boot/efi") flags.sysEFI = true;

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
        if (usefor == "SWAP") options = discgran ? "discard" : "defaults";
        else if (finalFormat().startsWith("FAT")) {
            options = "noatime,dmask=0002,fmask=0113";
            pass = 0;
            dump = false;
        } else {
            if (usefor == "/boot" || usefor == "/") {
                pass = (format == "btrfs") ? 0 : 1;
            }
            options.clear();
            const bool btrfs = (format == "btrfs" || type == SUBVOLUME);
            if (!flags.rotational && btrfs) options += "ssd,";
            if (discgran && (format == "ext4" || format == "xfs")) options += "discard,";
            else if (discgran && btrfs) options += "discard=async,";
            options += "noatime";
            if (btrfs && usefor != "/swap") options += ",compress=zstd:1";
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
        cdevice->path = "/dev/" + cdevice->name;
        partman.notifyChange(cdevice, PartMan::COL_DEVICE, PartMan::COL_DEVICE);
    }
    partman.resizeColumnsToFit();
}

// Return block device info that is suitable for a combo box.
void PartMan::Device::addToCombo(QComboBox *combo, bool warnNasty) const noexcept
{
    QString strout(QLocale::system().formattedDataSize(size, 1, QLocale::DataSizeTraditionalFormat));
    strout += ' ' + finalFormat();
    if (!label.isEmpty()) strout += " - " + label;
    if (!model.isEmpty()) strout += (label.isEmpty() ? " - " : "; ") + model;
    QString stricon;
    if (!flags.oldLayout || !usefor.isEmpty()) stricon = ":/appointment-soon";
    else if (flags.nasty && warnNasty) stricon = ":/dialog-warning";
    combo->addItem(QIcon(stricon), name + " (" + strout + ")", name);
}
// Split a device name into its drive and partition.
PartMan::NameParts PartMan::splitName(const QString &devname) noexcept
{
    const QRegularExpression rxdev1("^(?:\\/dev\\/)*(mmcblk.*|nvme.*)p([0-9]*)$");
    const QRegularExpression rxdev2("^(?:\\/dev\\/)*([a-z]*)([0-9]*)$");
    QRegularExpressionMatch rxmatch(rxdev1.match(devname));
    if (!rxmatch.hasMatch()) rxmatch = rxdev2.match(devname);
    return {rxmatch.captured(1), rxmatch.captured(2)};
}
QString PartMan::joinName(const QString &drive, int partnum) noexcept
{
    QString name = drive;
    if (name.startsWith("nvme") || name.startsWith("mmcblk")) name += 'p';
    return (name + QString::number(partnum));
}

/* A very slimmed down and non-standard one-way tree iterator. */
PartMan::Iterator::Iterator(const PartMan &partman) noexcept
{
    if (partman.root->children.size() < 1) return;
    ixParents.push(0);
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
        while (!chnext && parent) {
            parent = parent->parentItem;
            if (!parent) break;
            ixPos = ixParents.top();
            ixParents.pop();
            chnext = parent->child(ixPos+1);
        }
        if (chnext) ++ixPos;
        pos = chnext;
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
        for (const QString &fmt : device->allowedFormats()) {
            const int fw = option.fontMetrics.boundingRect(device->shownFormat(fmt)).width() + residue;
            if (fw > width) width = fw;
        }
        break;
    case PartMan::COL_USEFOR:
        for (const QString &use : device->allowedUsesFor(false)) {
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
    QComboBox *combo = nullptr;
    switch (index.column())
    {
    case PartMan::COL_SIZE:
        {
            QSpinBox *spin = new QSpinBox(parent);
            spin->setSuffix("MB");
            spin->setStepType(QSpinBox::AdaptiveDecimalStepType);
            spin->setAccelerated(true);
            spin->setSpecialValueText("MAX");
            widget = spin;
        }
        break;
    case PartMan::COL_USEFOR:
        widget = combo = new QComboBox(parent);
        combo->setEditable(true);
        combo->setInsertPolicy(QComboBox::NoInsert);
        combo->lineEdit()->setPlaceholderText("----");
        break;
    case PartMan::COL_FORMAT: widget = combo = new QComboBox(parent); break;
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
    if (combo) {
        connect(combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ItemDelegate::emitCommit);
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
            combo->addItem("");
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
                    combo->insertItem(0, device->shownFormat("PRESERVE"), "PRESERVE");
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
void PartMan::ItemDelegate::emitCommit() noexcept
{
    emit commitData(qobject_cast<QWidget *>(sender()));
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
    if (selFS == "PRESERVE") selFS = part->curFormat;
    if ((part->type == PartMan::Device::PARTITION && selFS == "btrfs") || part->type == PartMan::Device::SUBVOLUME) {
        QString tcommon;
        if (!part->flags.rotational) tcommon = "ssd,";
        if (part->discgran) tcommon = "discard=async,";
        tcommon += "noatime";
        QAction *action = menuTemplates->addAction(tr("Compression (Z&STD)"));
        action->setData(tcommon + ",compress=zstd");
        action = menuTemplates->addAction(tr("Compression (&LZO)"));
        action->setData(tcommon + ",compress=lzo");
        action = menuTemplates->addAction(tr("Compression (&ZLIB)"));
        action->setData(tcommon + ",compress=zlib");
    }
    menuTemplates->setDisabled(menuTemplates->isEmpty());
    QAction *action = menu->exec(QCursor::pos());
    if (menuTemplates->actions().contains(action)) edit->setText(action->data().toString());
    delete menu;
}
