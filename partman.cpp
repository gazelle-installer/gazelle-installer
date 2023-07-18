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
    : QAbstractItemModel(ui.boxMain), proc(mproc), root(DeviceItem::UNKNOWN), gui(ui)
{
    const QString &projShort = appConf.value("PROJECT_SHORTNAME").toString();
    volSpecs["BIOS-GRUB"] = {"BIOS GRUB"};
    volSpecs["/boot"] = {"boot"};
    volSpecs["/boot/efi"] = {"EFI-SYSTEM"};
    volSpecs["/"] = {"root" + projShort + appConf.value("VERSION").toString()};
    volSpecs["/home"] = {"home" + projShort};
    volSpecs["SWAP"] = {"swap" + projShort};
    brave = appArgs.isSet("brave");
    gptoverride = appArgs.isSet("gpt-override");

    // TODO: Eliminate when MX Boot Repair is fixed.
    goodluks = appArgs.isSet("good-luks");

    root.partman = this;
    gui.treePartitions->setModel(this);
    gui.treePartitions->setItemDelegate(new DeviceItemDelegate);
    gui.treePartitions->header()->setMinimumSectionSize(5);
    gui.treePartitions->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(gui.treePartitions, &QTreeView::customContextMenuRequested, this, &PartMan::treeMenu);
    connect(gui.treePartitions->selectionModel(), &QItemSelectionModel::selectionChanged, this, &PartMan::treeSelChange);
    connect(gui.pushPartClear, &QToolButton::clicked, this, &PartMan::partClearClick);
    connect(gui.pushPartAdd, &QToolButton::clicked, this, &PartMan::partAddClick);
    connect(gui.pushPartRemove, &QToolButton::clicked, this, &PartMan::partRemoveClick);
    connect(gui.pushPartReload, &QToolButton::clicked, this, &PartMan::partReloadClick);
    connect(gui.pushPartManRun, &QToolButton::clicked, this, &PartMan::partManRunClick);
    connect(this, &PartMan::dataChanged, this, &PartMan::treeItemChange);
    gui.pushPartAdd->setEnabled(false);
    gui.pushPartRemove->setEnabled(false);
    gui.pushPartClear->setEnabled(false);
    gui.boxCryptoPass->setEnabled(false);

    gui.pushGrid->setChecked(gui.treePartitions->grid());
    connect(gui.pushGrid, &QToolButton::toggled, gui.treePartitions, &MTreeView::setGrid);

    // Hide encryption options if cryptsetup not present.
    QFileInfo cryptsetup("/usr/sbin/cryptsetup");
    QFileInfo crypsetupinitramfs("/usr/share/initramfs-tools/conf-hooks.d/cryptsetup");
    if (!cryptsetup.exists() || !cryptsetup.isExecutable() || !crypsetupinitramfs.exists()) {
        gui.boxEncryptAuto->hide();
        gui.boxCryptoPass->hide();
        gui.treePartitions->setColumnHidden(COL_ENCRYPT, true);
    }

    // UUID of the device that the live system is booted from.
    if (QFile::exists("/live/config/initrd.out")) {
        QSettings livecfg("/live/config/initrd.out", QSettings::NativeFormat);
        bootUUID = livecfg.value("BOOT_UUID").toString();
    }
}

