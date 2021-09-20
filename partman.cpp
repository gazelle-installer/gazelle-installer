/***************************************************************************
 * Basic partition manager for the installer.
 ***************************************************************************
 *
 *   Copyright (C) 2020-2021 by AK-47
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
#include <QMessageBox>
#include <QMenu>
#include <QSpinBox>
#include <QComboBox>
#include <QLineEdit>
#include <QFormLayout>
#include <QDialogButtonBox>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QPainter>

#include "msettings.h"
#include "partman.h"

#define PARTMAN_SAFETY_MB 32 // 1MB at start + Compensate for rounding errors.

PartMan::PartMan(MProcess &mproc, BlockDeviceList &bdlist, Ui::MeInstall &ui, QWidget *parent)
    : QObject(parent), proc(mproc), listBlkDevs(bdlist), gui(ui), master(parent), model(*this, parent)
{
    QTimer::singleShot(0, this, &PartMan::setup);
}

void PartMan::setup()
{
    gui.treePartitions->setModel(&model);
    gui.treePartitions->setItemDelegate(new DeviceItemDelegate);
    gui.treePartitions->header()->setMinimumSectionSize(5);
    gui.treePartitions->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(gui.treePartitions, &QTreeView::customContextMenuRequested, this, &PartMan::treeMenu);
    connect(gui.treePartitions->selectionModel(), &QItemSelectionModel::selectionChanged, this, &PartMan::treeSelChange);
    connect(gui.pushPartClear, &QToolButton::clicked, this, &PartMan::partClearClick);
    connect(gui.pushPartAdd, &QToolButton::clicked, this, &PartMan::partAddClick);
    connect(gui.pushPartRemove, &QToolButton::clicked, this, &PartMan::partRemoveClick);
    connect(&model, &PartModel::dataChanged, this, &PartMan::treeItemChange);
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
        gui.treePartitions->setColumnHidden(PartModel::Encrypt, true);
    }
}

void PartMan::populate(DeviceItem *drvstart)
{
    if (!drvstart) model.clear();
    DeviceItem *curdrv = nullptr;
    for (const BlockDeviceInfo &bdinfo : listBlkDevs) {
        if (bdinfo.isStart && !brave) continue;
        DeviceItem *curdev = nullptr;
        if (bdinfo.isDrive) {
            if (!drvstart) curdrv = new DeviceItem(DeviceItem::Drive, model);
            else if (bdinfo.name == drvstart->device) curdrv = drvstart;
            else if (!curdrv) continue; // Skip until the drive is drvstart.
            else break; // Exit the loop early if the drive isn't drvstart.
            curdev = curdrv;
            curdev->curLabel = bdinfo.model; // Model
        } else {
            if (!curdrv) continue;
            curdev = new DeviceItem(DeviceItem::Partition, curdrv);
            curdev->curLabel = bdinfo.label;
            curdev->curFormat = bdinfo.fs;
            if (bdinfo.isBoot) curdev->setActive(true);
            if (bdinfo.isESP) curdev->usefor = "ESP";
        }
        assert(curdev != nullptr);
        curdev->flags.oldLayout = true;
        curdev->device = bdinfo.name;
        curdev->size = bdinfo.size;
        curdev->flags.mapLock = (bdinfo.mapCount > 0);
        model.notifyChange(curdev);
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
        for (int ixi = model.count() - 1; ixi >= 0; --ixi) {
            DeviceItem *twit = model.item(ixi);
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
        " TYPE,NAME,UUID,SIZE,PHY-SEC,FSTYPE,LABEL"
        " /dev/mapper/* 2>/dev/null", true);
    const QJsonObject &jsonObjBD = QJsonDocument::fromJson(bdRaw.toUtf8()).object();
    const QJsonArray &jsonBD = jsonObjBD["blockdevices"].toArray();
    if (jsonBD.empty()) {
        if (vdlit) delete vdlit;
        return;
    } else if (!vdlit) {
        vdlit = new DeviceItem(DeviceItem::VirtualDevices, model);
        vdlit->device = tr("Virtual Devices");
        vdlit->flags.oldLayout = true;
        gui.treePartitions->setFirstColumnSpanned(vdlit->row(), QModelIndex(), true);
    }
    assert(vdlit != nullptr);
    for (const QJsonValue &jsonDev : jsonBD) {
        const QString &bdname = jsonDev["name"].toString();
        const long long bdsize = jsonDev["size"].toVariant().toLongLong();
        const QString &bdlabel = jsonDev["label"].toString();
        const bool crypto = jsonDev["type"].toString() == "crypt";
        // Check if the device is already in the list.
        DeviceItem *devit = listed.value(bdname);
        if (devit) {
            if (bdsize != devit->size || bdlabel != devit->curLabel
                || crypto != devit->flags.cryptoV) {
                // List entry is different to the device, so refresh it.
                delete devit;
                devit = nullptr;
            }
            listed.remove(bdname);
        }
        // Create a new list entry if needed.
        if (!devit) {
            devit = new DeviceItem(DeviceItem::VirtualBD, vdlit);
            devit->device = devit->devMapper = bdname;
            devit->size = bdsize;
            devit->curLabel = bdlabel;
            devit->curFormat = jsonDev["fstype"].toString();
            devit->flags.cryptoV = crypto;
            model.notifyChange(devit);
        }
        DeviceItem *orit = findOrigin(bdname);
        if (orit) {
            orit->devMapper = bdname;
            orit->flags.mapLock = true;
            model.notifyChange(orit);
        }
    }
    for (const auto &it : listed.toStdMap()) delete it.second;
    vdlit->sortChildren();
    gui.treePartitions->expand(model.index(vdlit));
}

bool PartMan::manageConfig(MSettings &config, bool save)
{
    const int driveCount = model.count();
    for (int ixDrive = 0; ixDrive < driveCount; ++ixDrive) {
        DeviceItem *drvit = model.item(ixDrive);
        // Check if the drive is to be cleared and formatted.
        int partCount = drvit->childCount();
        bool drvPreserve = drvit->flags.oldLayout;
        const QString &configNewLayout = "Storage/NewLayout." + drvit->device;
        QVariant bootdev;
        if (save) {
            if (!drvPreserve) config.setValue(configNewLayout, partCount);
            if (drvit->active) {
                bootdev = drvit->active->device;
                config.setValue("Boot", bootdev);
            }
        } else if (config.contains(configNewLayout)) {
            drvPreserve = false;
            drvit->clear();
            partCount = config.value(configNewLayout).toInt();
            bootdev = config.value("Boot");
        }
        // Partition configuration.
        const long long sizeMax = drvit->size - (PARTMAN_SAFETY_MB * 1048576);
        long long sizeTotal = 0;
        for (int ixPart = 0; ixPart < partCount; ++ixPart) {
            DeviceItem *partit = nullptr;
            if (save || drvPreserve) {
                partit = drvit->child(ixPart);
                partit->flags.oldLayout = drvPreserve;
            } else {
                partit = new DeviceItem(DeviceItem::Partition, drvit);
                partit->device = BlockDeviceInfo::join(drvit->device, ixPart+1);
            }
            // Configuration management, accounting for automatic control correction order.
            const QString &groupPart = "Storage." + partit->device;
            config.beginGroup(groupPart);
            if (save) {
                config.setValue("Size", partit->size);
                config.setValue("Encrypt", partit->encrypt);
                if (partit->flags.setBoot) config.setValue("Boot", true);
                if (!partit->usefor.isEmpty()) config.setValue("UseFor", partit->usefor);
                if (!partit->format.isEmpty()) config.setValue("Format", partit->format);
                if (!partit->label.isEmpty()) config.setValue("Label", partit->label);
                if (!partit->options.isEmpty()) config.setValue("Options", partit->options);
                config.setValue("Dump", partit->dump);
                config.setValue("Pass", partit->pass);
            } else {
                if (!drvPreserve && config.contains("Size")) {
                    partit->size = config.value("Size").toLongLong();
                    sizeTotal += partit->size;
                    if (sizeTotal > sizeMax) return false;
                    if (config.value("Boot").toBool()) partit->setActive(true);
                }
                partit->usefor = config.value("UseFor").toString();
                partit->format = config.value("Format").toString();
                partit->encrypt = config.value("Encrypt").toBool();
                partit->label = config.value("Label").toString();
                partit->options = config.value("Options").toString();
                partit->dump = config.value("Dump").toBool();
                partit->pass = config.value("Pass").toInt();
            }
            if (bootdev == partit->device) partit->setActive(true);
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
            if (!save) gui.treePartitions->expand(model.index(partit));
        }
    }
    treeSelChange();
    return true;
}

void PartMan::labelParts(DeviceItem *drvit)
{
    for (int ixi = drvit->childCount() - 1; ixi >= 0; --ixi) {
        DeviceItem *chit = drvit->child(ixi);
        chit->device = BlockDeviceInfo::join(drvit->device, ixi + 1);
        model.notifyChange(chit, PartModel::Device, PartModel::Device);
    }
    resizeColumnsToFit();
}

void PartMan::resizeColumnsToFit()
{
    for (int ixi = PartModel::_TreeColumns_ - 1; ixi >= 0; --ixi) {
        gui.treePartitions->resizeColumnToContents(ixi);
    }
}

void PartMan::treeItemChange()
{
    // Encryption and bad blocks controls
    bool canCheck = false;
    bool cryptoAny = false;
    for (DeviceItemIterator it(model); DeviceItem *item = *it; it.next()) {
        if (item->type != DeviceItem::Partition) continue;
        if (item->format != "PRESERVE" && !item->usefor.isEmpty()) {
            if (item->format.startsWith("ext") || item->format == "jfs") canCheck = true;
        }
        if (item->canEncrypt() && item->encrypt) cryptoAny = true;
    }
    if (gui.boxCryptoPass->isEnabled() != cryptoAny) {
        gui.textCryptoPassCust->clear();
        gui.pushNext->setDisabled(cryptoAny);
        gui.boxCryptoPass->setEnabled(cryptoAny);
    }
    gui.checkBadBlocks->setEnabled(canCheck);
    treeSelChange();
}

void PartMan::treeSelChange()
{
    const QModelIndexList &indexes = gui.treePartitions->selectionModel()->selectedIndexes();
    DeviceItem *twit = (indexes.size() > 0) ? model.item(indexes.at(0)) : nullptr;
    if (twit && twit->type != DeviceItem::Subvolume) {
        const bool isdrive = (twit->type == DeviceItem::Drive);
        bool isold = twit->flags.oldLayout;
        bool islocked = true;
        if (isdrive) {
            islocked = twit->isLocked();
            if (twit->childCount() <= 0) isold = false;
        }
        gui.pushPartClear->setEnabled(!islocked);
        gui.pushPartRemove->setEnabled(!isold && !isdrive);
        // Only allow adding partitions if there is enough space.
        DeviceItem *drvit = twit->parent();
        if (!drvit) drvit = twit;
        if (!islocked && isold && isdrive) gui.pushPartAdd->setEnabled(false);
        else if (!isold) {
            long long maxMB = (drvit->size / 1048576) - PARTMAN_SAFETY_MB;
            for (int ixi = drvit->childCount() - 1; ixi >= 0; --ixi) {
                maxMB -= drvit->child(ixi)->size / 1048576;
            }
            gui.pushPartAdd->setEnabled(maxMB > 0);
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
    DeviceItem *twit = model.item(selIndex);
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
            actSetBoot->setChecked(twit->flags.setBoot);
        }
        if (twit->format == "btrfs") {
            menu.addSeparator();
            actNewSubvolume = menu.addAction(tr("New subvolume"));
            actScanSubvols = menu.addAction(tr("Scan subvolumes"));
            actScanSubvols->setDisabled(twit->willFormat());
        }
        if (twit->flags.mapLock && actUnlock) actUnlock->setEnabled(false);
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
        actReset->setDisabled(locked || twit->flags.oldLayout);
        menuTemplates->setDisabled(locked);
        actCrypto->setVisible(gui.boxCryptoPass->isVisible());

        QAction *action = menu.exec(QCursor::pos());
        if (action == actAdd) partAddClick(true);
        else if (action == actClear) partClearClick(true);
        else if (action == actBasic) layoutDefault(twit, -1, false);
        else if (action == actCrypto) layoutDefault(twit, -1, true);
        else if (action == actReset) {
            while (twit->childCount()) delete twit->child(0);
            populate(twit);
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
    DeviceItem *twit = (indexes.size() > 0) ? model.item(indexes.at(0)) : nullptr;
    if (!twit || twit->type != DeviceItem::Drive) return;
    twit->clear();
    treeSelChange();
}

void PartMan::partAddClick(bool)
{
    const QModelIndexList &indexes = gui.treePartitions->selectionModel()->selectedIndexes();
    DeviceItem *twit = (indexes.size() > 0) ? model.item(indexes.at(0)) : nullptr;
    if (!twit) return;
    DeviceItem *drive = twit->parent();
    if (!drive || drive->type == DeviceItem::Unknown) drive = twit;

    DeviceItem *part = new DeviceItem(DeviceItem::Partition, drive, twit);
    labelParts(drive);
    drive->flags.oldLayout = false;
    model.notifyChange(drive);
    gui.treePartitions->selectionModel()->select(model.index(part), QItemSelectionModel::ClearAndSelect);
}

void PartMan::partRemoveClick(bool)
{
    const QModelIndexList &indexes = gui.treePartitions->selectionModel()->selectedIndexes();
    DeviceItem *partit = (indexes.size() > 0) ? model.item(indexes.at(0)) : nullptr;
    if (!partit) return;
    DeviceItem *drvit = partit->parent();
    if (!drvit) return;
    delete partit;
    labelParts(drvit);
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
            twit->flags.mapLock = true;
            model.notifyChange(twit);
            // Refreshing the drive will not necessarily re-scan block devices.
            // This updates the device list manually to keep the tree accurate.
            const int ixBD = listBlkDevs.findDevice(twit->device);
            if (ixBD >= 0) listBlkDevs[ixBD].mapCount++;
            // Update virtual device list.
            scanVirtualDevices(true);
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
        const int ixBD = listBlkDevs.findDevice(origin->device);
        const int oMapCount = listBlkDevs.at(ixBD).mapCount;
        if (oMapCount > 0) listBlkDevs[ixBD].mapCount--;
        if (oMapCount <= 1) origin->flags.mapLock = false;
        origin->devMapper.clear();
        origin->flags.autoCrypto = false;
        model.notifyChange(origin);
    }
    // Refresh virtual devices list.
    scanVirtualDevices(true);
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
        model.notifyChange(svit);
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
    for (DeviceItemIterator it(model); DeviceItem *item = *it; it.next()) {
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
            DeviceItem *pit = item->parent();
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
        if (item->type != DeviceItem::VirtualBD && item->encrypt) {
            if (mount == "/") item->devMapper = "root.fsm";
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
    if (rootitem) {
        if (rootitem->willFormat() && mounts.contains("/home")) {
            const QString errmsg = tr("Cannot preserve /home inside root (/) if"
                " a separate /home partition is also mounted.");
            QMessageBox::critical(master, master->windowTitle(), errmsg);
            return false;
        }
        if (rootitem->encrypt) encryptRoot = true;
    } else {
        const QString &tmin = QLocale::system().formattedDataSize(rootSpaceNeeded + 1048575,
            1, QLocale::DataSizeTraditionalFormat);
        QMessageBox::critical(master, master->windowTitle(), tr("You must choose a root partition.\n"
            "The root partition must be at least %1.").arg(tmin));
        return false;
    }
    if (encryptRoot && !mounts.contains("/boot")) {
        QMessageBox::critical(master, master->windowTitle(),
            tr("You must choose a separate boot partition when encrypting root."));
        return false;
    }

    if (!automatic) {
        // Final warnings before the installer starts.
        QString details, biosgpt;
        for (int ixdrv = 0; ixdrv < model.count(); ++ixdrv) {
            DeviceItem *drvit = model.item(ixdrv);
            const int partCount = drvit->childCount();
            bool setupGPT = gptoverride || drvit->size >= 2199023255552 || partCount > 4;
            if (drvit->flags.oldLayout) {
                setupGPT = listBlkDevs.at(listBlkDevs.findDevice(drvit->device)).isGPT;
            } else if (drvit->type != DeviceItem::VirtualDevices) {
                details += tr("Prepare %1 partition table on %2").arg(setupGPT?"GPT":"MBR", drvit->device) + '\n';
            }
            bool hasBiosGrub = false;
            for (int ixdev = 0; ixdev < partCount; ++ixdev) {
                DeviceItem *partit = drvit->child(ixdev);
                const QString &dev = partit->shownDevice();
                const QString &use = partit->realUseFor();
                if (use.isEmpty()) continue;
                else if (use == "BIOS-GRUB") hasBiosGrub = true;
                if (partit->willFormat()) details += tr("Format %1").arg(dev);
                else if (use == "/") {
                    details += tr("Delete the data on %1 except for /home").arg(dev);
                } else {
                    details += tr("Reuse (no reformat) %1 as %2").arg(dev, partit->shownUseFor());
                }
                details += '\n';
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

    calculatePartBD();
    return true;
}

bool PartMan::calculatePartBD()
{
    listToUnmount.clear();
    const int driveCount = model.count();
    for (int ixDrive = 0; ixDrive < driveCount; ++ixDrive) {
        DeviceItem *drvit = model.item(ixDrive);
        if (drvit->type == DeviceItem::VirtualDevices) continue;
        const bool useExist = drvit->flags.oldLayout;
        QString drv = drvit->device;
        int ixDriveBD = listBlkDevs.findDevice(drv);
        const int partCount = drvit->childCount();
        const long long driveSize = drvit->size / 1048576;

        if (!useExist) {
            // Remove partitions from the list that belong to this drive.
            const int ixRemoveBD = ixDriveBD + 1;
            if (ixDriveBD < 0) return false;
            while (ixRemoveBD < listBlkDevs.size() && !listBlkDevs.at(ixRemoveBD).isDrive) {
                listToUnmount << listBlkDevs.at(ixRemoveBD).name;
                listBlkDevs.removeAt(ixRemoveBD);
            }
            // see if GPT needs to be used (either UEFI or >=2TB drive)
            listBlkDevs[ixDriveBD].isGPT = gptoverride || uefi || driveSize >= 2097152 || partCount > 4;
        }
        // Add (or mark) future partitions to the list.
        for (int ixPart=0, ixPartBD=ixDriveBD+1; ixPart < partCount; ++ixPart, ++ixPartBD) {
            DeviceItem *twit = drvit->child(ixPart);
            if (twit->usefor.isEmpty()) continue;
            QString bdFS;
            if (twit->encrypt) bdFS = QStringLiteral("crypto_LUKS");
            else bdFS = twit->format;
            if (useExist) {
                BlockDeviceInfo &bdinfo = listBlkDevs[ixPartBD];
                listToUnmount << bdinfo.name;
                bdinfo.label = twit->label;
                if (twit->willFormat() && !bdFS.isEmpty()) bdinfo.fs = bdFS;
                bdinfo.isNasty = false; // future partitions are safe
                bdinfo.isFuture = bdinfo.isNative = true;
                bdinfo.isESP = (twit->realUseFor() == "ESP");
            } else {
                const BlockDeviceInfo &drvbd = listBlkDevs.at(ixDriveBD);
                BlockDeviceInfo bdinfo;
                bdinfo.name = twit->device;
                bdinfo.fs = bdFS;
                bdinfo.size = twit->size;
                bdinfo.physec = drvbd.physec;
                bdinfo.isFuture = bdinfo.isNative = true;
                bdinfo.isGPT = drvbd.isGPT;
                bdinfo.isESP = (twit->realUseFor() == "ESP");
                listBlkDevs.insert(ixPartBD, bdinfo);
            }
        }
    }
    return true;
}

// Checks SMART status of the selected drives.
// Returns false if it detects errors and user chooses to abort.
bool PartMan::checkTargetDrivesOK()
{
    QString smartFail, smartWarn;
    for (int ixi = 0; ixi < model.count(); ++ixi) {
        DeviceItem *drvit =  model.item(ixi);
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
    const int ixdev = listBlkDevs.findDevice(dev);
    assert(ixdev >= 0);
    const int physec = listBlkDevs.at(ixdev).physec;
    QString cmd = "cryptsetup --batch-mode --key-size 512 --hash sha512 --pbkdf argon2id";
    if (physec > 0) cmd += " --sector-size=" + QString::number(physec);
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
    int bdindex = listBlkDevs.findDevice(drv);
    if (bdindex<0) return nullptr;
    for (int ixi = 0; ixi < model.count(); ++ixi) {
        DeviceItem *drvit = model.item(ixi);
        if (drvit->device == drv) return drvit;
    }
    return nullptr;
}

void PartMan::clearAllUses()
{
    for (DeviceItemIterator it(model); DeviceItem *item = *it; it.next()) {
        item->usefor.clear();
        if (item->type == DeviceItem::Partition) item->setActive(false);
        model.notifyChange(item);
    }
}

int PartMan::layoutDefault(DeviceItem *drvit,
    int rootPercent, bool crypto, bool updateTree)
{
    if (rootPercent<0) rootPercent = gui.sliderPart->value();
    if (updateTree) drvit->clear();
    const long long driveSize = drvit->size / 1048576;
    int rootFormatSize = static_cast<int>(driveSize - PARTMAN_SAFETY_MB);

    // Boot partitions.
    if (uefi) {
        if (updateTree) drvit->addPart(256, "ESP", crypto);
        rootFormatSize -= 256;
    } else if (driveSize >= 2097152 || gptoverride) {
        if (updateTree) drvit->addPart(1, "BIOS-GRUB", crypto);
        rootFormatSize -= 1;
    }
    int rootMinMB = static_cast<int>(rootSpaceNeeded / 1048576);
    const int bootMinMB = static_cast<int>(bootSpaceNeeded / 1048576);
    if (!crypto) rootMinMB += bootMinMB;
    else {
        int bootFormatSize = 512;
        if (bootFormatSize < bootMinMB) bootFormatSize = static_cast<int>(bootSpaceNeeded);
        if (updateTree) drvit->addPart(bootFormatSize, "boot", crypto);
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
        drvit->addPart(rootFormatSize, "root", crypto);
        if (swapFormatSize>0) drvit->addPart(swapFormatSize, "swap", crypto);
        if (homeFormatSize>0) drvit->addPart(homeFormatSize, "home", crypto);
        labelParts(drvit);
        drvit->driveAutoSetBoot();
        treeSelChange();
    }
    return rootFormatSize;
}

int PartMan::countPrepSteps()
{
    int nstep = 0;
    for (DeviceItemIterator it(model); DeviceItem *item = *it; it.next()) {
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

    // detach all existing partitions on the selected drive
    for (const QString &strdev : listToUnmount) {
        proc.exec("swapoff /dev/" + strdev, true);
        proc.exec("/bin/umount /dev/" + strdev, true);
    }

    // Prepare partition tables on devices which will have a new layout.
    for (int ixi = 0; ixi < model.count(); ++ixi) {
        DeviceItem *drvit = model.item(ixi);
        if (drvit->flags.oldLayout || drvit->type == DeviceItem::VirtualDevices) continue;
        const QString &drv = drvit->device;
        proc.status(tr("Preparing partition tables"));
        const int index = listBlkDevs.findDevice(drv);
        assert (index >= 0);

        // Wipe the first and last 4MB to clear the partition tables, turbo-nuke style.
        const long long bytes = listBlkDevs.at(index).size;
        const long long offset = (bytes / 65536) - 63; // Account for integer rounding.
        const QString &cmd = QStringLiteral("dd conv=notrunc bs=64K count=64 if=/dev/zero of=/dev/") + drv;
        // First 17KB = primary partition table (accounts for both MBR and GPT disks).
        // First 17KB, from 32KB = sneaky iso-hybrid partition table (maybe USB with an ISO burned onto it).
        proc.exec(cmd);
        // Last 17KB = secondary partition table (for GPT disks).
        proc.exec(cmd + " seek=" + QString::number(offset));

        const bool useGPT = listBlkDevs.at(index).isGPT;
        if (!proc.exec("parted -s /dev/" + drv + " mklabel " + (useGPT ? "gpt" : "msdos"))) return false;
    }

    // Prepare partition tables, creating tables and partitions when needed.
    proc.status(tr("Preparing required partitions"));
    for (int ixi = 0; ixi < model.count(); ++ixi) {
        DeviceItem *drvit = model.item(ixi);
        if (drvit->type == DeviceItem::VirtualDevices) continue;
        const QString &drvdev = drvit->device;
        const int devCount = drvit->childCount();
        const int ixBlkDev = listBlkDevs.findDevice(drvdev);
        const bool useGPT = (ixBlkDev >= 0 && listBlkDevs.at(ixBlkDev).isGPT);
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
                const QStringList &devsplit = BlockDeviceInfo::split(twit->device);
                if (!proc.exec(cmd.arg(devsplit.at(0), devsplit.at(1), ptype))) return false;
                proc.status();
            }
        } else {
            // Creating new partitions.
            const QString cmdParted("parted -s --align optimal /dev/" + drvdev + " mkpart primary %1MiB %2MiB");
            long long start = 1; // start with 1 MB to aid alignment
            for (int ixdev = 0; ixdev<devCount; ++ixdev) {
                DeviceItem *twit = drvit->child(ixdev);
                const long long end = start + twit->size / 1048576;
                const bool rc = proc.exec(cmdParted.arg(QString::number(start), QString::number(end)));
                if (!rc) return false;
                start = end;
                proc.status();
            }
        }
        // Partition flags.
        for (int ixdev=0; ixdev<devCount; ++ixdev) {
            DeviceItem *twit = drvit->child(ixdev);
            if (twit->usefor.isEmpty()) continue;
            QStringList devsplit(BlockDeviceInfo::split(twit->device));
            QString cmd = "parted -s /dev/" + devsplit.at(0) + " set " + devsplit.at(1);
            bool ok = true;
            if (twit->flags.setBoot) {
                if (!useGPT) ok = proc.exec(cmd + " boot on");
                else ok = proc.exec(cmd + " legacy_boot on");
            }
            if (!ok) return false;
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
    const bool badblocks = gui.checkBadBlocks->isChecked();

    for (DeviceItemIterator it(model); DeviceItem *twit = *it; it.next()) {
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
            if (!proc.exec("mkfs.msdos -F " + twit->format.mid(3)+' '+dev)) return false;
            // Sets boot flag and ESP flag.
            const QStringList &devsplit = BlockDeviceInfo::split(dev);
            if (!proc.exec("parted -s /dev/" + devsplit.at(0)
                + " set " + devsplit.at(1) + " esp on")) return false;
        } else if (useFor == "BIOS-GRUB") {
            proc.exec("dd bs=64K if=/dev/zero of=" + dev);
            const QStringList &devsplit = BlockDeviceInfo::split(dev);
            if (!proc.exec("parted -s /dev/" + devsplit.at(0)
                + " set " + devsplit.at(1) + " bios_grub on")) return false;
        } else if (useFor == "SWAP") {
            QString cmd("/sbin/mkswap " + dev);
            if (!twit->label.isEmpty()) cmd.append(" -L \"" + twit->label + '"');
            if (!proc.exec(cmd, true)) return false;
        } else {
            if (!formatLinuxPartition(dev, twit->format, badblocks, twit->label)) return false;
        }
    }
    // Prepare subvolumes on all that (are to) contain them.
    for (DeviceItemIterator it(model); *it; it.next()) {
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
    for (DeviceItemIterator it(model); DeviceItem *item = *it; it.next()) {
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
            for (DeviceItemIterator it(model); *it; it.next()) {
                if ((*it)->device == trdev) return *it;
            }
            return nullptr;
        }
    }
    return nullptr;
}

/* Model View Control */

