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
    : QAbstractItemModel(ui.boxMain), proc(mproc), root(DeviceItem::Unknown), gui(ui)
{
    const QString &projShort = appConf.value("PROJECT_SHORTNAME").toString();
    volSpecs["ESP"] = {"EFI System"};
    volSpecs["BIOS-GRUB"] = {"BIOS GRUB"};
    volSpecs["/boot"] = {"boot"};
    volSpecs["/"] = {"root" + projShort + appConf.value("VERSION").toString()};
    volSpecs["/home"] = {"home" + projShort};
    volSpecs["SWAP"] = {"swap" + projShort};
    brave = appArgs.isSet("brave");
    gptoverride = appArgs.isSet("gpt-override");

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
    QFileInfo cryptsetup("/sbin/cryptsetup");
    QFileInfo crypsetupinitramfs("/usr/share/initramfs-tools/conf-hooks.d/cryptsetup");
    if (!cryptsetup.exists() || !cryptsetup.isExecutable() || !crypsetupinitramfs.exists()) {
        gui.boxEncryptAuto->hide();
        gui.boxCryptoPass->hide();
        gui.treePartitions->setColumnHidden(Encrypt, true);
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
        "TYPE,NAME,PATH,UUID,ROTA,DISC-GRAN,SIZE,PHY-SEC,PTTYPE,PARTTYPENAME,FSTYPE,LABEL,MODEL,PARTFLAGS"});
    if (drvstart) cargs.append(drvstart->path);
    proc.exec("lsblk", cargs, nullptr, true);
    const QJsonObject &jsonObjBD = QJsonDocument::fromJson(proc.readOut(true).toUtf8()).object();
    const QJsonArray &jsonBD = jsonObjBD["blockdevices"].toArray();

    if (!drvstart) {
        proc.exec("partprobe", {"-s"}); // Inform the kernel of partition changes.
        proc.exec("blkid", {"-c", "/dev/null"}); // Refresh cached blkid information.
    }
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
        DeviceItem *drvit = drvstart;
        if (!drvstart) drvit = new DeviceItem(DeviceItem::Drive, &root);
        else if (jdPath != drvstart->path) continue;

        drvit->clear();
        drvit->flags.curEmpty = true;
        drvit->flags.oldLayout = true;
        drvit->device = jdName;
        drvit->path = jdPath;
        const QString &ptType = jsonDrive["pttype"].toString();
        drvit->flags.curGPT = (ptType == "gpt");
        drvit->flags.curMBR = (ptType == "dos");
        drvit->flags.rotational = jsonDrive["rota"].toBool();
        drvit->discgran = jsonDrive["disc-gran"].toInt();
        drvit->size = jsonDrive["size"].toVariant().toLongLong();
        drvit->physec = jsonDrive["phy-sec"].toInt();
        drvit->curLabel = jsonDrive["label"].toString();
        drvit->model = jsonDrive["model"].toString();
        const QJsonArray &jsonParts = jsonDrive["children"].toArray();
        for (const QJsonValue &jsonPart : jsonParts) {
            const QString &partTypeName = jsonPart["parttypename"].toString();
            if (partTypeName == "Extended" || partTypeName == "W95 Ext'd (LBA)"
                || partTypeName == "Linux extended") continue;
            DeviceItem *partit = new DeviceItem(DeviceItem::Partition, drvit);
            drvit->flags.curEmpty = false;
            partit->flags.oldLayout = true;
            partit->device = jsonPart["name"].toString();
            partit->path = jsonPart["path"].toString();
            partit->uuid = jsonPart["uuid"].toString();
            partit->order = order.indexOf(partit->device);
            partit->size = jsonPart["size"].toVariant().toLongLong();
            partit->physec = jsonPart["phy-sec"].toInt();
            partit->curLabel = jsonPart["label"].toString();
            partit->model = jsonPart["model"].toString();
            partit->flags.rotational = jsonPart["rota"].toBool();
            partit->discgran = jsonPart["disc-gran"].toInt();
            const int partflags = jsonPart["partflags"].toString().toUInt(nullptr, 0);
            if ((partflags & 0x80) || (partflags & 0x04)) partit->setActive(true);
            partit->mapCount = jsonPart["children"].toArray().count();
            partit->flags.curESP = partTypeName.startsWith("EFI "); // "System"/"(FAT-12/16/32)"
            partit->flags.bootRoot = (!bootUUID.isEmpty() && partit->uuid == bootUUID);
            partit->curFormat = jsonPart["fstype"].toString();
            if (partTypeName == "BIOS boot") partit->curFormat = "BIOS-GRUB";
            // Touching MacOS first drive, or MS LDM may brick the system.
            if ((proc.detectMac() && partit->device.startsWith("sda"))
                || partTypeName.startsWith("Microsoft LDM")) {
                partit->flags.nasty = true;
            }
            // Propagate the boot and nasty flags up to the drive.
            if (partit->flags.bootRoot) drvit->flags.bootRoot = true;
            if (partit->flags.nasty) drvit->flags.nasty = true;
            else if (partit->flags.curESP && partit->allowedUsesFor().contains("ESP")) {
                partit->usefor = "ESP";
                partit->format = partit->allowedFormats().at(0);
            }
        }
        for (int ixPart = drvit->childCount() - 1; ixPart >= 0; --ixPart) {
            // Propagate the boot flag across the entire drive.
            if (drvit->flags.bootRoot) drvit->child(ixPart)->flags.bootRoot = true;
        }
        drvit->sortChildren();
        notifyChange(drvit);
        // Hide the live boot media and its partitions by default.
        if (!brave && drvit->flags.bootRoot) {
            gui.treePartitions->setRowHidden(drvit->row(), QModelIndex(), true);
        }
    }

    if (!drvstart) scanVirtualDevices(false);
    gui.treePartitions->expandAll();
    resizeColumnsToFit();
    treeSelChange();
}