void PartMan::scan(DeviceItem *drvstart)
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
        if (part) order.append(DeviceItem::join(curdev, part));
        else if (sect.startsWith("/dev/")) curdev = sect.mid(5);
    }

    if (!drvstart) root.clear();
    for (const QJsonValue &jsonDrive : jsonBD) {
        const QString &jdName = jsonDrive["name"].toString();
        const QString &jdPath = jsonDrive["path"].toString();
        if (jsonDrive["type"] != "disk") continue;
        if (jdName.startsWith("zram")) continue;
        DeviceItem *drive = drvstart;
        if (!drvstart) drive = new DeviceItem(DeviceItem::DRIVE, &root);
        else if (jdPath != drvstart->path) continue;

        drive->clear();
        drive->flags.curEmpty = true;
        drive->flags.oldLayout = true;
        drive->device = jdName;
        drive->path = jdPath;
        const QString &ptType = jsonDrive["pttype"].toString();
        drive->flags.curGPT = (ptType == "gpt");
        drive->flags.curMBR = (ptType == "dos");
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
            DeviceItem *part = new DeviceItem(DeviceItem::PARTITION, drive);
            drive->flags.curEmpty = false;
            part->flags.oldLayout = true;
            part->device = jsonPart["name"].toString();
            part->path = jsonPart["path"].toString();
            part->uuid = jsonPart["uuid"].toString();
            part->order = order.indexOf(part->device);
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
        for (int ixPart = drive->childCount() - 1; ixPart >= 0; --ixPart) {
            // Propagate the boot flag across the entire drive.
            if (drive->flags.bootRoot) drive->child(ixPart)->flags.bootRoot = true;
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
    DeviceItem *virtdevs = nullptr;
    std::map<QString, DeviceItem *> listed;
    if (rescan) {
        for (int ixi = root.childCount() - 1; ixi >= 0; --ixi) {
            DeviceItem *drive = root.child(ixi);
            if (drive->type == DeviceItem::VIRTUAL_DEVICES) {
                virtdevs = drive;
                const int vdcount = virtdevs->childCount();
                for (int ixVD = 0; ixVD < vdcount; ++ixVD) {
                    DeviceItem *device = virtdevs->child(ixVD);
                    listed[device->path] = device;
                }
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
        virtdevs = new DeviceItem(DeviceItem::VIRTUAL_DEVICES, &root);
        virtdevs->device = tr("Virtual Devices");
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
        DeviceItem *device = nullptr;
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
            device = new DeviceItem(DeviceItem::VIRTUAL, virtdevs);
            device->device = jsonDev["name"].toString();
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
    for(int ixi = 0; ixi < virtdevs->childCount(); ++ixi) {
        const QString &name = virtdevs->child(ixi)->device;
        DeviceItem *vit = virtdevs->child(ixi);
        vit->origin = nullptr; // Clear stale origin pointer.
        // Find the associated partition and decrement its reference count if found.
        proc.exec("cryptsetup", {"status", vit->path}, nullptr, true);
        for (const QString &line : proc.readOutLines()) {
            const QString &trline = line.trimmed();
            if (!trline.startsWith("device:")) continue;
            vit->origin = findByPath(trline.mid(8).trimmed());
            if (vit->origin) {
                vit->origin->devMapper = name;
                notifyChange(vit->origin);
                break;
            }
        }
    }
    gui.treePartitions->expand(index(virtdevs));
}

bool PartMan::manageConfig(MSettings &config, bool save) noexcept
{
    const int driveCount = root.childCount();
    for (int ixDrive = 0; ixDrive < driveCount; ++ixDrive) {
        DeviceItem *drive = root.child(ixDrive);
        // Check if the drive is to be cleared and formatted.
        int partCount = drive->childCount();
        bool drvPreserve = drive->flags.oldLayout;
        const QString &configNewLayout = "Storage/NewLayout." + drive->device;
        if (save) {
            if (!drvPreserve) config.setValue(configNewLayout, partCount);
        } else if (config.contains(configNewLayout)) {
            drvPreserve = false;
            drive->clear();
            partCount = config.value(configNewLayout).toInt();
            if (partCount > PARTMAN_MAX_PARTS) return false;
        }
        // Partition configuration.
        const long long sizeMax = drive->size - PARTMAN_SAFETY;
        long long sizeTotal = 0;
        for (int ixPart = 0; ixPart < partCount; ++ixPart) {
            DeviceItem *part = nullptr;
            if (save || drvPreserve) {
                part = drive->child(ixPart);
                if (!save) part->flags.oldLayout = true;
            } else {
                part = new DeviceItem(DeviceItem::PARTITION, drive);
                part->device = DeviceItem::join(drive->device, ixPart+1);
            }
            // Configuration management, accounting for automatic control correction order.
            const QString &groupPart = "Storage." + part->device;
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
            int subvolCount = 0;
            if (part->format == "btrfs") {
                if (!save) subvolCount = config.value("Subvolumes").toInt();
                else {
                    subvolCount = part->childCount();
                    config.setValue("Subvolumes", subvolCount);
                }
            }
            config.endGroup();
            // Btrfs subvolume configuration.
            for (int ixSV=0; ixSV<subvolCount; ++ixSV) {
                DeviceItem *subvol = nullptr;
                if (save) subvol = part->child(ixSV);
                else subvol = new DeviceItem(DeviceItem::SUBVOLUME, part);
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
        gui.treePartitions->resizeColumnToContents(ixi);
    }
}

void PartMan::treeItemChange() noexcept
{
    // Encryption and bad blocks controls
    bool cryptoAny = false;
    for (DeviceItemIterator it(*this); DeviceItem *item = *it; it.next()) {
        if (item->type != DeviceItem::PARTITION) continue;
        if (item->canEncrypt() && item->encrypt) cryptoAny = true;
    }
    if (gui.boxCryptoPass->isEnabled() != cryptoAny) {
        gui.textCryptoPassCust->clear();
        gui.pushNext->setDisabled(cryptoAny);
        gui.boxCryptoPass->setEnabled(cryptoAny);
    }
    treeSelChange();
}

void PartMan::treeSelChange() noexcept
{
    const QModelIndexList &indexes = gui.treePartitions->selectionModel()->selectedIndexes();
    DeviceItem *seldev = (indexes.size() > 0) ? item(indexes.at(0)) : nullptr;
    if (seldev && seldev->type != DeviceItem::SUBVOLUME) {
        const bool isdrive = (seldev->type == DeviceItem::DRIVE);
        bool isold = seldev->flags.oldLayout;
        const bool islocked = seldev->isLocked();
        if (isdrive && seldev->flags.curEmpty) isold = false;
        gui.pushPartClear->setEnabled(isdrive && !islocked);
        gui.pushPartRemove->setEnabled(!isold && !isdrive);
        // Only allow adding partitions if there is enough space.
        DeviceItem *drive = seldev->parent();
        if (!drive) drive = seldev;
        if (!islocked && isold && isdrive) gui.pushPartAdd->setEnabled(false);
        else {
            gui.pushPartAdd->setEnabled(seldev->driveFreeSpace(true) > 1*MB
                && !isold && drive->childCount() < PARTMAN_MAX_PARTS);
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
    DeviceItem *seldev = item(selIndex);
    if (seldev->type == DeviceItem::VIRTUAL_DEVICES) return;
    QMenu menu(gui.treePartitions);
    if (seldev->isVolume()) {
        QAction *actAdd = menu.addAction(tr("&Add partition"));
        actAdd->setEnabled(gui.pushPartAdd->isEnabled());
        QAction *actRemove = menu.addAction(tr("&Remove partition"));
        QAction *actLock = nullptr;
        QAction *actUnlock = nullptr;
        QAction *actAddCrypttab = nullptr;
        QAction *actNewSubvolume = nullptr, *actScanSubvols = nullptr;
        QAction *actActive = nullptr, *actESP = nullptr;
        actRemove->setEnabled(gui.pushPartRemove->isEnabled());
        menu.addSeparator();
        if (seldev->type == DeviceItem::VIRTUAL) {
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
            menu.addSeparator();
            actActive = menu.addAction(tr("Active partition"));
            actESP = menu.addAction(tr("EFI System Partition"));
            actActive->setCheckable(true);
            actActive->setChecked(seldev->isActive());
            actESP->setCheckable(true);
            actESP->setChecked(seldev->flags.sysEFI);
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
        else if (action == actActive) seldev->setActive(action->isChecked());
        else if (action == actAddCrypttab) seldev->addToCrypttab = action->isChecked();
        else if (action == actESP) {
            seldev->flags.sysEFI = action->isChecked();
            seldev->autoFill(1 << COL_FORMAT);
            notifyChange(seldev);
        } else if (action == actNewSubvolume) {
            DeviceItem *subvol = new DeviceItem(DeviceItem::SUBVOLUME, seldev);
            subvol->autoFill();
            gui.treePartitions->expand(selIndex);
        } else if (action == actScanSubvols) {
            scanSubvolumes(seldev);
            gui.treePartitions->expand(selIndex);
        }
    } else if (seldev->type == DeviceItem::DRIVE) {
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
    } else if (seldev->type == DeviceItem::SUBVOLUME) {
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

// Partition manager list buttons
void PartMan::partClearClick(bool)
{
    const QModelIndexList &indexes = gui.treePartitions->selectionModel()->selectedIndexes();
    DeviceItem *seldev = (indexes.size() > 0) ? item(indexes.at(0)) : nullptr;
    if (!seldev || seldev->type != DeviceItem::DRIVE) return;
    seldev->clear();
    treeSelChange();
}

void PartMan::partAddClick(bool) noexcept
{
    const QModelIndexList &indexes = gui.treePartitions->selectionModel()->selectedIndexes();
    DeviceItem *selitem = (indexes.size() > 0) ? item(indexes.at(0)) : nullptr;
    if (!selitem) return;
    DeviceItem *volume = selitem->parent();
    if (!volume) volume = selitem;

    DeviceItem::DeviceType newtype = DeviceItem::PARTITION;
    if (selitem->type == DeviceItem::SUBVOLUME) newtype = DeviceItem::SUBVOLUME;

    DeviceItem *newitem = new DeviceItem(newtype, volume, selitem);
    if (newtype == DeviceItem::PARTITION) {
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
    DeviceItem *seldev = (indexes.size() > 0) ? item(indexes.at(0)) : nullptr;
    if (!seldev) return;
    DeviceItem *parent = seldev->parent();
    if (!parent) return;
    const bool notSub = (seldev->type != DeviceItem::SUBVOLUME);
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
void PartMan::partMenuUnlock(DeviceItem *part)
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

void PartMan::partMenuLock(DeviceItem *volume)
{
    qApp->setOverrideCursor(QCursor(Qt::WaitCursor));
    gui.boxMain->setEnabled(false);
    bool ok = false;
    // Find the associated partition and decrement its reference count if found.
    DeviceItem *origin = volume->origin;
    if (origin) ok = proc.exec("cryptsetup", {"close", volume->path});
    if (ok) {
        if (origin->mapCount > 0) origin->mapCount--;
        origin->devMapper.clear();
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
        QMessageBox::critical(gui.boxMain, QString(), tr("Failed to close %1").arg(volume->device));
    }
}

void PartMan::scanSubvolumes(DeviceItem *part)
{
    qApp->setOverrideCursor(Qt::WaitCursor);
    gui.boxMain->setEnabled(false);
    while (part->childCount()) delete part->child(0);
    QStringList lines;
    if (!proc.exec("mount", {"--mkdir", "-o", "subvolid=5,ro",
        part->mappedDevice(), "/mnt/btrfs-scratch"})) goto END;
    proc.exec("btrfs", {"subvolume", "list", "/mnt/btrfs-scratch"}, nullptr, true);
    lines = proc.readOutLines();
    proc.exec("umount", {"/mnt/btrfs-scratch"});
    for (const QString &line : qAsConst(lines)) {
        const int start = line.indexOf("path") + 5;
        if (line.length() <= start) goto END;
        DeviceItem *subvol = new DeviceItem(DeviceItem::SUBVOLUME, part);
        subvol->flags.oldLayout = true;
        subvol->label = subvol->curLabel = line.mid(start);
        subvol->format = "PRESERVE";
        notifyChange(subvol);
    }
 END:
    gui.boxMain->setEnabled(true);
    qApp->restoreOverrideCursor();
}

bool PartMan::composeValidate(bool automatic, const QString &project) noexcept
{
    mounts.clear();
    // Partition use and other validation
    int swapnum = 0;
    for (DeviceItemIterator it(*this); DeviceItem *item = *it; it.next()) {
        if (item->type == DeviceItem::DRIVE || item->type == DeviceItem::VIRTUAL_DEVICES) continue;
        if (item->type == DeviceItem::SUBVOLUME) {
            // Ensure the subvolume label entry is valid.
            bool ok = true;
            const QString &cmptext = item->label.trimmed().toUpper();
            if (cmptext.isEmpty()) ok = false;
            if (cmptext.count(QRegularExpression("[^A-Z0-9\\/\\@\\.\\-\\_]|\\/\\/"))) ok = false;
            if (cmptext.startsWith('/') || cmptext.endsWith('/')) ok = false;
            if (!ok) {
                QMessageBox::critical(gui.boxMain, QString(), tr("Invalid subvolume label"));
                return false;
            }
            // Check for duplicate subvolume label entries.
            DeviceItem *pit = item->parentItem;
            assert(pit != nullptr);
            const int count = pit->childCount();
            const int index = pit->indexOfChild(item);
            for (int ixi = 0; ixi < count; ++ixi) {
                if (ixi == index) continue;
                if (pit->child(ixi)->label.trimmed().toUpper() == cmptext) {
                    QMessageBox::critical(gui.boxMain, QString(), tr("Duplicate subvolume label"));
                    return false;
                }
            }
        }
        QString mount = item->usefor;
        if (mount.isEmpty()) continue;
        if (!mount.startsWith("/") && !item->allowedUsesFor().contains(mount)) {
            QMessageBox::critical(gui.boxMain, QString(),
                tr("Invalid use for %1: %2").arg(item->shownDevice(), mount));
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
        } else if(item->canMount(false)) {
            mounts[mount] = item;
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

    if (!automatic) {
        // Final warnings before the installer starts.
        QString details;
        for (int ixdrv = 0; ixdrv < root.childCount(); ++ixdrv) {
            DeviceItem *drive = root.child(ixdrv);
            assert(drive != nullptr);
            const int partCount = drive->childCount();
            if (!drive->flags.oldLayout && drive->type != DeviceItem::VIRTUAL_DEVICES) {
                const char *pttype = drive->willUseGPT() ? "GPT" : "MBR";
                details += tr("Prepare %1 partition table on %2").arg(pttype, drive->device) + '\n';
            }
            for (int ixdev = 0; ixdev < partCount; ++ixdev) {
                DeviceItem *part = drive->child(ixdev);
                assert(part != nullptr);
                const int subcount = part->childCount();
                QString actmsg;
                if (drive->flags.oldLayout) {
                    if (part->usefor.isEmpty()) {
                        if (subcount > 0) actmsg = tr("Reuse (no reformat) %1");
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
                details += actmsg.replace("%1", part->shownDevice()).replace("%2", part->usefor) + '\n';

                for (int ixsv = 0; ixsv < subcount; ++ixsv) {
                    DeviceItem *subvol = part->child(ixsv);
                    assert(subvol != nullptr);
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
                    details += " + " + actmsg.replace("%1", subvol->label).replace("%2", subvol->usefor) + '\n';
                }
            }
        }
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

        msgbox.setText(tr("The %1 installer will now perform the requested actions.").arg(project));
        msgbox.setInformativeText(tr("These actions cannot be undone. Do you want to continue?"));
        details.chop(1); // Remove trailing new-line character.
        msgbox.setDetailedText(details);
        const QList<QAbstractButton *> &btns = msgbox.buttons();
        for (auto btn : btns) {
            if (msgbox.buttonRole(btn) == QMessageBox::ActionRole) {
                btn->click(); // Simulate clicking the "Show Details..." button.
                break;
            }
        }
        if (msgbox.exec() != QMessageBox::Yes) return false;
    }

    return true;
}
bool PartMan::confirmSpace(QMessageBox &msgbox) noexcept
{
    // Isolate used points from each other in total calculations
    QStringList vols;
    for (DeviceItemIterator it(*this); *it; it.next()) {
        const QString &use = (*it)->usefor;
        if (!use.isEmpty()) vols.append(use);
    }

    QStringList toosmall;
    for (int ixdrv = 0; ixdrv < root.childCount(); ++ixdrv) {
        DeviceItem *drive = root.child(ixdrv);
        const int partCount = drive->childCount();
        for (int ixdev = 0; ixdev < partCount; ++ixdev) {
            DeviceItem *part = drive->child(ixdev);
            assert(part != nullptr);
            const int subcount = part->childCount();
            bool isused = !part->usefor.isEmpty();
            long long minsize = isused ? volSpecTotal(part->usefor, vols).minimum : 0;

            // First pass = get the total minimum required for all subvolumes.
            for (int ixsv = 0; ixsv < subcount; ++ixsv) {
                DeviceItem *subvol = part->child(ixsv);
                assert(subvol != nullptr);
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
                for (int ixsv = 0; ixsv < subcount; ++ixsv) {
                    DeviceItem *subvol = part->child(ixsv);
                    assert(subvol != nullptr);
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
    for (int ixdrv = 0; ixdrv < root.childCount(); ++ixdrv) {
        DeviceItem *drive = root.child(ixdrv);
        const int partCount = drive->childCount();
        bool hasBiosGrub = false;
        for (int ixdev = 0; ixdev < partCount; ++ixdev) {
            DeviceItem *part = drive->child(ixdev);
            assert(part != nullptr);
            if (part->finalFormat() == "BIOS-GRUB") {
                hasBiosGrub = true;
                break;
            }
        }
        // Potentially unbootable GPT when on a BIOS-based PC.
        const bool hasBoot = (drive->active != nullptr);
        if (!proc.detectEFI() && drive->willUseGPT() && hasBoot && !hasBiosGrub) {
            biosgpt += ' ' + drive->device;
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
bool PartMan::checkTargetDrivesOK()
{
    MProcess::Section sect(proc, nullptr); // No exception on execution error for this block.
    QString smartFail, smartWarn;
    for (int ixi = 0; ixi < root.childCount(); ++ixi) {
        DeviceItem *drive = root.child(ixi);
        if (drive->type == DeviceItem::VIRTUAL_DEVICES) continue;
        QStringList purposes;
        for (DeviceItemIterator it(drive); const DeviceItem *item = *it; it.next()) {
            if (!item->usefor.isEmpty()) purposes << item->usefor;
        }
        // If any partitions are selected run the SMART tests.
        if (!purposes.isEmpty()) {
            QString smartMsg = drive->device + " (" + purposes.join(", ") + ")";
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
        int ans;
        msg += tr("If unsure, please exit the Installer and run GSmartControl for more information.") + "\n\n";
        if (!smartFail.isEmpty()) {
            msg += tr("Do you want to abort the installation?");
            ans = QMessageBox::critical(gui.boxMain, QString(), msg,
                    QMessageBox::Yes|QMessageBox::Default|QMessageBox::Escape, QMessageBox::No);
            if (ans == QMessageBox::Yes) return false;
        } else {
            msg += tr("Do you want to continue?");
            ans = QMessageBox::warning(gui.boxMain, QString(), msg,
                    QMessageBox::Yes|QMessageBox::Default, QMessageBox::No|QMessageBox::Escape);
            if (ans != QMessageBox::Yes) return false;
        }
    }
    return true;
}

DeviceItem *PartMan::selectedDriveAuto() noexcept
{
    QString drv(gui.comboDisk->currentData().toString());
    if (!findByPath("/dev/" + drv)) return nullptr;
    for (int ixi = 0; ixi < root.childCount(); ++ixi) {
        DeviceItem *const drive = root.child(ixi);
        if (drive->device == drv) return drive;
    }
    return nullptr;
}

void PartMan::clearAllUses() noexcept
{
    for (DeviceItemIterator it(*this); DeviceItem *item = *it; it.next()) {
        item->usefor.clear();
        if (item->type == DeviceItem::PARTITION) item->setActive(false);
        notifyChange(item);
    }
}

int PartMan::countPrepSteps() noexcept
{
    int nstep = 0;
    for (DeviceItemIterator it(*this); DeviceItem *item = *it; it.next()) {
        if (item->type == DeviceItem::DRIVE) {
            if (!item->flags.oldLayout) ++nstep; // New partition table
        } else if (item->isVolume()) {
            // Preparation
            if (!item->flags.oldLayout) ++nstep; // New partition
            else if (!item->usefor.isEmpty()) ++nstep; // Existing partition
            // Formatting
            if (item->encrypt) nstep += 2; // LUKS Format
            if (item->willFormat()) ++nstep; // New file system
            // Mounting
            if (item->usefor.startsWith('/')) ++nstep;
        } else if (item->type == DeviceItem::SUBVOLUME) {
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
            << it.second->devMapper << it.second->path;
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
    std::map<QString, DeviceItem *> alldevs;
    for (DeviceItemIterator it(*this); DeviceItem *item = *it; it.next()) {
        alldevs[item->path] = item;
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
    for (int ixDrive = 0; ixDrive < root.childCount(); ++ixDrive) {
        DeviceItem *drive = root.child(ixDrive);
        if (drive->type == DeviceItem::VIRTUAL_DEVICES) continue;
        const int partCount = drive->childCount();
        if (drive->flags.oldLayout) {
            // Using the existing layout, so only mark used partitions for unmounting.
            for (int ixPart=0; ixPart < partCount; ++ixPart) {
                DeviceItem *part = drive->child(ixPart);
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
        for (const QString &devpath : qAsConst(listToUnmount)) {
            if (swaps.contains(devpath)) proc.exec("swapoff", {devpath});
            if (fsmounts.contains(devpath)) proc.exec("umount", {"-q", devpath});
        }
    }

    // Prepare partition tables on devices which will have a new layout.
    for (int ixi = 0; ixi < root.childCount(); ++ixi) {
        DeviceItem *drive = root.child(ixi);
        if (drive->flags.oldLayout || drive->type == DeviceItem::VIRTUAL_DEVICES) continue;
        proc.status(tr("Preparing partition tables"));

        // Wipe the first and last 4MB to clear the partition tables, turbo-nuke style.
        const int gran = std::max(drive->discgran, drive->physec);
        const char *opts = drive->discgran ? "-fv" : "-fvz";
        // First 17KB = primary partition table (accounts for both MBR and GPT disks).
        // First 17KB, from 32KB = sneaky iso-hybrid partition table (maybe USB with an ISO burned onto it).
        const long long length = (4*MB + (gran - 1)) / gran; // ceiling
        proc.exec("blkdiscard", {opts, "-l", QString::number(length*gran), drive->path});
        // Last 17KB = secondary partition table (for GPT disks).
        const long long offset = (drive->size - 4*MB) / gran; // floor
        proc.exec("blkdiscard", {opts, "-o", QString::number(offset*gran), drive->path});

        proc.exec("parted", {"-s", drive->path, "mklabel", (drive->willUseGPT() ? "gpt" : "msdos")});
    }

    // Prepare partition tables, creating tables and partitions when needed.
    proc.status(tr("Preparing required partitions"));
    for (int ixi = 0; ixi < root.childCount(); ++ixi) {
        bool partupdate = false;
        DeviceItem *drive = root.child(ixi);
        if (drive->type == DeviceItem::VIRTUAL_DEVICES) continue;
        const int devCount = drive->childCount();
        const bool useGPT = drive->willUseGPT();
        if (drive->flags.oldLayout) {
            // Using existing partitions.
            QString cmd; // command to set the partition type
            if (useGPT) cmd = "sgdisk /dev/%1 -q --typecode=%2:%3";
            else cmd = "sfdisk /dev/%1 -q --part-type %2 %3";
            // Set the type for partitions that will be used in this installation.
            for (int ixdev = 0; ixdev < devCount; ++ixdev) {
                DeviceItem *part = drive->child(ixdev);
                assert(part != nullptr);
                if (part->usefor.isEmpty()) continue;
                const char *ptype = useGPT ? "8303" : "83";
                if (part->flags.sysEFI) ptype = useGPT ? "ef00" : "ef";
                const DeviceItem::NameParts &devsplit = DeviceItem::split(part->device);
                proc.shell(cmd.arg(devsplit.drive, devsplit.partition, ptype));
                partupdate = true;
                proc.status();
            }
        } else {
            // Creating new partitions.
            long long start = 1; // start with 1 MB to aid alignment
            for (int ixdev = 0; ixdev<devCount; ++ixdev) {
                DeviceItem *part = drive->child(ixdev);
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
            DeviceItem *part = drive->child(ixdev);
            if (part->usefor.isEmpty()) continue;
            if (part->isActive()) {
                const DeviceItem::NameParts &devsplit = DeviceItem::split(part->device);
                QStringList cargs({"-s", "/dev/" + devsplit.drive, "set", devsplit.partition});
                cargs.append(useGPT ? "legacy_boot" : "boot");
                cargs.append("on");
                proc.exec("parted", cargs);
                partupdate = true;
            }
            if (part->flags.sysEFI != part->flags.curESP) {
                const DeviceItem::NameParts &devsplit = DeviceItem::split(part->device);
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
    const QByteArray &encPass = (gui.radioEntireDisk->isChecked()
        ? gui.textCryptoPass : gui.textCryptoPassCust)->text().toUtf8();
    for (DeviceItemIterator it(*this); DeviceItem *part = *it; it.next()) {
        if (!part->encrypt || !part->willFormat()) continue;
        proc.status(tr("Creating encrypted volume: %1").arg(part->device));
        proc.exec("cryptsetup", {"--batch-mode", "--key-size=512",
            "--hash=sha512", "luksFormat", part->path}, &encPass);
        proc.status();

        // TODO: Eliminate when MX Boot Repair is fixed.
        if (!goodluks && part->devMapper.isEmpty() && part->usefor == "/") {
            part->devMapper = "root.fsm";
        }

        luksOpen(part, encPass);
    }
}
void PartMan::luksOpen(DeviceItem *part, const QByteArray &password)
{
    MProcess::Section sect(proc, QT_TR_NOOP("Failed to open LUKS container."));
    if (part->devMapper.isEmpty()) {
        proc.exec("cryptsetup", {"luksUUID", part->path}, nullptr, true);
        part->devMapper = "luks-" + proc.readAll().trimmed();
    }
    QStringList cargs({"open", part->path, part->devMapper, "--type", "luks"});
    if (part->discgran) cargs.prepend("--allow-discards");
    proc.exec("cryptsetup", cargs, &password);
}

void PartMan::formatPartitions()
{
    proc.log(__PRETTY_FUNCTION__, MProcess::LOG_MARKER);
    MProcess::Section sect(proc, QT_TR_NOOP("Failed to format partition."));

    // Format partitions.
    for (DeviceItemIterator it(*this); DeviceItem *volume = *it; it.next()) {
        if (volume->type != DeviceItem::PARTITION && volume->type != DeviceItem::VIRTUAL) continue;
        if (!volume->willFormat()) continue;
        const QString &dev = volume->mappedDevice();
        const QString &fmtstatus = tr("Formatting: %1");
        if (volume->usefor == "FORMAT") proc.status(fmtstatus.arg(volume->device));
        else proc.status(fmtstatus.arg(volume->usefor));
        if (volume->usefor == "BIOS-GRUB") {
            const DeviceItem::NameParts &devsplit = DeviceItem::split(dev);
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
    for (DeviceItemIterator it(*this); *it; it.next()) {
        if ((*it)->finalFormat() == "btrfs") prepareSubvolumes(*it);
    }
}

void PartMan::prepareSubvolumes(DeviceItem *part)
{
    const int subvolcount = part->childCount();
    if (subvolcount <= 0) return;
    MProcess::Section sect(proc, QT_TR_NOOP("Failed to prepare subvolumes."));
    proc.status(tr("Preparing subvolumes"));

    proc.exec("mount", {"--mkdir", "-o", "subvolid=5,noatime", part->mappedDevice(), "/mnt/btrfs-scratch"});
    const char *errmsg = nullptr;
    try {
        // Since the default subvolume cannot be deleted, ensure the default is set to the top.
        proc.exec("btrfs", {"-q", "subvolume", "set-default", "5", "/mnt/btrfs-scratch"});
        for (int ixsv = 0; ixsv < subvolcount; ++ixsv) {
            DeviceItem *subvol = part->child(ixsv);
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
    for (DeviceItemIterator it(*this); DeviceItem *item = *it; it.next()) {
        if (item->addToCrypttab) ++cryptcount;
    }
    // Add devices to crypttab.
    QFile file("/mnt/antiX/etc/crypttab");
    if (!file.open(QIODevice::WriteOnly)) return false;
    QTextStream out(&file);
    for (DeviceItemIterator it(*this); DeviceItem *item = *it; it.next()) {
        if (!item->addToCrypttab) continue;
        out << item->devMapper << " UUID=" << item->uuid << " none luks";
        if (cryptcount > 1) out << ",keyscript=decrypt_keyctl";
        if (!item->flags.rotational) out << ",no-read-workqueue,no-write-workqueue";
        if (item->discgran) out << ",discard";
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
        const DeviceItem *volume = it.second;
        const QString &dev = volume->mappedDevice();
        qDebug() << "Creating fstab entry for:" << it.first << dev;
        // Device ID or UUID
        if (volume->willMap()) out << dev;
        else out << "UUID=" << volume->assocUUID();
        // Mount point, file system
        const QString &fsfmt = volume->finalFormat();
        if (fsfmt == "swap") out << " swap swap";
        else {
            out << ' ' << it.first;
            if (fsfmt.startsWith("FAT")) out << " vfat";
            else out << ' ' << fsfmt;
        }
        // Options
        const QString &mountopts = volume->options;
        if (volume->type == DeviceItem::SUBVOLUME) {
            out << " subvol=" << volume->label;
            if (!mountopts.isEmpty()) out << ',' << mountopts;
        } else {
            if (mountopts.isEmpty()) out << " defaults";
            else out << ' ' << mountopts;
        }
        out << ' ' << (volume->dump ? 1 : 0);
        out << ' ' << volume->pass << '\n';
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
        if (it.second->type == DeviceItem::SUBVOLUME) {
            opts.append("subvol=" + it.second->label);
        }
        // Use noatime to speed up the installation.
        opts.append(it.second->options.split(','));
        opts.removeAll("ro");
        opts.removeAll("defaults");
        opts.removeAll("atime");
        opts.removeAll("relatime");
        if (!opts.contains("async")) opts.append("async");
        if (!opts.contains("noiversion")) opts.append("noiversion");
        if (!opts.contains("noatime")) opts.append("noatime");
        proc.exec("mount", {"--mkdir", "-o", opts.join(','), dev, point});
    }
}

void PartMan::clearWorkArea()
{
    // Close swap files that may have been opened (should not have swap opened in the first place)
    proc.exec("swapon", {"--show=NAME", "--noheadings"}, nullptr, true);
    QStringList swaps = proc.readOutLines();
    for (DeviceItemIterator it(*this); *it; it.next()) {
        const QString &dev = (*it)->mappedDevice();
        if (swaps.contains(dev)) proc.exec("swapoff", {dev});
    }
    // Unmount everything in /mnt/antiX which is only to be for working on the target system.
    if (QFileInfo::exists("/mnt/antiX")) proc.exec("umount", {"-qR", "/mnt/antiX"});
    // Close encrypted containers that were opened by the installer.
    for (DeviceItemIterator it(*this); DeviceItem *item = *it; it.next()) {
        if (item->encrypt && item->type != DeviceItem::VIRTUAL && QFile::exists(item->mappedDevice())) {
            proc.exec("cryptsetup", {"close", item->devMapper});
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

DeviceItem *PartMan::findByPath(const QString &devpath) const noexcept
{
    for (DeviceItemIterator it(*this); *it; it.next()) {
        if ((*it)->path == devpath) return *it;
    }
    return nullptr;
}
DeviceItem *PartMan::findHostDev(const QString &path) const noexcept
{
    QString spath = path;
    DeviceItem *device = nullptr;
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
    DeviceItem *item = static_cast<DeviceItem*>(index.internalPointer());
    const bool isDriveOrVD = (item->type == DeviceItem::DRIVE || item->type == DeviceItem::VIRTUAL_DEVICES);
    if (role == Qt::EditRole) {
        switch (index.column())
        {
        case COL_DEVICE: return item->device; break;
        case COL_SIZE: return item->size; break;
        case COL_USEFOR: return item->usefor; break;
        case COL_LABEL: return item->label; break;
        case COL_FORMAT: return item->format; break;
        case COL_OPTIONS: return item->options; break;
        case COL_PASS: return item->pass; break;
        }
    } else if (role == Qt::CheckStateRole && !isDriveOrVD
        && index.flags() & Qt::ItemIsUserCheckable) {
        switch (index.column())
        {
        case COL_ENCRYPT: return item->encrypt ? Qt::Checked : Qt::Unchecked; break;
        case COL_CHECK: return item->chkbadblk ? Qt::Checked : Qt::Unchecked; break;
        case COL_DUMP: return item->dump ? Qt::Checked : Qt::Unchecked; break;
        }
    } else if (role == Qt::DisplayRole) {
        switch (index.column()) {
        case COL_DEVICE:
            if (item->type == DeviceItem::SUBVOLUME) {
                return item->isActive() ? "++++" : "----";
            } else {
                QString dev = item->device;
                if (item->isActive()) dev += '*';
                if (item->flags.sysEFI) dev += QChar(u'†');
                return dev;
            }
            break;
        case COL_SIZE:
            if (item->type == DeviceItem::SUBVOLUME) {
                return item->isActive() ? "++++" : "----";
            } else {
                return QLocale::system().formattedDataSize(item->size,
                    1, QLocale::DataSizeTraditionalFormat);
            }
            break;
        case COL_USEFOR: return item->usefor; break;
        case COL_LABEL:
            if (index.flags() & Qt::ItemIsEditable) return item->label;
            else return item->curLabel;
            break;
        case COL_FORMAT:
            if (item->type == DeviceItem::DRIVE) {
                if (item->willUseGPT()) return "GPT";
                else if (item->flags.curMBR || !item->flags.oldLayout) return "MBR";
            } else if (item->type == DeviceItem::SUBVOLUME) {
                return item->shownFormat(item->format);
            } else {
                if (item->usefor.isEmpty()) return item->curFormat;
                else return item->shownFormat(item->format);
            }
            break;
        case COL_OPTIONS:
            if (item->canMount(false)) return item->options;
            else if (!isDriveOrVD) return "--------";
            break;
        case COL_PASS:
            if (item->canMount()) return item->pass;
            else if (!isDriveOrVD) return "--";
            break;
        }
    } else if (role == Qt::ToolTipRole) {
        switch (index.column()) {
        case COL_DEVICE:
            if (!item->model.isEmpty()) return tr("Model: %1").arg(item->model);
            else return item->path;
            break;
        case COL_SIZE:
            if (item->type == DeviceItem::SUBVOLUME) return "----";
            else if (item->type == DeviceItem::DRIVE) {
                long long fs = item->driveFreeSpace();
                if (fs <= 0) fs = 0;
                return tr("Free space: %1").arg(QLocale::system().formattedDataSize(fs,
                    1, QLocale::DataSizeTraditionalFormat));
            }
            break;
        }
        return item->shownDevice();
    } else if (role == Qt::DecorationRole && index.column() == COL_DEVICE) {
        if ((item->type == DeviceItem::DRIVE || item->type == DeviceItem::SUBVOLUME) && !item->flags.oldLayout) {
            return QIcon(":/appointment-soon");
        } else if (item->type == DeviceItem::VIRTUAL && item->flags.volCrypto) {
            return QIcon::fromTheme("unlock");
        } else if (item->mapCount) {
            return QIcon::fromTheme("lock");
        }
    }
    return QVariant();
}
bool PartMan::setData(const QModelIndex &index, const QVariant &value, int role) noexcept
{
    if (role == Qt::CheckStateRole) {
        DeviceItem *item = static_cast<DeviceItem *>(index.internalPointer());
        changeBegin(item);
        switch (index.column())
        {
        case COL_ENCRYPT: item->encrypt = (value == Qt::Checked); break;
        case COL_CHECK: item->chkbadblk = (value == Qt::Checked); break;
        case COL_DUMP: item->dump = (value == Qt::Checked); break;
        }
        const int changed = changeEnd(false);
        item->autoFill(changed);
        if (changed) notifyChange(item);
        else emit dataChanged(index, index);
    }
    return true;
}
Qt::ItemFlags PartMan::flags(const QModelIndex &index) const noexcept
{
    DeviceItem *item = static_cast<DeviceItem *>(index.internalPointer());
    if (item->type == DeviceItem::VIRTUAL_DEVICES) return Qt::ItemIsEnabled;
    Qt::ItemFlags flagsOut({Qt::ItemIsSelectable, Qt::ItemIsEnabled});
    if (item->mapCount) return flagsOut;
    switch (index.column())
    {
    case COL_DEVICE: break;
    case COL_SIZE:
        if (item->type == DeviceItem::PARTITION && !item->flags.oldLayout) {
            flagsOut |= Qt::ItemIsEditable;
        }
        break;
    case COL_USEFOR:
        if (item->allowedUsesFor().count() >= 1 && item->format != "DELETE") {
            flagsOut |= Qt::ItemIsEditable;
        }
        break;
    case COL_LABEL:
        if (item->format != "PRESERVE" && item->format != "DELETE") {
            if (item->type == DeviceItem::SUBVOLUME) flagsOut |= Qt::ItemIsEditable;
            else if (!item->usefor.isEmpty()) flagsOut |= Qt::ItemIsEditable;
        }
        break;
    case COL_ENCRYPT:
        if (item->canEncrypt()) flagsOut |= Qt::ItemIsUserCheckable;
        break;
    case COL_FORMAT:
        if (item->allowedFormats().count() > 1) flagsOut |= Qt::ItemIsEditable;
        break;
    case COL_CHECK:
        if (item->format.startsWith("ext") || item->format == "jfs"
            || item->format.startsWith("FAT", Qt::CaseInsensitive)) {
            flagsOut |= Qt::ItemIsUserCheckable;
        }
        break;
    case COL_OPTIONS:
        if (item->canMount(false)) flagsOut |= Qt::ItemIsEditable;
        break;
    case COL_DUMP:
        if (item->canMount()) flagsOut |= Qt::ItemIsUserCheckable;
        break;
    case COL_PASS:
        if (item->canMount()) flagsOut |= Qt::ItemIsEditable;
        break;
    }
    return flagsOut;
}
QVariant PartMan::headerData(int section, Qt::Orientation orientation, int role) const noexcept
{
    assert(orientation == Qt::Horizontal);
    if (role == Qt::DisplayRole) {
        switch (section)
        {
        case COL_DEVICE: return tr("Device"); break;
        case COL_SIZE: return tr("Size"); break;
        case COL_USEFOR: return tr("Use For"); break;
        case COL_LABEL: return tr("Label"); break;
        case COL_ENCRYPT: return tr("Encrypt"); break;
        case COL_FORMAT: return tr("Format"); break;
        case COL_CHECK: return tr("Check"); break;
        case COL_OPTIONS: return tr("Options"); break;
        case COL_DUMP: return tr("Dump"); break;
        case COL_PASS: return tr("Pass"); break;
        }
    } else if (role == Qt::FontRole && (section == COL_ENCRYPT || section == COL_CHECK || section == COL_DUMP)) {
        QFont smallFont;
        smallFont.setPointSizeF(smallFont.pointSizeF() * 0.6);
        return smallFont;
    }
    return QVariant();
}
QModelIndex PartMan::index(DeviceItem *item) const noexcept
{
    if (item == &root) return QModelIndex();
    return createIndex(item->row(), 0, item);
}
QModelIndex PartMan::index(int row, int column, const QModelIndex &parent) const
{
    if (!hasIndex(row, column, parent)) return QModelIndex();
    const DeviceItem *pit = parent.isValid() ? static_cast<DeviceItem *>(parent.internalPointer()) : &root;
    DeviceItem *cit = pit->child(row);
    if (cit) return createIndex(row, column, cit);
    return QModelIndex();
}
QModelIndex PartMan::parent(const QModelIndex &index) const noexcept
{
    if (!index.isValid()) return QModelIndex();
    DeviceItem *cit = static_cast<DeviceItem *>(index.internalPointer());
    DeviceItem *pit = cit->parentItem;
    if (!pit || pit == &root) return QModelIndex();
    return createIndex(pit->row(), 0, pit);
}
inline DeviceItem *PartMan::item(const QModelIndex &index) const noexcept
{
    return static_cast<DeviceItem *>(index.internalPointer());
}
int PartMan::rowCount(const QModelIndex &parent) const noexcept
{
    if (parent.column() > 0) return 0;
    if (parent.isValid()) {
        return static_cast<DeviceItem *>(parent.internalPointer())->childCount();
    }
    return root.childCount();
}

bool PartMan::changeBegin(DeviceItem *item) noexcept
{
    if (changing) return false;
    root.flags = item->flags;
    root.size = item->size;
    root.usefor = item->usefor;
    root.label = item->label;
    root.encrypt = item->encrypt;
    root.format = item->format;
    root.options = item->options;
    root.dump = item->dump;
    root.pass = item->pass;
    changing = item;
    return true;
}
int PartMan::changeEnd(bool notify) noexcept
{
    if (!changing) return false;
    int changed = 0;
    if (changing->size != root.size) {
        if (!changing->usefor.startsWith('/') && !changing->allowedUsesFor().contains(changing->usefor)) {
            changing->usefor.clear();
        }
        if (!changing->canEncrypt()) changing->encrypt = false;
        changed |= (1 << COL_SIZE);
    }
    if (changing->usefor != root.usefor) {
        if (changing->usefor.isEmpty()) changing->format.clear();
        else {
            const QStringList &allowed = changing->allowedFormats();
            if (!allowed.contains(changing->format)) changing->format = allowed.at(0);
        }
        changed |= (1 << COL_USEFOR);
    }
    if (changing->encrypt != root.encrypt) {
        const QStringList &allowed = changing->allowedFormats();
        if (!allowed.contains(changing->format)) changing->format = allowed.at(0);
        changed |= (1 << COL_ENCRYPT);
    }
    if (changing->format != root.format || changing->usefor != root.usefor) {
        changing->dump = false;
        changing->pass = 2;
        if (changing->usefor.isEmpty() || changing->usefor == "FORMAT") {
            changing->options.clear();
        }
        if (changing->format != "btrfs") {
            // Clear all subvolumes if not supported.
            while (changing->childCount()) delete changing->child(0);
        } else {
            // Remove preserve option from all subvolumes.
            for (int ixi = 0; ixi < changing->childCount(); ++ixi) {
                DeviceItem *child = changing->child(ixi);
                child->format = "CREATE";
                notifyChange(child);
            }
        }
        if (changing->format != root.format) changed |= (1 << COL_FORMAT);
    }
    if (changed && notify) notifyChange(changing);
    changing = nullptr;
    return changed;
}
void PartMan::notifyChange(class DeviceItem *item, int first, int last) noexcept
{
    if (first < 0) first = 0;
    if (last < 0) last = TREE_COLUMNS - 1;
    const int row = item->row();
    emit dataChanged(createIndex(row, first, item), createIndex(row, last, item));
}

/* Model element */

DeviceItem::DeviceItem(enum DeviceType type, DeviceItem *parent, DeviceItem *preceding) noexcept
    : parentItem(parent), type(type)
{
    if (type == PARTITION) size = 1*MB;
    if (parent) {
        flags.rotational = parent->flags.rotational;
        discgran = parent->discgran;
        if (type == SUBVOLUME) size = parent->size;
        physec = parent->physec;
        partman = parent->partman;
        const int i = preceding ? (parentItem->indexOfChild(preceding) + 1) : parentItem->childCount();
        if (partman) partman->beginInsertRows(partman->index(parent), i, i);
        parent->children.insert(std::next(parent->children.begin(), i), this);
        if (partman) partman->endInsertRows();
    }
}
DeviceItem::~DeviceItem()
{
    if (parentItem && partman) {
        const int r = parentItem->indexOfChild(this);
        partman->beginRemoveRows(partman->index(parentItem), r, r);
    }
    for (DeviceItem *cit : qAsConst(children)) {
        cit->partman = nullptr; // Stop unnecessary signals.
        cit->parentItem = nullptr; // Stop double deletes.
        delete cit;
    }
    children.clear();
    if (parentItem) {
        if (parentItem->active == this) parentItem->active = nullptr;
        for (auto it = parentItem->children.begin(); it != parentItem->children.end();) {
            if (*it == this) it = parentItem->children.erase(it);
            else ++it;
        }
        if (partman) {
            partman->endRemoveRows();
            partman->notifyChange(parentItem);
        }
    }
}
void DeviceItem::clear() noexcept
{
    const int chcount = children.size();
    if (partman && chcount > 0) partman->beginRemoveRows(partman->index(this), 0, chcount - 1);
    for (DeviceItem *cit : qAsConst(children)) {
        cit->partman = nullptr; // Stop unnecessary signals.
        cit->parentItem = nullptr; // Stop double deletes.
        delete cit;
    }
    children.clear();
    active = nullptr;
    flags.oldLayout = false;
    if (partman && chcount > 0) partman->endRemoveRows();
}
inline int DeviceItem::row() const noexcept
{
    return parentItem ? parentItem->indexOfChild(this) : 0;
}
DeviceItem *DeviceItem::parent() const noexcept
{
    if (parentItem && !parentItem->parentItem) return nullptr; // Invisible root
    return parentItem;
}
inline DeviceItem *DeviceItem::child(int row) const noexcept
{
    if (row < 0 || row >= static_cast<int>(children.size())) return nullptr;
    return children[row];
}
inline int DeviceItem::indexOfChild(const DeviceItem *child) const noexcept
{
    for (size_t ixi = 0; ixi < children.size(); ++ixi) {
        if (children[ixi] == child) return ixi;
    }
    return -1;
}
inline int DeviceItem::childCount() const noexcept
{
    return children.size();
}
void DeviceItem::sortChildren() noexcept
{
    auto cmp = [](DeviceItem *l, DeviceItem *r) {
        if (l->order != r->order) return l->order < r->order;
        return l->device < r->device;
    };
    std::sort(children.begin(), children.end(), cmp);
    if (partman) {
        for (DeviceItem *c : qAsConst(children)) partman->notifyChange(c);
    }
}
/* Helpers */
void DeviceItem::setActive(bool on) noexcept
{
    if (!parentItem) return;
    if (partman && parentItem->active != this) {
        if (parentItem->active) partman->notifyChange(parentItem->active);
    }
    parentItem->active = on ? this : nullptr;
    if (partman) partman->notifyChange(this);
}
inline bool DeviceItem::isActive() const noexcept
{
    if (!parentItem) return false;
    return (parentItem->active == this);
}
bool DeviceItem::isLocked() const noexcept
{
    for (const DeviceItem *child : children) {
        if (child->isLocked()) return true;
    }
    return (mapCount != 0); // In use by at least one virtual device.
}
bool DeviceItem::willUseGPT() const noexcept
{
    if (type != DRIVE) return false;
    if (flags.oldLayout) return flags.curGPT;
    else if (size >= 2*TB || children.size() > 4) return true;
    else if (partman) return (partman->gptoverride || partman->proc.detectEFI());
    return false;
}
bool DeviceItem::willFormat() const noexcept
{
    return format != "PRESERVE" && !usefor.isEmpty();
}
bool DeviceItem::canEncrypt() const noexcept
{
    if (type != PARTITION) return false;
    return !(flags.sysEFI || usefor.isEmpty() || usefor == "BIOS-GRUB" || usefor == "/boot");
}
inline bool DeviceItem::willEncrypt() const noexcept
{
    if (type == SUBVOLUME) return parentItem->encrypt;
    return encrypt;
}
QString DeviceItem::assocUUID() const noexcept
{
    if (type == SUBVOLUME) return parentItem->uuid;
    return uuid;
}
QString DeviceItem::mappedDevice() const noexcept
{
    const DeviceItem *device = this;
    if (device->type == SUBVOLUME) device = device->parentItem;
    if (device->type == PARTITION) {
        const QVariant &d = device->devMapper;
        if (!d.isNull()) return "/dev/mapper/" + d.toString();
    }
    return device->path;
}
inline bool DeviceItem::willMap() const noexcept
{
    if (type == DRIVE || type == VIRTUAL_DEVICES) return false;
    else if (type == SUBVOLUME) return !parentItem->devMapper.isEmpty();
    return !devMapper.isEmpty();
}
QString DeviceItem::shownDevice() const noexcept
{
    if (type == SUBVOLUME) return parentItem->device + '[' + label + ']';
    return device;
}
QStringList DeviceItem::allowedUsesFor(bool all) const noexcept
{
    if (!isVolume() && type != SUBVOLUME) return QStringList();
    QStringList list;
    auto checkAndAdd = [&](const QString &use) {
        const auto fit = partman->volSpecs.find(usefor);
        if (all || !partman || fit == partman->volSpecs.end() || size >= fit->second.minimum) {
            list.append(use);
        }
    };

    if (type != SUBVOLUME) {
        checkAndAdd("FORMAT");
        if (type != VIRTUAL) {
            if (all || size <= 1*MB) {
                if (parentItem && parentItem->willUseGPT()) checkAndAdd("BIOS-GRUB");
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
QStringList DeviceItem::allowedFormats() const noexcept
{
    QStringList list;
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
QString DeviceItem::finalFormat() const noexcept
{
    if (type == SUBVOLUME) return format;
    return (usefor.isEmpty() || format == "PRESERVE") ? curFormat : format;
}
QString DeviceItem::shownFormat(const QString &fmt) const noexcept
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
bool DeviceItem::canMount(bool pointonly) const noexcept
{
    return !usefor.isEmpty() && usefor != "FORMAT" && usefor != "BIOS-GRUB"
        && (!pointonly || usefor != "SWAP");
}
long long DeviceItem::driveFreeSpace(bool inclusive) const noexcept
{
    const DeviceItem *drive = parent();
    if (!drive) drive = this;
    long long free = drive->size - PARTMAN_SAFETY;
    for (const DeviceItem *child : drive->children) {
        if (inclusive || child != this) free -= child->size;
    }
    return free;
}
/* Convenience */
DeviceItem *DeviceItem::addPart(long long defaultSize, const QString &defaultUse, bool crypto) noexcept
{
    DeviceItem *part = new DeviceItem(DeviceItem::PARTITION, this);
    if (!defaultUse.isEmpty()) part->usefor = defaultUse;
    part->size = defaultSize;
    if (part->canEncrypt()) part->encrypt = crypto;
    part->autoFill();
    if (partman) partman->notifyChange(part);
    return part;
}
void DeviceItem::driveAutoSetActive() noexcept
{
    if (active) return;
    if (partman && partman->proc.detectEFI() && willUseGPT()) return;
    // Cannot use partman->mounts map here as it may not be populated.
    for (const QString &pref : QStringList({"/boot", "/"})) {
        for (DeviceItemIterator it(this); DeviceItem *item = *it; it.next()) {
            if (item->usefor == pref) {
                while (item && item->type != PARTITION) item = item->parentItem;
                if (item) item->setActive(true);
                return;
            }
        }
    }
}
void DeviceItem::autoFill(unsigned int changed) noexcept
{
    if (changed & (1 << PartMan::COL_USEFOR)) {
        // Default labels
        if (type == SUBVOLUME) {
            QStringList chklist;
            const int count = parentItem->childCount();
            const int index = parentItem->indexOfChild(this);
            for (int ixi = 0; ixi < count; ++ixi) {
                if (ixi == index) continue;
                chklist << parentItem->child(ixi)->label;
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
        } else if (partman) {
            const auto fit = partman->volSpecs.find(usefor);
            if (fit == partman->volSpecs.cend()) label.clear();
            else label = fit->second.defaultLabel;
        }
        // Automatic default boot device selection
        if ((type != VIRTUAL) && (usefor == "/boot" || usefor == "/")) {
            DeviceItem *drive = this;
            while (drive && drive->type != DRIVE) drive = drive->parentItem;
            if (drive) drive->driveAutoSetActive();
        }
        if (type == SUBVOLUME && usefor == "/") setActive(true);
        if (usefor == "/boot/efi") flags.sysEFI = true;

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
        if (format == "SWAP") options = discgran ? "discard" : "defaults";
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
void DeviceItem::labelParts() noexcept
{
    const size_t nchildren = childCount();
    for (size_t ixi = 0; ixi < nchildren; ++ixi) {
        DeviceItem *chit = children[ixi];
        chit->device = join(device, ixi + 1);
        chit->path = "/dev/" + chit->device;
        if (partman) partman->notifyChange(chit, PartMan::COL_DEVICE, PartMan::COL_DEVICE);
    }
    if (partman) partman->resizeColumnsToFit();
}

// Return block device info that is suitable for a combo box.
void DeviceItem::addToCombo(QComboBox *combo, bool warnNasty) const noexcept
{
    QString strout(QLocale::system().formattedDataSize(size, 1, QLocale::DataSizeTraditionalFormat));
    if (!curFormat.isEmpty()) strout += ' ' + curFormat;
    else if (!format.isEmpty()) strout += ' ' + format;
    if (!label.isEmpty()) strout += " - " + label;
    if (!model.isEmpty()) strout += (label.isEmpty() ? " - " : "; ") + model;
    QString stricon;
    if (!flags.oldLayout || !usefor.isEmpty()) stricon = ":/appointment-soon";
    else if (flags.nasty && warnNasty) stricon = ":/dialog-warning";
    combo->addItem(QIcon(stricon), device + " (" + strout + ")", device);
}
// Split a device name into its drive and partition.
DeviceItem::NameParts DeviceItem::split(const QString &devname) noexcept
{
    const QRegularExpression rxdev1("^(?:\\/dev\\/)*(mmcblk.*|nvme.*)p([0-9]*)$");
    const QRegularExpression rxdev2("^(?:\\/dev\\/)*([a-z]*)([0-9]*)$");
    QRegularExpressionMatch rxmatch(rxdev1.match(devname));
    if (!rxmatch.hasMatch()) rxmatch = rxdev2.match(devname);
    return {rxmatch.captured(1), rxmatch.captured(2)};
}
QString DeviceItem::join(const QString &drive, int partnum) noexcept
{
    QString name = drive;
    if (name.startsWith("nvme") || name.startsWith("mmcblk")) name += 'p';
    return (name + QString::number(partnum));
}

/* A very slimmed down and non-standard one-way tree iterator. */
DeviceItemIterator::DeviceItemIterator(const PartMan &partman) noexcept
{
    if (partman.root.childCount() < 1) return;
    ixParents.push(0);
    pos = partman.root.child(0);
}
void DeviceItemIterator::next() noexcept
{
    if (!pos) return;
    if (pos->childCount()) {
        ixParents.push(ixPos);
        ixPos = 0;
        pos = pos->child(0);
    } else {
        DeviceItem *parent = pos->parentItem;
        if (!parent) {
            pos = nullptr;
            return;
        }
        DeviceItem *chnext = parent->child(ixPos+1);
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

void DeviceItemDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
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
        if(index.column() == PartMan::COL_FORMAT || index.column() == PartMan::COL_USEFOR) {
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
QSize DeviceItemDelegate::sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    DeviceItem *item = static_cast<DeviceItem*>(index.internalPointer());
    int width = QStyledItemDelegate::sizeHint(option, index).width();
    // In the case of Format and Use For, the cell should accommodate all options in the list.
    const int residue = 10 + width - option.fontMetrics.boundingRect(index.data(Qt::DisplayRole).toString()).width();
    switch(index.column()) {
    case PartMan::COL_FORMAT:
        for (const QString &fmt : item->allowedFormats()) {
            const int fw = option.fontMetrics.boundingRect(item->shownFormat(fmt)).width() + residue;
            if (fw > width) width = fw;
        }
        break;
    case PartMan::COL_USEFOR:
        for (const QString &use : item->allowedUsesFor(false)) {
            const int uw = option.fontMetrics.boundingRect(use).width() + residue;
            if (uw > width) width = uw;
        }
        break;
    }
    return QSize(width + 4, option.fontMetrics.height() + 4);
}

QWidget *DeviceItemDelegate::createEditor(QWidget *parent, const QStyleOptionViewItem &, const QModelIndex &index) const
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
            connect(edit, &QLineEdit::customContextMenuRequested,
                this, &DeviceItemDelegate::partOptionsMenu);
            widget = edit;
        }
        break;
    default: widget = new QLineEdit(parent);
    }
    assert(widget != nullptr);
    widget->setAutoFillBackground(true);
    widget->setFocusPolicy(Qt::StrongFocus);
    if (combo) {
        connect(combo,  QOverload<int>::of(&QComboBox::activated),
            this, &DeviceItemDelegate::emitCommit);
    }
    return widget;
}
void DeviceItemDelegate::setEditorData(QWidget *editor, const QModelIndex &index) const
{
    DeviceItem *item = static_cast<DeviceItem*>(index.internalPointer());
    switch (index.column())
    {
    case PartMan::COL_SIZE:
        {
            QSpinBox *spin = qobject_cast<QSpinBox *>(editor);
            spin->setRange(0, static_cast<int>(item->driveFreeSpace() / MB));
            spin->setValue(item->size / MB);
        }
        break;
    case PartMan::COL_USEFOR:
        {
            QComboBox *combo = qobject_cast<QComboBox *>(editor);
            combo->clear();
            combo->addItem("");
            QStringList &&uses = item->allowedUsesFor(false);
            for (QString &use : uses) {
                if (use == "/boot/efi") use = "ESP";
            }
            combo->addItems(uses);
            combo->setCurrentText(item->usefor);
        }
        break;
    case PartMan::COL_LABEL:
        qobject_cast<QLineEdit *>(editor)->setText(item->label);
        break;
    case PartMan::COL_FORMAT:
        {
            QComboBox *combo = qobject_cast<QComboBox *>(editor);
            combo->clear();
            const QStringList &formats = item->allowedFormats();
            assert(!formats.isEmpty());
            for (const QString &fmt : formats) {
                if (fmt != "PRESERVE") combo->addItem(item->shownFormat(fmt), fmt);
                else {
                    // Add an item at the start to allow preserving the existing format.
                    combo->insertItem(0, item->shownFormat("PRESERVE"), "PRESERVE");
                    combo->insertSeparator(1);
                }
            }
            const int ixfmt = combo->findData(item->format);
            if (ixfmt >= 0) combo->setCurrentIndex(ixfmt);
        }
        break;
    case PartMan::COL_PASS:
        qobject_cast<QSpinBox *>(editor)->setValue(item->pass);
        break;
    case PartMan::COL_OPTIONS:
        qobject_cast<QLineEdit *>(editor)->setText(item->options);
        break;
    }
}
void DeviceItemDelegate::setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const
{
    DeviceItem *item = static_cast<DeviceItem*>(index.internalPointer());
    PartMan *partman = qobject_cast<PartMan *>(model);
    partman->changeBegin(item);
    switch (index.column())
    {
    case PartMan::COL_SIZE:
        item->size = qobject_cast<QSpinBox *>(editor)->value();
        item->size *= 1048576; // Separate step to prevent int overflow.
        if (item->size == 0) item->size = item->driveFreeSpace();
        // Set subvolume sizes to keep allowed uses accurate.
        for (DeviceItem *subvol : item->children) {
            subvol->size = item->size;
        }
        break;
    case PartMan::COL_USEFOR:
        item->usefor = qobject_cast<QComboBox *>(editor)->currentText().trimmed();
        // Convert user-friendly entries to real mounts.
        if (!item->usefor.startsWith('/')) item->usefor = item->usefor.toUpper();
        if (item->usefor == "ESP") item->usefor = "/boot/efi";
        break;
    case PartMan::COL_LABEL:
        item->label = qobject_cast<QLineEdit *>(editor)->text().trimmed();
        break;
    case PartMan::COL_FORMAT:
        item->format = qobject_cast<QComboBox *>(editor)->currentData().toString();
        break;
    case PartMan::COL_PASS:
        item->pass = qobject_cast<QSpinBox *>(editor)->value();
        break;
    case PartMan::COL_OPTIONS:
        item->options = qobject_cast<QLineEdit *>(editor)->text().trimmed();
        break;
    }
    const int changed = partman->changeEnd(false);
    item->autoFill(changed);
    if (changed) partman->notifyChange(item);
}
void DeviceItemDelegate::emitCommit() noexcept
{
    emit commitData(qobject_cast<QWidget *>(sender()));
}

void DeviceItemDelegate::partOptionsMenu() noexcept
{
    QLineEdit *edit = static_cast<QLineEdit *>(sender());
    if (!edit) return;
    DeviceItem *part = static_cast<DeviceItem *>(edit->property("row").value<void *>());
    if (!part) return;
    QMenu *menu = edit->createStandardContextMenu();
    menu->addSeparator();
    QMenu *menuTemplates = menu->addMenu(tr("&Templates"));
    QString selFS = part->format;
    if (selFS == "PRESERVE") selFS = part->curFormat;
    if ((part->type == DeviceItem::PARTITION && selFS == "btrfs") || part->type == DeviceItem::SUBVOLUME) {
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