DeviceItem::DeviceItem(enum DeviceType type, DeviceItem *parent, DeviceItem *preceding)
    : parentItem(parent), type(type)
{
    if (parent) {
        model = parentItem->model;
        const int i = preceding ? (parentItem->children.indexOf(preceding) + 1) : parentItem->childCount();
        if (model) {
            model->beginInsertRows(model->createIndex(parentItem->row(), 0, parentItem), i, i);
        }
        parentItem->children.insert(i, this);
        if (model) model->endInsertRows();
    }
}
DeviceItem::DeviceItem(enum DeviceType type, PartModel &container, DeviceItem *preceding)
    : parentItem(container.root), model(&container), type(type)
{
    DeviceItem *root = container.root;
    if (root) {
        const int i = preceding ? (root->children.indexOf(preceding) + 1) : root->childCount();
        container.beginInsertRows(QModelIndex(), i, i);
        root->children.insert(i, this);
        container.endInsertRows();
    }
}
DeviceItem::~DeviceItem()
{
    if (parentItem && model) {
        const int r = parentItem->indexOfChild(this);
        model->beginRemoveRows(model->createIndex(parentItem->row(), 0, parentItem), r, r);
    }
    for (DeviceItem *cit : children) {
        cit->model = nullptr; // Stop unnecessary signals.
        cit->parentItem = nullptr; // Stop double deletes.
        delete cit;
    }
    children.clear();
    if (parentItem) {
        if (flags.setBoot) parentItem->active = nullptr;
        parentItem->children.removeAll(this);
        if (model) {
            model->endRemoveRows();
            model->notifyChange(parentItem);
        }
    }
}
void DeviceItem::clear()
{
    if (model) model->beginRemoveRows(model->createIndex(0, 0, this), 0, children.count()-1);
    for (DeviceItem *cit : children) {
        cit->model = nullptr; // Stop unnecessary signals.
        cit->parentItem = nullptr; // Stop double deletes.
        delete cit;
    }
    children.clear();
    active = nullptr;
    flags.oldLayout = false;
    if (model) model->endRemoveRows();
}
int DeviceItem::row() const
{
    return parentItem ? parentItem->children.indexOf(const_cast<DeviceItem *>(this)) : 0;
}
DeviceItem *DeviceItem::parent() const
{
    return parentItem;
}
DeviceItem *DeviceItem::child(int row)
{
    if (row < 0 || row >= children.count()) return nullptr;
    return children.at(row);
}
int DeviceItem::indexOfChild(DeviceItem *child)
{
    return children.indexOf(child);
}
int DeviceItem::childCount() const
{
    return children.count();
}
void DeviceItem::sortChildren()
{
    auto cmp = [](DeviceItem *l, DeviceItem *r) {
        return l->device < r->device;
    };
    std::sort(children.begin(), children.end(), cmp);
    if (model) {
        for (DeviceItem *c : children) model->notifyChange(c);
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
    DeviceItem *const drvit = parent();
    if (!drvit) return;
    const int partCount = drvit->childCount();
    const int ixThis = drvit->indexOfChild(this);
    for (int ixPart = 0; ixPart < partCount; ++ixPart) {
        if (ixPart == ixThis) continue;
        DeviceItem *chit = drvit->child(ixPart);
        if (chit->flags.setBoot) {
            chit->flags.setBoot = false;
            if (model) model->notifyChange(chit);
        }
    }
    DeviceItem *const curActive = drvit->active;
    drvit->active = boot ? this : nullptr;
    flags.setBoot = boot;
    if (model && curActive != drvit->active) model->notifyChange(this);
}
bool DeviceItem::isLocked() const
{
    const int partCount = children.count();
    for (int ixPart = 0; ixPart < partCount; ++ixPart) {
        if (children.at(ixPart)->isLocked()) return true;
    }
    return flags.mapLock;
}
bool DeviceItem::willFormat() const
{
    return usefor != "PRESERVE" && !usefor.isEmpty();
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
    if (twit->type == Subvolume) twit = twit->parent();
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
    QStringList list;
    if (type == Subvolume) list << "root" << "home"; // swap requires Linux 5.0 or later
    else {
        list << "Format";
        if (type != VirtualBD) {
            if (size <= 16777216) list << "BIOS-GRUB";
            if (size <= 4294967296) list << "ESP" << "boot";
        }
        list << "root" << "swap" << "home";
    }
    if (real) {
        for(QString &use : list) use = realUseFor(use);
    }
    return list;
}
QStringList DeviceItem::allowedFormats() const
{
    QStringList list;
    bool allowPreserve = false, selPreserve = false;
    if (type == Subvolume) list << "CREATE";
    else {
        const QString &use = realUseFor();
        if (use == "/boot") list << "ext4";
        else if (use == "BIOS-GRUB") list << "GRUB";
        else if (use == "ESP") {
            list << "FAT32" << "FAT16" << "FAT12";
            selPreserve = allowPreserve = (list.contains(curFormat, Qt::CaseInsensitive)
                || !curFormat.compare("VFAT", Qt::CaseInsensitive));
        } else if (use == "SWAP") {
            list << "SWAP";
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
bool DeviceItem::isVolume() const
{
    return (type == Partition || type == VirtualBD);
}
bool DeviceItem::canMount() const
{
    const QString &use = realUseFor();
    return !(use.isEmpty() || use == "FORMAT" || use == "ESP" || use == "SWAP");
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
    if (model) model->notifyChange(partit);
    return partit;
}
void DeviceItem::driveAutoSetBoot()
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
    if (changed & (1 << PartModel::UseFor)) {
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
        } else if (model) {
            label = model->partman.defaultLabels.value(use);
        }
        // Automatic default boot device selection
        if ((type != VirtualBD) && (use == "/boot" || use == "/")) {
            if (parentItem) parentItem->driveAutoSetBoot();
        }
    }
    if (format.isEmpty()) {
        const QStringList &af = allowedFormats();
        if (!af.isEmpty()) {
            format = af.at(0);
            changed |= (1 << PartModel::Format);
        }
    }
    if (changed & (1 << PartModel::Format)) {
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

/* A very slimmed down and non-standard one-way tree iterator. */
DeviceItemIterator::DeviceItemIterator(PartModel &model) : pos(model.item(-1))
{
    next();
}
void DeviceItemIterator::next()
{
    if (!pos) return;
    if (pos->childCount()) {
        ixParents.push(ixPos);
        ixPos = 0;
        pos = pos->child(0);
    } else {
        DeviceItem *parent = pos->parent();
        if (!parent) {
            pos = nullptr;
            return;
        }
        DeviceItem *chnext = parent->child(ixPos+1);
        while (!chnext && parent) {
            parent = parent->parent();
            if (!parent) break;
            ixPos = ixParents.pop();
            chnext = parent->child(ixPos+1);
        }
        if (chnext) ++ixPos;
        pos = chnext;
    }
}

/* Model */

PartModel::PartModel(PartMan &pman, QObject *parent)
    : QAbstractItemModel(parent), partman(pman)
{
    root = new DeviceItem(DeviceItem::Unknown, *this);
}
PartModel::~PartModel()
{
    if (root) delete root;
}
QVariant PartModel::data(const QModelIndex &index, int role) const
{
    DeviceItem *item = static_cast<DeviceItem*>(index.internalPointer());
    const bool isDriveOrVD = (item->type == DeviceItem::Drive || item->type == DeviceItem::VirtualDevices);
    if (role == Qt::DisplayRole) {
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
            case Label: return (index.flags() & Qt::ItemIsEditable) ? item->label : item->curLabel; break;
            case UseFor: return item->usefor; break;
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
    } else if (role == Qt::EditRole) {
        switch (index.column()) {
            case Device: return item->device; break;
            case Size: return item->size; break;
            case Label: return item->label; break;
            case UseFor: return item->usefor; break;
            case Format: return item->format; break;
            case Options: return item->options; break;
            case Pass: return item->pass; break;
        }
    } else if (role == Qt::CheckStateRole && !isDriveOrVD
        && index.flags() & Qt::ItemIsUserCheckable) {
        switch (index.column()) {
            case Encrypt: return item->encrypt ? Qt::Checked : Qt::Unchecked; break;
            case Dump: return item->dump ? Qt::Checked : Qt::Unchecked; break;
        }
    } else if (role == Qt::DecorationRole && index.column() == Device) {
        if (item->type == DeviceItem::Drive && !item->flags.oldLayout) {
            return QIcon(":/appointment-soon");
        } else if (item->type == DeviceItem::VirtualBD && item->flags.cryptoV) {
            return QIcon::fromTheme("unlock");
        } else if (item->flags.mapLock) {
            return QIcon::fromTheme("lock");
        }
    } else if (role == Qt::FontRole && index.column() == Device) {
        QFont font;
        font.setItalic(item->flags.setBoot);
        return font;
    }
    return QVariant();
}
bool PartModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if (role == Qt::CheckStateRole) {
        DeviceItem *item = static_cast<DeviceItem *>(index.internalPointer());
        changeBegin(item);
        switch (index.column()) {
            case Encrypt: item->encrypt = (value == Qt::Checked); break;
            case Dump: item->dump = (value == Qt::Checked); break;
        }
    }
    if(!changeEnd()) emit dataChanged(index, index);
    return true;
}

Qt::ItemFlags PartModel::flags(const QModelIndex &index) const
{
    DeviceItem *item = static_cast<DeviceItem *>(index.internalPointer());
    Qt::ItemFlags flagsOut({Qt::ItemIsSelectable, Qt::ItemIsEnabled});
    if (item->flags.mapLock) return flagsOut;
    switch (index.column()) {
        case Device: break;
        case Size:
            if (item->type == DeviceItem::Partition && !item->flags.oldLayout) {
                flagsOut |= Qt::ItemIsEditable;
            }
            break;
        case Label:
            if (item->type == DeviceItem::Subvolume) {
                if (item->format != "PRESERVE") flagsOut |= Qt::ItemIsEditable;
            } else {
                if (!item->usefor.isEmpty()) flagsOut |= Qt::ItemIsEditable;
            }
            break;
        case UseFor:
            if (item->type != DeviceItem::Drive) flagsOut |= Qt::ItemIsEditable;
            break;
        case Encrypt:
            if (item->canEncrypt()) flagsOut |= Qt::ItemIsUserCheckable;
            break;
        case Format:
            if (!item->usefor.isEmpty()) flagsOut |= Qt::ItemIsEditable;
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
QVariant PartModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    assert(orientation == Qt::Horizontal);
    if (role == Qt::DisplayRole) {
        switch (section) {
            case Device: return tr("Device"); break;
            case Size: return tr("Size"); break;
            case Label: return tr("Label"); break;
            case UseFor: return tr("Use For"); break;
            case Encrypt: return tr("Encrypt"); break;
            case Format: return tr("Format"); break;
            case Options: return tr("Options"); break;
            case Dump: return tr("Dump"); break;
            case Pass: return tr("Pass"); break;
        }
    } else if (role == Qt::FontRole && section == Encrypt) {
        QFont smallFont;
        smallFont.setPointSizeF(smallFont.pointSizeF() * 0.6);
        return smallFont;
    }
    return QVariant();
}
QModelIndex PartModel::index(int row, int column, const QModelIndex &parent) const
{
    if (!hasIndex(row, column, parent)) return QModelIndex();
    DeviceItem *pit = parent.isValid() ? static_cast<DeviceItem *>(parent.internalPointer()) : root;
    DeviceItem *cit = pit->child(row);
    if (cit) return createIndex(row, column, cit);
    return QModelIndex();
}
QModelIndex PartModel::parent(const QModelIndex &index) const
{
    if (!index.isValid()) return QModelIndex();
    DeviceItem *cit = static_cast<DeviceItem *>(index.internalPointer());
    DeviceItem *pit = cit->parent();
    if (!pit) return QModelIndex();
    return createIndex(pit->row(), 0, pit);
}
DeviceItem *PartModel::item(const QModelIndex &index) const
{
    return static_cast<DeviceItem *>(index.internalPointer());
}
DeviceItem *PartModel::item(int index) const
{
    if (index < 0) return root;
    if (!root || index > root->childCount()) return nullptr;
    return root->child(index);
}
int PartModel::count() const
{
    return root->childCount();
}
int PartModel::rowCount(const QModelIndex &parent) const
{
    if (parent.column() > 0) return 0;
    if (parent.isValid()) {
        return static_cast<DeviceItem *>(parent.internalPointer())->childCount();
    }
    return root->childCount();
}
void PartModel::clear()
{
    root->clear();
}

bool PartModel::changeBegin(DeviceItem *item)
{
    if (changing) return false;
    root->flags = item->flags;
    root->size = item->size;
    root->label = item->label;
    root->usefor = item->usefor;
    root->encrypt = item->encrypt;
    root->format = item->format;
    root->options = item->options;
    root->dump = item->dump;
    root->pass = item->pass;
    changing = item;
    return true;
}
int PartModel::changeEnd(bool notify)
{
    if (!changing) return false;
    int changed = 0;
    if (changing->size != root->size) {
        if (!changing->allowedUsesFor().contains(changing->realUseFor())) {
            changing->usefor.clear();
        }
        if (!changing->canEncrypt()) changing->encrypt = false;
        changed |= (1 << Size);
    }
    if (changing->usefor != root->usefor) {
        if (changing->usefor.isEmpty()) changing->format.clear();
        else changing->format = changing->allowedFormats().at(0);
        changed |= (1 << UseFor);
    }
    if (changing->encrypt != root->encrypt) {
        const QStringList &allowed = changing->allowedFormats();
        if (!allowed.contains(changing->format)) changing->format = allowed.at(0);
    }
    if (changing->format != root->format || changing->usefor != root->usefor) {
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
        if (changing->format != root->format) changed |= (1 << Format);
    }
    if (changed && notify) notifyChange(changing);
    changing = nullptr;
    return changed;
}
void PartModel::notifyChange(class DeviceItem *item, int first, int last)
{
    DeviceItem *p = item->parent();
    if (first < 0) first = 0;
    if (last < 0) last = p ? p->childCount() - 1 : 0;
    const int row = p ? p->indexOfChild(item) : 0;
    emit dataChanged(createIndex(row, first, item), createIndex(row, last, item));
}

/* Delegate */

void DeviceItemDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    QStyledItemDelegate::paint(painter, option, index);
    // Frame to indicate editable cells
    if (index.flags() & Qt::ItemIsEditable) {
        painter->setPen(option.palette.color(QPalette::Active, QPalette::Text));
        painter->drawRect(option.rect.adjusted(0, 0, -1, -1));
    }
}

QWidget *DeviceItemDelegate::createEditor(QWidget *parent, const QStyleOptionViewItem &, const QModelIndex &index) const
{
    QWidget *widget = nullptr;
    switch (index.column()) {
        case PartModel::Size: widget = new QSpinBox(parent); break;
        case PartModel::UseFor: {
            QComboBox *combo = new QComboBox(parent);
            combo->setEditable(true);
            combo->setInsertPolicy(QComboBox::NoInsert);
            combo->lineEdit()->setPlaceholderText("----");
            widget = combo;
            break;
        }
        case PartModel::Format: widget = new QComboBox(parent); break;
        case PartModel::Pass: widget = new QSpinBox(parent); break;
        case PartModel::Options: {
            QLineEdit *edit = new QLineEdit(parent);
            edit->setProperty("row", QVariant::fromValue<void *>(index.internalPointer()));
            edit->setContextMenuPolicy(Qt::CustomContextMenu);
            connect(edit, &QLineEdit::customContextMenuRequested,
                this, &DeviceItemDelegate::partOptionsMenu);
            widget = edit;
            break;
        }
        default: widget = new QLineEdit(parent);
    }
    assert(widget != nullptr);
    widget->setAutoFillBackground(true);
    widget->setFocusPolicy(Qt::StrongFocus);
    return widget;
}
void DeviceItemDelegate::setEditorData(QWidget *editor, const QModelIndex &index) const
{
    DeviceItem *item = static_cast<DeviceItem*>(index.internalPointer());
    switch (index.column()) {
        case PartModel::Size: {
            DeviceItem *drvit = item->parent();
            assert(drvit != nullptr);
            long long max = drvit->size - (PARTMAN_SAFETY_MB*1048576);
            for (int ixi = drvit->childCount() - 1; ixi >= 0; --ixi) {
                DeviceItem *partit = drvit->child(ixi);
                if (partit != item) max -= partit->size;
            }
            QSpinBox *spin = qobject_cast<QSpinBox *>(editor);
            spin->setRange(1, static_cast<int>(max / 1048576));
            spin->setSuffix("MB");
            spin->setStepType(QSpinBox::AdaptiveDecimalStepType);
            spin->setAccelerated(true);
            spin->setValue(item->size / 1048576);
            break;
        }
        case PartModel::UseFor: {
            QComboBox *combo = qobject_cast<QComboBox *>(editor);
            combo->addItem("");
            combo->addItems(item->allowedUsesFor(false));
            combo->setCurrentText(item->usefor);
            break;
        }
        case PartModel::Label: {
            qobject_cast<QLineEdit *>(editor)->setText(item->label);
            break;
        }
        case PartModel::Format: {
            QComboBox *combo = qobject_cast<QComboBox *>(editor);
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
            break;
        }
        case PartModel::Pass: {
            QSpinBox *spin = qobject_cast<QSpinBox *>(editor);
            spin->setValue(item->pass);
            break;
        }
        case PartModel::Options: {
            qobject_cast<QLineEdit *>(editor)->setText(item->options);
            break;
        }
    }
}
void DeviceItemDelegate::setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const
{
    DeviceItem *item = static_cast<DeviceItem*>(index.internalPointer());
    PartModel *pmodel = qobject_cast<PartModel *>(model);
    pmodel->changeBegin(item);
    switch (index.column()) {
        case PartModel::Size:
            item->size = qobject_cast<QSpinBox *>(editor)->value();
            item->size *= 1048576; // Separate step to prevent int overflow.
            break;
        case PartModel::UseFor:
            item->usefor = qobject_cast<QComboBox *>(editor)->currentText().trimmed();
            break;
        case PartModel::Label:
            item->label = qobject_cast<QLineEdit *>(editor)->text().trimmed();
            break;
        case PartModel::Format:
            item->format = qobject_cast<QComboBox *>(editor)->currentData().toString();
            break;
        case PartModel::Pass:
            item->pass = qobject_cast<QSpinBox *>(editor)->value();
            break;
        case PartModel::Options:
            item->options = qobject_cast<QLineEdit *>(editor)->text().trimmed();
            break;
    }
    const int changed = pmodel->changeEnd(false);
    item->autoFill(changed);
    if (changed) pmodel->notifyChange(item);
}

void DeviceItemDelegate::partOptionsMenu(const QPoint &)
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
