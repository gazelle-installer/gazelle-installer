/***************************************************************************
 * Basic partition manager for the installer.
 ***************************************************************************
 *
 *   Copyright (C) 2019, 2020-2021 by AK-47
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

#include <sys/sysinfo.h>
#include <sys/stat.h>
#include <QDebug>
#include <QTimer>
#include <QLocale>
#include <QFileInfo>
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

#include "msettings.h"
#include "partman.h"

#define PARTMAN_SAFETY_MB 8 // 1MB at start + Compensate for rounding errors.
#define PARTMAN_MAX_PARTS 128 // Maximum number of partitions Linux supports.

PartMan::PartMan(MProcess &mproc, Ui::MeInstall &ui, QWidget *parent)
    : QAbstractItemModel(parent), proc(mproc), root(DeviceItem::Unknown), gui(ui), master(parent)
{
    root.partman = this;
    QTimer::singleShot(0, this, &PartMan::setup);
}

void PartMan::setup()
{
    gui.treePartitions->setModel(this);
    gui.treePartitions->setItemDelegate(new DeviceItemDelegate);
    gui.treePartitions->header()->setMinimumSectionSize(5);
    gui.treePartitions->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(gui.treePartitions, &QTreeView::customContextMenuRequested, this, &PartMan::treeMenu);
    connect(gui.treePartitions->selectionModel(), &QItemSelectionModel::selectionChanged, this, &PartMan::treeSelChange);
    connect(gui.pushPartClear, &QToolButton::clicked, this, &PartMan::partClearClick);
    connect(gui.pushPartAdd, &QToolButton::clicked, this, &PartMan::partAddClick);
    connect(gui.pushPartRemove, &QToolButton::clicked, this, &PartMan::partRemoveClick);
    connect(this, &PartMan::dataChanged, this, &PartMan::treeItemChange);
    gui.pushPartAdd->setEnabled(false);
    gui.pushPartRemove->setEnabled(false);
    gui.pushPartClear->setEnabled(false);
    gui.boxCryptoPass->setEnabled(false);
    defaultLabels["ESP"] = "EFI System";
    defaultLabels["BIOS-GRUB"] = "BIOS GRUB";

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
    // expressions for matching various partition types
    const QRegularExpression rxExtended("^(0x05|0x0f|0x15|0x1f|0x91|0x9b|0xc5|0xcf|0xd5)$");
    const QRegularExpression rxESP("^(c12a7328-f81f-11d2-ba4b-00a0c93ec93b|0xef)$");
    const QRegularExpression rxWinLDM("^(0x42|5808c8aa-7e8f-42e0-85d2-e1e90434cfb3"
                                      "|af9b60a0-1431-4f62-bc68-3311714a69ad)$"); // Windows LDM

    // Backup ESP detection. Populated when needed.
    QStringList backup_list;
    bool backup_checked = false;
    // Collect information and populate the block device list.
    QString cmd("lsblk -T -bJo TYPE,NAME,UUID,SIZE,PHY-SEC,PTTYPE,PARTTYPE,FSTYPE,LABEL,MODEL,PARTFLAGS");
    if (drvstart) cmd += " /dev/" + drvstart->device;
    const QString &bdRaw = proc.execOut(cmd, true);
    const QJsonObject &jsonObjBD = QJsonDocument::fromJson(bdRaw.toUtf8()).object();
    const QJsonArray &jsonBD = jsonObjBD["blockdevices"].toArray();

    if (!drvstart) {
        proc.exec("partprobe -s", true); // Inform the kernel of partition changes.
        proc.exec("blkid -c /dev/null", true); // Refresh cached blkid information.
    }
    // Partitions listed in order of their physical locations.
    QStringList order;
    QString curdev;
    for(const QString &line : proc.execOutLines("parted -lm", true)) {
        const QString &sect = line.section(':', 0, 0);
        const int part = sect.toInt();
        if (part) order.append(DeviceItem::join(curdev, part));
        else if (sect.startsWith("/dev/")) curdev = sect.mid(5);
    }

    if (!drvstart) root.clear();
    for (const QJsonValue &jsonDrive : jsonBD) {
        const QString &jdName = jsonDrive["name"].toString();
        if (jsonDrive["type"] != "disk") continue;
        if (jdName.startsWith("zram")) continue;
        DeviceItem *drvit = drvstart;
        if (!drvstart) drvit = new DeviceItem(DeviceItem::Drive, &root);
        else if (jdName != drvstart->device) continue;

        drvit->clear();
        drvit->flags.curEmpty = true;
        drvit->flags.oldLayout = true;
        drvit->device = jdName;
        drvit->flags.useGPT = (jsonDrive["pttype"]=="gpt");
        drvit->size = jsonDrive["size"].toVariant().toLongLong();
        drvit->physec = jsonDrive["phy-sec"].toInt();
        drvit->curLabel = jsonDrive["label"].toString();
        drvit->model = jsonDrive["model"].toString();
        const QJsonArray &jsonParts = jsonDrive["children"].toArray();
        for (const QJsonValue &jsonPart : jsonParts) {
            const QString &partType = jsonPart["parttype"].toString();
            if (partType.count(rxExtended)) continue;
            DeviceItem *partit = new DeviceItem(DeviceItem::Partition, drvit);
            drvit->flags.curEmpty = false;
            partit->flags.oldLayout = true;
            partit->device = jsonPart["name"].toString();
            partit->order = order.indexOf(partit->device);
            partit->size = jsonPart["size"].toVariant().toLongLong();
            partit->physec = jsonPart["phy-sec"].toInt();
            partit->curLabel = jsonPart["label"].toString();
            partit->model = jsonPart["model"].toString();
            const int partflags = jsonPart["partflags"].toString().toUInt(nullptr, 0);
            if ((partflags & 0x80) || (partflags & 0x04)) partit->setActive(true);
            partit->mapCount = jsonPart["children"].toArray().count();

            if (!partType.isEmpty()) {
                partit->flags.curESP = (partType.count(rxESP) >= 1);
                if (!partit->flags.nasty) partit->flags.nasty = (partType.count(rxWinLDM) >= 1);
            }
            if (!partit->flags.curESP) {
                // Backup detection for drives that don't have UUID for ESP.
                if (!backup_checked) {
                    backup_list = proc.execOutLines("fdisk -l -o DEVICE,TYPE"
                        " | grep 'EFI System' |cut -d\\  -f1 | cut -d/ -f3");
                    backup_checked = true;
                }
                partit->flags.curESP = backup_list.contains(partit->device);
            }

            partit->flags.bootRoot = (!bootUUID.isEmpty() && jsonPart["uuid"]==bootUUID);
            partit->curFormat = jsonPart["fstype"].toString();
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
                    listed.insert(devit->device, devit);
                }
                break;
            }
        }
    }
    const QString &bdRaw = proc.execOut("lsblk -T -bJo"
        " TYPE,NAME,UUID,SIZE,PHY-SEC,FSTYPE,LABEL /dev/mapper/* 2>/dev/null", true);
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
        const QString &name = jsonDev["name"].toString();
        const long long size = jsonDev["size"].toVariant().toLongLong();
        const int physec = jsonDev["phy-sec"].toInt();
        const QString &label = jsonDev["label"].toString();
        const bool crypto = (jsonDev["type"].toString() == "crypt");
        // Check if the device is already in the list.
        DeviceItem *devit = listed.value(name);
        if (devit) {
            if (size != devit->size || physec != devit->physec || label != devit->curLabel
                || crypto != devit->flags.cryptoV) {
                // List entry is different to the device, so refresh it.
                delete devit;
                devit = nullptr;
            }
            listed.remove(name);
        }
        // Create a new list entry if needed.
        if (!devit) {
            devit = new DeviceItem(DeviceItem::VirtualBD, vdlit);
            devit->device = devit->devMapper = name;
            devit->size = size;
            devit->physec = physec;
            devit->flags.bootRoot = (!bootUUID.isEmpty() && jsonDev["uuid"]==bootUUID);
            devit->curLabel = label;
            devit->curFormat = jsonDev["fstype"].toString();
            devit->flags.cryptoV = crypto;
            devit->flags.oldLayout = true;
        }
    }
    for (const auto &it : listed.toStdMap()) delete it.second;
    vdlit->sortChildren();
    for(int ixi = 0; ixi < vdlit->childCount(); ++ixi) {
        const QString &name = vdlit->child(ixi)->device;
        DeviceItem *orit = findOrigin(name);
        if (orit) {
            orit->devMapper = name;
            notifyChange(orit);
        }
    }
    gui.treePartitions->expand(index(vdlit));
}

bool PartMan::manageConfig(MSettings &config, bool save)
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
        const long long sizeMax = drvit->size - (PARTMAN_SAFETY_MB * 1048576);
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
                config.setValue("Encrypt", partit->encrypt);
                if (partit->isActive()) config.setValue("Boot", true);
                if (!partit->usefor.isEmpty()) config.setValue("UseFor", partit->usefor);
                if (!partit->format.isEmpty()) config.setValue("Format", partit->format);
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
                    if (config.value("Boot").toBool()) partit->setActive(true);
                }
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

void PartMan::resizeColumnsToFit()
{
    for (int ixi = _TreeColumns_ - 1; ixi >= 0; --ixi) {
        gui.treePartitions->resizeColumnToContents(ixi);
    }
}

void PartMan::treeItemChange()
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

void PartMan::treeSelChange()
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
                && twit->driveFreeSpace(true) > 1048576);
        }
    } else {
        gui.pushPartClear->setEnabled(false);
        gui.pushPartAdd->setEnabled(false);
        gui.pushPartRemove->setEnabled(false);
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
        QAction *actSetBoot = nullptr;
        actRemove->setEnabled(gui.pushPartRemove->isEnabled());
        menu.addSeparator();
        if (twit->type == DeviceItem::VirtualBD) {
            actRemove->setEnabled(false);
            if (twit->flags.cryptoV) actLock = menu.addAction(tr("&Lock"));
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
                actAddCrypttab->setChecked(twit->flags.autoCrypto);
                actAddCrypttab->setEnabled(twit->willMap());
            }
            menu.addSeparator();
            actSetBoot = menu.addAction(tr("Set boot flag"));
            actSetBoot->setCheckable(true);
            actSetBoot->setChecked(twit->isActive());
        }
        if (twit->format == "btrfs") {
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
        else if (action == actSetBoot) twit->setActive(action->isChecked());
        else if (action == actAddCrypttab) {
            twit->flags.autoCrypto = action->isChecked();
        } else if (action == actNewSubvolume) {
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
        QMenu *menuTemplates = menu.addMenu(tr("&Templates"));
        const QAction *actBasic = menuTemplates->addAction(tr("&Standard install"));
        QAction *actCrypto = menuTemplates->addAction(tr("&Encrypted system"));

        const bool locked = twit->isLocked();
        actClear->setDisabled(locked);
        actReset->setDisabled(locked);
        menuTemplates->setDisabled(locked);
        actCrypto->setVisible(gui.boxCryptoPass->isVisible());

        QAction *action = menu.exec(QCursor::pos());
        if (action == actAdd) partAddClick(true);
        else if (action == actClear) partClearClick(true);
        else if (action == actBasic) twit->layoutDefault(-1, false);
        else if (action == actCrypto) twit->layoutDefault(-1, true);
        else if (action == actReset) {
            gui.boxMain->setEnabled(false);
            scan(twit);
            gui.boxMain->setEnabled(true);
        }
    } else if (twit->type == DeviceItem::Subvolume) {
        QAction *actRemSubvolume = menu.addAction(tr("Remove subvolume"));
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

void PartMan::partAddClick(bool)
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

void PartMan::partRemoveClick(bool)
{
    const QModelIndexList &indexes = gui.treePartitions->selectionModel()->selectedIndexes();
    DeviceItem *partit = (indexes.size() > 0) ? item(indexes.at(0)) : nullptr;
    if (!partit) return;
    DeviceItem *drvit = partit->parent();
    if (!drvit) return;
    delete partit;
    drvit->labelParts();
    treeSelChange();
}

// Partition menu items
void PartMan::partMenuUnlock(DeviceItem *twit)
{
    QDialog dialog(master);
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
        const QString &mapdev = editVDev->text();
        const bool ok = luksOpen(twit->device, mapdev, editPass->text().toUtf8());
        if (ok) {
            twit->usefor.clear();
            twit->flags.autoCrypto = checkCrypttab->isChecked();
            twit->mapCount++;
            notifyChange(twit);
            scanVirtualDevices(true);
            resizeColumnsToFit();
            treeSelChange();
        }
        gui.boxMain->setEnabled(true);
        qApp->restoreOverrideCursor();
        if (!ok) {
            QMessageBox::warning(master, master->windowTitle(),
                tr("Could not unlock device. Possible incorrect password."));
        }
    }
}

void PartMan::partMenuLock(DeviceItem *twit)
{
    qApp->setOverrideCursor(QCursor(Qt::WaitCursor));
    gui.boxMain->setEnabled(false);
    const QString &dev = twit->mappedDevice();
    bool ok = false;
    // Find the associated partition and decrement its reference count if found.
    DeviceItem *origin = findOrigin(dev);
    if (origin) ok = proc.exec("cryptsetup close " + dev, true);
    if (ok) {
        if (origin->mapCount > 0) origin->mapCount--;
        origin->devMapper.clear();
        origin->flags.autoCrypto = false;
        notifyChange(origin);
    }
    // Refresh virtual devices list.
    scanVirtualDevices(true);
    resizeColumnsToFit();
    treeSelChange();

    gui.boxMain->setEnabled(true);
    qApp->restoreOverrideCursor();
    // If not OK then a command failed, or trying to close a non-LUKS device.
    if (!ok) QMessageBox::critical(master, master->windowTitle(),
            tr("Failed to close %1").arg(dev));
}

void PartMan::scanSubvolumes(DeviceItem *partit)
{
    qApp->setOverrideCursor(Qt::WaitCursor);
    gui.boxMain->setEnabled(false);
    while (partit->childCount()) delete partit->child(0);
    mkdir("/mnt/btrfs-scratch", 0755);
    QStringList lines;
    if (!proc.exec("mount -o noatime " + partit->mappedDevice(true)
        + " /mnt/btrfs-scratch", true)) goto END;
    lines = proc.execOutLines("btrfs subvolume list /mnt/btrfs-scratch", true);
    proc.exec("umount /mnt/btrfs-scratch", true);
    for (const QString &line : lines) {
        const int start = line.indexOf("path") + 5;
        if (line.length() <= start) goto END;
        DeviceItem *svit = new DeviceItem(DeviceItem::Subvolume, partit);
        svit->label = line.mid(start);
        notifyChange(svit);
    }
 END:
    gui.boxMain->setEnabled(true);
    qApp->restoreOverrideCursor();
}

bool PartMan::composeValidate(bool automatic, const QString &project)
{
    bool encryptRoot = false;
    mounts.clear();
    // Partition use and other validation
    int mapnum = 0, swapnum = 0;
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
                QMessageBox::critical(master, master->windowTitle(), tr("Invalid subvolume label"));
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
                    QMessageBox::critical(master, master->windowTitle(), tr("Duplicate subvolume label"));
                    return false;
                }
            }
        }
        QString mount = item->realUseFor();
        if (mount.isEmpty()) continue;
        if (!mount.startsWith("/") && !item->allowedUsesFor().contains(mount)) {
            QMessageBox::critical(master, master->windowTitle(),
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
            QMessageBox::critical(master, master->windowTitle(), tr("%1 is already"
                " selected for: %2").arg(twit->shownDevice(), twit->shownUseFor()));
            return false;
        } else if(!mount.isEmpty() && mount != "FORMAT" && mount != "ESP"){
            mounts.insert(mount, *it);
        }
        if (item->type != DeviceItem::VirtualBD) {
            if (!item->encrypt) item->devMapper.clear();
            else if (mount == "/") item->devMapper = "root.fsm";
            else if (mount.startsWith("SWAP")) item->devMapper = mount.toLower();
            else item->devMapper = QString::number(++mapnum) + mount.replace('/','.') + ".fsm";
        }
    }
    qDebug() << "Mount points:";
    for (const auto &it : mounts.toStdMap()) {
        qDebug() << " -" << it.first << '-' << it.second->shownDevice()
            << it.second->mappedDevice() << it.second->mappedDevice(true);
    }

    DeviceItem *rootitem = mounts.value("/");
    if (!rootitem || rootitem->size < rootSpaceNeeded) {
        const QString &tmin = QLocale::system().formattedDataSize(rootSpaceNeeded + 1048575,
            1, QLocale::DataSizeTraditionalFormat);
        QMessageBox::critical(master, master->windowTitle(),
            tr("A root partition of at least %1 is required.").arg(tmin));
        return false;
    } else {
        if (!rootitem->willFormat() && mounts.contains("/home")) {
            const QString errmsg = tr("Cannot preserve /home inside root (/) if"
                " a separate /home partition is also mounted.");
            QMessageBox::critical(master, master->windowTitle(), errmsg);
            return false;
        }
        if (rootitem->encrypt) encryptRoot = true;
    }
    if (encryptRoot && !mounts.contains("/boot")) {
        QMessageBox::critical(master, master->windowTitle(),
            tr("You must choose a separate boot partition when encrypting root."));
        return false;
    }

    if (!automatic) {
        // Final warnings before the installer starts.
        QString details, biosgpt;
        for (int ixdrv = 0; ixdrv < root.childCount(); ++ixdrv) {
            DeviceItem *drvit = root.child(ixdrv);
            const int partCount = drvit->childCount();
            bool setupGPT = uefi || gptoverride || drvit->size >= 2199023255552 || partCount > 4;
            if (drvit->flags.oldLayout) {
                setupGPT = drvit->flags.useGPT;
            } else if (drvit->type != DeviceItem::VirtualDevices) {
                details += tr("Prepare %1 partition table on %2").arg(setupGPT?"GPT":"MBR", drvit->device) + '\n';
            }
            bool hasBiosGrub = false;
            for (int ixdev = 0; ixdev < partCount; ++ixdev) {
                DeviceItem *partit = drvit->child(ixdev);
                const QString &use = partit->realUseFor();
                QString actmsg;
                if (drvit->flags.oldLayout) {
                    if (use.isEmpty()) continue;
                    else if (partit->willFormat()) actmsg = tr("Format %1 to use for %2");
                    else if (use != "/") actmsg = tr("Reuse (no reformat) %1 as %2");
                    else actmsg = tr("Delete the data on %1 except for /home, to use for %2");
                } else {
                    if (use.isEmpty()) actmsg = tr("Create %1 without formatting");
                    else actmsg = tr("Create %1, format to use for %2");
                }
                details += actmsg.arg(partit->shownDevice(), partit->shownUseFor()) + '\n';
                if (use == "BIOS-GRUB") hasBiosGrub = true;
            }
            // Potentially unbootable GPT when on a BIOS-based PC.
            const bool hasBoot = (drvit->active != nullptr);
            if (!uefi && setupGPT && hasBoot && !hasBiosGrub) biosgpt += ' ' + drvit->device;
        }
        // Warning messages
        QMessageBox msgbox(master);
        msgbox.setIcon(QMessageBox::Warning);
        msgbox.setWindowTitle(master->windowTitle());
        msgbox.setStandardButtons(QMessageBox::Yes|QMessageBox::No);
        msgbox.setDefaultButton(QMessageBox::No);
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
        msgbox.setDetailedText(details);
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
            const QString &drive = drvit->device;
            QString smartMsg = drive + " (" + purposes.join(", ") + ")";
            proc.exec("smartctl -H /dev/" + drive, true);
            if (proc.exitStatus() == MProcess::NormalExit && proc.exitCode() & 8) {
                // See smartctl(8) manual: EXIT STATUS (Bit 3)
                smartFail += " - " + smartMsg + "\n";
            } else {
                const QString &out = proc.execOut("smartctl -A /dev/" + drive
                        + "| grep -E \"^  5|^196|^197|^198\" | awk '{ if ( $10 != 0 ) { print $1,$2,$10} }'");
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
            ans = QMessageBox::critical(master, master->windowTitle(), msg,
                    QMessageBox::Yes|QMessageBox::Default|QMessageBox::Escape, QMessageBox::No);
            if (ans == QMessageBox::Yes) return false;
        } else {
            msg += tr("Do you want to continue?");
            ans = QMessageBox::warning(master, master->windowTitle(), msg,
                    QMessageBox::Yes|QMessageBox::Default, QMessageBox::No|QMessageBox::Escape);
            if (ans != QMessageBox::Yes) return false;
        }
    }
    return true;
}

bool PartMan::luksFormat(const QString &dev, const QByteArray &password)
{
    DeviceItem *item = findDevice(dev);
    assert(item != nullptr);
    QString cmd = "cryptsetup --batch-mode --key-size 512 --hash sha512 --pbkdf argon2id";
    if (item->physec > 0) cmd += " --sector-size=" + QString::number(item->physec);
    cmd += " luksFormat /dev/" + dev;
    if (!proc.exec(cmd, true, &password)) return false;
    return true;
}

bool PartMan::luksOpen(const QString &dev, const QString &luksfs,
    const QByteArray &password, const QString &options)
{
    QString cmd = "cryptsetup luksOpen /dev/" + dev;
    if (!luksfs.isEmpty()) cmd += " " + luksfs;
    if (!options.isEmpty()) cmd += " " + options;
    return proc.exec(cmd, true, &password);
}

DeviceItem *PartMan::selectedDriveAuto()
{
    QString drv(gui.comboDisk->currentData().toString());
    if (!findDevice(drv)) return nullptr;
    for (int ixi = 0; ixi < root.childCount(); ++ixi) {
        DeviceItem *const drvit = root.child(ixi);
        if (drvit->device == drv) return drvit;
    }
    return nullptr;
}

void PartMan::clearAllUses()
{
    for (DeviceItemIterator it(*this); DeviceItem *item = *it; it.next()) {
        item->usefor.clear();
        if (item->type == DeviceItem::Partition) item->setActive(false);
        notifyChange(item);
    }
}

int PartMan::countPrepSteps()
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
            if (item->willFormat()) ++nstep; // New file system or subvolume
            // Mounting
            if (tuse.startsWith('/')) ++nstep;
        }
    }
    return nstep;
}

bool PartMan::preparePartitions()
{
    proc.log(__PRETTY_FUNCTION__);

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
                if (!twit->usefor.isEmpty()) listToUnmount << twit->device;
            }
        } else {
            // Clearing the drive, so mark all partitions on the drive for unmounting.
            listToUnmount << proc.execOutLines("lsblk -nro name /dev/" + drvit->device, true);
            // See if GPT needs to be used (either UEFI or >=2TB drive)
            drvit->flags.useGPT = (gptoverride || uefi || drvit->size >= 2199023255552 || partCount > 4);
        }
    }
    for (const QString &strdev : listToUnmount) {
        proc.exec("swapoff /dev/" + strdev, true);
        proc.exec("/bin/umount /dev/" + strdev, true);
    }

    // Prepare partition tables on devices which will have a new layout.
    for (int ixi = 0; ixi < root.childCount(); ++ixi) {
        DeviceItem *drvit = root.child(ixi);
        if (drvit->flags.oldLayout || drvit->type == DeviceItem::VirtualDevices) continue;
        proc.status(tr("Preparing partition tables"));

        // Wipe the first and last 4MB to clear the partition tables, turbo-nuke style.
        const long long offset = (drvit->size / 65536) - 63; // Account for integer rounding.
        const QString &cmd = QStringLiteral("dd conv=notrunc bs=64K count=64 if=/dev/zero of=/dev/") + drvit->device;
        // First 17KB = primary partition table (accounts for both MBR and GPT disks).
        // First 17KB, from 32KB = sneaky iso-hybrid partition table (maybe USB with an ISO burned onto it).
        proc.exec(cmd);
        // Last 17KB = secondary partition table (for GPT disks).
        proc.exec(cmd + " seek=" + QString::number(offset));
        if (!proc.exec("parted -s /dev/" + drvit->device + " mklabel "
            + (drvit->flags.useGPT ? "gpt" : "msdos"))) return false;
    }
    proc.sleep(1000);

    // Prepare partition tables, creating tables and partitions when needed.
    proc.status(tr("Preparing required partitions"));
    for (int ixi = 0; ixi < root.childCount(); ++ixi) {
        DeviceItem *drvit = root.child(ixi);
        if (drvit->type == DeviceItem::VirtualDevices) continue;
        const int devCount = drvit->childCount();
        const bool useGPT = drvit->flags.useGPT;
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
                if (!proc.exec(cmd.arg(devsplit.at(0), devsplit.at(1), ptype))) return false;
                proc.sleep(1000);
                proc.status();
            }
        } else {
            // Creating new partitions.
            const QString cmdParted("parted -s --align optimal /dev/" + drvit->device + " mkpart primary %1MiB %2MiB");
            long long start = 1; // start with 1 MB to aid alignment
            for (int ixdev = 0; ixdev<devCount; ++ixdev) {
                DeviceItem *twit = drvit->child(ixdev);
                const long long end = start + twit->size / 1048576;
                const bool rc = proc.exec(cmdParted.arg(QString::number(start), QString::number(end)));
                if (!rc) return false;
                start = end;
                proc.sleep(1000);
                proc.status();
            }
        }
        // Partition flags.
        for (int ixdev=0; ixdev<devCount; ++ixdev) {
            DeviceItem *twit = drvit->child(ixdev);
            if (twit->usefor.isEmpty()) continue;
            QStringList devsplit(DeviceItem::split(twit->device));
            QString cmd = "parted -s /dev/" + devsplit.at(0) + " set " + devsplit.at(1);
            bool ok = true;
            if (twit->isActive()) {
                if (!useGPT) ok = proc.exec(cmd + " boot on");
                else ok = proc.exec(cmd + " legacy_boot on");
            }
            if (!ok) return false;
            proc.sleep(1000);
            proc.status();
        }
    }
    proc.exec("partprobe -s", true);
    proc.sleep(1000);
    return true;
}

bool PartMan::formatPartitions()
{
    proc.log(__PRETTY_FUNCTION__);

    const QByteArray &encPass = (gui.radioEntireDisk->isChecked()
        ? gui.textCryptoPass : gui.textCryptoPassCust)->text().toUtf8();

    // Format partitions.
    for (DeviceItemIterator it(*this); DeviceItem *twit = *it; it.next()) {
        if (twit->type != DeviceItem::Partition || !twit->willFormat()) continue;
        const QString &dev = twit->mappedDevice(true);
        const QString &useFor = twit->realUseFor();
        if (twit->encrypt) {
            proc.status(tr("Creating encrypted volume: %1").arg(twit->device));
            if (!luksFormat(twit->device, encPass)) return false;
            proc.status();
            if (!luksOpen(twit->device, twit->mappedDevice(false), encPass)) return false;
        }
        const QString &fmtstatus = tr("Formatting: %1");
        if (useFor == "FORMAT") proc.status(fmtstatus.arg(dev));
        else proc.status(fmtstatus.arg(twit->shownUseFor()));
        if (useFor == "ESP") {
            QString cmd("mkfs.msdos -F " + twit->format.mid(3));
            if (twit->chkbadblk) cmd.append(" -c");
            if (!twit->label.isEmpty()) cmd += " -n \"" + twit->label.trimmed().left(11) + '\"';
            if (!proc.exec(cmd + ' ' + dev)) return false;
            // Sets boot flag and ESP flag.
            const QStringList &devsplit = DeviceItem::split(dev);
            if (!proc.exec("parted -s /dev/" + devsplit.at(0)
                + " set " + devsplit.at(1) + " esp on")) return false;
        } else if (useFor == "BIOS-GRUB") {
            proc.exec("dd bs=64K if=/dev/zero of=" + dev);
            const QStringList &devsplit = DeviceItem::split(dev);
            if (!proc.exec("parted -s /dev/" + devsplit.at(0)
                + " set " + devsplit.at(1) + " bios_grub on")) return false;
        } else if (useFor == "SWAP") {
            QString cmd("/sbin/mkswap " + dev);
            if (!twit->label.isEmpty()) cmd.append(" -L \"" + twit->label + '"');
            if (!proc.exec(cmd, true)) return false;
        } else {
            if (!formatLinuxPartition(dev, twit->format, twit->chkbadblk, twit->label)) return false;
        }
        proc.sleep(1000);
    }
    // Prepare subvolumes on all that (are to) contain them.
    for (DeviceItemIterator it(*this); *it; it.next()) {
        if ((*it)->type != DeviceItem::Partition) continue;
        else if (!prepareSubvolumes(*it)) return false;
    }

    return true;
}

// Transplanted straight from minstall.cpp
bool PartMan::formatLinuxPartition(const QString &devpath, const QString &format, bool chkBadBlocks, const QString &label)
{
    QString cmd;
    if (format == "reiserfs") {
        cmd = "mkfs.reiserfs -q";
    } else if (format == "btrfs") {
        // btrfs and set up fsck
        proc.exec("/bin/cp -fp /bin/true /sbin/fsck.auto");
        // set creation options for small drives using btrfs
        QString size_str = proc.execOut("/sbin/sfdisk -s " + devpath);
        long long size = size_str.toLongLong();
        size = size / 1024; // in MiB
        // if drive is smaller than 6GB, create in mixed mode
        if (size < 6000) {
            cmd = "mkfs.btrfs -f -M -O skinny-metadata";
        } else {
            cmd = "mkfs.btrfs -f";
        }
    } else if (format == "xfs" || format == "f2fs") {
        cmd = "mkfs." + format + " -f";
    } else { // jfs, ext2, ext3, ext4
        cmd = "mkfs." + format;
        if (format == "jfs") cmd.append(" -q");
        else cmd.append(" -F");
        if (chkBadBlocks) cmd.append(" -c");
    }

    cmd.append(" " + devpath);
    if (!label.isEmpty()) {
        if (format == "reiserfs" || format == "f2fs") cmd.append(" -l \"");
        else cmd.append(" -L \"");
        cmd.append(label + "\"");
    }
    if (!proc.exec(cmd)) return false;

    if (format.startsWith("ext")) {
        // ext4 tuning
        proc.exec("/sbin/tune2fs -c0 -C0 -i1m " + devpath);
    }
    return true;
}

bool PartMan::prepareSubvolumes(DeviceItem *partit)
{
    const int svcount = partit->childCount();
    QStringList svlist;
    for (int ixi = 0; ixi < svcount; ++ixi) {
        DeviceItem *svit = partit->child(ixi);
        if (svit->willFormat()) svlist << svit->label;
    }

    if (svlist.isEmpty()) return true;
    proc.status(tr("Preparing subvolumes"));
    svlist.sort(); // This ensures nested subvolumes are created in the right order.
    bool ok = true;
    mkdir("/mnt/btrfs-scratch", 0755);
    if (!proc.exec("mount -o noatime " + partit->mappedDevice(true)
        + " /mnt/btrfs-scratch", true)) return false;
    for (const QString &subvol : svlist) {
        proc.exec("btrfs subvolume delete /mnt/btrfs-scratch/" + subvol, true);
        if (!proc.exec("btrfs subvolume create /mnt/btrfs-scratch/" + subvol, true)) ok = false;
        if (!ok) break;
        proc.status();
    }
    if (!proc.exec("umount /mnt/btrfs-scratch", true)) return false;
    return ok;
}

// write out crypttab if encrypting for auto-opening
bool PartMan::fixCryptoSetup(const QString &keyfile, bool isNewKey)
{
    QFile file("/mnt/antiX/etc/crypttab");
    if (!file.open(QIODevice::WriteOnly)) return false;
    QTextStream out(&file);
    const QLineEdit *passedit = gui.radioEntireDisk->isChecked()
        ? gui.textCryptoPass : gui.textCryptoPassCust;
    const QByteArray password(passedit->text().toUtf8());
    const QString cmdAddKey("cryptsetup luksAddKey /dev/%1 /mnt/antiX" + keyfile);
    const QString cmdBlkID("blkid -s UUID -o value /dev/");
    // Find the file system device which contains the key
    QString keyMount('/'); // If no mount point matches, it's in a directory on the root.
    const bool noKey = keyfile.isEmpty();
    if (!noKey) {
        for (const auto &it : mounts.toStdMap()) {
            if (keyfile.startsWith(it.first+'/')) keyMount = it.first;
        }
    }
    // Find extra devices
    QMap<QString, QString> extraAdd;
    for (DeviceItemIterator it(*this); DeviceItem *item = *it; it.next()) {
        if (item->flags.autoCrypto) extraAdd.insert(item->device, item->mappedDevice());
    }
    // File systems
    for (auto &it : mounts.toStdMap()) {
        if (!it.second->encrypt) continue;
        const QString &dev = it.second->device;
        QString uuid = proc.execOut(cmdBlkID + dev);
        out << it.second->mappedDevice(false) << " /dev/disk/by-uuid/" << uuid;
        if (noKey || it.first == keyMount) out << " none";
        else {
            if (isNewKey) {
                if (!proc.exec(cmdAddKey.arg(dev), true, &password)) return false;
            }
            out << ' ' << keyfile;
        }
        out << " luks\n";
        extraAdd.remove(dev);
    }
    // Extra devices
    for (auto &it : extraAdd.toStdMap()) {
        QString uuid = proc.execOut(cmdBlkID + it.first);
        out << it.second << " /dev/disk/by-uuid/" << uuid;
        if (noKey) out << " none";
        else {
            if (isNewKey) {
                if (!proc.exec(cmdAddKey.arg(it.first), true, &password)) return false;
            }
            out << ' ' << keyfile;
        }
        out << " luks\n";
    }
    // Update cryptdisks if the key is not in the root file system.
    if (keyMount != '/') {
        if (!proc.exec("sed -i 's/^CRYPTDISKS_MOUNT.*$/CRYPTDISKS_MOUNT=\"\\" + keyMount
            + "\"/' /mnt/antiX/etc/default/cryptdisks", false)) return false;
    }
    file.close();
    return true;
}

// create /etc/fstab file
bool PartMan::makeFstab(bool populateMediaMounts)
{
    QFile file("/mnt/antiX/etc/fstab");
    if (!file.open(QIODevice::WriteOnly)) return false;
    QTextStream out(&file);
    out << "# Pluggable devices are handled by uDev, they are not in fstab\n";
    const QString cmdBlkID("blkid -o value UUID -s UUID ");
    // File systems and swap space.
    for (auto &it : mounts.toStdMap()) {
        const DeviceItem *twit = it.second;
        const QString &dev = twit->mappedDevice(true);
        qDebug() << "Creating fstab entry for:" << it.first << dev;
        // Device ID or UUID
        if (twit->willMap()) out << dev;
        else out << "UUID=" + proc.execOut(cmdBlkID + dev);
        // Mount point, file system
        QString fsfmt = proc.execOut("blkid " + dev + " -o value -s TYPE");
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
        const QString espdev = "/dev/" + gui.comboBoot->currentData().toString();
        const QString espdevUUID = "UUID=" + proc.execOut(cmdBlkID + espdev);
        qDebug() << "espdev" << espdev << espdevUUID;
        out << espdevUUID + " /boot/efi vfat noatime,dmask=0002,fmask=0113 0 0\n";
    }
    file.close();
    if (populateMediaMounts) {
        if (!proc.exec("/sbin/make-fstab -O --install=/mnt/antiX"
            " --mntpnt=/media")) return false;
    }
    return true;
}

bool PartMan::mountPartitions()
{
    proc.log(__PRETTY_FUNCTION__);
    for (auto &it : mounts.toStdMap()) {
        if (it.first.at(0) != '/') continue;
        const QString point("/mnt/antiX" + it.first);
        const QString &dev = it.second->mappedDevice(true);
        proc.status(tr("Mounting: %1").arg(dev));
        if (!proc.mkpath(point)) return false;
        if (it.first == "/boot") {
             // needed to run fsck because sfdisk --part-type can mess up the partition
            if (!proc.exec("fsck.ext4 -y " + dev)) return false;
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
        if (!opts.contains("noatime")) opts << "noatime";
        const QString &cmd = QString("/bin/mount %1 %2 -o %3").arg(dev, point, opts.join(','));
        if (!proc.exec(cmd)) return false;
    }
    return true;
}

void PartMan::unmount()
{
    QMapIterator<QString, DeviceItem *> it(mounts);
    it.toBack();
    while (it.hasPrevious()) {
        it.previous();
        if (it.key().at(0) != '/') continue;
        DeviceItem *twit = it.value();
        if (!it.key().startsWith("SWAP")) {
            proc.exec("swapoff " + twit->mappedDevice(true), true);
        }
        proc.exec("/bin/umount -l /mnt/antiX" + it.key(), true);
        if (twit->encrypt) {
            QString cmd("cryptsetup close %1");
            proc.exec(cmd.arg(twit->mappedDevice(twit)), true);
        }
    }
}

// Public properties
bool PartMan::willFormat(const QString &point)
{
    DeviceItem *twit = mounts.value(point);
    if (twit) return twit->willFormat();
    return false;
}

QString PartMan::getMountDev(const QString &point, const bool mapped)
{
    const DeviceItem *twit = mounts.value(point);
    if (!twit) return QString();
    if (mapped) return twit->mappedDevice(true);
    QString rstr("/dev/");
    if (twit->type == DeviceItem::VirtualBD) rstr.append("mapper/");
    return rstr.append(twit->device);
}

int PartMan::swapCount()
{
    int count = 0;
    for (const auto &mount : mounts.toStdMap()) {
        if (mount.first.startsWith("SWAP")) ++count;
    }
    return count;
}

int PartMan::isEncrypt(const QString &point)
{
    int count = 0;
    if (point.isEmpty()) {
        for (DeviceItem *twit : mounts) {
            if (twit->encrypt) ++count;
        }
    } else if (point == "SWAP") {
        for (const auto &mount : mounts.toStdMap()) {
            if (mount.first.startsWith("SWAP") && mount.second->encrypt) ++count;
        }
    } else {
        const DeviceItem *twit = mounts.value(point);
        if (twit && twit->encrypt) ++count;
    }
    return count;
}

DeviceItem *PartMan::findOrigin(const QString &vdev)
{
    QStringList lines = proc.execOutLines("cryptsetup status " + vdev, true);
    // Find the associated partition and decrement its reference count if found.
    for (const QString &line : lines) {
        const QString &trline = line.trimmed();
        if (trline.startsWith("device:")) {
            const QString &trdev = trline.mid(trline.lastIndexOf('/') + 1);
            for (DeviceItemIterator it(*this); *it; it.next()) {
                if ((*it)->device == trdev) return *it;
            }
            return nullptr;
        }
    }
    return nullptr;
}

DeviceItem *PartMan::findDevice(const QString &devname) const
{
    for (DeviceItemIterator it(*this); *it; it.next()) {
        if ((*it)->device == devname) return *it;
    }
    return nullptr;
}

/* Model View Controller */