void PartMan::scanVirtualDevices(bool rescan)
{
    DeviceItem *vdlit = nullptr;
    QMap<QString, DeviceItem *> listed;
    if (rescan) {
        for (int ixi = root.childCount() - 1; ixi >= 0; --ixi) {
            DeviceItem *twit = root.child(ixi);
            if (twit->type == DeviceItem::VirtualDevices) {
                vdlit = twit;
                const int vdcount = vdlit->childCount();
                for (int ixVD = 0; ixVD < vdcount; ++ixVD) {
                    DeviceItem *devit = vdlit->child(ixVD);
                    listed.insert(devit->path, devit);
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
        if (vdlit) delete vdlit;
        return;
    } else if (!vdlit) {
        vdlit = new DeviceItem(DeviceItem::VirtualDevices, &root);
        vdlit->device = tr("Virtual Devices");
        vdlit->flags.oldLayout = true;
        gui.treePartitions->setFirstColumnSpanned(vdlit->row(), QModelIndex(), true);
    }
    assert(vdlit != nullptr);
    for (const QJsonValue &jsonDev : jsonBD) {
        const QString &path = jsonDev["path"].toString();
        const bool rota = jsonDev["rota"].toBool();
        const int discgran = jsonDev["disc-gran"].toInt();
        const long long size = jsonDev["size"].toVariant().toLongLong();
        const int physec = jsonDev["phy-sec"].toInt();
        const QString &label = jsonDev["label"].toString();
        const bool crypto = (jsonDev["type"].toString() == "crypt");
        // Check if the device is already in the list.
        DeviceItem *devit = listed.value(path);
        if (devit) {
            if (rota != devit->flags.rotational || discgran != devit->discgran
                || size != devit->size || physec != devit->physec
                || label != devit->curLabel || crypto != devit->flags.volCrypto) {
                // List entry is different to the device, so refresh it.
                delete devit;
                devit = nullptr;
            }
            listed.remove(path);
        }
        // Create a new list entry if needed.
        if (!devit) {
            devit = new DeviceItem(DeviceItem::VirtualBD, vdlit);
            devit->device = jsonDev["name"].toString();
            devit->path = path;
            devit->uuid = jsonDev["uuid"].toString();
            devit->flags.rotational = rota;
            devit->discgran = discgran;
            devit->size = size;
            devit->physec = physec;
            devit->flags.bootRoot = (!bootUUID.isEmpty() && devit->uuid == bootUUID);
            devit->curLabel = label;
            devit->curFormat = jsonDev["fstype"].toString();
            devit->flags.volCrypto = crypto;
            devit->flags.oldLayout = true;
        }
    }
    for (const auto &it : listed.toStdMap()) delete it.second;
    vdlit->sortChildren();
    for(int ixi = 0; ixi < vdlit->childCount(); ++ixi) {
        const QString &name = vdlit->child(ixi)->device;
        DeviceItem *vit = vdlit->child(ixi);
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
    gui.treePartitions->expand(index(vdlit));
}

bool PartMan::manageConfig(MSettings &config, bool save) noexcept
{
    const int driveCount = root.childCount();
    for (int ixDrive = 0; ixDrive < driveCount; ++ixDrive) {
        DeviceItem *drvit = root.child(ixDrive);
        // Check if the drive is to be cleared and formatted.
        int partCount = drvit->childCount();
        bool drvPreserve = drvit->flags.oldLayout;
        const QString &configNewLayout = "Storage/NewLayout." + drvit->device;
        if (save) {
            if (!drvPreserve) config.setValue(configNewLayout, partCount);
        } else if (config.contains(configNewLayout)) {
            drvPreserve = false;
            drvit->clear();
            partCount = config.value(configNewLayout).toInt();
            if (partCount > PARTMAN_MAX_PARTS) return false;
        }
        // Partition configuration.
        const long long sizeMax = drvit->size - PARTMAN_SAFETY;
        long long sizeTotal = 0;
        for (int ixPart = 0; ixPart < partCount; ++ixPart) {
            DeviceItem *partit = nullptr;
            if (save || drvPreserve) {
                partit = drvit->child(ixPart);
                if (!save) partit->flags.oldLayout = true;
            } else {
                partit = new DeviceItem(DeviceItem::Partition, drvit);
                partit->device = DeviceItem::join(drvit->device, ixPart+1);
            }
            // Configuration management, accounting for automatic control correction order.
            const QString &groupPart = "Storage." + partit->device;
            config.beginGroup(groupPart);
            if (save) {
                config.setValue("Size", partit->size);
                if (partit->isActive()) config.setValue("Active", true);
                if (partit->addToCrypttab) config.setValue("AddToCrypttab", true);
                if (!partit->usefor.isEmpty()) config.setValue("UseFor", partit->usefor);
                if (!partit->format.isEmpty()) config.setValue("Format", partit->format);
                config.setValue("Encrypt", partit->encrypt);
                if (!partit->label.isEmpty()) config.setValue("Label", partit->label);
                if (!partit->options.isEmpty()) config.setValue("Options", partit->options);
                config.setValue("CheckBadBlocks", partit->chkbadblk);
                config.setValue("Dump", partit->dump);
                config.setValue("Pass", partit->pass);
            } else {
                if (!drvPreserve && config.contains("Size")) {
                    partit->size = config.value("Size").toLongLong();
                    sizeTotal += partit->size;
                    if (sizeTotal > sizeMax) return false;
                    if (config.value("Active").toBool()) partit->setActive(true);
                }
                partit->addToCrypttab = config.value("AddToCrypttab").toBool();
                partit->usefor = config.value("UseFor", partit->usefor).toString();
                partit->format = config.value("Format", partit->format).toString();
                partit->chkbadblk = config.value("CheckBadBlocks", partit->chkbadblk).toBool();
                partit->encrypt = config.value("Encrypt", partit->encrypt).toBool();
                partit->label = config.value("Label", partit->label).toString();
                partit->options = config.value("Options", partit->options).toString();
                partit->dump = config.value("Dump", partit->dump).toBool();
                partit->pass = config.value("Pass", partit->pass).toInt();
            }
            int subvolCount = 0;
            if (partit->format == "btrfs") {
                if (!save) subvolCount = config.value("Subvolumes").toInt();
                else {
                    subvolCount = partit->childCount();
                    config.setValue("Subvolumes", subvolCount);
                }
            }
            config.endGroup();
            // Btrfs subvolume configuration.
            for (int ixSV=0; ixSV<subvolCount; ++ixSV) {
                DeviceItem *svit = nullptr;
                if (save) svit = partit->child(ixSV);
                else svit = new DeviceItem(DeviceItem::Subvolume, partit);
                if (!svit) return false;
                config.beginGroup(groupPart + ".Subvolume" + QString::number(ixSV+1));
                if (save) {
                    if (!svit->usefor.isEmpty()) config.setValue("UseFor", svit->usefor);
                    if (!svit->label.isEmpty()) config.setValue("Label", svit->label);
                    if (!svit->options.isEmpty()) config.setValue("Options", svit->options);
                    config.setValue("Dump", svit->dump);
                    config.setValue("Pass", svit->pass);
                } else {
                    svit->usefor = config.value("UseFor").toString();
                    svit->label = config.value("Label").toString();
                    svit->options = config.value("Options").toString();
                    svit->dump = config.value("Dump").toBool();
                    svit->pass = config.value("Pass").toInt();
                }
                config.endGroup();
            }
            if (!save) gui.treePartitions->expand(index(partit));
        }
    }
    treeSelChange();
    return true;
}

void PartMan::resizeColumnsToFit() noexcept
{
    for (int ixi = _TreeColumns_ - 1; ixi >= 0; --ixi) {
        gui.treePartitions->resizeColumnToContents(ixi);
    }
}

void PartMan::treeItemChange() noexcept
{
    // Encryption and bad blocks controls
    bool cryptoAny = false;
    for (DeviceItemIterator it(*this); DeviceItem *item = *it; it.next()) {
        if (item->type != DeviceItem::Partition) continue;
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
    DeviceItem *twit = (indexes.size() > 0) ? item(indexes.at(0)) : nullptr;
    if (twit && twit->type != DeviceItem::Subvolume) {
        const bool isdrive = (twit->type == DeviceItem::Drive);
        bool isold = twit->flags.oldLayout;
        bool islocked = true;
        if (isdrive) {
            islocked = twit->isLocked();
            if (twit->flags.curEmpty) isold = false;
        }
        gui.pushPartClear->setEnabled(!islocked);
        gui.pushPartRemove->setEnabled(!isold && !isdrive);
        // Only allow adding partitions if there is enough space.
        DeviceItem *drvit = twit->parent();
        if (!drvit) drvit = twit;
        if (!islocked && isold && isdrive) gui.pushPartAdd->setEnabled(false);
        else if (!isold) {
            gui.pushPartAdd->setEnabled(drvit->childCount() < PARTMAN_MAX_PARTS
                && twit->driveFreeSpace(true) > 1*MB);
        }
    } else {
        gui.pushPartClear->setEnabled(false);
        gui.pushPartAdd->setEnabled(false);
        gui.pushPartRemove->setEnabled(twit && twit->type == DeviceItem::Subvolume);
    }
}

void PartMan::treeMenu(const QPoint &)
{
    const QModelIndexList &ixlist = gui.treePartitions->selectionModel()->selectedIndexes();
    if (ixlist.count() < 1) return;
    const QModelIndex &selIndex = ixlist.at(0);
    if (!selIndex.isValid()) return;
    DeviceItem *twit = item(selIndex);
    if (twit->type == DeviceItem::VirtualDevices) return;
    QMenu menu(gui.treePartitions);
    if (twit->isVolume()) {
        QAction *actAdd = menu.addAction(tr("&Add partition"));
        actAdd->setEnabled(gui.pushPartAdd->isEnabled());
        QAction *actRemove = menu.addAction(tr("&Remove partition"));
        QAction *actLock = nullptr;
        QAction *actUnlock = nullptr;
        QAction *actAddCrypttab = nullptr;
        QAction *actNewSubvolume = nullptr, *actScanSubvols = nullptr;
        QAction *actActive = nullptr;
        actRemove->setEnabled(gui.pushPartRemove->isEnabled());
        menu.addSeparator();
        if (twit->type == DeviceItem::VirtualBD) {
            actRemove->setEnabled(false);
            if (twit->flags.volCrypto) actLock = menu.addAction(tr("&Lock"));
        } else {
            bool allowCryptTab = false;
            if (twit->flags.oldLayout && twit->curFormat == "crypto_LUKS") {
                actUnlock = menu.addAction(tr("&Unlock"));
                allowCryptTab = true;
            }
            if (twit->realUseFor() == "FORMAT" && twit->canEncrypt()) allowCryptTab = true;
            if (allowCryptTab) {
                actAddCrypttab = menu.addAction(tr("Add to crypttab"));
                actAddCrypttab->setCheckable(true);
                actAddCrypttab->setChecked(twit->addToCrypttab);
                actAddCrypttab->setEnabled(twit->willMap());
            }
            menu.addSeparator();
            actActive = menu.addAction(tr("Active partition"));
            actActive->setCheckable(true);
            actActive->setChecked(twit->isActive());
        }
        if (twit->finalFormat() == "btrfs") {
            menu.addSeparator();
            actNewSubvolume = menu.addAction(tr("New subvolume"));
            actScanSubvols = menu.addAction(tr("Scan subvolumes"));
            actScanSubvols->setDisabled(twit->willFormat());
        }
        if (twit->mapCount && actUnlock) actUnlock->setEnabled(false);
        QAction *action = menu.exec(QCursor::pos());
        if (!action) return;
        else if (action == actAdd) partAddClick(true);
        else if (action == actRemove) partRemoveClick(true);
        else if (action == actUnlock) partMenuUnlock(twit);
        else if (action == actLock) partMenuLock(twit);
        else if (action == actActive) twit->setActive(action->isChecked());
        else if (action == actAddCrypttab) twit->addToCrypttab = action->isChecked();
        else if (action == actNewSubvolume) {
            new DeviceItem(DeviceItem::Subvolume, twit);
            gui.treePartitions->expand(selIndex);
        } else if (action == actScanSubvols) {
            scanSubvolumes(twit);
            gui.treePartitions->expand(selIndex);
        }
    } else if (twit->type == DeviceItem::Drive) {
        QAction *actAdd = menu.addAction(tr("&Add partition"));
        actAdd->setEnabled(gui.pushPartAdd->isEnabled());
        menu.addSeparator();
        QAction *actClear = menu.addAction(tr("New &layout"));
        QAction *actReset = menu.addAction(tr("&Reset layout"));
        menu.addSeparator();
        QAction *actBuilder = menu.addAction(tr("Layout &Builder..."));

        const bool locked = twit->isLocked();
        actClear->setDisabled(locked);
        actReset->setDisabled(locked);

        const long long minSpace = brave ? 0 : volSpecTotal("/", QStringList()).minimum;
        actBuilder->setEnabled(!locked && autopart && twit->size >= minSpace);

        QAction *action = menu.exec(QCursor::pos());
        if (action == actAdd) partAddClick(true);
        else if (action == actClear) partClearClick(true);
        else if (action == actBuilder) {
            if (autopart) autopart->builderGUI(twit);
            treeSelChange();
        } else if (action == actReset) {
            gui.boxMain->setEnabled(false);
            scan(twit);
            gui.boxMain->setEnabled(true);
        }
    } else if (twit->type == DeviceItem::Subvolume) {
        QAction *actRemSubvolume = menu.addAction(tr("Remove subvolume"));
        actRemSubvolume->setDisabled(twit->flags.oldLayout);
        if (menu.exec(QCursor::pos()) == actRemSubvolume) delete twit;
    }
}

// Partition manager list buttons
void PartMan::partClearClick(bool)
{
    const QModelIndexList &indexes = gui.treePartitions->selectionModel()->selectedIndexes();
    DeviceItem *twit = (indexes.size() > 0) ? item(indexes.at(0)) : nullptr;
    if (!twit || twit->type != DeviceItem::Drive) return;
    twit->clear();
    treeSelChange();
}

void PartMan::partAddClick(bool) noexcept
{
    const QModelIndexList &indexes = gui.treePartitions->selectionModel()->selectedIndexes();
    DeviceItem *twit = (indexes.size() > 0) ? item(indexes.at(0)) : nullptr;
    if (!twit) return;
    DeviceItem *drive = twit->parent();
    if (!drive) drive = twit;

    DeviceItem *part = new DeviceItem(DeviceItem::Partition, drive, twit);
    drive->labelParts();
    drive->flags.oldLayout = false;
    notifyChange(drive);
    gui.treePartitions->selectionModel()->select(index(part),
        QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
}

void PartMan::partRemoveClick(bool) noexcept
{
    const QModelIndexList &indexes = gui.treePartitions->selectionModel()->selectedIndexes();
    DeviceItem *devit = (indexes.size() > 0) ? item(indexes.at(0)) : nullptr;
    if (!devit) return;
    DeviceItem *drvit = devit->parent();
    if (!drvit) return;
    const bool notSub = (devit->type != DeviceItem::Subvolume);
    delete devit;
    if (notSub) {
        drvit->labelParts();
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
    QGuiApplication::setOverrideCursor(Qt::WaitCursor);
    gui.boxMain->setEnabled(false);
    if (QFile::exists("/usr/sbin/gparted")) proc.exec("/usr/sbin/gparted");
    else proc.exec("/usr/bin/partitionmanager");
    scan();
    gui.boxMain->setEnabled(true);
    QGuiApplication::restoreOverrideCursor();
}

// Partition menu items
void PartMan::partMenuUnlock(DeviceItem *twit)
{
    QDialog dialog(gui.boxMain);
    QFormLayout layout(&dialog);
    dialog.setWindowTitle(tr("Unlock Drive"));
    QLineEdit *editVDev = new QLineEdit(&dialog);
    QLineEdit *editPass = new QLineEdit(&dialog);
    QCheckBox *checkCrypttab = new QCheckBox(tr("Add to crypttab"), &dialog);
    editPass->setEchoMode(QLineEdit::Password);
    checkCrypttab->setChecked(true);
    layout.addRow(tr("Virtual Device:"), editVDev);
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
            const QString &mapdev = editVDev->text();
            luksOpen(twit, mapdev, editPass->text().toUtf8());
            twit->usefor.clear();
            twit->addToCrypttab = checkCrypttab->isChecked();
            twit->mapCount++;
            notifyChange(twit);
            scanVirtualDevices(true);
            resizeColumnsToFit();
            treeSelChange();
            gui.boxMain->setEnabled(true);
            qApp->restoreOverrideCursor();
        } catch(const char *msg) {
            // This time, failure is not a dealbreaker.
            qDebug() << "partMenuUnlock() failed:" << msg;
            gui.boxMain->setEnabled(true);
            qApp->restoreOverrideCursor();
            QMessageBox::warning(gui.boxMain, QString(),
                tr("Could not unlock device. Possible incorrect password."));
        }
    }
}

void PartMan::partMenuLock(DeviceItem *twit)
{
    qApp->setOverrideCursor(QCursor(Qt::WaitCursor));
    gui.boxMain->setEnabled(false);
    bool ok = false;
    // Find the associated partition and decrement its reference count if found.
    DeviceItem *origin = twit->origin;
    if (origin) ok = proc.exec("cryptsetup", {"close", twit->path});
    if (ok) {
        if (origin->mapCount > 0) origin->mapCount--;
        origin->devMapper.clear();
        origin->addToCrypttab = false;
        notifyChange(origin);
    }
    twit->origin = nullptr;
    // Refresh virtual devices list.
    scanVirtualDevices(true);
    resizeColumnsToFit();
    treeSelChange();

    gui.boxMain->setEnabled(true);
    qApp->restoreOverrideCursor();
    // If not OK then a command failed, or trying to close a non-LUKS device.
    if (!ok) {
        QMessageBox::critical(gui.boxMain, QString(), tr("Failed to close %1").arg(twit->device));
    }
}

void PartMan::scanSubvolumes(DeviceItem *partit)
{
    qApp->setOverrideCursor(Qt::WaitCursor);
    gui.boxMain->setEnabled(false);
    while (partit->childCount()) delete partit->child(0);
    QStringList lines;
    if (!proc.exec("mount", {"--mkdir", "-o", "subvolid=5,ro",
        partit->mappedDevice(), "/mnt/btrfs-scratch"})) goto END;
    proc.exec("btrfs", {"subvolume", "list", "/mnt/btrfs-scratch"}, nullptr, true);
    lines = proc.readOutLines();
    proc.exec("umount", {"/mnt/btrfs-scratch"});
    for (const QString &line : qAsConst(lines)) {
        const int start = line.indexOf("path") + 5;
        if (line.length() <= start) goto END;
        DeviceItem *svit = new DeviceItem(DeviceItem::Subvolume, partit);
        svit->flags.oldLayout = true;
        svit->label = svit->curLabel = line.mid(start);
        svit->format = "PRESERVE";
        notifyChange(svit);
    }
 END:
    gui.boxMain->setEnabled(true);
    qApp->restoreOverrideCursor();
}

bool PartMan::composeValidate(bool automatic, const QString &project) noexcept
{
    mounts.clear();
    // Partition use and other validation
    int mapnum = 0, swapnum = 0, volnum = 0;
    for (DeviceItemIterator it(*this); DeviceItem *item = *it; it.next()) {
        if (item->type == DeviceItem::Drive || item->type == DeviceItem::VirtualDevices) continue;
        if (item->type == DeviceItem::Subvolume) {
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
        QString mount = item->realUseFor();
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
        DeviceItem *twit = mounts.value(mount);

        // The mount can only be selected once.
        if (twit) {
            QMessageBox::critical(gui.boxMain, QString(), tr("%1 is already"
                " selected for: %2").arg(twit->shownDevice(), twit->shownUseFor()));
            return false;
        } else if(item->canMount(false)) {
            mounts.insert(mount, item);
        }
        if (item->type != DeviceItem::VirtualBD) {
            if (!item->encrypt) item->devMapper.clear();
            else if (mount == "FORMAT") item->devMapper = "vol" + QString::number(++volnum);
            else if (mount == "/") item->devMapper = "root.fsm";
            else if (mount.startsWith("SWAP")) item->devMapper = mount.toLower();
            else item->devMapper = QString::number(++mapnum) + mount.replace('/','.') + ".fsm";
        }
    }
    qDebug() << "Mount points:";
    for (const auto &it : mounts.toStdMap()) {
        qDebug() << " -" << it.first << '-' << it.second->shownDevice()
            << it.second->devMapper << it.second->path;
    }

    DeviceItem *rootitem = mounts.value("/");
    DeviceItem *bootitem = mounts.value("/boot");
    const long long rootMin = volSpecTotal("/").minimum;
    const QString &tMinRoot = QLocale::system().formattedDataSize(rootMin,
        1, QLocale::DataSizeTraditionalFormat);
    if (!rootitem) {
        QMessageBox::critical(gui.boxMain, QString(),
            tr("A root partition of at least %1 is required.").arg(tMinRoot));
        return false;
    } else if (rootitem->willEncrypt() && !bootitem) {
        QMessageBox::critical(gui.boxMain, QString(),
            tr("You must choose a separate boot partition when encrypting root."));
        return false;
    } else if (!rootitem->willFormat() && mounts.contains("/home")) {
        QMessageBox::critical(gui.boxMain, QString(),
            tr("Cannot preserve /home inside root (/) if a separate /home partition is also mounted."));
        return false;
    }

    if (!automatic) {
        // Final warnings before the installer starts.
        QString details, biosgpt;
        QStringList tooSmall;
        const QString &msgszItem(" - " + tr("%1 (%2) requires %3"));
        for (int ixdrv = 0; ixdrv < root.childCount(); ++ixdrv) {
            DeviceItem *drvit = root.child(ixdrv);
            const int partCount = drvit->childCount();
            bool setupGPT = drvit->willUseGPT();
            if (!drvit->flags.oldLayout && drvit->type != DeviceItem::VirtualDevices) {
                details += tr("Prepare %1 partition table on %2").arg(setupGPT?"GPT":"MBR", drvit->device) + '\n';
            }
            bool hasBiosGrub = false;
            for (int ixdev = 0; ixdev < partCount; ++ixdev) {
                DeviceItem *partit = drvit->child(ixdev);
                const int subcount = partit->childCount();
                const QString &use = partit->realUseFor();
                QString actmsg;
                if (drvit->flags.oldLayout) {
                    if (use.isEmpty()) {
                        if (partit->curFormat == "BIOS-GRUB") hasBiosGrub = true;
                        else if (subcount > 0) actmsg = tr("Reuse (no reformat) %1");
                        else continue;
                    } else {
                        if (partit->willFormat()) actmsg = tr("Format %1 to use for %2");
                        else if (use != "/") actmsg = tr("Reuse (no reformat) %1 as %2");
                        else actmsg = tr("Delete the data on %1 except for /home, to use for %2");
                    }
                } else {
                    if (use.isEmpty()) actmsg = tr("Create %1 without formatting");
                    else actmsg = tr("Create %1, format to use for %2");
                }
                details += actmsg.arg(partit->shownDevice(), partit->shownUseFor()) + '\n';
                if (use == "BIOS-GRUB") hasBiosGrub = true;

                if (partit->finalFormat() == "btrfs") {
                    for (int ixsv = 0; ixsv < subcount; ++ixsv) {
                        DeviceItem *svit = partit->child(ixsv);
                        const QString &svuse = svit->realUseFor();
                        const bool nouse = svuse.isEmpty();
                        if (svit->format == "PRESERVE") {
                            if (nouse) continue;
                            else actmsg = tr("Reuse subvolume %1 as %2");
                        } else if (svit->format == "DELETE") {
                            actmsg = tr("Delete subvolume %1");
                        } else if (svit->format == "CREATE") {
                            if (svit->flags.oldLayout) {
                                if (nouse) actmsg = tr("Overwrite subvolume %1");
                                else actmsg = tr("Overwrite subvolume %1 to use for %2");
                            } else {
                                if (nouse) actmsg = tr("Create subvolume %1");
                                else actmsg = tr("Create subvolume %1 to use for %2");
                            }
                        }
                        details += " + " + actmsg.arg(svit->label, svuse) + '\n';
                    }
                }
                // If this volume is too small, add to the warning list.
                const long long minSize = volSpecTotal(partit->realUseFor()).minimum;
                if (partit->size < minSize) {
                    const QString &mstr = QLocale::system().formattedDataSize(minSize,
                        1, QLocale::DataSizeTraditionalFormat);
                    tooSmall.append(msgszItem.arg(partit->shownUseFor(), partit->device, mstr));
                }
            }
            // Potentially unbootable GPT when on a BIOS-based PC.
            const bool hasBoot = (drvit->active != nullptr);
            if (!proc.detectEFI() && setupGPT && hasBoot && !hasBiosGrub) {
                biosgpt += ' ' + drvit->device;
            }
        }
        // Warning messages
        QMessageBox msgbox(gui.boxMain);
        msgbox.setIcon(QMessageBox::Warning);
        msgbox.setStandardButtons(QMessageBox::Yes|QMessageBox::No);
        msgbox.setDefaultButton(QMessageBox::No);
        if (!tooSmall.isEmpty()) {
            tooSmall.sort();
            msgbox.setText(tr("The installation may fail because"
                " the following volumes are too small:") + "\n\n"
                + tooSmall.join('\n')
                + "\n\n" + tr("Are you sure you want to continue?"));
            if (msgbox.exec() != QMessageBox::Yes) return false;
        }
        if (!biosgpt.isEmpty()) {
            biosgpt.prepend(tr("The following drives are, or will be, setup with GPT,"
                " but do not have a BIOS-GRUB partition:") + "\n\n");
            biosgpt += "\n\n" + tr("This system may not boot from GPT drives without a BIOS-GRUB partition.")
                + '\n' + tr("Are you sure you want to continue?");
            msgbox.setText(biosgpt);
            if (msgbox.exec() != QMessageBox::Yes) return false;
        }
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

// Checks SMART status of the selected drives.
// Returns false if it detects errors and user chooses to abort.
bool PartMan::checkTargetDrivesOK()
{
    QString smartFail, smartWarn;
    for (int ixi = 0; ixi < root.childCount(); ++ixi) {
        DeviceItem *drvit =  root.child(ixi);
        if (drvit->type == DeviceItem::VirtualDevices) continue;
        QStringList purposes;
        for (DeviceItemIterator it(drvit); *it; it.next()) {
            const QString &useFor = (*it)->realUseFor();
            if (!useFor.isEmpty()) purposes << useFor;
        }
        // If any partitions are selected run the SMART tests.
        if (!purposes.isEmpty()) {
            QString smartMsg = drvit->device + " (" + purposes.join(", ") + ")";
            proc.exec("smartctl", {"-H", drvit->path});
            if (proc.exitStatus() == MProcess::NormalExit && proc.exitCode() & 8) {
                // See smartctl(8) manual: EXIT STATUS (Bit 3)
                smartFail += " - " + smartMsg + "\n";
            } else {
                proc.shell("smartctl -A " + drvit->path + "| grep -E \"^  5|^196|^197|^198\""
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

void PartMan::luksFormat(DeviceItem *partit, const QByteArray &password)
{
    QStringList cargs({"--batch-mode", "--key-size=512", "--hash=sha512"});
    if (partit->physec > 0) cargs.append("--sector-size=" + QString::number(partit->physec));
    cargs << "luksFormat" << partit->path;
    if (!proc.exec("cryptsetup", cargs, &password)) {
        throw QT_TR_NOOP("Failed to format LUKS container.");
    }
}

void PartMan::luksOpen(DeviceItem *partit, const QString &luksfs, const QByteArray &password)
{
    QStringList cargs({"open", partit->path, luksfs, "--type", "luks"});
    if (partit->discgran) cargs.prepend("--allow-discards");
    if (!proc.exec("cryptsetup", cargs, &password)) {
        throw QT_TR_NOOP("Failed to open LUKS container.");
    }
}

DeviceItem *PartMan::selectedDriveAuto() noexcept
{
    QString drv(gui.comboDisk->currentData().toString());
    if (!findByPath("/dev/" + drv)) return nullptr;
    for (int ixi = 0; ixi < root.childCount(); ++ixi) {
        DeviceItem *const drvit = root.child(ixi);
        if (drvit->device == drv) return drvit;
    }
    return nullptr;
}

void PartMan::clearAllUses() noexcept
{
    for (DeviceItemIterator it(*this); DeviceItem *item = *it; it.next()) {
        item->usefor.clear();
        if (item->type == DeviceItem::Partition) item->setActive(false);
        notifyChange(item);
    }
}

int PartMan::countPrepSteps() noexcept
{
    int nstep = 0;
    for (DeviceItemIterator it(*this); DeviceItem *item = *it; it.next()) {
        if (item->type == DeviceItem::Drive) {
            if (!item->flags.oldLayout) ++nstep; // New partition table
        } else if (item->isVolume()) {
            const QString &tuse = item->realUseFor();
            // Preparation
            if (!item->flags.oldLayout) ++nstep; // New partition
            else if (!tuse.isEmpty()) ++nstep; // Existing partition
            // Formatting
            if (item->encrypt) nstep += 2; // LUKS Format
            if (item->willFormat()) ++nstep; // New file system
            // Mounting
            if (tuse.startsWith('/')) ++nstep;
        } else if (item->type == DeviceItem::Subvolume) {
            ++nstep; // Create a new subvolume.
        }
    }
    return nstep;
}
void PartMan::prepStorage()
{
    preparePartitions();
    formatPartitions();
    mountPartitions();
    // Run blkid -c /dev/null to freshen UUID cache
    proc.exec("blkid", {"-c", "/dev/null"});
    // Refresh the UUIDs
    proc.exec("lsblk", {"--list", "-bJo", "PATH,UUID"}, nullptr, true);
    const QJsonObject &jsonObjBD = QJsonDocument::fromJson(proc.readOut(true).toUtf8()).object();
    const QJsonArray &jsonBD = jsonObjBD["blockdevices"].toArray();
    QMap<QString, DeviceItem *> alldevs;
    for (DeviceItemIterator it(*this); DeviceItem *item = *it; it.next()) {
        alldevs.insert(item->path, item);
    }
    for (const QJsonValue &jsonDev : jsonBD) {
        DeviceItem *devit = alldevs.value(jsonDev["path"].toString());
        if (devit) devit->uuid = jsonDev["uuid"].toString();
    }
}
void PartMan::installTabs()
{
    makeFstab();
    if (!fixCryptoSetup()) {
        throw QT_TR_NOOP("Failed to finalize encryption setup.");
    }
}

void PartMan::preparePartitions()
{
    proc.log(__PRETTY_FUNCTION__, MProcess::Section);
    static const char *const msgfail = QT_TR_NOOP("Failed to prepare required partitions.");

    // Detach all existing partitions.
    QStringList listToUnmount;
    for (int ixDrive = 0; ixDrive < root.childCount(); ++ixDrive) {
        DeviceItem *drvit = root.child(ixDrive);
        if (drvit->type == DeviceItem::VirtualDevices) continue;
        const int partCount = drvit->childCount();
        if (drvit->flags.oldLayout) {
            // Using the existing layout, so only mark used partitions for unmounting.
            for (int ixPart=0; ixPart < partCount; ++ixPart) {
                DeviceItem *twit = drvit->child(ixPart);
                if (!twit->usefor.isEmpty()) listToUnmount << twit->path;
            }
        } else {
            // Clearing the drive, so mark all partitions on the drive for unmounting.
            proc.exec("lsblk", {"-nro", "path", drvit->path}, nullptr, true);
            listToUnmount << proc.readOutLines();
        }
    }
    proc.exec("swapon", {"--show=NAME", "--noheadings"}, nullptr, true);
    const QStringList swaps = proc.readOutLines();
    for (const QString &devpath : qAsConst(listToUnmount)) {
        if (swaps.contains(devpath)) proc.shell("grep -q " + devpath + " /proc/swaps && swapoff " + devpath);
        else proc.exec("/usr/bin/umount", {"-q", devpath});
    }

    // Prepare partition tables on devices which will have a new layout.
    for (int ixi = 0; ixi < root.childCount(); ++ixi) {
        DeviceItem *drvit = root.child(ixi);
        if (drvit->flags.oldLayout || drvit->type == DeviceItem::VirtualDevices) continue;
        proc.status(tr("Preparing partition tables"));

        // Wipe the first and last 4MB to clear the partition tables, turbo-nuke style.
        const long long offset = (drvit->size / 65536) - 63; // Account for integer rounding.
        QStringList cargs({"conv=notrunc", "bs=64K", "count=64", "if=/dev/zero", "of=" + drvit->path});
        // First 17KB = primary partition table (accounts for both MBR and GPT disks).
        // First 17KB, from 32KB = sneaky iso-hybrid partition table (maybe USB with an ISO burned onto it).
        proc.exec("dd", cargs);
        // Last 17KB = secondary partition table (for GPT disks).
        cargs.append("seek=" + QString::number(offset));
        proc.exec("dd", cargs);
        if (!proc.exec("parted", {"-s", drvit->path, "mklabel",
            (drvit->willUseGPT() ? "gpt" : "msdos")})) throw msgfail;
    }

    // Prepare partition tables, creating tables and partitions when needed.
    proc.status(tr("Preparing required partitions"));
    for (int ixi = 0; ixi < root.childCount(); ++ixi) {
        DeviceItem *drvit = root.child(ixi);
        if (drvit->type == DeviceItem::VirtualDevices) continue;
        const int devCount = drvit->childCount();
        const bool useGPT = drvit->willUseGPT();
        if (drvit->flags.oldLayout) {
            // Using existing partitions.
            QString cmd; // command to set the partition type
            if (useGPT) cmd = "/sbin/sgdisk /dev/%1 --typecode=%2:%3";
            else cmd = "/sbin/sfdisk /dev/%1 --part-type %2 %3";
            // Set the type for partitions that will be used in this installation.
            for (int ixdev = 0; ixdev < devCount; ++ixdev) {
                DeviceItem *twit = drvit->child(ixdev);
                const QString &useFor = twit->realUseFor();
                const char *ptype = useGPT ? "8303" : "83";
                if (useFor.isEmpty()) continue;
                else if (useFor == "ESP") ptype = useGPT ? "ef00" : "ef";
                const QStringList &devsplit = DeviceItem::split(twit->device);
                if (!proc.shell(cmd.arg(devsplit.at(0), devsplit.at(1), ptype))) throw msgfail;
                proc.status();
            }
        } else {
            // Creating new partitions.
            long long start = 1; // start with 1 MB to aid alignment
            for (int ixdev = 0; ixdev<devCount; ++ixdev) {
                DeviceItem *twit = drvit->child(ixdev);
                const long long end = start + twit->size / MB;
                const bool rc = proc.exec("parted", {"-s", "--align", "optimal", drvit->path,
                    "mkpart" , "primary", QString::number(start) + "MiB", QString::number(end) + "MiB"});
                if (!rc) throw msgfail;
                start = end;
                proc.status();
            }
        }
        // Partition flags.
        for (int ixdev=0; ixdev<devCount; ++ixdev) {
            DeviceItem *twit = drvit->child(ixdev);
            if (twit->usefor.isEmpty()) continue;
            if (twit->isActive()) {
                QStringList devsplit(DeviceItem::split(twit->device));
                QStringList cargs({"-s", "/dev/" + devsplit.at(0), "set", devsplit.at(1)});
                cargs.append(useGPT ? "legacy_boot" : "boot");
                cargs.append("on");
                if(!proc.exec("parted", cargs)) throw msgfail;
            }
            proc.status();
        }
    }
    proc.exec("partprobe", {"-s"});
}

void PartMan::formatPartitions()
{
    proc.log(__PRETTY_FUNCTION__, MProcess::Section);

    const QByteArray &encPass = (gui.radioEntireDisk->isChecked()
        ? gui.textCryptoPass : gui.textCryptoPassCust)->text().toUtf8();

    // Format partitions.
    for (DeviceItemIterator it(*this); DeviceItem *twit = *it; it.next()) {
        if (twit->type != DeviceItem::Partition && twit->type != DeviceItem::VirtualBD) continue;
        if (!twit->willFormat()) continue;
        const QString &dev = twit->mappedDevice();
        const QString &useFor = twit->realUseFor();
        if (twit->encrypt) {
            proc.status(tr("Creating encrypted volume: %1").arg(twit->device));
            luksFormat(twit, encPass);
            proc.status();
            luksOpen(twit, twit->devMapper, encPass);
        }
        const QString &fmtstatus = tr("Formatting: %1");
        if (useFor == "FORMAT") proc.status(fmtstatus.arg(dev));
        else proc.status(fmtstatus.arg(twit->shownUseFor()));
        static const char *const msgfail = QT_TR_NOOP("Failed to format partition.");
        if (useFor == "ESP") {
            QStringList cargs({"-F", twit->format.mid(3)});
            if (twit->chkbadblk) cargs.append("-c");
            if (!twit->label.isEmpty()) cargs << "-n" << twit->label.trimmed().left(11);
            cargs.append(dev);
            if (!proc.exec("mkfs.msdos", cargs)) throw msgfail;
            // Sets boot flag and ESP flag.
            const QStringList &devsplit = DeviceItem::split(dev);
            if (!proc.exec("parted", {"-s", "/dev/" + devsplit.at(0),
                "set", devsplit.at(1), "esp", "on"})) throw msgfail;
        } else if (useFor == "BIOS-GRUB") {
            proc.exec("dd", {"bs=64K", "if=/dev/zero", "of=" + dev});
            const QStringList &devsplit = DeviceItem::split(dev);
            if (!proc.exec("parted", {"-s", "/dev/" + devsplit.at(0),
                "set", devsplit.at(1), "bios_grub", "on"})) throw msgfail;
        } else if (useFor == "SWAP") {
            QStringList cargs(dev);
            if (!twit->label.isEmpty()) cargs << "-L" << twit->label;
            if (!proc.exec("/sbin/mkswap", cargs)) throw msgfail;
        } else {
            // Transplanted from minstall.cpp and modified to suit.
            const QString &format = twit->format;
            QStringList cargs;
            if (format == "btrfs") {
                cargs.append("-f");
                proc.exec("/bin/cp", {"-fp", "/bin/true", "/sbin/fsck.auto"});
                if (twit->size < 6000000000) {
                    cargs << "-M" << "-O" << "skinny-metadata"; // Mixed mode (-M)
                }
            } else if (format == "xfs" || format == "f2fs") {
                cargs.append("-f");
            } else { // jfs, ext2, ext3, ext4
                if (format == "jfs") cargs.append("-q");
                else cargs.append("-F");
                if (twit->chkbadblk) cargs.append("-c");
            }
            cargs.append(dev);
            if (!twit->label.isEmpty()) {
                if (format == "f2fs") cargs.append("-l");
                else cargs.append("-L");
                cargs.append(twit->label);
            }
            if (!proc.exec("mkfs." + format, cargs)) throw msgfail;
            if (format.startsWith("ext")) {
                proc.exec("/sbin/tune2fs", {"-c0", "-C0", "-i1m", dev}); // ext4 tuning
            }
        }
    }
    // Prepare subvolumes on all that (are to) contain them.
    for (DeviceItemIterator it(*this); *it; it.next()) {
        if ((*it)->finalFormat() == "btrfs") prepareSubvolumes(*it);
    }
}

void PartMan::prepareSubvolumes(DeviceItem *partit)
{
    const int subvolcount = partit->childCount();
    if (subvolcount <= 0) return;
    proc.setExceptionMode(QT_TR_NOOP("Failed to prepare subvolumes."));
    proc.status(tr("Preparing subvolumes"));

    proc.mkpath("/mnt/btrfs-scratch");
    proc.exec("mount", {"-o", "subvolid=5,noatime", partit->mappedDevice(), "/mnt/btrfs-scratch"});
    const char *errmsg = nullptr;
    try {
        // Since the default subvolume cannot be deleted, ensure the default is set to the top.
        proc.exec("btrfs", {"subvolume", "set-default", "5", "/mnt/btrfs-scratch"});
        for (int ixsv = 0; ixsv < subvolcount; ++ixsv) {
            DeviceItem *svit = partit->child(ixsv);
            const QString &svpath = "/mnt/btrfs-scratch/" + svit->label;
            if (svit->format != "PRESERVE" && QFileInfo::exists(svpath)) {
                proc.exec("btrfs", {"subvolume", "delete", svpath});
            }
            if (svit->format == "CREATE") proc.exec("btrfs", {"subvolume", "create", svpath});
            proc.status();
        }
        // Make the root subvolume the default again.
        DeviceItem *devit = mounts.value("/");
        assert (devit != nullptr);
        if (devit->type == DeviceItem::Subvolume) {
            proc.exec("btrfs", {"subvolume", "set-default", "/mnt/btrfs-scratch/"+devit->label});
        }
    } catch(const char *msg) {
        errmsg = msg;
    }
    proc.exec("umount", {"/mnt/btrfs-scratch"});
    if (errmsg) throw errmsg;
    proc.setExceptionMode(nullptr);
}

void PartMan::loadKeyMaterial(const QString &keyfile)
{
    if (keyfile.isEmpty()) return;
    key.load(keyfile.toUtf8().constData(), -1);
    const int keylen = key.length();
    if (keylen > 0) {
        proc.log(QStringLiteral("Loaded %1-byte key material: ").arg(keylen) + keyfile);
    }
}

// write out crypttab if encrypting for auto-opening
bool PartMan::fixCryptoSetup()
{
    if (proc.halted()) return false;

    const QLineEdit *passedit = gui.radioEntireDisk->isChecked()
        ? gui.textCryptoPass : gui.textCryptoPassCust;
    const QByteArray password(passedit->text().toUtf8());
    // Write the key file.
    static const char *const rngfile = "/dev/urandom";
    const unsigned int keylength = 4096;
    QString keyfile;
    bool isNewKey = true;
    if (isEncrypt("/")) { // if encrypting root
        isNewKey = (key.length() == 0);
        keyfile = "/mnt/antiX/root/keyfile";
        if (isNewKey) key.load(rngfile, keylength);
        key.save(keyfile.toUtf8().constData(), 0400);
    } else if (isEncrypt("/home") && isEncrypt(QString())>1) {
        // if encrypting /home without encrypting root
        keyfile = "/mnt/antiX/home/.keyfileDONOTdelete";
        key.load(rngfile, keylength);
        key.save(keyfile.toUtf8().constData(), 0400);
        key.erase();
    }
    keyfile.remove(0,10); // Eliminate "/mnt/antiX"

    // Find the file system and device which contains the key.
    QString keyMount('/'); // If no mount point matches, it's in a directory on the root.
    QString keyDev;
    const bool noKey = keyfile.isEmpty();
    if (!noKey) {
        for (const auto &it : mounts.toStdMap()) {
            if (keyfile.startsWith(it.first+'/')) keyMount = it.first;
        }
        DeviceItem *devit = mounts.value(keyMount);
        if (devit && devit->type == DeviceItem::Subvolume) devit = devit->parentItem;
        if (devit) keyDev = devit->device;
    }
    // Find extra devices
    QMap<QString, DeviceItem *> cryptAdd;
    for (DeviceItemIterator it(*this); DeviceItem *item = *it; it.next()) {
        if (item->addToCrypttab) cryptAdd.insert(item->device, item);
    }
    // File systems
    for (auto &it : mounts.toStdMap()) {
        if (it.second->encrypt) cryptAdd.insert(it.second->device, it.second);
        else if (it.second->type == DeviceItem::Subvolume) {
            // Treat format-only, encrypted parent BTRFS partition as an extra device.
            DeviceItem *partit = it.second->parent();
            if (partit->encrypt && partit->realUseFor() == "FORMAT") {
                cryptAdd.insert(partit->device, partit);
            }
        }
    }

    QFile file("/mnt/antiX/etc/crypttab");
    if (!file.open(QIODevice::WriteOnly)) return false;
    QTextStream out(&file);
    // Add devices to crypttab.
    for (auto &it : cryptAdd.toStdMap()) {
        out << it.second->devMapper << " /dev/disk/by-uuid/" << it.second->uuid;
        if (noKey || it.first == keyDev) out << " none";
        else {
            if (isNewKey) {
                if (!proc.exec("cryptsetup", {"luksAddKey",
                    "/dev/"+it.first, "/mnt/antiX"+keyfile}, &password)) return false;
            }
            out << ' ' << keyfile;
        }
        out << " luks";
        if (!it.second->flags.rotational) out << ",no-read-workqueue,no-write-workqueue";
        if (it.second->discgran) out << ",discard";
        out << '\n';
    }
    // Update cryptdisks if the key is not in the root file system.
    if (keyMount != '/') {
        if (!proc.shell("sed -i 's/^CRYPTDISKS_MOUNT.*$/CRYPTDISKS_MOUNT=\"\\"
            + keyMount + "\"/' /mnt/antiX/etc/default/cryptdisks")) return false;
    }
    file.close();
    return true;
}

// create /etc/fstab file
bool PartMan::makeFstab()
{
    QFile file("/mnt/antiX/etc/fstab");
    if (!file.open(QIODevice::WriteOnly)) return false;
    QTextStream out(&file);
    out << "# Pluggable devices are handled by uDev, they are not in fstab\n";
    // File systems and swap space.
    for (auto &it : mounts.toStdMap()) {
        const DeviceItem *twit = it.second;
        const QString &dev = twit->mappedDevice();
        qDebug() << "Creating fstab entry for:" << it.first << dev;
        // Device ID or UUID
        if (twit->willMap()) out << dev;
        else out << "UUID=" << twit->assocUUID();
        // Mount point, file system
        proc.exec("blkid", {dev, "-o", "value", "-s", "TYPE"}, nullptr, true);
        const QString &fsfmt = proc.readOut();
        if (fsfmt == "swap") out << " swap swap";
        else out << ' ' << it.first << ' ' << fsfmt;
        // Options
        const QString &mountopts = twit->options;
        if (twit->type == DeviceItem::Subvolume) {
            out << " subvol=" << twit->label;
            if (!mountopts.isEmpty()) out << ',' << mountopts;
        } else {
            if (mountopts.isEmpty()) out << " defaults";
            else out << ' ' << mountopts;
        }
        out << ' ' << (twit->dump ? 1 : 0);
        out << ' ' << twit->pass << '\n';
    }
    // EFI System Partition
    if (gui.radioBootESP->isChecked()) {
        DeviceItem *devit = findByPath("/dev/" + gui.comboBoot->currentData().toString());
        if (!devit) return false;
        out << "UUID=" << devit->uuid << " /boot/efi vfat noatime,dmask=0002,fmask=0113 0 0\n";
    }
    file.close();
    return true;
}

void PartMan::mountPartitions()
{
    proc.log(__PRETTY_FUNCTION__, MProcess::Section);
    proc.setExceptionMode(QT_TR_NOOP("Failed to mount partition."));
    for (auto &it : mounts.toStdMap()) {
        if (it.first.at(0) != '/') continue;
        const QString point("/mnt/antiX" + it.first);
        const QString &dev = it.second->mappedDevice();
        proc.status(tr("Mounting: %1").arg(dev));
        if (it.first == "/boot") {
             // needed to run fsck because sfdisk --part-type can mess up the partition
            proc.exec("fsck.ext4", {"-y", dev});
        }
        QStringList opts;
        if (it.second->type == DeviceItem::Subvolume) {
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
        proc.exec("/bin/mount", {"--mkdir", "-o", opts.join(','), dev, point});
    }
    proc.setExceptionMode(nullptr);
}

void PartMan::unmount()
{
    QMapIterator<QString, DeviceItem *> mit(mounts);
    mit.toBack();
    while (mit.hasPrevious()) {
        mit.previous();
        DeviceItem *twit = mit.value();
        if (!twit) continue;
        if (mit.key().startsWith("SWAP")) {
            proc.shell("grep -q " + twit->mappedDevice() + " /proc/swaps &&  swapoff " + twit->mappedDevice());
        } else if (mit.key().at(0) == '/') {
            proc.exec("/bin/umount", {"-l", "/mnt/antiX" + mit.key()});
        }
    }
    for (DeviceItemIterator it(*this); DeviceItem *item = *it; it.next()) {
        if (item->encrypt && item->type != DeviceItem::VirtualBD) {
            proc.exec("cryptsetup", {"close", item->devMapper});
        }
    }
}

// Public properties
int PartMan::swapCount() const noexcept
{
    int count = 0;
    for (const auto &mount : mounts.toStdMap()) {
        if (mount.first.startsWith("SWAP")) ++count;
    }
    return count;
}

int PartMan::isEncrypt(const QString &point) const noexcept
{
    int count = 0;
    if (point.isEmpty()) {
        for (DeviceItem *twit : qAsConst(mounts)) {
            if (twit->willEncrypt()) ++count;
        }
    } else if (point == "SWAP") {
        for (const auto &mount : mounts.toStdMap()) {
            if (mount.first.startsWith("SWAP") && mount.second->willEncrypt()) ++count;
        }
    } else {
        const DeviceItem *twit = mounts.value(point);
        if (twit && twit->willEncrypt()) ++count;
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
    DeviceItem *devit = nullptr;
    do {
        spath = QDir::cleanPath(QFileInfo(spath).path());
        devit = mounts.value(spath);
    } while(!devit && !spath.isEmpty() && spath != '/' && spath != '.');
    return devit;
}

struct PartMan::VolumeSpec PartMan::volSpecTotal(const QString &path, const QStringList &vols) const noexcept
{
    struct VolumeSpec vspec = volSpecs.value(path);
    const QString &spath = (path!='/' ? path+'/' : path);
    for (const auto &it : volSpecs.toStdMap()) {
        if (it.first.size() > 1 && it.first.startsWith(spath) && !vols.contains(it.first)) {
            vspec.image += it.second.image;
            vspec.minimum += it.second.minimum;
            vspec.preferred += it.second.preferred;
        }
    }
    return vspec;
}

/*************************\
 * Model View Controller *
\*************************/

QVariant PartMan::data(const QModelIndex &index, int role) const noexcept
{
    DeviceItem *item = static_cast<DeviceItem*>(index.internalPointer());
    const bool isDriveOrVD = (item->type == DeviceItem::Drive || item->type == DeviceItem::VirtualDevices);
    if (role == Qt::EditRole) {
        switch (index.column())
        {
        case Device: return item->device; break;
        case Size: return item->size; break;
        case UseFor: return item->usefor; break;
        case Label: return item->label; break;
        case Format: return item->format; break;
        case Options: return item->options; break;
        case Pass: return item->pass; break;
        }
    } else if (role == Qt::CheckStateRole && !isDriveOrVD
        && index.flags() & Qt::ItemIsUserCheckable) {
        switch (index.column())
        {
        case Encrypt: return item->encrypt ? Qt::Checked : Qt::Unchecked; break;
        case Check: return item->chkbadblk ? Qt::Checked : Qt::Unchecked; break;
        case Dump: return item->dump ? Qt::Checked : Qt::Unchecked; break;
        }
    } else if (role == Qt::DisplayRole) {
        switch (index.column()) {
        case Device:
            if (item->type == DeviceItem::Subvolume) return "----";
            else if (item->isActive()) return item->device + '*';
            else return item->device;
            break;
        case Size:
            if (item->type == DeviceItem::Subvolume) return "----";
            else {
                return QLocale::system().formattedDataSize(item->size,
                    1, QLocale::DataSizeTraditionalFormat);
            }
            break;
        case UseFor: return item->usefor; break;
        case Label:
            if (index.flags() & Qt::ItemIsEditable) return item->label;
            else return item->curLabel;
            break;
        case Format:
            if (item->type == DeviceItem::Drive) {
                if (item->willUseGPT()) return "GPT";
                else if (item->flags.curMBR || !item->flags.oldLayout) return "MBR";
            } else if (item->type == DeviceItem::Subvolume) {
                return item->shownFormat(item->format);
            } else {
                if (item->usefor.isEmpty()) return item->curFormat;
                else return item->shownFormat(item->format);
            }
            break;
        case Options:
            if (item->canMount(false)) return item->options;
            else if (!isDriveOrVD) return "--------";
            break;
        case Pass:
            if (item->canMount()) return item->pass;
            else if (!isDriveOrVD) return "--";
            break;
        }
    } else if (role == Qt::ToolTipRole) {
        switch (index.column()) {
        case Device:
            if (!item->model.isEmpty()) return tr("Model: %1").arg(item->model);
            else return item->path;
            break;
        case Size:
            if (item->type == DeviceItem::Subvolume) return "----";
            else if (item->type == DeviceItem::Drive) {
                long long fs = item->driveFreeSpace();
                if (fs <= 0) fs = 0;
                return tr("Free space: %1").arg(QLocale::system().formattedDataSize(fs,
                    1, QLocale::DataSizeTraditionalFormat));
            }
            break;
        }
        return item->shownDevice();
    } else if (role == Qt::DecorationRole && index.column() == Device) {
        if ((item->type == DeviceItem::Drive || item->type == DeviceItem::Subvolume) && !item->flags.oldLayout) {
            return QIcon(":/appointment-soon");
        } else if (item->type == DeviceItem::VirtualBD && item->flags.volCrypto) {
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
        case Encrypt: item->encrypt = (value == Qt::Checked); break;
        case Check: item->chkbadblk = (value == Qt::Checked); break;
        case Dump: item->dump = (value == Qt::Checked); break;
        }
    }
    if(!changeEnd()) emit dataChanged(index, index);
    return true;
}
Qt::ItemFlags PartMan::flags(const QModelIndex &index) const noexcept
{
    DeviceItem *item = static_cast<DeviceItem *>(index.internalPointer());
    if (item->type == DeviceItem::VirtualDevices) return Qt::ItemIsEnabled;
    Qt::ItemFlags flagsOut({Qt::ItemIsSelectable, Qt::ItemIsEnabled});
    if (item->mapCount) return flagsOut;
    switch (index.column())
    {
    case Device: break;
    case Size:
        if (item->type == DeviceItem::Partition && !item->flags.oldLayout) {
            flagsOut |= Qt::ItemIsEditable;
        }
        break;
    case UseFor:
        if (item->allowedUsesFor().count() >= 1 && item->format != "DELETE") {
            flagsOut |= Qt::ItemIsEditable;
        }
        break;
    case Label:
        if (item->format != "PRESERVE" && item->format != "DELETE") {
            if (item->type == DeviceItem::Subvolume) flagsOut |= Qt::ItemIsEditable;
            else if (!item->usefor.isEmpty()) flagsOut |= Qt::ItemIsEditable;
        }
        break;
    case Encrypt:
        if (item->canEncrypt()) flagsOut |= Qt::ItemIsUserCheckable;
        break;
    case Format:
        if (item->allowedFormats().count() > 1) flagsOut |= Qt::ItemIsEditable;
        break;
    case Check:
        if (item->format.startsWith("ext") || item->format == "jfs"
            || item->format.startsWith("FAT", Qt::CaseInsensitive)) {
            flagsOut |= Qt::ItemIsUserCheckable;
        }
        break;
    case Options:
        if (item->canMount(false)) flagsOut |= Qt::ItemIsEditable;
        break;
    case Dump:
        if (item->canMount()) flagsOut |= Qt::ItemIsUserCheckable;
        break;
    case Pass:
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
        case Device: return tr("Device"); break;
        case Size: return tr("Size"); break;
        case UseFor: return tr("Use For"); break;
        case Label: return tr("Label"); break;
        case Encrypt: return tr("Encrypt"); break;
        case Format: return tr("Format"); break;
        case Check: return tr("Check"); break;
        case Options: return tr("Options"); break;
        case Dump: return tr("Dump"); break;
        case Pass: return tr("Pass"); break;
        }
    } else if (role == Qt::FontRole && (section == Encrypt || section == Check || section == Dump)) {
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
        if (!changing->allowedUsesFor().contains(changing->realUseFor())) {
            changing->usefor.clear();
        }
        if (!changing->canEncrypt()) changing->encrypt = false;
        changed |= (1 << Size);
    }
    if (changing->usefor != root.usefor) {
        if (changing->usefor.isEmpty()) changing->format.clear();
        else changing->format = changing->allowedFormats().at(0);
        changed |= (1 << UseFor);
    }
    if (changing->encrypt != root.encrypt) {
        const QStringList &allowed = changing->allowedFormats();
        if (!allowed.contains(changing->format)) changing->format = allowed.at(0);
    }
    if (changing->format != root.format || changing->usefor != root.usefor) {
        changing->dump = false;
        changing->pass = 2;
        const QString &use = changing->realUseFor();
        if (use.isEmpty() || use == "FORMAT" || use == "ESP") changing->options.clear();
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
        if (changing->format != root.format) changed |= (1 << Format);
    }
    if (changed && notify) notifyChange(changing);
    changing = nullptr;
    return changed;
}
void PartMan::notifyChange(class DeviceItem *item, int first, int last) noexcept
{
    if (first < 0) first = 0;
    if (last < 0) last = _TreeColumns_ - 1;
    const int row = item->row();
    emit dataChanged(createIndex(row, first, item), createIndex(row, last, item));
}

/* Model element */

DeviceItem::DeviceItem(enum DeviceType type, DeviceItem *parent, DeviceItem *preceding) noexcept
    : parentItem(parent), type(type)
{
    if (type == Partition) size = 1*MB;
    if (parent) {
        flags.rotational = parent->flags.rotational;
        discgran = parent->discgran;
        if (type == Subvolume) size = parent->size;
        physec = parent->physec;
        partman = parent->partman;
        const int i = preceding ? (parentItem->children.indexOf(preceding) + 1) : parentItem->childCount();
        if (partman) partman->beginInsertRows(partman->index(parent), i, i);
        parent->children.insert(i, this);
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
        parentItem->children.removeAll(this);
        if (partman) {
            partman->endRemoveRows();
            partman->notifyChange(parentItem);
        }
    }
}
void DeviceItem::clear() noexcept
{
    const int chcount = children.count();
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
    return parentItem ? parentItem->children.indexOf(const_cast<DeviceItem *>(this)) : 0;
}
DeviceItem *DeviceItem::parent() const noexcept
{
    if (parentItem && !parentItem->parentItem) return nullptr; // Invisible root
    return parentItem;
}
inline DeviceItem *DeviceItem::child(int row) const noexcept
{
    if (row < 0 || row >= children.count()) return nullptr;
    return children.at(row);
}
inline int DeviceItem::indexOfChild(DeviceItem *child) noexcept
{
    return children.indexOf(child);
}
inline int DeviceItem::childCount() const noexcept
{
    return children.count();
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
QString DeviceItem::realUseFor(const QString &use) noexcept
{
    if (use == "root") return QStringLiteral("/");
    else if (use == "boot") return QStringLiteral("/boot");
    else if (use == "home") return QStringLiteral("/home");
    else if (!use.startsWith('/')) return use.toUpper();
    else return use;
}
QString DeviceItem::shownUseFor(const QString &use) noexcept
{
    if (use == "/") return "/ (root)";
    else if (use == "ESP") return tr("EFI System Partition");
    else if (use == "SWAP") return tr("swap space");
    else if (use == "FORMAT") return tr("format only");
    return use;
}
void DeviceItem::setActive(bool boot) noexcept
{
    if (!parentItem) return;
    if (partman && parentItem->active != this) {
        if (parentItem->active) partman->notifyChange(parentItem->active);
    }
    parentItem->active = boot ? this : nullptr;
    if (partman) partman->notifyChange(this);
}
inline bool DeviceItem::isActive() const noexcept
{
    if (!parentItem) return false;
    return (parentItem->active == this);
}
bool DeviceItem::isLocked() const noexcept
{
    const int partCount = children.count();
    for (int ixPart = 0; ixPart < partCount; ++ixPart) {
        if (children.at(ixPart)->isLocked()) return true;
    }
    return (mapCount != 0);
}
bool DeviceItem::willUseGPT() const noexcept
{
    if (type != Drive) return false;
    if (flags.oldLayout) return flags.curGPT;
    else if (size >= 2*TB || children.count() > 4) return true;
    else if (partman) return (partman->gptoverride || partman->proc.detectEFI());
    return false;
}
bool DeviceItem::willFormat() const noexcept
{
    return format != "PRESERVE" && !usefor.isEmpty();
}
bool DeviceItem::canEncrypt() const noexcept
{
    if (type != Partition) return false;
    const QString &use = realUseFor();
    return !(use.isEmpty() || use == "ESP" || use == "BIOS-GRUB" || use == "/boot");
}
inline bool DeviceItem::willEncrypt() const noexcept
{
    if (type == Subvolume) return parentItem->encrypt;
    return encrypt;
}
QString DeviceItem::assocUUID() const noexcept
{
    if (type == Subvolume) return parentItem->uuid;
    return uuid;
}
QString DeviceItem::mappedDevice() const noexcept
{
    const DeviceItem *twit = this;
    if (twit->type == Subvolume) twit = twit->parentItem;
    if (twit->type == Partition) {
        const QVariant &d = twit->devMapper;
        if (!d.isNull()) return "/dev/mapper/" + d.toString();
    }
    return twit->path;
}
inline bool DeviceItem::willMap() const noexcept
{
    if (type == Drive || type == VirtualDevices) return false;
    else if (type == Subvolume) return !parentItem->devMapper.isEmpty();
    return !devMapper.isEmpty();
}
QString DeviceItem::shownDevice() const noexcept
{
    if (type == Subvolume) return parentItem->device + '[' + label + ']';
    return device;
}
QStringList DeviceItem::allowedUsesFor(bool real, bool all) const noexcept
{
    if (!isVolume() && type != Subvolume) return QStringList();
    QStringList list;
    auto checkAndAdd = [&](const QString &use) {
        const QString &realUse = realUseFor(use);
        if (all || !partman || size >= partman->volSpecs.value(realUse).minimum) {
            list.append(real ? realUse : use);
        }
    };

    if (type != Subvolume) checkAndAdd("Format");
    checkAndAdd("root");
    if (type != VirtualBD) {
        if (all || size <= 16*MB) checkAndAdd("BIOS-GRUB");
        if (all || size <= 8*GB) {
            if (size < (2*TB - 512)) checkAndAdd("ESP");
            checkAndAdd("boot");
        }
    }
    checkAndAdd("home");
    if (type != Subvolume) checkAndAdd("swap");
    else checkAndAdd("/swap");
    return list;
}
QStringList DeviceItem::allowedFormats() const noexcept
{
    QStringList list;
    const QString &use = realUseFor();
    bool allowPreserve = false;
    if (isVolume()) {
        if (use.isEmpty()) return QStringList();
        else if (use == "/boot") list.append("ext4");
        else if (use == "BIOS-GRUB") list.append("GRUB");
        else if (use == "ESP") {
            list.append("FAT32");
            if (size <= (4*GB - 64*KB)) list.append("FAT16");
            if (size <= (32*MB - 512)) list.append("FAT12");
            allowPreserve = (list.contains(curFormat, Qt::CaseInsensitive)
                || !curFormat.compare("VFAT", Qt::CaseInsensitive));
        } else if (use == "SWAP") {
            list.append("SWAP");
            allowPreserve = list.contains(curFormat, Qt::CaseInsensitive);
        } else {
            list << "ext4" << "ext3" << "ext2";
            list << "f2fs" << "jfs" << "xfs" << "btrfs";
            if (use != "FORMAT") allowPreserve = list.contains(curFormat, Qt::CaseInsensitive);
        }
    } else if (type == Subvolume) {
        list.append("CREATE");
        if (flags.oldLayout) {
            list.append("DELETE");
            allowPreserve = true;
        }
    }

    if (encrypt) allowPreserve = false;
    if (allowPreserve) {
        if (use != "/") list.prepend("PRESERVE");
        else list.append("PRESERVE");
    }
    return list;
}
QString DeviceItem::finalFormat() const noexcept
{
    if (type == Subvolume) return format;
    return (usefor.isEmpty() || format == "PRESERVE") ? curFormat : format;
}
QString DeviceItem::shownFormat(const QString &fmt) const noexcept
{
    if (fmt == "CREATE") return flags.oldLayout ? tr("Overwrite") : tr("Create");
    else if (fmt == "DELETE") return tr("Delete");
    else if (fmt != "PRESERVE") return fmt;
    else {
        if (type == Subvolume) return tr("Preserve");
        else if (realUseFor() != "/") return tr("Preserve (%1)").arg(curFormat);
        else return tr("Preserve /home (%1)").arg(curFormat);
    }
}
bool DeviceItem::canMount(bool pointonly) const noexcept
{
    const QString &use = realUseFor();
    return !use.isEmpty() && use != "FORMAT" && use != "ESP" && use != "BIOS-GRUB"
        && (!pointonly || use != "SWAP");
}
long long DeviceItem::driveFreeSpace(bool inclusive) const noexcept
{
    const DeviceItem *drvit = parent();
    if (!drvit) drvit = this;
    long long free = drvit->size - PARTMAN_SAFETY;
    for (int ixi = drvit->children.count() - 1; ixi >= 0; --ixi) {
        DeviceItem *partit = drvit->children.at(ixi);
        if (inclusive || partit != this) free -= partit->size;
    }
    return free;
}
/* Convenience */
DeviceItem *DeviceItem::addPart(long long defaultSize, const QString &defaultUse, bool crypto) noexcept
{
    DeviceItem *partit = new DeviceItem(DeviceItem::Partition, this);
    if (!defaultUse.isEmpty()) partit->usefor = defaultUse;
    partit->size = defaultSize;
    partit->autoFill();
    if (partit->canEncrypt()) partit->encrypt = crypto;
    if (partman) partman->notifyChange(partit);
    return partit;
}
void DeviceItem::driveAutoSetActive() noexcept
{
    if (active) return;
    if (partman && partman->proc.detectEFI() && willUseGPT()) return;
    // Cannot use partman->mounts map here as it may not be populated.
    for (const QString &pref : QStringList({"/boot", "/"})) {
        for (DeviceItemIterator it(this); DeviceItem *item = *it; it.next()) {
            if (item->realUseFor() == pref) {
                while (item && item->type != Partition) item = item->parentItem;
                if (item) item->setActive(true);
                return;
            }
        }
    }
}
void DeviceItem::autoFill(unsigned int changed) noexcept
{
    const QString &use = realUseFor();
    if (changed & (1 << PartMan::UseFor)) {
        // Default labels
        if (type == Subvolume) {
            QStringList chklist;
            const int count = parentItem->childCount();
            const int index = parentItem->indexOfChild(this);
            for (int ixi = 0; ixi < count; ++ixi) {
                if (ixi == index) continue;
                chklist << parentItem->child(ixi)->label;
            }
            QString newLabel;
            if (use.startsWith('/')) {
                const QString base = use.mid(1).replace('/','.');
                newLabel = '@' + base;
                for (int ixi = 2; chklist.contains(newLabel, Qt::CaseInsensitive); ++ixi) {
                    newLabel = QString::number(ixi) + '@' + base;
                }
            } else {
                newLabel = use;
                for (int ixi = 2; chklist.contains(newLabel, Qt::CaseInsensitive); ++ixi) {
                    newLabel = usefor + QString::number(ixi);
                }
            }
            label = newLabel;
        } else if (partman) {
            label = partman->volSpecs.value(use).defaultLabel;
        }
        // Automatic default boot device selection
        if ((type != VirtualBD) && (use == "/boot" || use == "/")) {
            DeviceItem *drvit = this;
            while (drvit && drvit->type != Drive) drvit = drvit->parentItem;
            if (drvit) drvit->driveAutoSetActive();
        }
    }
    if (format.isEmpty()) {
        const QStringList &af = allowedFormats();
        if (!af.isEmpty()) {
            format = af.at(0);
            changed |= (1 << PartMan::Format);
        }
    }
    if ((changed & ((1 << PartMan::UseFor) | (1 << PartMan::Format))) && canMount(false)) {
        // Default options, dump and pass
        if (format == "SWAP") options = discgran ? "discard" : "defaults";
        else {
            if (use == "/boot" || use == "/") {
                pass = (format == "btrfs") ? 0 : 1;
            }
            options.clear();
            const bool btrfs = (format == "btrfs" || type == Subvolume);
            if (!flags.rotational && btrfs) options += "ssd,";
            if (discgran && (format == "ext4" || format == "xfs")) options += "discard,";
            else if (discgran && btrfs) options += "discard=async,";
            options += "noatime";
            if (btrfs && use != "/swap") options += ",compress=zstd:1";
            dump = true;
        }
    }
}
void DeviceItem::labelParts() noexcept
{
    for (int ixi = childCount() - 1; ixi >= 0; --ixi) {
        DeviceItem *chit = children.at(ixi);
        chit->device = join(device, ixi + 1);
        chit->path = "/dev/" + chit->device;
        if (partman) partman->notifyChange(chit, PartMan::Device, PartMan::Device);
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
QStringList DeviceItem::split(const QString &devname) noexcept
{
    const QRegularExpression rxdev1("^(?:\\/dev\\/)*(mmcblk.*|nvme.*)p([0-9]*)$");
    const QRegularExpression rxdev2("^(?:\\/dev\\/)*([a-z]*)([0-9]*)$");
    QRegularExpressionMatch rxmatch(rxdev1.match(devname));
    if (!rxmatch.hasMatch()) rxmatch = rxdev2.match(devname);
    QStringList list(rxmatch.capturedTexts());
    if (!list.isEmpty()) list.removeFirst();
    return list;
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
            ixPos = ixParents.pop();
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
        if(index.column() == PartMan::Format || index.column() == PartMan::UseFor) {
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
    case PartMan::Format:
        for (const QString &fmt : item->allowedFormats()) {
            const int fw = option.fontMetrics.boundingRect(item->shownFormat(fmt)).width() + residue;
            if (fw > width) width = fw;
        }
        break;
    case PartMan::UseFor:
        for (const QString &use : item->allowedUsesFor(false, false)) {
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
    case PartMan::Size:
        {
            QSpinBox *spin = new QSpinBox(parent);
            spin->setSuffix("MB");
            spin->setStepType(QSpinBox::AdaptiveDecimalStepType);
            spin->setAccelerated(true);
            spin->setSpecialValueText("MAX");
            widget = spin;
        }
        break;
    case PartMan::UseFor:
        widget = combo = new QComboBox(parent);
        combo->setEditable(true);
        combo->setInsertPolicy(QComboBox::NoInsert);
        combo->lineEdit()->setPlaceholderText("----");
        break;
    case PartMan::Format: widget = combo = new QComboBox(parent); break;
    case PartMan::Pass: widget = new QSpinBox(parent); break;
    case PartMan::Options:
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
    case PartMan::Size:
        {
            QSpinBox *spin = qobject_cast<QSpinBox *>(editor);
            spin->setRange(0, static_cast<int>(item->driveFreeSpace() / MB));
            spin->setValue(item->size / MB);
        }
        break;
    case PartMan::UseFor:
        {
            QComboBox *combo = qobject_cast<QComboBox *>(editor);
            combo->clear();
            combo->addItem("");
            combo->addItems(item->allowedUsesFor(false, false));
            combo->setCurrentText(item->usefor);
        }
        break;
    case PartMan::Label:
        qobject_cast<QLineEdit *>(editor)->setText(item->label);
        break;
    case PartMan::Format:
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
    case PartMan::Pass:
        qobject_cast<QSpinBox *>(editor)->setValue(item->pass);
        break;
    case PartMan::Options:
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
    case PartMan::Size:
        item->size = qobject_cast<QSpinBox *>(editor)->value();
        item->size *= 1048576; // Separate step to prevent int overflow.
        if (item->size == 0) item->size = item->driveFreeSpace();
        break;
    case PartMan::UseFor:
        item->usefor = qobject_cast<QComboBox *>(editor)->currentText().trimmed();
        break;
    case PartMan::Label:
        item->label = qobject_cast<QLineEdit *>(editor)->text().trimmed();
        break;
    case PartMan::Format:
        item->format = qobject_cast<QComboBox *>(editor)->currentData().toString();
        break;
    case PartMan::Pass:
        item->pass = qobject_cast<QSpinBox *>(editor)->value();
        break;
    case PartMan::Options:
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
    DeviceItem *partit = static_cast<DeviceItem *>(edit->property("row").value<void *>());
    if (!partit) return;
    QMenu *menu = edit->createStandardContextMenu();
    menu->addSeparator();
    QMenu *menuTemplates = menu->addMenu(tr("&Templates"));
    QString selFS = partit->format;
    if (selFS == "PRESERVE") selFS = partit->curFormat;
    if ((partit->type == DeviceItem::Partition && selFS == "btrfs") || partit->type == DeviceItem::Subvolume) {
        QString tcommon;
        if (!partit->flags.rotational) tcommon = "ssd,";
        if (partit->discgran) tcommon = "discard=async,";
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