QVariant PartMan::data(const QModelIndex &index, int role) const
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
            if (item->usefor.isEmpty()) return item->curFormat;
            else return item->shownFormat(item->format);
            break;
        case Options:
            if (item->canMount() || item->realUseFor() == "SWAP") return item->options;
            else if (!isDriveOrVD) return "--------";
            break;
        case Pass:
            if (item->canMount()) return item->pass;
            else if (!isDriveOrVD) return "--";
            break;
        }
    } else if (role == Qt::ToolTipRole) {
        switch (index.column()) {
        case Device: if (!item->model.isEmpty()) return tr("Model: %1").arg(item->model); break;
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
        if (item->type == DeviceItem::Drive && !item->flags.oldLayout) {
            return QIcon(":/appointment-soon");
        } else if (item->type == DeviceItem::VirtualBD && item->flags.cryptoV) {
            return QIcon::fromTheme("unlock");
        } else if (item->mapCount) {
            return QIcon::fromTheme("lock");
        }
    } else if (role == Qt::FontRole && index.column() == Device) {
        QFont font;
        font.setItalic(item->isActive());
        return font;
    }
    return QVariant();
}
bool PartMan::setData(const QModelIndex &index, const QVariant &value, int role)
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
Qt::ItemFlags PartMan::flags(const QModelIndex &index) const
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
        if (item->allowedUsesFor().count() > 1) flagsOut |= Qt::ItemIsEditable;
        break;
    case Label:
        if (item->type == DeviceItem::Subvolume) {
            if (item->format != "PRESERVE") flagsOut |= Qt::ItemIsEditable;
        } else {
            if (!item->usefor.isEmpty()) flagsOut |= Qt::ItemIsEditable;
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
        if (item->canMount() || item->realUseFor() == "SWAP") flagsOut |= Qt::ItemIsEditable;
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
QVariant PartMan::headerData(int section, Qt::Orientation orientation, int role) const
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
QModelIndex PartMan::index(DeviceItem *item) const
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
QModelIndex PartMan::parent(const QModelIndex &index) const
{
    if (!index.isValid()) return QModelIndex();
    DeviceItem *cit = static_cast<DeviceItem *>(index.internalPointer());
    DeviceItem *pit = cit->parentItem;
    if (!pit || pit == &root) return QModelIndex();
    return createIndex(pit->row(), 0, pit);
}
inline DeviceItem *PartMan::item(const QModelIndex &index) const
{
    return static_cast<DeviceItem *>(index.internalPointer());
}
int PartMan::rowCount(const QModelIndex &parent) const
{
    if (parent.column() > 0) return 0;
    if (parent.isValid()) {
        return static_cast<DeviceItem *>(parent.internalPointer())->childCount();
    }
    return root.childCount();
}

bool PartMan::changeBegin(DeviceItem *item)
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
int PartMan::changeEnd(bool notify)
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
void PartMan::notifyChange(class DeviceItem *item, int first, int last)
{
    if (first < 0) first = 0;
    if (last < 0) last = _TreeColumns_ - 1;
    const int row = item->row();
    emit dataChanged(createIndex(row, first, item), createIndex(row, last, item));
}

/* Model element */

DeviceItem::DeviceItem(enum DeviceType type, DeviceItem *parent, DeviceItem *preceding)
    : parentItem(parent), type(type)
{
    if (type == Partition) size = 1048576;
    if (parent) {
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
    for (DeviceItem *cit : children) {
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
void DeviceItem::clear()
{
    const int chcount = children.count();
    if (partman && chcount > 0) partman->beginRemoveRows(partman->index(this), 0, chcount - 1);
    for (DeviceItem *cit : children) {
        cit->partman = nullptr; // Stop unnecessary signals.
        cit->parentItem = nullptr; // Stop double deletes.
        delete cit;
    }
    children.clear();
    active = nullptr;
    flags.oldLayout = false;
    if (partman && chcount > 0) partman->endRemoveRows();
}
inline int DeviceItem::row() const
{
    return parentItem ? parentItem->children.indexOf(const_cast<DeviceItem *>(this)) : 0;
}
inline DeviceItem *DeviceItem::parent() const
{
    if (parentItem && !parentItem->parentItem) return nullptr; // Invisible root
    return parentItem;
}
inline DeviceItem *DeviceItem::child(int row) const
{
    if (row < 0 || row >= children.count()) return nullptr;
    return children.at(row);
}
inline int DeviceItem::indexOfChild(DeviceItem *child)
{
    return children.indexOf(child);
}
inline int DeviceItem::childCount() const
{
    return children.count();
}
void DeviceItem::sortChildren()
{
    auto cmp = [](DeviceItem *l, DeviceItem *r) {
        if (l->order != r->order) return l->order < r->order;
        return l->device < r->device;
    };
    std::sort(children.begin(), children.end(), cmp);
    if (partman) {
        for (DeviceItem *c : children) partman->notifyChange(c);
    }
}
/* Helpers */
QString DeviceItem::realUseFor(const QString &use)
{
    if (use == "root") return QStringLiteral("/");
    else if (use == "boot") return QStringLiteral("/boot");
    else if (use == "home") return QStringLiteral("/home");
    else if (!use.startsWith('/')) return use.toUpper();
    else return use;
}
QString DeviceItem::shownUseFor() const
{
    const QString &use = realUseFor();
    if (use == "/") return "/ (root)";
    else if (use == "ESP") return qApp->tr("EFI System Partition");
    else if (use == "SWAP") return qApp->tr("swap space");
    else if (use == "FORMAT") return qApp->tr("format only");
    return use;
}
void DeviceItem::setActive(bool boot)
{
    if (!parentItem) return;
    if (partman && parentItem->active != this) {
        if (parentItem->active) partman->notifyChange(parentItem->active);
    }
    parentItem->active = boot ? this : nullptr;
    if (partman) partman->notifyChange(this);
}
inline bool DeviceItem::isActive() const
{
    if (!parentItem) return false;
    return (parentItem->active == this);
}
bool DeviceItem::isLocked() const
{
    const int partCount = children.count();
    for (int ixPart = 0; ixPart < partCount; ++ixPart) {
        if (children.at(ixPart)->isLocked()) return true;
    }
    return (mapCount != 0);
}
inline bool DeviceItem::willFormat() const
{
    return format != "PRESERVE" && !usefor.isEmpty();
}
bool DeviceItem::canEncrypt() const
{
    if (type != Partition) return false;
    const QString &use = realUseFor();
    return !(use.isEmpty() || use == "ESP" || use == "BIOS-GRUB" || use == "/boot");
}
QString DeviceItem::mappedDevice(const bool full) const
{
    const DeviceItem *twit = this;
    if (twit->type == Subvolume) twit = twit->parentItem;
    if (twit->type == Partition) {
        const QVariant &d = twit->devMapper;
        if (!d.isNull()) {
            if (full) return "/dev/mapper/" + d.toString();
            return d.toString();
        }
    }
    if (!full) return twit->device;
    return "/dev/" + twit->device;
}
inline bool DeviceItem::willMap() const
{
    if (type == Drive || type == VirtualDevices) return false;
    return !devMapper.isEmpty();
}
QString DeviceItem::shownDevice() const
{
    if (type == Subvolume) return parentItem->device + '[' + label + ']';
    return device;
}
QStringList DeviceItem::allowedUsesFor(bool real) const
{
    if (!isVolume()) return QStringList();
    QStringList list;
    if (!partman || size >= partman->rootSpaceNeeded) list << "root";
    if (type == Subvolume) list << "home"; // swap requires Linux 5.0 or later
    else {
        list.prepend("Format");
        if (type != VirtualBD) {
            if (size <= 16777216) list << "BIOS-GRUB";
            if (size <= 8589934592) list << "ESP" << "boot";
        }
        list << "swap" << "home";
    }
    if (real) {
        for(QString &use : list) use = realUseFor(use);
    }
    return list;
}
QStringList DeviceItem::allowedFormats() const
{
    if (!isVolume()) return QStringList();
    QStringList list;
    bool allowPreserve = false, selPreserve = false;
    if (type == Subvolume) list.append("CREATE");
    else {
        const QString &use = realUseFor();
        if (use.isEmpty()) return QStringList();
        else if (use == "/boot") list.append("ext4");
        else if (use == "BIOS-GRUB") list.append("GRUB");
        else if (use == "ESP") {
            list.append("FAT32");
            if (size <= 4294901760) list.append("FAT16");
            if (size <= 33553920) list.append("FAT12");
            selPreserve = allowPreserve = (list.contains(curFormat, Qt::CaseInsensitive)
                || !curFormat.compare("VFAT", Qt::CaseInsensitive));
        } else if (use == "SWAP") {
            list.append("SWAP");
            selPreserve = allowPreserve = list.contains(curFormat, Qt::CaseInsensitive);
        } else {
            list << "ext4" << "ext3" << "ext2";
            list << "f2fs" << "jfs" << "xfs" << "btrfs" << "reiserfs";
            if (use != "FORMAT") allowPreserve = list.contains(curFormat, Qt::CaseInsensitive);
            if (use == "/home") selPreserve = allowPreserve;
        }
    }
    if (encrypt) allowPreserve = false;
    if (allowPreserve) {
        if (selPreserve) list.prepend("PRESERVE");
        else list.append("PRESERVE");
    }
    return list;
}
QString DeviceItem::shownFormat(const QString &fmt) const
{
    if (fmt == "CREATE") return qApp->tr("Create");
    else if (fmt != "PRESERVE") return fmt;
    else {
        if (type == Subvolume) return qApp->tr("Preserve");
        else if (realUseFor() != "/") return qApp->tr("Preserve (%1)").arg(curFormat);
        else return qApp->tr("Preserve /home (%1)").arg(curFormat);
    }
}
bool DeviceItem::canMount() const
{
    const QString &use = realUseFor();
    return !(use.isEmpty() || use == "FORMAT" || use == "ESP" || use == "SWAP");
}
long long DeviceItem::driveFreeSpace(bool inclusive) const
{
    const DeviceItem *drvit = parent();
    if (!drvit) drvit = this;
    long long free = drvit->size - (PARTMAN_SAFETY_MB * 1048576);
    for (int ixi = drvit->children.count() - 1; ixi >= 0; --ixi) {
        DeviceItem *partit = drvit->children.at(ixi);
        if (inclusive || partit != this) free -= partit->size;
    }
    return free;
}
/* Convenience */
DeviceItem *DeviceItem::addPart(int defaultMB, const QString &defaultUse, bool crypto)
{
    DeviceItem *partit = new DeviceItem(DeviceItem::Partition, this);
    if (!defaultUse.isEmpty()) partit->usefor = defaultUse;
    partit->size = defaultMB;
    partit->size *= 1048576;
    partit->autoFill();
    if (partit->canEncrypt()) partit->encrypt = crypto;
    if (partman) partman->notifyChange(partit);
    return partit;
}
void DeviceItem::driveAutoSetActive()
{
    if (active) return;
    const int partcount = children.count();
    for (const QString &pref : QStringList({"/boot", "/"})) {
        for (int ixPart = 0; ixPart < partcount; ++ixPart) {
            DeviceItem *partit = child(ixPart);
            if (partit->realUseFor() == pref) {
                partit->setActive(true);
                return;
            } else {
                const int svcount = partit->childCount();
                for (int ixSV = 0; ixSV < svcount; ++ixSV) {
                    if (partit->child(ixSV)->realUseFor() == pref) {
                        partit->setActive(true);
                        return;
                    }
                }
            }
        }
    }
}
void DeviceItem::autoFill(unsigned int changed)
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
            label = partman->defaultLabels.value(use);
        }
        // Automatic default boot device selection
        if ((type != VirtualBD) && (use == "/boot" || use == "/")) {
            if (parentItem) parentItem->driveAutoSetActive();
        }
    }
    if (format.isEmpty()) {
        const QStringList &af = allowedFormats();
        if (!af.isEmpty()) {
            format = af.at(0);
            changed |= (1 << PartMan::Format);
        }
    }
    if (changed & ((1 << PartMan::UseFor) | (1 << PartMan::Format))) {
        // Default options, dump and pass
        if (!(use.isEmpty() || use == "FORMAT" || use == "ESP")) {
            const QString &lformat = format.toLower();
            if (lformat == "reiserfs") {
                options = "noatime,notail";
                pass = 0;
            } else if (lformat == "swap") {
                options = "defaults";
            } else {
                if (use == "/boot" || use == "/") {
                    pass = (format == "btrfs") ? 0 : 1;
                }
                options = "noatime";
                dump = true;
            }
        }
    }
}
void DeviceItem::labelParts()
{
    for (int ixi = childCount() - 1; ixi >= 0; --ixi) {
        DeviceItem *chit = children.at(ixi);
        chit->device = join(device, ixi + 1);
        if (partman) partman->notifyChange(chit, PartMan::Device, PartMan::Device);
    }
    if (partman) partman->resizeColumnsToFit();
}

int DeviceItem::layoutDefault(int rootPercent, bool crypto, bool updateTree)
{
    assert (partman != nullptr);
    if (rootPercent<0) rootPercent = partman->gui.sliderPart->value();
    if (updateTree) clear();
    const long long driveSize = size / 1048576;
    int rootFormatSize = static_cast<int>(driveSize - PARTMAN_SAFETY_MB);

    // Boot partitions.
    if (partman->uefi) {
        if (updateTree) addPart(256, "ESP", crypto);
        rootFormatSize -= 256;
    } else if (driveSize >= 2097152 || partman->gptoverride) {
        if (updateTree) addPart(1, "BIOS-GRUB", crypto);
        rootFormatSize -= 1;
    }
    int rootMinMB = static_cast<int>(partman->rootSpaceNeeded / 1048576);
    const int bootMinMB = static_cast<int>(partman->bootSpaceNeeded / 1048576);
    if (!crypto) rootMinMB += bootMinMB;
    else {
        int bootFormatSize = 512;
        if (bootFormatSize < bootMinMB) bootFormatSize = static_cast<int>(partman->bootSpaceNeeded);
        if (updateTree) addPart(bootFormatSize, "boot", crypto);
        rootFormatSize -= bootFormatSize;
    }
    // Swap space.
    int swapFormatSize = rootFormatSize-rootMinMB;
    struct sysinfo sinfo;
    if (sysinfo(&sinfo) != 0) sinfo.totalram = 2048;
    else sinfo.totalram = (sinfo.totalram / (1048576 * 2)) * 3; // 1.5xRAM
    sinfo.totalram /= 128; ++sinfo.totalram; sinfo.totalram *= 128; // Multiple of 128MB
    if (swapFormatSize > static_cast<int>(sinfo.totalram)) swapFormatSize = static_cast<int>(sinfo.totalram);
    int swapMaxMB = rootFormatSize / (20 * 128); ++swapMaxMB; swapMaxMB *= 128; // 5% root
    if (swapMaxMB > 8192) swapMaxMB = 8192; // 8GB cap for the whole calculation.
    if (swapFormatSize > swapMaxMB) swapFormatSize = swapMaxMB;
    rootFormatSize -= swapFormatSize;
    // Home
    int homeFormatSize = rootFormatSize;
    rootFormatSize = (rootFormatSize * rootPercent) / 100;
    if (rootFormatSize < rootMinMB) rootFormatSize = rootMinMB;
    homeFormatSize -= rootFormatSize;

    if (updateTree) {
        addPart(rootFormatSize, "root", crypto);
        if (swapFormatSize>0) addPart(swapFormatSize, "swap", crypto);
        if (homeFormatSize>0) addPart(homeFormatSize, "home", crypto);
        labelParts();
        driveAutoSetActive();
        partman->treeSelChange();
    }
    return rootFormatSize;
}

// Return block device info that is suitable for a combo box.
void DeviceItem::addToCombo(QComboBox *combo, bool warnNasty) const
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
QStringList DeviceItem::split(const QString &devname)
{
    const QRegularExpression rxdev1("^(?:\\/dev\\/)*(mmcblk.*|nvme.*)p([0-9]*)$");
    const QRegularExpression rxdev2("^(?:\\/dev\\/)*([a-z]*)([0-9]*)$");
    QRegularExpressionMatch rxmatch(rxdev1.match(devname));
    if (!rxmatch.hasMatch()) rxmatch = rxdev2.match(devname);
    QStringList list(rxmatch.capturedTexts());
    if (!list.isEmpty()) list.removeFirst();
    return list;
}
QString DeviceItem::join(const QString &drive, int partnum)
{
    QString name = drive;
    if (name.startsWith("nvme") || name.startsWith("mmcblk")) name += 'p';
    return (name + QString::number(partnum));
}

/* A very slimmed down and non-standard one-way tree iterator. */
DeviceItemIterator::DeviceItemIterator(const PartMan &partman)
{
    if (partman.root.childCount() < 1) return;
    ixParents.push(0);
    pos = partman.root.child(0);
}
void DeviceItemIterator::next()
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
        painter->drawRect(option.rect.adjusted(0, 0, -pen.width(), -pen.width()));
        painter->restore();
    }
}
QSize DeviceItemDelegate::sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    DeviceItem *item = static_cast<DeviceItem*>(index.internalPointer());
    int width = QStyledItemDelegate::sizeHint(option, index).width();
    // In the case of Format and Use For, the cell should accommodate all options in the list.
    const int residue = width - option.fontMetrics.boundingRect(index.data(Qt::DisplayRole).toString()).width();
    switch(index.column()) {
    case PartMan::Format:
        for (const QString &fmt : item->allowedFormats()) {
            const int fw = option.fontMetrics.boundingRect(item->shownFormat(fmt)).width() + residue;
            if (fw > width) width = fw;
        }
        break;
    case PartMan::UseFor:
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
    case PartMan::Size:
        {
            QSpinBox *spin = new QSpinBox(parent);
            spin->setSuffix("MB");
            connect(spin, QOverload<int>::of(&QSpinBox::valueChanged),
                this, &DeviceItemDelegate::spinSizeValueChange);
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
            spin->setRange(0, static_cast<int>(item->driveFreeSpace() / 1048576));
            spin->setValue(item->size / 1048576);
        }
        break;
    case PartMan::UseFor:
        {
            QComboBox *combo = qobject_cast<QComboBox *>(editor);
            combo->clear();
            combo->addItem("");
            combo->addItems(item->allowedUsesFor(false));
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
void DeviceItemDelegate::emitCommit()
{
    emit commitData(qobject_cast<QWidget *>(sender()));
}

void DeviceItemDelegate::partOptionsMenu()
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
        QAction *action = menuTemplates->addAction(tr("Compression (&ZLIB)"));
        action->setData("noatime,compress-force=zlib");
        action = menuTemplates->addAction(tr("Compression (&LZO)"));
        action->setData("noatime,compress-force=lzo");
    }
    menuTemplates->setDisabled(menuTemplates->isEmpty());
    QAction *action = menu->exec(QCursor::pos());
    if (menuTemplates->actions().contains(action)) edit->setText(action->data().toString());
    delete menu;
}

// No setStepType() in Debian Buster.
void DeviceItemDelegate::spinSizeValueChange(int i)
{
    QSpinBox *spin = static_cast<QSpinBox *>(sender());
    if (!spin) return;
    int stepval = 1;
    for (int ixi = 1; ixi <= (i / 10); ixi *= 10) stepval = ixi;
    spin->setSingleStep(stepval);
    spin->blockSignals(false);
}
