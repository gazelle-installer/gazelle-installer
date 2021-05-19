// Basic partition manager for the installer.
//
//   Copyright (C) 2020-2021 by AK-47
//   Transplanted code, marked with comments further down this file:
//    - Copyright (C) 2003-2010 by Warren Woodford
//    - Heavily edited, with permision, by anticapitalista for antiX 2011-2014.
//    - Heavily revised by dolphin oracle, adrian, and anticaptialista 2018.
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

#include <sys/sysinfo.h>
#include <sys/stat.h>
#include <QDebug>
#include <QTimer>
#include <QLocale>
#include <QMessageBox>
#include <QTreeWidget>
#include <QMenu>
#include <QSpinBox>
#include <QComboBox>
#include <QLineEdit>
#include <QStandardItemModel>
#include <QFormLayout>
#include <QDialogButtonBox>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

#include "msettings.h"
#include "partman.h"

#define PARTMAN_SAFETY_MB 32 // 1MB at start + Compensate for rounding errors.

PartMan::PartMan(MProcess &mproc, BlockDeviceList &bdlist, Ui::MeInstall &ui, QWidget *parent)
    : QObject(parent), proc(mproc), listBlkDevs(bdlist), gui(ui), master(parent)
{
    QTimer::singleShot(0, this, &PartMan::setup);
}
void PartMan::setup()
{
    QFont smallFont = gui.treePartitions->headerItem()->font(Encrypt);
    smallFont.setPointSizeF(smallFont.pointSizeF() * 0.6);
    gui.treePartitions->headerItem()->setFont(Encrypt, smallFont);
    gui.treePartitions->header()->setMinimumSectionSize(5);
    gui.treePartitions->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(gui.treePartitions, &QTreeWidget::customContextMenuRequested, this,&PartMan::treeMenu);
    connect(gui.treePartitions, &QTreeWidget::itemChanged, this, &PartMan::treeItemChange);
    connect(gui.treePartitions, &QTreeWidget::itemSelectionChanged, this, &PartMan::treeSelChange);
    connect(gui.buttonPartClear, &QToolButton::clicked, this, &PartMan::partClearClick);
    connect(gui.buttonPartAdd, &QToolButton::clicked, this, &PartMan::partAddClick);
    connect(gui.buttonPartRemove, &QToolButton::clicked, this, &PartMan::partRemoveClick);
    gui.buttonPartAdd->setEnabled(false);
    gui.buttonPartRemove->setEnabled(false);
    gui.buttonPartClear->setEnabled(false);
    defaultLabels["ESP"] = "EFI System";
    defaultLabels["bios_grub"] = "BIOS GRUB";
}

void PartMan::populate(QTreeWidgetItem *drvstart)
{
    gui.treePartitions->blockSignals(true);
    if(!drvstart) gui.treePartitions->clear();
    QTreeWidgetItem *curdrv = nullptr;
    for (const BlockDeviceInfo &bdinfo : listBlkDevs) {
        if(bdinfo.isBoot && !brave) continue;
        QTreeWidgetItem *curdev = nullptr;
        if (bdinfo.isDrive) {
            if(!drvstart) curdrv = new QTreeWidgetItem(gui.treePartitions);
            else if(bdinfo.name == drvstart->text(Device)) curdrv = drvstart;
            else if(!curdrv) continue; // Skip until the drive is drvstart.
            else break; // Exit the loop early if the drive isn't drvstart.
            drvitMarkLayout(curdrv, true);
            twitSetFlag(curdrv, TwitFlag::Drive, true);
            curdev = curdrv;
            // Model
            curdev->setText(Label, bdinfo.model);
        } else {
            if(!curdrv) continue;
            curdev = new QTreeWidgetItem(curdrv);
            setupPartitionItem(curdev, &bdinfo);
        }
        assert(curdev!=nullptr);
        // Device name and size
        curdev->setText(Device, bdinfo.name);
        curdev->setText(Size, QLocale::system().formattedDataSize(bdinfo.size, 1, QLocale::DataSizeTraditionalFormat));
        curdev->setData(Size, Qt::UserRole, QVariant(bdinfo.size));
        if(bdinfo.mapCount > 0) {
            twitComboBox(curdev, UseFor)->setDisabled(true);
            curdev->setIcon(0, QIcon::fromTheme("lock"));
        }
    }
    if(!drvstart) scanVirtualDevices(false);
    gui.treePartitions->expandAll();
    resizeColumnsToFit();
    comboFormatTextChange(QString()); // For the badblocks checkbox.
    gui.treePartitions->blockSignals(false);
    treeSelChange();
}
void PartMan::scanVirtualDevices(bool rescan)
{
    QTreeWidgetItem *vdlit = nullptr;
    QMap<QString, QTreeWidgetItem *> listed;
    if(rescan) {
        for(int ixi = gui.treePartitions->topLevelItemCount()-1; ixi>=0; --ixi) {
            QTreeWidgetItem *drvit = gui.treePartitions->topLevelItem(ixi);
            if(twitFlag(drvit, TwitFlag::VirtualDevices)) {
                vdlit = drvit;
                const int vdcount = vdlit->childCount();
                for(int ixVD = 0; ixVD < vdcount; ++ixVD) {
                    QTreeWidgetItem *devit = vdlit->child(ixVD);
                    listed.insert(devit->text(Device), devit);
                }
                break;
            }
        }
    }
    const QString &bdRaw = proc.execOut("lsblk -T -bJo"
        " TYPE,NAME,UUID,SIZE,FSTYPE,LABEL"
        " /dev/mapper/* 2>/dev/null", true);
    const QJsonObject &jsonObjBD = QJsonDocument::fromJson(bdRaw.toUtf8()).object();
    const QJsonArray &jsonBD = jsonObjBD["blockdevices"].toArray();
    if(jsonBD.empty()) {
        if(vdlit) delete vdlit;
        return;
    } else if(!vdlit) {
        vdlit = new QTreeWidgetItem(gui.treePartitions);
        vdlit->setText(0, tr("Virtual Devices"));
        vdlit->setFirstColumnSpanned(true);
        twitSetFlag(vdlit, TwitFlag::VirtualDevices, true);
    }
    assert(vdlit!=nullptr);
    for(const QJsonValue &jsonDev : jsonBD) {
        BlockDeviceInfo bdinfo;
        bdinfo.isDrive = false;
        bdinfo.name = jsonDev["name"].toString();
        bdinfo.size = jsonDev["size"].toVariant().toLongLong();
        bdinfo.label = jsonDev["label"].toString();
        bdinfo.fs = jsonDev["fstype"].toString();
        const bool crypto = jsonDev["type"].toString()=="crypt";
        // Check if the device is already in the list.
        QTreeWidgetItem *devit = listed.value(bdinfo.name);
        if(devit) {
            if(bdinfo.size != devit->data(Size, Qt::UserRole).toLongLong()
                || bdinfo.label != devit->data(Label, Qt::UserRole).toString()
                || crypto != twitFlag(devit, TwitFlag::CryptoV)) {
                // List entry is different to the device, so refresh it.
                delete devit;
                devit = nullptr;
            }
            listed.remove(bdinfo.name);
        }
        // Create a new list entry if needed.
        if(!devit) {
            devit = new QTreeWidgetItem(vdlit);
            twitSetFlag(devit, TwitFlag::VirtualBD, true);
            setupPartitionItem(devit, &bdinfo);
            devit->setText(Device, bdinfo.name);
            devit->setData(Device, Qt::UserRole, bdinfo.name);
            const QString &tsize = QLocale::system().formattedDataSize(bdinfo.size,
                1, QLocale::DataSizeTraditionalFormat);
            devit->setText(Size, tsize);
            devit->setData(Size, Qt::UserRole, QVariant(bdinfo.size));
            if(crypto) {
                twitSetFlag(devit, TwitFlag::CryptoV, true);
                devit->setIcon(0, QIcon::fromTheme("unlock"));
            }
        }
        QTreeWidgetItem *orit = findOrigin(bdinfo.name);
        if(orit) orit->setData(Device, Qt::UserRole, bdinfo.name);
    }
    for(const auto &it : listed.toStdMap()) delete it.second;
    drvitMarkLayout(vdlit, true);
    vdlit->sortChildren(Device, Qt::AscendingOrder);
}
void PartMan::clearLayout(QTreeWidgetItem *drvit)
{
    while(drvit->childCount()) delete drvit->child(0);
    drvitMarkLayout(drvit, false);
}

bool PartMan::manageConfig(MSettings &config, bool save)
{
    const int driveCount = gui.treePartitions->topLevelItemCount();
    for(int ixDrive = 0; ixDrive < driveCount; ++ixDrive) {
        QTreeWidgetItem *drvit = gui.treePartitions->topLevelItem(ixDrive);
        const QString &drvDevice = drvit->text(Device);
        // Check if the drive is to be cleared and formatted.
        int partCount = drvit->childCount();
        bool drvPreserve = twitFlag(drvit, TwitFlag::OldLayout);
        const QString &configNewLayout = "Storage/NewLayout."+drvDevice;
        if(save) {
            if(!drvPreserve) config.setValue(configNewLayout, partCount);
        } else if(config.contains(configNewLayout)) {
            drvPreserve = false;
            clearLayout(drvit);
            partCount = config.value(configNewLayout).toInt();
        }
        // Partition configuration.
        const long long maxMB = twitSize(drvit) - PARTMAN_SAFETY_MB;
        long long totalMB = 0;
        for(int ixPart = 0; ixPart < partCount; ++ixPart) {
            QTreeWidgetItem *partit = nullptr;
            if(save || drvPreserve) {
                partit = drvit->child(ixPart);
                twitSetFlag(partit, TwitFlag::OldLayout, drvPreserve);
            } else {
                partit = new QTreeWidgetItem(drvit);
                setupPartitionItem(partit, nullptr);
                partit->setText(Device, BlockDeviceInfo::join(drvDevice, ixPart+1));
            }
            // Configuration management, accounting for automatic control correction order.
            const QString &groupPart = "Storage." + partit->text(Device);
            config.beginGroup(groupPart);
            if(save) {
                config.setValue("Size", twitSize(partit, true));
                config.setValue("Encrypt", partit->checkState(Encrypt)==Qt::Checked);
            } else if(!drvPreserve && config.contains("Size")) {
                const int size = config.value("Size").toLongLong() / 1048576;
                totalMB += size;
                QWidget *spinSize = gui.treePartitions->itemWidget(partit, Size);
                static_cast<QSpinBox *>(spinSize)->setValue(size);
                if(totalMB > maxMB) config.markBadWidget(spinSize);
            }
            config.manageComboBox("UseFor", twitComboBox(partit, UseFor), false);
            config.manageComboBox("Format", twitComboBox(partit, Format), true);
            if(!save && config.contains("Encrypt")) {
                const bool crypto = config.value("Encrypt").toBool();
                partit->setCheckState(Encrypt, crypto ? Qt::Checked : Qt::Unchecked);
            }
            config.manageLineEdit("Label", twitLineEdit(partit, Label));
            config.manageLineEdit("Options", twitLineEdit(partit, Options));
            int subvolCount = 0;
            if(twitComboBox(partit, Format)->currentData() == "btrfs") {
                if(!save) subvolCount = config.value("Subvolumes").toInt();
                else {
                    subvolCount = partit->childCount();
                    config.setValue("Subvolumes", subvolCount);
                }
            }
            config.endGroup();
            // Btrfs subvolume configuration.
            for(int ixSV=0; ixSV<subvolCount; ++ixSV) {
                QTreeWidgetItem *svit = nullptr;
                if(save) svit = partit->child(ixSV);
                else svit = addSubvolumeItem(partit);
                if(!svit) return false;
                config.beginGroup(groupPart + ".Subvolume" + QString::number(ixSV+1));
                config.manageComboBox("UseFor", twitComboBox(svit, UseFor), false);
                config.manageLineEdit("Label", twitLineEdit(svit, Label));
                config.manageLineEdit("Options", twitLineEdit(svit, Options));
                config.endGroup();
            }
            if(!save) partit->setExpanded(true);
        }
    }
    treeSelChange();
    return true;
}

inline QTreeWidgetItem *PartMan::addItem(QTreeWidgetItem *parent,
    int defaultMB, const QString &defaultUse, bool crypto) {
    QTreeWidgetItem *partit = new QTreeWidgetItem(parent);
    setupPartitionItem(partit, nullptr, defaultMB, defaultUse);
    // If the layout needs crypto and it is supported, check the box.
    if(partit->data(Encrypt, Qt::CheckStateRole).isValid()) {
        partit->setCheckState(Encrypt, (crypto ? Qt::Checked : Qt::Unchecked));
    }
    return partit;
}
void PartMan::setupPartitionItem(QTreeWidgetItem *partit, const BlockDeviceInfo *bdinfo,
    int defaultMB, const QString &defaultUse)
{
    twitSetFlag(partit, TwitFlag::Partition, true);
    // Size
    if(!bdinfo) {
        QSpinBox *spinSize = new QSpinBox(gui.treePartitions);
        spinSize->setAutoFillBackground(true);
        gui.treePartitions->setItemWidget(partit, Size, spinSize);
        spinSize->setFocusPolicy(Qt::StrongFocus);
        spinSize->installEventFilter(this);
        const int maxMB = (int)twitSize(partit->parent())-PARTMAN_SAFETY_MB;
        spinSize->setRange(1, maxMB);
        spinSize->setProperty("row", QVariant::fromValue<void *>(partit));
        spinSize->setSuffix("MB");
        connect(spinSize, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &PartMan::spinSizeValueChange);
        if(!defaultMB) defaultMB = maxMB;
        spinSize->setValue(defaultMB);
        // TODO: re-enable when Debian Bullseye is about to be released.
        //spinSize->setStepType(QSpinBox::AdaptiveDecimalStepType);
        spinSize->setAccelerated(true);
    }
    // Label
    QLineEdit *editLabel = new QLineEdit(gui.treePartitions);
    editLabel->setAutoFillBackground(true);
    gui.treePartitions->setItemWidget(partit, Label, editLabel);
    editLabel->setEnabled(false);
    if(bdinfo) {
        partit->setText(Label, bdinfo->label);
        editLabel->setText(bdinfo->label);
    }
    // Use For
    QComboBox *comboUse = new QComboBox(gui.treePartitions);
    comboUse->setAutoFillBackground(true);
    gui.treePartitions->setItemWidget(partit, UseFor, comboUse);
    comboUse->setFocusPolicy(Qt::StrongFocus);
    comboUse->installEventFilter(this);
    comboUse->setEditable(true);
    comboUse->setInsertPolicy(QComboBox::NoInsert);
    comboUse->addItem("");
    comboUse->addItem("Format");
    if(!twitFlag(partit, TwitFlag::VirtualBD)) {
        if(!bdinfo || bdinfo->size <= 16) comboUse->addItem("bios_grub");
        if(!bdinfo || bdinfo->size <= 4294967296) {
            comboUse->addItem("ESP");
            comboUse->addItem("boot");
        }
    }
    comboUse->addItem("root");
    comboUse->addItem("swap");
    comboUse->addItem("home");
    comboUse->setProperty("row", QVariant::fromValue<void *>(partit));
    comboUse->lineEdit()->setPlaceholderText("----");
    connect(comboUse, &QComboBox::currentTextChanged, this, &PartMan::comboUseTextChange);
    // Format
    QComboBox *comboFormat = new QComboBox(gui.treePartitions);
    comboFormat->setAutoFillBackground(true);
    gui.treePartitions->setItemWidget(partit, Format, comboFormat);
    comboFormat->setFocusPolicy(Qt::StrongFocus);
    comboFormat->installEventFilter(this);
    comboFormat->setEnabled(false);
    if(bdinfo) {
        partit->setText(Format, bdinfo->fs);
        comboFormat->addItem(bdinfo->fs, bdinfo->fs);
    }
    connect(comboFormat, &QComboBox::currentTextChanged, this, &PartMan::comboFormatTextChange);
    // Mount options
    QLineEdit *editOptions = new QLineEdit(gui.treePartitions);
    editOptions->setAutoFillBackground(true);
    gui.treePartitions->setItemWidget(partit, Options, editOptions);
    editOptions->setEnabled(false);
    editOptions->setText("noatime");
    editOptions->setProperty("row", QVariant::fromValue<void *>(partit));
    editOptions->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(editOptions, &QLineEdit::customContextMenuRequested, this, &PartMan::partOptionsMenu);

    if(!defaultUse.isEmpty()) comboUse->setCurrentText(defaultUse);
}

void PartMan::labelParts(QTreeWidgetItem *drvit)
{
    const QString &drv = drvit->text(Device);
    for(int ixi = drvit->childCount() - 1; ixi >= 0; --ixi) {
        drvit->child(ixi)->setText(Device, BlockDeviceInfo::join(drv, ixi+1));
    }
    resizeColumnsToFit();
}

void PartMan::resizeColumnsToFit()
{
    for (int ixi = gui.treePartitions->columnCount() - 1; ixi >= 0; --ixi) {
        if(ixi != Label) gui.treePartitions->resizeColumnToContents(ixi);
    }
    // Pad the column to work around a Buster Qt bug where combo box bleeds out of column.
    QFile file("/etc/debian_version");
    if (file.open(QFile::ReadOnly | QFile::Text)) {
        const int ver = file.readLine().split('.').at(0).toInt();
        if (ver==10) {
            gui.treePartitions->setColumnWidth(UseFor,
                gui.treePartitions->columnWidth(UseFor)+16);
        }
    }
}

QString PartMan::translateUse(const QString &alias)
{
    if(alias == "root") return QStringLiteral("/");
    else if(alias == "boot") return QStringLiteral("/boot");
    else if(alias == "home") return QStringLiteral("/home");
    else if(!alias.startsWith('/')) return alias.toUpper();
    else return alias;
}
QString PartMan::describeUse(const QString &use)
{
    if(use == "/") return "/ (root)";
    else if(use == "ESP") return tr("EFI System Partition");
    else if(use == "SWAP") return tr("swap space");
    else if(use.startsWith("SWAP")) return tr("swap space #%1").arg(use.mid(4));
    else if(use.startsWith("FORMAT")) return tr("format only");
    return use;
}

void PartMan::setEncryptChecks(const QString &use,
    enum Qt::CheckState state, QTreeWidgetItem *exclude)
{
    QTreeWidgetItemIterator it(gui.treePartitions);
    for(; *it; ++it) {
        if(*it == exclude) continue;
        if(twitUseFor(*it)==use && (*it)->data(Encrypt, Qt::CheckStateRole).isValid()) {
            (*it)->setCheckState(Encrypt, state);
        }
    }
}

void PartMan::spinSizeValueChange(int i)
{
    QSpinBox *spin = static_cast<QSpinBox *>(sender());
    if(!spin) return;
    QTreeWidgetItem *partit = static_cast<QTreeWidgetItem *>(spin->property("row").value<void *>());
    if(!partit) return;
    QTreeWidgetItem *drvit = partit->parent();
    if(!drvit) return;
    // Partition size validation
    spin->blockSignals(true);
    long long maxMB = twitSize(drvit)-PARTMAN_SAFETY_MB;
    for(int ixi = drvit->childCount()-1; ixi >= 0; --ixi) {
        QTreeWidgetItem *twit = drvit->child(ixi);
        if(twit!=partit) maxMB -= twitSize(twit);
    }
    if(i > maxMB) spin->setValue(maxMB);
    // TODO: Remove when Bullseye is about to be released.
    int stepval = 1;
    for(int ixi=1; ixi<=(i/10); ixi*=10) stepval = ixi;
    spin->setSingleStep(stepval);
    // End of setStepType() replacement.
    gui.buttonPartAdd->setEnabled(i < maxMB);
    spin->blockSignals(false);
}
void PartMan::comboUseTextChange(const QString &text)
{
    gui.treePartitions->blockSignals(true);
    QComboBox *combo = static_cast<QComboBox *>(sender());
    if(!combo) return;
    QTreeWidgetItem *partit = static_cast<QTreeWidgetItem *>(combo->property("row").value<void *>());
    if(!partit) return;
    QLineEdit *editLabel = twitLineEdit(partit, Label);

    const QString &usetext = translateUse(text);
    int useClass = -1;
    if(usetext.isEmpty()) useClass = 0;
    else if(usetext == "ESP") useClass = 1;
    else if(usetext == "bios_grub") useClass = 2;
    else if(usetext == "/boot") useClass = 3;
    else if(usetext == "SWAP") useClass = 4;
    else if(usetext == "FORMAT") useClass = 100;
    else useClass = 5;
    int oldUseClass = combo->property("class").toInt();
    bool allowPreserve = false, selPreserve = false;
    if(useClass != oldUseClass) {
        QComboBox *comboFormat = twitComboBox(partit, Format);
        comboFormat->blockSignals(true);
        const QVariant oldFormat = comboFormat->currentData(Qt::UserRole);
        comboFormat->clear();
        const QString &curfmt = partit->text(Format);
        switch(useClass) {
        case 0:
            editLabel->setText(partit->text(Label));
            comboFormat->addItem(curfmt, curfmt);
            break;
        case 1:
            comboFormat->addItem("FAT32", "FAT32");
            comboFormat->addItem("FAT16", "FAT16");
            comboFormat->addItem("FAT12", "FAT12");
            if(comboFormat->findData(curfmt, Qt::UserRole, Qt::MatchFixedString)>=0
                || !curfmt.compare("VFAT", Qt::CaseInsensitive)) allowPreserve = true;
            selPreserve = allowPreserve;
            break;
        case 2: comboFormat->addItem("GRUB", "GRUB"); break;
        case 3: comboFormat->addItem("ext4", "ext4"); break;
        case 4:
            comboFormat->addItem("SWAP", "SWAP");
            if(!curfmt.compare("SWAP", Qt::CaseInsensitive)) allowPreserve = true;
            selPreserve = allowPreserve;
            break;
        default:
            comboFormat->addItem("ext4", "ext4");
            comboFormat->addItem("ext3", "ext3");
            comboFormat->addItem("ext2", "ext2");
            comboFormat->addItem("f2fs", "f2fs");
            comboFormat->addItem("jfs", "jfs");
            comboFormat->addItem("xfs", "xfs");
            comboFormat->addItem("btrfs", "btrfs");
            comboFormat->addItem("reiserfs", "reiserfs");
            if(useClass<100 && comboFormat->findData(curfmt, Qt::UserRole,
                Qt::MatchFixedString)>=0) allowPreserve = true;
        }
        // Changing to and from a mount/use that support encryption.
        if(useClass >= 0 && useClass <= 3) {
            partit->setData(Encrypt, Qt::CheckStateRole, QVariant());
        } else if(oldUseClass >= 0 && oldUseClass <= 3) {
            // Qt::PartiallyChecked tells treeItemChange() to include this item in the refresh.
            partit->setCheckState(Encrypt, Qt::PartiallyChecked);
        }
        if(twitFlag(partit, TwitFlag::VirtualBD)) partit->setData(Encrypt, Qt::CheckStateRole, QVariant());
        if(allowPreserve) {
            // Add an item at the start to allow preserving the existing format.
            const QString &prestext = (usetext!="/") ? tr("Preserve (%1)") : tr("Preserve /home (%1)");
            comboFormat->insertItem(0, prestext.arg(curfmt), "PRESERVE");
            comboFormat->insertSeparator(1);
        }
        if(selPreserve) comboFormat->setCurrentIndex(0);
        else {
            const int ixOldFmt = comboFormat->findData(oldFormat);
            if(ixOldFmt>=0) comboFormat->setCurrentIndex(ixOldFmt);
        }
        comboFormatTextChange(comboFormat->currentText());
        comboFormat->blockSignals(false);
        comboFormat->setEnabled(comboFormat->count()>1);
        // Label and options
        editLabel->setEnabled(useClass!=0);
        gui.treePartitions->itemWidget(partit, Options)->setEnabled(useClass!=0 && useClass<100);
        combo->setProperty("class", QVariant(useClass));
    }
    if(useClass!=0 && (!(editLabel->isModified()) || editLabel->text().isEmpty())) {
        editLabel->setText(defaultLabels.value(usetext));
    }
    gui.treePartitions->blockSignals(false);
    treeItemChange(partit, UseFor);
}
void PartMan::comboFormatTextChange(const QString &)
{
    QTreeWidgetItemIterator it(gui.treePartitions);
    bool canCheckBlocks = false;
    for(; *it; ++it) {
        if(!twitFlag(*it, TwitFlag::Partition)) continue;
        QComboBox *comboFormat = twitComboBox(*it, Format);
        QLineEdit *editOpts = twitLineEdit(*it, Options);
        if(!comboFormat || !editOpts) return;
        const QString &format = comboFormat->currentText();
        if(comboFormat->currentData(Qt::UserRole)!="PRESERVE") {
            if(format!="btrfs") {
                // Clear all subvolumes if not supported.
                while((*it)->childCount()) delete (*it)->child(0);
            } else {
                // Remove preserve option from all subvolumes.
                const int svcount = (*it)->childCount();
                for(int ixi = 0; ixi < svcount; ++ixi) {
                    QComboBox *comboSubvolFmt = twitComboBox((*it)->child(ixi), Format);
                    comboSubvolFmt->clear();
                    comboSubvolFmt->addItem(tr("Create"));
                    comboSubvolFmt->setEnabled(false);
                }
            }
            if(!twitUseFor(*it).isEmpty()) {
                if(format.startsWith("ext") || format == "jfs") canCheckBlocks = true;
                if(format=="reiser") editOpts->setText("noatime,notail");
                else if(format=="SWAP") editOpts->setText("defaults");
                else editOpts->setText("noatime");
            }
        }
    }
    gui.badblocksCheck->setEnabled(canCheckBlocks);
}
void PartMan::comboSubvolUseTextChange(const QString &text)
{
    gui.treePartitions->blockSignals(true);
    QComboBox *combo = static_cast<QComboBox *>(sender());
    if(!combo) return;
    QTreeWidgetItem *svit = static_cast<QTreeWidgetItem *>(combo->property("row").value<void *>());
    if(!svit) return;
    const QString &usetext = translateUse(text);
    gui.treePartitions->itemWidget(svit, Options)->setEnabled(!usetext.isEmpty());
    QLineEdit *editLabel = twitLineEdit(svit, Label);
    if(editLabel->isEnabled() && (!(editLabel->isModified()) || editLabel->text().isEmpty())) {
        if(!usetext.startsWith('/')) editLabel->clear();
        else if(usetext=='/') editLabel->setText("root");
        else {
            QStringList chklist;
            QTreeWidgetItem *partit = svit->parent();
            const int count = partit->childCount();
            const int index = partit->indexOfChild(svit);
            for(int ixi = 0; ixi < count; ++ixi) {
                if(ixi==index) continue;
                chklist << twitLineEdit(partit->child(ixi), Label)->text().trimmed();
            }
            int ixnum = 0;
            const QString base = usetext.mid(1).replace('/','.');
            QString newLabel = base;
            while(chklist.contains(newLabel, Qt::CaseInsensitive)) newLabel = QString::number(++ixnum) + '.' + base;
            editLabel->setText(newLabel);
        }
    }
    gui.treePartitions->blockSignals(false);
}
void PartMan::comboSubvolFormatTextChange(const QString &)
{
    QComboBox *combo = static_cast<QComboBox *>(sender());
    if(!combo) return;
    QTreeWidgetItem *svit = static_cast<QTreeWidgetItem *>(combo->property("row").value<void *>());
    if(!svit) return;
    QLineEdit *editLabel = twitLineEdit(svit, Label);
    if(combo->currentData()!="PRESERVE") editLabel->setEnabled(true);
    else {
        editLabel->setText(svit->text(Label));
        editLabel->setEnabled(false);
    }
}

void PartMan::treeItemChange(QTreeWidgetItem *twit, int column)
{
    if(column==Encrypt || column==UseFor) {
        // Check all items in the tree for those marked for encryption.
        bool cryptoAny=false;
        QTreeWidgetItemIterator it(gui.treePartitions);
        for(; *it && !cryptoAny; ++it) {
            if((*it)->checkState(Encrypt)==Qt::Checked) cryptoAny = true;
        }
        if(gui.gbEncrPass->isEnabled() != cryptoAny) {
            gui.FDEpassCust->clear();
            gui.nextButton->setDisabled(cryptoAny);
            gui.gbEncrPass->setEnabled(cryptoAny);
        }
        const Qt::CheckState csCrypto = twit->checkState(Encrypt);
        if(csCrypto!=Qt::Unchecked) {
            // New items are pre-set with Qt::PartiallyChecked and cannot be excluded.
            QTreeWidgetItem *exclude = (csCrypto==Qt::PartiallyChecked) ? nullptr : twit;
            // Set checkboxes and enable the crypto password controls as needed.
            if(cryptoAny) setEncryptChecks("SWAP", Qt::Checked, exclude);
            if(twitUseFor(twit)=="/" && csCrypto==Qt::Checked) {
                setEncryptChecks("/home", Qt::Checked, exclude);
            }
            if(csCrypto==Qt::PartiallyChecked) twit->setCheckState(Encrypt, Qt::Unchecked);
        }
        // Cannot preserve if encrypting.
        QComboBox *combo = twitComboBox(twit, Format);
        const int ixPres = combo->findData("PRESERVE");
        if(ixPres>=0) {
            const bool cryptoThis = twit->checkState(Encrypt)==Qt::Checked;
            auto *model = qobject_cast<QStandardItemModel*>(combo->model());
            model->item(ixPres)->setEnabled(!cryptoThis);
            // Select the next non-separator item if preserving is not possible.
            int ixCur = combo->currentIndex();
            if(cryptoThis && ixCur==ixPres) {
                const int count = combo->count();
                ++ixCur;
                while(ixCur<count && combo->itemText(ixCur).isEmpty()) ++ixCur;
                combo->setCurrentIndex(ixCur);
            }
        }
    }
}
void PartMan::treeSelChange()
{
    QTreeWidgetItem *twit = gui.treePartitions->selectedItems().value(0);
    if(twit && !twitFlag(twit, TwitFlag::Subvolume)) {
        const bool isold = twitFlag(twit, TwitFlag::OldLayout);
        const bool isdrive = twitFlag(twit, TwitFlag::Drive);
        bool islocked = true;
        if(isdrive) islocked = drvitIsLocked(twit);
        gui.buttonPartClear->setEnabled(!islocked);
        gui.buttonPartRemove->setEnabled(!isold && !isdrive);
        // Only allow adding partitions if there is enough space.
        QTreeWidgetItem *drvit = twit->parent();
        if(!drvit) drvit = twit;
        if(!islocked && isold && isdrive) gui.buttonPartAdd->setEnabled(false);
        else {
            long long maxMB = twitSize(drvit)-PARTMAN_SAFETY_MB;
            for(int ixi = drvit->childCount()-1; ixi >= 0; --ixi) {
                maxMB -= twitSize(drvit->child(ixi));
            }
            gui.buttonPartAdd->setEnabled(maxMB > 0);
        }
    } else {
        gui.buttonPartClear->setEnabled(false);
        gui.buttonPartAdd->setEnabled(false);
        gui.buttonPartRemove->setEnabled(false);
    }
}

void PartMan::treeMenu(const QPoint &)
{
    QTreeWidgetItem *twit = gui.treePartitions->selectedItems().value(0);
    if(!twit) return;
    if(twitFlag(twit, TwitFlag::VirtualDevices)) return;
    QMenu menu(gui.treePartitions);
    if(twitFlag(twit, TwitFlag::Partition)) {
        QComboBox *comboFormat = twitComboBox(twit, Format);
        QAction *actAdd = menu.addAction(tr("&Add partition"));
        actAdd->setEnabled(gui.buttonPartAdd->isEnabled());
        QAction *actRemove = menu.addAction(tr("&Remove partition"));
        QAction *actLock = nullptr;
        QAction *actUnlock = nullptr;
        QAction *actAddCrypttab = nullptr;
        QAction *actNewSubvolume = nullptr, *actScanSubvols = nullptr;
        actRemove->setEnabled(gui.buttonPartRemove->isEnabled());
        menu.addSeparator();
        if(twitFlag(twit, TwitFlag::VirtualBD)) {
            actRemove->setEnabled(false);
            if(twitFlag(twit, TwitFlag::CryptoV)) actLock = menu.addAction(tr("&Lock"));
        } else {
            bool allowCryptTab = false;
            if(twitFlag(twit, TwitFlag::OldLayout) && twit->text(Format)=="crypto_LUKS") {
                actUnlock = menu.addAction(tr("&Unlock"));
                allowCryptTab = true;
            }
            if(twitUseFor(twit)=="FORMAT" && twit->data(Encrypt, Qt::CheckStateRole).isValid()) {
                allowCryptTab = true;
            }
            if(allowCryptTab) {
                actAddCrypttab = menu.addAction(tr("Add to crypttab"));
                actAddCrypttab->setCheckable(true);
                actAddCrypttab->setChecked(twitFlag(twit, TwitFlag::AutoCrypto));
                actAddCrypttab->setEnabled(twitWillMap(twit));
            }
        }
        const int ixBTRFS = comboFormat->findData("btrfs");
        if(ixBTRFS>=0 && comboFormat->currentIndex()==ixBTRFS) {
            menu.addSeparator();
            actNewSubvolume = menu.addAction(tr("New subvolume"));
            actScanSubvols = menu.addAction(tr("Scan subvolumes"));
            actScanSubvols->setDisabled(twitWillFormat(twit));
        }
        if(!twitCanUse(twit) && actUnlock) actUnlock->setEnabled(false);
        QAction *action = menu.exec(QCursor::pos());
        if(!action) return;
        else if(action==actAdd) partAddClick(true);
        else if(action==actRemove) partRemoveClick(true);
        else if(action==actUnlock) partMenuUnlock(twit);
        else if(action==actLock) partMenuLock(twit);
        else if(action==actAddCrypttab) twitSetFlag(twit, TwitFlag::AutoCrypto, action->isChecked());
        else if(action==actNewSubvolume) {
            addSubvolumeItem(twit);
            twit->setExpanded(true);
        } else if(action==actScanSubvols) {
            scanSubvolumes(twit);
            twit->setExpanded(true);
        }
    } else if(twitFlag(twit, TwitFlag::Drive)) {
        QAction *actAdd = menu.addAction(tr("&Add partition"));
        actAdd->setEnabled(gui.buttonPartAdd->isEnabled());
        menu.addSeparator();
        QAction *actClear = menu.addAction(tr("New &layout"));
        QAction *actReset = menu.addAction(tr("&Reset layout"));
        QMenu *menuTemplates = menu.addMenu(tr("&Templates"));
        const QAction *actBasic = menuTemplates->addAction(tr("&Standard install"));
        QAction *actCrypto = menuTemplates->addAction(tr("&Encrypted system"));

        const bool locked = drvitIsLocked(twit);
        actClear->setDisabled(locked);
        actReset->setDisabled(locked || twitFlag(twit, TwitFlag::OldLayout));
        menuTemplates->setDisabled(locked);
        actCrypto->setVisible(gui.gbEncrPass->isVisible());

        QAction *action = menu.exec(QCursor::pos());
        if(action==actAdd) partAddClick(true);
        else if(action==actClear) partClearClick(true);
        else if(action==actBasic) layoutDefault(twit, -1, false);
        else if(action==actCrypto) layoutDefault(twit, -1, true);
        else if(action==actReset) {
            while(twit->childCount()) delete twit->child(0);
            populate(twit);
        }
    } else if(twitFlag(twit, TwitFlag::Subvolume)) {
        QAction *actRemSubvolume = menu.addAction(tr("Remove subvolume"));
        if(menu.exec(QCursor::pos()) == actRemSubvolume) delete twit;
    }
}

void PartMan::partOptionsMenu(const QPoint &)
{
    QLineEdit *edit = static_cast<QLineEdit *>(sender());
    if(!edit) return;
    QTreeWidgetItem *partit = static_cast<QTreeWidgetItem *>(edit->property("row").value<void *>());
    if(!partit) return;
    QString selFS = twitComboBox(partit, Format)->currentData().toString();
    QMenu *menu = edit->createStandardContextMenu();
    menu->addSeparator();
    QMenu *menuTemplates = menu->addMenu(tr("&Templates"));
    if(selFS=="PRESERVE") selFS = partit->text(Format);
    if((twitFlag(partit, TwitFlag::Partition) && selFS == "btrfs") || twitFlag(partit, TwitFlag::Subvolume)) {
        QAction *action = menuTemplates->addAction(tr("Compression (&ZLIB)"));
        action->setData("noatime,compress-force=zlib");
        action = menuTemplates->addAction(tr("Compression (&LZO)"));
        action->setData("noatime,compress-force=lzo");
    }
    menuTemplates->setDisabled(menuTemplates->isEmpty());
    QAction *action = menu->exec(QCursor::pos());
    if(menuTemplates->actions().contains(action)) edit->setText(action->data().toString());
    delete menu;
}

// Partition manager list buttons
void PartMan::partClearClick(bool)
{
    QTreeWidgetItem *drvit = gui.treePartitions->selectedItems().value(0);
    if(!drvit || !twitFlag(drvit, TwitFlag::Drive)) return;
    clearLayout(drvit);
    comboFormatTextChange(QString()); // For the badblocks checkbox.
    treeSelChange();
}
void PartMan::partAddClick(bool)
{
    QTreeWidgetItem *twit = gui.treePartitions->selectedItems().value(0);
    if(!twit) return;
    QTreeWidgetItem *drive = twit->parent();
    QTreeWidgetItem *preceeding = nullptr;
    if(!drive) drive = twit;
    else preceeding = twit;

    QTreeWidgetItem *part = new QTreeWidgetItem(drive, preceeding);
    setupPartitionItem(part, nullptr);

    labelParts(drive);
    drvitMarkLayout(drive, false);
    part->setSelected(true);
    twit->setSelected(false);
}
void PartMan::partRemoveClick(bool)
{
    QTreeWidgetItem *partit = gui.treePartitions->selectedItems().value(0);
    QTreeWidgetItem *drvit = partit->parent();
    if(!partit || !drvit) return;
    delete partit;
    labelParts(drvit);
    treeSelChange();
}

// Partition menu items
void PartMan::partMenuUnlock(QTreeWidgetItem *twit)
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

    if(dialog.exec()==QDialog::Accepted) {
        qApp->setOverrideCursor(Qt::WaitCursor);
        gui.mainFrame->setEnabled(false);
        const QString &mapdev = editVDev->text();
        const bool ok = luksOpen("/dev/" + twit->text(Device),
            mapdev, editPass->text().toUtf8());
        if(ok) {
            twit->setIcon(Device, QIcon::fromTheme("lock"));
            QComboBox *comboUseFor = twitComboBox(twit, UseFor);
            comboUseFor->setCurrentIndex(0);
            comboUseFor->setEnabled(false);
            twitSetFlag(twit, TwitFlag::AutoCrypto, checkCrypttab->isChecked());
            // Refreshing the drive will not necessarily re-scan block devices.
            // This updates the device list manually to keep the tree accurate.
            const int ixBD = listBlkDevs.findDevice(twit->text(Device));
            if(ixBD >= 0) listBlkDevs[ixBD].mapCount++;
            // Update virtual device list.
            gui.treePartitions->blockSignals(true);
            scanVirtualDevices(true);
            comboFormatTextChange(QString()); // For the badblocks checkbox.
            gui.treePartitions->blockSignals(false);
            treeSelChange();
        }
        gui.mainFrame->setEnabled(true);
        qApp->restoreOverrideCursor();
        if(!ok) {
            QMessageBox::warning(master, master->windowTitle(),
                tr("Could not unlock device. Possible incorrect password."));
        }
    }
}
void PartMan::partMenuLock(QTreeWidgetItem *twit)
{
    qApp->setOverrideCursor(QCursor(Qt::WaitCursor));
    gui.mainFrame->setEnabled(false);
    const QString &dev = twitMappedDevice(twit);
    bool ok = false;
    // Find the associated partition and decrement its reference count if found.
    QTreeWidgetItem *twitOrigin = findOrigin(dev);
    if(twitOrigin) ok = proc.exec("cryptsetup close " + dev, true);
    if(ok) {
        const int ixBD = listBlkDevs.findDevice(twitOrigin->text(Device));
        const int oMapCount = listBlkDevs.at(ixBD).mapCount;
        if(oMapCount > 0) listBlkDevs[ixBD].mapCount--;
        if(oMapCount <= 1) {
            twitOrigin->setIcon(Device, QIcon());
            twitComboBox(twitOrigin, UseFor)->setEnabled(true);
        }
        twitOrigin->setData(Device, Qt::UserRole, QVariant());
        twitSetFlag(twitOrigin, TwitFlag::AutoCrypto, false);
    }
    // Refresh virtual devices list.
    gui.treePartitions->blockSignals(true);
    scanVirtualDevices(true);
    comboFormatTextChange(QString()); // For the badblocks checkbox.
    gui.treePartitions->blockSignals(false);
    treeSelChange();

    gui.mainFrame->setEnabled(true);
    qApp->restoreOverrideCursor();
    // If not OK then a command failed, or trying to close a non-LUKS device.
    if(!ok) {
        QMessageBox::critical(master, master->windowTitle(),
            tr("Failed to close %1").arg(dev));
    }
}

QTreeWidgetItem *PartMan::addSubvolumeItem(QTreeWidgetItem *twit)
{
    QTreeWidgetItem *svit = new QTreeWidgetItem(twit);
    twitSetFlag(svit, TwitFlag::Subvolume, true);
    // Device
    svit->setText(Device, "-");
    // Size
    svit->setText(Size, "----");
    // Label
    QLineEdit *editLabel = new QLineEdit(gui.treePartitions);
    editLabel->setAutoFillBackground(true);
    gui.treePartitions->setItemWidget(svit, Label, editLabel);
    // Use For
    QComboBox *comboUse = new QComboBox(gui.treePartitions);
    comboUse->setAutoFillBackground(true);
    gui.treePartitions->setItemWidget(svit, UseFor, comboUse);
    comboUse->setFocusPolicy(Qt::StrongFocus);
    comboUse->installEventFilter(this);
    comboUse->setEditable(true);
    comboUse->setInsertPolicy(QComboBox::NoInsert);
    comboUse->addItem("");
    comboUse->addItem("root");
    // comboUse->addItem("swap"); - requires Linux 5.0 or later
    comboUse->addItem("home");
    comboUse->setProperty("row", QVariant::fromValue<void *>(svit));
    comboUse->lineEdit()->setPlaceholderText("----");
    connect(comboUse, &QComboBox::currentTextChanged, this, &PartMan::comboSubvolUseTextChange);
    // Format
    QComboBox *comboFormat = new QComboBox(gui.treePartitions);
    comboFormat->setAutoFillBackground(true);
    gui.treePartitions->setItemWidget(svit, Format, comboFormat);
    comboFormat->setFocusPolicy(Qt::StrongFocus);
    comboFormat->installEventFilter(this);
    comboFormat->setEnabled(false);
    comboFormat->addItem(tr("Create"));
    comboFormat->setProperty("row", QVariant::fromValue<void *>(svit));
    connect(comboFormat, &QComboBox::currentTextChanged, this, &PartMan::comboSubvolFormatTextChange);
    // Mount options
    QLineEdit *editOptions = new QLineEdit(gui.treePartitions);
    editOptions->setAutoFillBackground(true);
    gui.treePartitions->setItemWidget(svit, Options, editOptions);
    editOptions->setEnabled(false);
    editOptions->setText("noatime");
    editOptions->setProperty("row", QVariant::fromValue<void *>(svit));
    editOptions->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(editOptions, &QLineEdit::customContextMenuRequested, this, &PartMan::partOptionsMenu);

    return svit;
}
void PartMan::scanSubvolumes(QTreeWidgetItem *partit)
{
    qApp->setOverrideCursor(Qt::WaitCursor);
    gui.mainFrame->setEnabled(false);
    while(partit->childCount()) delete partit->child(0);
    mkdir("/mnt/btrfs-scratch", 0755);
    QStringList lines;
    if(!proc.exec("mount -o noatime " + twitMappedDevice(partit, true)
        + " /mnt/btrfs-scratch", true)) goto END;
    lines = proc.execOutLines("btrfs subvolume list /mnt/btrfs-scratch", true);
    proc.exec("umount /mnt/btrfs-scratch", true);
    for(const QString &line : lines) {
        const int start = line.indexOf("path") + 5;
        if(line.length() <= start) goto END;
        QTreeWidgetItem *svit = addSubvolumeItem(partit);
        const QString &label = line.mid(start);
        twitLineEdit(svit, Label)->setText(label);
        svit->setText(Label, label);
        QComboBox *comboFormat = twitComboBox(svit, Format);
        comboFormat->insertItem(0, tr("Preserve"), "PRESERVE");
        comboFormat->insertSeparator(1);
        comboFormat->setEnabled(true);
    }
 END:
    gui.mainFrame->setEnabled(true);
    qApp->restoreOverrideCursor();
}

// Mouse wheel event filter for partition tree objects
bool PartMan::eventFilter(QObject *object, QEvent *event)
{
    if(event->type() == QEvent::Wheel) {
        QWidget *widget = static_cast<QWidget *>(object);
        if(widget && !(widget->hasFocus())) return true;
    }
    return false;
}

QWidget *PartMan::composeValidate(bool automatic,
    const QString &minSizeText, const QString &project)
{
    QStringList msgForeignList;
    QString msgConfirm;
    bool encryptAny = false, encryptRoot = false;
    mounts.clear();
    // Partition use and other validation
    int mapnum=0, swapnum=0, formatnum=0;
    QTreeWidgetItemIterator it(gui.treePartitions);
    for(; *it; ++it) {
        if(twitFlag(*it, TwitFlag::Drive) || twitFlag(*it, TwitFlag::VirtualDevices)) continue;
        QComboBox *comboUse = twitComboBox(*it, UseFor);
        QLineEdit *editLabel = twitLineEdit(*it, Label);
        if(!comboUse || !editLabel || comboUse->currentText().isEmpty()) continue;
        if(twitFlag(*it, TwitFlag::Subvolume)) {
            bool ok = true;
            const QString &cmptext = editLabel->text().trimmed().toUpper();
            if(cmptext.isEmpty()) ok = false;
            QStringList check;
            QTreeWidgetItem *pit = (*it)->parent();
            const int count = pit->childCount();
            const int index = pit->indexOfChild(*it);
            for(int ixi = 0; ixi < count; ++ixi) {
                if(ixi==index) continue;
                if(twitLineEdit(pit->child(ixi), Label)->text().trimmed().toUpper() == cmptext) ok = false;
            }
            if(!ok) {
                QMessageBox::critical(master, master->windowTitle(), tr("Invalid subvolume label"));
                return editLabel;
            }
        }
        QString mount = translateUse(comboUse->currentText());
        const QString &devname = (*it)->text(Device);
        if(!mount.startsWith("/") && comboUse->findText(mount, Qt::MatchFixedString)<0) {
            QMessageBox::critical(master, master->windowTitle(),
                tr("Invalid use for %1: %2").arg(devname, mount));
            return comboUse;
        }
        if(mount=="FORMAT") mount += QString::number(++formatnum);
        else if(mount=="SWAP") {
            ++swapnum;
            if(swapnum>1) mount+=QString::number(swapnum);
        }
        QTreeWidgetItem *twit = mounts.value(mount);

        // The mount can only be selected once.
        if(twit) {
            QMessageBox::critical(master, master->windowTitle(), tr("%1 is already"
                " selected for: %2").arg(twit->text(Device), describeUse(mount)));
            return comboUse;
        } else {
            if(!mount.isEmpty()) mounts.insert(mount, *it);
            if(twitFlag(*it, TwitFlag::OldLayout) && twitWillFormat(*it)) {
                // Warn if using a non-Linux partition (potential install of another OS).
                const int bdindex = listBlkDevs.findDevice(devname);
                if (bdindex >= 0 && !listBlkDevs.at(bdindex).isNative) {
                    msgForeignList << devname << describeUse(mount);
                }
            }
        }
        if(!twitFlag(*it, TwitFlag::VirtualBD)) {
            QVariant mapperData;
            if((*it)->checkState(Encrypt) == Qt::Checked) {
                if(mount == "/") mapperData = "root.fsm";
                else if(mount.startsWith("SWAP")) mapperData = mount.toLower();
                else {
                    mapperData = QString::number(++mapnum)
                        + mount.replace('/','.') + ".fsm";
                }
                encryptAny = true;
            }
            (*it)->setData(Device, Qt::UserRole, mapperData);
        }
    }
    qDebug() << "Mount points:";
    for(const auto &it : mounts.toStdMap()) {
        qDebug() << " -" << it.first << '-' << it.second->text(Device)
            << twitMappedDevice(it.second) << twitMappedDevice(it.second, true);
    }

    if(encryptAny) {
        // TODO: Validate encryption settings.
    }
    QTreeWidgetItem *rootitem = mounts.value("/");
    if(rootitem) {
        if(!twitWillFormat(rootitem) && mounts.contains("/home")) {
            const QString errmsg = tr("Cannot preserve /home inside root (/) if"
                " a separate /home partition is also mounted.");
            QMessageBox::critical(master, master->windowTitle(), errmsg);
            return gui.treePartitions;
        }
        if(rootitem->checkState(Encrypt) == Qt::Checked) encryptRoot = true;
    } else {
        QMessageBox::critical(master, master->windowTitle(), tr("You must choose a root partition.\n"
            "The root partition must be at least %1.").arg(minSizeText));
        return gui.treePartitions;
    }
    if (encryptRoot && !mounts.contains("/boot")) {
        QMessageBox::critical(master, master->windowTitle(),
            tr("You must choose a separate boot partition when encrypting root."));
        return gui.treePartitions;
    }

    if(!automatic) {
        QString msg;
        QStringList msgFormatList;
        // Format (vs just mounting or configuring) partitions.
        for(const auto &it : mounts.toStdMap()) {
            const QString &dev = it.second->text(Device);
            if(twitWillFormat(it.second)) msgFormatList << dev << describeUse(it.first);
            else if(it.first == "/") {
                msgConfirm += " - " + tr("Delete the data on %1"
                    " except for /home").arg(dev) + "\n";
            } else {
                msgConfirm += " - " + tr("Reuse (no reformat) %1 as"
                    " %2").arg(dev, describeUse(it.first)) + "\n";
            }
        }

        int ans;
        const QString msgPartSel = " - " + tr("%1, to be used for %2") + "\n";
        // message to advise of issues found.
        if (msgForeignList.count() > 0) {
            msg += tr("The following partitions you selected are not Linux partitions:") + "\n\n";
            for (QStringList::Iterator it = msgForeignList.begin(); it != msgForeignList.end(); ++it) {
                QString &s = *it;
                msg += msgPartSel.arg(s, describeUse(*(++it)));
            }
            msg += "\n";
        }
        if (!msg.isEmpty()) {
            msg += tr("Are you sure you want to reformat these partitions?");
            ans = QMessageBox::warning(master, master->windowTitle(),
                msg, QMessageBox::Yes, QMessageBox::No);
            if (ans != QMessageBox::Yes) return gui.treePartitions;
        }
        // final message before the installer starts.
        msg.clear();
        if (msgFormatList.count() > 0) {
            msg += tr("The %1 installer will now format and destroy the data on the following partitions:").arg(project) + "\n\n";
            for (QStringList::Iterator it = msgFormatList.begin(); it != msgFormatList.end(); ++it) {
                QString &s = *it;
                msg += msgPartSel.arg(s, describeUse(*(++it)));
            }
            if (!msgConfirm.isEmpty()) msg += "\n";
        }
        if (!msgConfirm.isEmpty()) {
            msg += tr("The %1 installer will now perform the following actions:").arg(project);
            msg += "\n\n" + msgConfirm;
        }
        if(!msg.isEmpty()) {
            msg += "\n" + tr("These actions cannot be undone. Do you want to continue?");
            ans = QMessageBox::warning(master, master->windowTitle(),
                msg, QMessageBox::Yes, QMessageBox::No);
            if (ans != QMessageBox::Yes) return gui.treePartitions;
        }
    }

    calculatePartBD();
    return nullptr;
}

bool PartMan::calculatePartBD()
{
    listToUnmount.clear();
    const int driveCount = gui.treePartitions->topLevelItemCount();
    for(int ixDrive = 0; ixDrive < driveCount; ++ixDrive) {
        QTreeWidgetItem *drvit = gui.treePartitions->topLevelItem(ixDrive);
        const bool useExist = twitFlag(drvit, TwitFlag::OldLayout);
        QString drv = drvit->text(Device);
        int ixDriveBD = listBlkDevs.findDevice(drv);
        const int partCount = drvit->childCount();
        const long long driveSize = twitSize(drvit);

        if(!useExist) {
            // Remove partitions from the list that belong to this drive.
            const int ixRemoveBD = ixDriveBD + 1;
            if (ixDriveBD < 0) return false;
            while (ixRemoveBD < listBlkDevs.size() && !listBlkDevs.at(ixRemoveBD).isDrive) {
                listToUnmount << listBlkDevs.at(ixRemoveBD).name;
                listBlkDevs.removeAt(ixRemoveBD);
            }
            // see if GPT needs to be used (either UEFI or >=2TB drive)
            listBlkDevs[ixDriveBD].isGPT = gptoverride || uefi
                || driveSize >= 2097152 || partCount>4 || gptoverride;
        }
        // code for adding future partitions to the list
        for(int ixPart=0, ixPartBD=ixDriveBD+1; ixPart < partCount; ++ixPart, ++ixPartBD) {
            QTreeWidgetItem *twit = drvit->child(ixPart);
            const QString &useFor = twitUseFor(twit);
            if(useFor.isEmpty()) continue;
            QString bdFS;
            if(twit->checkState(Encrypt) == Qt::Checked) bdFS = QStringLiteral("crypto_LUKS");
            else bdFS = twitComboBox(twit, Format)->currentText();
            if(useExist) {
                BlockDeviceInfo &bdinfo = listBlkDevs[ixPartBD];
                listToUnmount << bdinfo.name;
                QLineEdit *editLabel = static_cast<QLineEdit *>(gui.treePartitions->itemWidget(twit, Label));
                if(editLabel) bdinfo.label = editLabel->text();
                if(twitWillFormat(twit) && !bdFS.isEmpty()) bdinfo.fs = bdFS;
                bdinfo.isNasty = false; // future partitions are safe
                bdinfo.isFuture = bdinfo.isNative = true;
                bdinfo.isESP = (useFor=="ESP");
            } else {
                BlockDeviceInfo bdinfo;
                bdinfo.name = twit->text(Device);
                bdinfo.fs = bdFS;
                bdinfo.size = twitSize(twit, true); // back into bytes
                bdinfo.isFuture = bdinfo.isNative = true;
                bdinfo.isGPT = listBlkDevs.at(ixDriveBD).isGPT;
                bdinfo.isESP = (useFor=="ESP");
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
    for(int ixi = 0; ixi < gui.treePartitions->topLevelItemCount(); ++ixi) {
        QTreeWidgetItem *drvit =  gui.treePartitions->topLevelItem(ixi);
        if(twitFlag(drvit, TwitFlag::VirtualDevices)) continue;
        QStringList purposes;
        for(int oxo = 0; oxo < drvit->childCount(); ++oxo) {
            const QString &useFor = twitUseFor(drvit->child(oxo));
            if(!useFor.isEmpty()) purposes << useFor;
        }
        // If any partitions are selected run the SMART tests.
        if (!purposes.isEmpty()) {
            const QString &drive = drvit->text(Device);
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

bool PartMan::luksMake(const QString &dev, const QByteArray &password)
{
    QString strCipherSpec = gui.comboFDEcipher->currentText()
        + "-" + gui.comboFDEchain->currentText();
    if (gui.comboFDEchain->currentText() != "ECB") {
        strCipherSpec += "-" + gui.comboFDEivgen->currentText();
        if (gui.comboFDEivgen->currentText() == "ESSIV") {
            strCipherSpec += ":" + gui.comboFDEivhash->currentData().toString();
        }
    }
    QString cmd = "cryptsetup --batch-mode"
        " --cipher " + strCipherSpec.toLower()
        + " --key-size " + gui.spinFDEkeysize->cleanText()
        + " --hash " + gui.comboFDEhash->currentText().toLower().remove('-')
        + " --use-" + gui.comboFDErandom->currentText()
        + " --iter-time " + gui.spinFDEroundtime->cleanText()
        + " luksFormat " + dev;
    if (!proc.exec(cmd, true, &password)) return false;
    proc.sleep(1000);
    return true;
}

bool PartMan::luksOpen(const QString &dev, const QString &luksfs,
    const QByteArray &password, const QString &options)
{
    QString cmd = "cryptsetup luksOpen " + dev;
    if (!luksfs.isEmpty()) cmd += " " + luksfs;
    if (!options.isEmpty()) cmd += " " + options;
    return proc.exec(cmd, true, &password);
}

QTreeWidgetItem *PartMan::selectedDriveAuto()
{
    QString drv(gui.diskCombo->currentData().toString());
    int bdindex = listBlkDevs.findDevice(drv);
    if (bdindex<0) return nullptr;
    for(int ixi = gui.treePartitions->topLevelItemCount()-1; ixi>=0; --ixi) {
        QTreeWidgetItem *drvit = gui.treePartitions->topLevelItem(ixi);
        if(drvit->text(Device) == drv) return drvit;
    }
    return nullptr;
}
int PartMan::layoutDefault(QTreeWidgetItem *drvit,
    int rootPercent, bool crypto, bool updateTree)
{
    if(rootPercent<0) rootPercent = gui.sliderPart->value();
    if(updateTree) clearLayout(drvit);
    // Drive geometry basics.
    const long long driveSize = twitSize(drvit);
    int rootFormatSize = driveSize-PARTMAN_SAFETY_MB;
    // Boot partitions.
    if(uefi) {
        if(updateTree) addItem(drvit, 256, "ESP", crypto);
        rootFormatSize -= 256;
    } else if(driveSize >= 2097152 || gptoverride) {
        if(updateTree) addItem(drvit, 1, "bios_grub", crypto);
        rootFormatSize -= 1;
    }
    int rootMinMB = rootSpaceNeeded / 1048576;
    const int bootMinMB = bootSpaceNeeded / 1048576;
    if(!crypto) rootMinMB += bootMinMB;
    else {
        int bootFormatSize = 512;
        if(bootFormatSize < bootMinMB) bootFormatSize = bootSpaceNeeded;
        if(updateTree) addItem(drvit, bootFormatSize, "boot", crypto);
        rootFormatSize -= bootFormatSize;
    }
    // Swap space.
    int swapFormatSize = rootFormatSize-rootMinMB;
    struct sysinfo sinfo;
    if(sysinfo(&sinfo)!=0) sinfo.totalram = 2048;
    else sinfo.totalram = (sinfo.totalram / (1048576*2)) * 3; // 1.5xRAM
    sinfo.totalram/=128; ++sinfo.totalram; sinfo.totalram*=128; // Multiple of 128MB
    if(swapFormatSize > (int)sinfo.totalram) swapFormatSize = sinfo.totalram;
    int swapMaxMB = rootFormatSize/(20*128); ++swapMaxMB; swapMaxMB*=128; // 5% root
    if(swapMaxMB > 8192) swapMaxMB = 8192; // 8GB cap for the whole calculation.
    if(swapFormatSize > swapMaxMB) swapFormatSize = swapMaxMB;
    rootFormatSize -= swapFormatSize;
    // Home
    int homeFormatSize = rootFormatSize;
    rootFormatSize = (rootFormatSize * rootPercent) / 100;
    if(rootFormatSize < rootMinMB) rootFormatSize = rootMinMB;
    homeFormatSize -= rootFormatSize;
    if(updateTree) {
        addItem(drvit, rootFormatSize, "root", crypto);
        if(swapFormatSize>0) addItem(drvit, swapFormatSize, "swap", crypto);
        if(homeFormatSize>0) addItem(drvit, homeFormatSize, "home", crypto);
        labelParts(drvit);
        treeSelChange();
    }
    return rootFormatSize;
}

int PartMan::countPrepSteps()
{
    int pcount = 0;
    // Creation of new partitions
    for(int ixi = gui.treePartitions->topLevelItemCount(); ixi>=0; --ixi) {
        QTreeWidgetItem *drvit = gui.treePartitions->topLevelItem(ixi);
        if(twitComboBox(drvit, Format)) pcount += drvit->childCount() + 1; // Partition table creation included.
    }
    // Formatting partitions
    pcount += mounts.count();
    QMapIterator<QString, QTreeWidgetItem *> mi(mounts);
    while(mi.hasNext()) {
        mi.next();
        QTreeWidgetItem *partit = mi.value();
        if(partit->checkState(Encrypt)==Qt::Checked) ++pcount; //LUKS format.
    }
    // Final count
    return pcount;
}

// Transplanted straight from minstall.cpp
void PartMan::clearPartitionTables(const QString &dev)
{
    //setup block size and offsets info
    QString bytes = proc.execOut("parted --script /dev/" + dev + " unit B print 2>/dev/null | sed -rn 's/^Disk.*: ([0-9]+)B$/\\1/ip\'");
    qDebug() << "bytes is " << bytes;
    int block_size = 512;
    int pt_size = 17 * 1024;
    int pt_count = pt_size / block_size;
    int total_blocks = bytes.toLongLong() / block_size;
    qDebug() << "total blocks is " << total_blocks;

    //clear primary partition table
    proc.exec("dd if=/dev/zero of=/dev/" + dev + " bs=" + QString::number(block_size) + " count=" + QString::number(pt_count));

    // Clear out sneaky iso-hybrid partition table
    proc.exec("dd if=/dev/zero of=/dev/" + dev +" bs=" + QString::number(block_size) + " count=" + QString::number(pt_count) + " seek=64");

    // clear secondary partition table
    if ( ! bytes.isEmpty()) {
        int offset = total_blocks - pt_count;
        proc.exec("dd conv=notrunc if=/dev/zero of=/dev/" + dev + " bs=" + QString::number(block_size) + " count=" + QString::number(pt_count) + " seek=" + QString::number(offset));
    }
}

bool PartMan::preparePartitions()
{
    proc.log(__PRETTY_FUNCTION__);

    // detach all existing partitions on the selected drive
    for (const QString &strdev : listToUnmount) {
        proc.exec("swapoff /dev/" + strdev, true);
        proc.exec("/bin/umount /dev/" + strdev, true);
    }

    qDebug() << " ---- PARTITION FORMAT SCHEDULE ----";
    for(const auto &it : mounts.toStdMap()) {
        QTreeWidgetItem *twit = it.second;
        qDebug().nospace().noquote() << it.first << ": "
            << twit->text(Device) << " (" << twitSize(twit) << "MB)";
    }

    // Clear the existing partition tables on devices which will have a new layout.
    for(int ixi = gui.treePartitions->topLevelItemCount()-1; ixi>=0; --ixi) {
        QTreeWidgetItem *twit = gui.treePartitions->topLevelItem(ixi);
        if(twitFlag(twit, TwitFlag::OldLayout)) continue;
        const QString &drv = twit->text(Device);
        proc.status(tr("Clearing existing partition tables"));
        clearPartitionTables(drv);
        const bool useGPT = listBlkDevs.at(listBlkDevs.findDevice(drv)).isGPT;
        if (!proc.exec("parted -s /dev/" + drv + " mklabel " + (useGPT ? "gpt" : "msdos"))) return false;
    }
    proc.sleep(1000);

    // Prepare partition tables, creating tables and partitions when needed.
    proc.status(tr("Preparing required partitions"));
    for(int ixi = gui.treePartitions->topLevelItemCount()-1; ixi>=0; --ixi) {
        QTreeWidgetItem *drvit = gui.treePartitions->topLevelItem(ixi);
        if(twitFlag(drvit, TwitFlag::VirtualDevices)) continue;
        const QString &drvdev = drvit->text(Device);
        const int devCount = drvit->childCount();
        if(twitFlag(drvit, TwitFlag::OldLayout)) {
            // Using existing partitions.
            QString cmd; // command to set the partition type
            const int ixBlkDev = listBlkDevs.findDevice(drvdev);
            if (ixBlkDev >= 0 && listBlkDevs.at(ixBlkDev).isGPT) {
                cmd = "/sbin/sgdisk /dev/%1 --typecode=%2:8303";
            } else {
                cmd = "/sbin/sfdisk /dev/%1 --part-type %2 83";
            }
            // Set the type for partitions that will be used in this installation.
            for(int ixdev=0; ixdev<devCount; ++ixdev) {
                QTreeWidgetItem *twit = drvit->child(ixdev);
                if(twitUseFor(twit).isEmpty()) continue;
                const QStringList &devsplit = BlockDeviceInfo::split("/dev/" + twit->text(Device));
                if(!proc.exec(cmd.arg(devsplit.at(0), devsplit.at(1)))) return false;
                proc.sleep(1000);
            }
        } else {
            // Creating new partitions.
            const QString cmdParted("parted -s --align optimal /dev/" + drvdev);
            long long start = 1; // start with 1 MB to aid alignment
            for(int ixdev=0; ixdev<devCount; ++ixdev) {
                QTreeWidgetItem *twit = drvit->child(ixdev);
                const QString &useFor = twitUseFor(twit);
                if(useFor.isEmpty()) continue;
                const QString type(useFor!="ESP" ? " mkpart primary " : " mkpart ESP ");
                const long long end = start + twitSize(twit);
                bool rc = proc.exec(cmdParted + type
                    + QString::number(start) + "MiB " + QString::number(end) + "MiB");
                if(!rc) return false;
                start = end;
                proc.sleep(1000);
            }
        }
    }
    proc.exec("partprobe -s", true);
    proc.sleep(1000);
    return true;
}
bool PartMan::formatPartitions()
{
    proc.log(__PRETTY_FUNCTION__);
    QString rootdev;
    QString swapdev;
    QString homedev;

    // set up LUKS containers
    const QByteArray &encPass = (gui.entireDiskButton->isChecked()
                                 ? gui.FDEpassword : gui.FDEpassCust)->text().toUtf8();
    const QString &statup = tr("Setting up LUKS encrypted containers");
    for(QTreeWidgetItem *twit : mounts) {
        const QString dev = "/dev/" + twit->text(Device);
        if(twit->checkState(Encrypt)!=Qt::Checked) continue;
        if(twitWillFormat(twit)) {
            proc.status(statup);
            if (!luksMake(dev, encPass)) return false;
        }
        proc.status(statup);
        if (!luksOpen(dev, twitMappedDevice(twit), encPass)) return false;
    }

    // Format partitions.
    const bool badblocks = gui.badblocksCheck->isChecked();
    for(const auto &it : mounts.toStdMap()) {
        QTreeWidgetItem *twit = it.second;
        if(twitFlag(twit, TwitFlag::Subvolume)) continue;
        if(!twitWillFormat(twit)) continue;
        const QString &dev = twitMappedDevice(twit, true);
        const QString &useFor = translateUse(twitComboBox(twit, UseFor)->currentText());
        const QString &fmtstatus = tr("Formatting: %1");
        if(useFor=="FORMAT") proc.status(fmtstatus.arg(dev));
        else proc.status(fmtstatus.arg(describeUse(it.first)));
        if(useFor=="ESP") {
            const QString &fmt = twitComboBox(twit, Format)->currentText();
            if (!proc.exec("mkfs.msdos -F "+fmt.mid(3)+' '+dev)) return false;
            // Sets boot flag and ESP flag.
            const QStringList &devsplit = BlockDeviceInfo::split(dev);
            proc.exec("parted -s /dev/" + devsplit.at(0)
                + " set " + devsplit.at(1) + " esp on");
        } else if(useFor.startsWith("SWAP")) {
            QString cmd("/sbin/mkswap " + dev);
            const QString &label = twitLineEdit(twit, Label)->text();
            if(!label.isEmpty()) cmd.append(" -L \"" + label + '"');
            if(!proc.exec(cmd, true)) return false;
        } else {
            const QString &format = twitComboBox(twit, Format)->currentText();
            if(!formatLinuxPartition(dev, format, badblocks,
                twitLineEdit(twit, Label)->text())) return false;
        }
        proc.sleep(1000);
    }
    // Prepare subvolumes on all that (are to) contain them.
    QTreeWidgetItemIterator it(gui.treePartitions);
    for(; *it; ++it) {
        if(!twitFlag(*it, TwitFlag::Partition)) continue;
        else if(!prepareSubvolumes(*it)) return false;
    }

    return true;
}

// Transplanted straight from minstall.cpp
bool PartMan::formatLinuxPartition(const QString &dev, const QString &format, bool chkBadBlocks, const QString &label)
{
    proc.log(__PRETTY_FUNCTION__);

    QString cmd;
    if (format == "reiserfs") {
        cmd = "mkfs.reiserfs -q";
    } else if (format == "btrfs") {
        // btrfs and set up fsck
        proc.exec("/bin/cp -fp /bin/true /sbin/fsck.auto");
        // set creation options for small drives using btrfs
        QString size_str = proc.execOut("/sbin/sfdisk -s " + dev);
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

    cmd.append(" " + dev);
    if (!label.isEmpty()) {
        if (format == "reiserfs" || format == "f2fs") cmd.append(" -l \"");
        else cmd.append(" -L \"");
        cmd.append(label + "\"");
    }
    if (!proc.exec(cmd)) return false;

    if (format.startsWith("ext")) {
        // ext4 tuning
        proc.exec("/sbin/tune2fs -c0 -C0 -i1m " + dev);
    }
    return true;
}

bool PartMan::prepareSubvolumes(QTreeWidgetItem *partit)
{
    const int svcount = partit->childCount();
    if(svcount<=0) return true;
    proc.status(tr("Preparing subvolumes"));
    mkdir("/mnt/btrfs-scratch", 0755);
    if(!proc.exec("mount -o noatime " + twitMappedDevice(partit, true)
        + " /mnt/btrfs-scratch", true)) return false;
    bool ok = true;
    for(int ixi = 0; ok && ixi < svcount; ++ixi) {
        QTreeWidgetItem *svit = partit->child(ixi);
        if(twitWillFormat(svit)) {
            const QString &subvol = twitLineEdit(svit, Label)->text();
            proc.exec("btrfs subvolume delete /mnt/btrfs-scratch/" + subvol, true);
            if(!proc.exec("btrfs subvolume create /mnt/btrfs-scratch/" + subvol, true)) ok = false;
        }
    }
    if(!proc.exec("umount /mnt/btrfs-scratch", true)) return false;
    return ok;
}

// write out crypttab if encrypting for auto-opening
bool PartMan::fixCryptoSetup(const QString &keyfile, bool isNewKey)
{
    QFile file("/mnt/antiX/etc/crypttab");
    if(!file.open(QIODevice::WriteOnly)) return false;
    QTextStream out(&file);
    const QLineEdit *passedit = gui.checkBoxEncryptAuto->isChecked()
        ? gui.FDEpassword : gui.FDEpassCust;
    const QByteArray password(passedit->text().toUtf8());
    const QString cmdAddKey("cryptsetup luksAddKey /dev/%1 /mnt/antiX" + keyfile);
    const QString cmdBlkID("blkid -s UUID -o value /dev/");
    // Find the file system device which contains the key
    QString keyMount('/'); // If no mount point matches, it's in a directory on the root.
    const bool noKey = keyfile.isEmpty();
    if(!noKey) {
        for(const auto &it : mounts.toStdMap()) {
            if(keyfile.startsWith(it.first+'/')) keyMount = it.first;
        }
    }
    // Find extra devices
    QMap<QString, QString> extraAdd;
    QTreeWidgetItemIterator it(gui.treePartitions);
    for(; *it; ++it) {
        if(twitUseFor(*it).isEmpty() && twitFlag(*it, TwitFlag::AutoCrypto)) {
            extraAdd.insert((*it)->text(Device), twitMappedDevice(*it));
        }
    }
    // File systems
    for(auto &it : mounts.toStdMap()) {
        if(it.second->checkState(Encrypt)!=Qt::Checked) continue;
        if(it.first.startsWith("FORMAT") && !twitFlag(it.second, TwitFlag::AutoCrypto)) continue;
        const QString &dev = it.second->text(Device);
        QString uuid = proc.execOut(cmdBlkID + dev);
        out << twitMappedDevice(it.second, false) << " /dev/disk/by-uuid/" << uuid;
        if(noKey || it.first==keyMount) out << " none";
        else {
            if(isNewKey) {
                if(!proc.exec(cmdAddKey.arg(dev), true, &password)) return false;
            }
            out << ' ' << keyfile;
        }
        if(!it.first.startsWith("SWAP")) out << " luks \n";
        else out << " luks,nofail \n";
        extraAdd.remove(dev);
    }
    // Extra devices
    for(auto &it : extraAdd.toStdMap()) {
        QString uuid = proc.execOut(cmdBlkID + it.first);
        out << it.second << " /dev/disk/by-uuid/" << uuid;
        if(noKey) out << " none";
        else {
            if(isNewKey) {
                if(!proc.exec(cmdAddKey.arg(it.first), true, &password)) return false;
            }
            out << ' ' << keyfile;
        }
        out << " luks,nofail \n";
    }
    // Update cryptdisks if the key is not in the root file system.
    if(keyMount!='/') {
        if(!proc.exec("sed -i 's/^CRYPTDISKS_MOUNT.*$/CRYPTDISKS_MOUNT=\"\\" + keyMount
            + "\"/' /mnt/antiX/etc/default/cryptdisks", false)) return false;
    }
    file.close();
    return true;
}

// create /etc/fstab file
bool PartMan::makeFstab(bool populateMediaMounts)
{
    QFile file("/mnt/antiX/etc/fstab");
    if(!file.open(QIODevice::WriteOnly)) return false;
    QTextStream out(&file);
    out << "# Pluggable devices are handled by uDev, they are not in fstab\n";
    const QString cmdBlkID("blkid -o value UUID -s UUID ");
    // File systems and swap space.
    for(auto &it : mounts.toStdMap()) {
        if(it.first.startsWith("FORMAT")) continue; // Format without mounting.
        if(it.first == "ESP") continue; // EFI will be dealt with later.
        const QString &dev = twitMappedDevice(it.second, true);
        qDebug() << "Creating fstab entry for:" << it.first << dev;
        // Device ID or UUID
        if(twitWillMap(it.second)) out << dev;
        else out << "UUID=" + proc.execOut(cmdBlkID + dev);
        // Mount point, file system
        QString fsfmt = proc.execOut("blkid " + dev + " -o value -s TYPE");
        if(fsfmt=="swap") out << " swap swap";
        else out << ' ' << it.first << ' ' << fsfmt;
        // Options
        const QString &mountopts = twitLineEdit(it.second, Options)->text();
        if(!twitFlag(it.second, TwitFlag::Subvolume)) {
            if(mountopts.isEmpty()) out << " defaults";
            else out << ' ' << mountopts;
        } else {
            out << " subvol=" << twitLineEdit(it.second, Label)->text();
            if(!mountopts.isEmpty()) out << ',' << mountopts;
        }
        // Dump, pass
        if(fsfmt=="swap") out << " 0 0\n";
        else if(fsfmt.startsWith("reiser")) out << " 0 0\n";
        else if(it.first=="/boot") out << " 1 1\n";
        else if(it.first=="/") {
            if(fsfmt=="btrfs") out << " 1 0\n";
            else out << " 1 1\n";
        } else out << " 1 2\n";
    }
    // EFI System Partition
    if (gui.grubEspButton->isChecked()) {
        const QString espdev = "/dev/" + gui.grubBootCombo->currentData().toString();
        const QString espdevUUID = "UUID=" + proc.execOut(cmdBlkID + espdev);
        qDebug() << "espdev" << espdev << espdevUUID;
        out << espdevUUID + " /boot/efi vfat noatime,dmask=0002,fmask=0113 0 0\n";
    }
    file.close();
    if (populateMediaMounts) {
        if(!proc.exec("/sbin/make-fstab -O --install=/mnt/antiX"
            " --mntpnt=/media")) return false;
    }
    return true;
}

bool PartMan::mountPartitions()
{
    proc.log(__PRETTY_FUNCTION__);
    for(auto &it : mounts.toStdMap()) {
        if(it.first.at(0)!='/') continue;
        const QString point("/mnt/antiX" + it.first);
        const QString &dev = twitMappedDevice(it.second, true);
        proc.status(tr("Mounting: %1").arg(dev));
        if(!proc.mkpath(point)) return false;
        if(it.first=="/boot") {
             // needed to run fsck because sfdisk --part-type can mess up the partition
            if(!proc.exec("fsck.ext4 -y " + dev)) return false;
        }
        QStringList opts;
        if(twitFlag(it.second, TwitFlag::Subvolume)) {
            opts.append("subvol=" + twitLineEdit(it.second, Label)->text());
        }
        // Use noatime to speed up the installation.
        opts.append(twitLineEdit(it.second, Options)->text().split(','));
        opts.removeAll("ro");
        opts.removeAll("defaults");
        opts.removeAll("atime");
        opts.removeAll("relatime");
        if(!opts.contains("noatime")) opts << "noatime";
        const QString &cmd = QString("/bin/mount %1 %2 -o %3").arg(dev, point, opts.join(','));
        if(!proc.exec(cmd)) return false;
    }
    return true;
}

void PartMan::unmount()
{
    QMapIterator<QString, QTreeWidgetItem *> it(mounts);
    it.toBack();
    while(it.hasPrevious()) {
        it.previous();
        if(it.key().at(0)!='/') continue;
        if(!it.key().startsWith("SWAP")) {
            proc.exec("/bin/umount -l /mnt/antiX" + it.key(), true);
        }
        proc.exec("/bin/umount -l /mnt/antiX" + it.key(), true);
        QTreeWidgetItem *twit = it.value();
        if(twit->checkState(Encrypt)==Qt::Checked) {
            QString cmd("cryptsetup close %1");
            proc.exec(cmd.arg(twitMappedDevice(twit)), true);
        }
    }
}

// Public properties
bool PartMan::willFormat(const QString &point)
{
    QTreeWidgetItem *twit = mounts.value(point);
    if(twit) return twitWillFormat(twit);
    return false;
}
QString PartMan::getMountDev(const QString &point, const bool mapped)
{
    const QTreeWidgetItem *twit = mounts.value(point);
    if(!twit) return QString();
    if(mapped) return twitMappedDevice(twit, true);
    QString rstr("/dev/");
    if(twitFlag(twit, TwitFlag::VirtualBD)) rstr.append("mapper/");
    return rstr.append(twit->text(Device));
}
int PartMan::swapCount()
{
    int count = 0;
    for(const auto &mount : mounts.toStdMap()) {
        if(mount.first.startsWith("SWAP")) ++count;
    }
    return count;
}
int PartMan::isEncrypt(const QString &point)
{
    int count = 0;
    if(point.isEmpty()) {
        for(QTreeWidgetItem *twit : mounts) {
            if(twit->checkState(Encrypt)==Qt::Checked) ++count;
        }
    } else if(point=="SWAP") {
        for(const auto &mount : mounts.toStdMap()) {
            if(mount.first.startsWith("SWAP")
                && mount.second->checkState(Encrypt)==Qt::Checked) ++count;
        }
    } else {
        const QTreeWidgetItem *twit = mounts.value(point);
        if(twit && twit->checkState(Encrypt)==Qt::Checked) ++count;
    }
    return count;
}

QTreeWidgetItem *PartMan::findOrigin(const QString &vdev)
{
    QStringList lines = proc.execOutLines("cryptsetup status " + vdev, true);
    // Find the associated partition and decrement its reference count if found.
    for(const QString &line : lines) {
        const QString &trline = line.trimmed();
        if(trline.startsWith("device:")) {
            const QString &trdev = trline.mid(trline.lastIndexOf('/')+1);
            QTreeWidgetItemIterator it(gui.treePartitions);
            for(; *it; ++it) {
                if((*it)->text(Device) == trdev) return *it;
            }
            return nullptr;
        }
    }
    return nullptr;
}

// Helpers
inline void PartMan::drvitMarkLayout(QTreeWidgetItem *drvit, const bool old)
{
    if(old) drvit->setIcon(Device, QIcon());
    else drvit->setIcon(Device, QIcon(":/appointment-soon"));
    twitSetFlag(drvit, TwitFlag::OldLayout, old);
    gui.treePartitions->resizeColumnToContents(Device);
}
inline bool PartMan::drvitIsLocked(const QTreeWidgetItem *drvit)
{
    const int partCount = drvit->childCount();
    for(int ixPart=0; ixPart<partCount; ++ixPart) {
        if(!(twitComboBox(drvit->child(ixPart), UseFor)->isEnabled())) return true;
    }
    return false;
}
inline bool PartMan::twitFlag(const QTreeWidgetItem *twit, const TwitFlag flag) const
{
    return !!(twit->data(Options, Qt::UserRole).toUInt() & (1 << flag));
}
inline void PartMan::twitSetFlag(QTreeWidgetItem *twit, const TwitFlag flag, const bool value)
{
    unsigned int newFlags = twit->data(Options, Qt::UserRole).toUInt();
    const unsigned int bit = 1 << flag;
    if(value != !!(newFlags & bit)) {
        if(value) newFlags |= bit;
        else newFlags &= ~bit;
        twit->setData(Options, Qt::UserRole, QVariant(newFlags));
    }
}
inline bool PartMan::twitCanUse(QTreeWidgetItem *twit)
{
    QComboBox *combo = twitComboBox(twit, UseFor);
    return combo ? combo->isEnabled() : false;
}
inline long long PartMan::twitSize(QTreeWidgetItem *twit, const bool bytes)
{
    QWidget *spin = gui.treePartitions->itemWidget(twit, Size);
    long long size = 0;
    if(spin) {
        size = static_cast<QSpinBox *>(spin)->value();
        if(bytes) size *= 1048576;
    } else {
        size = twit->data(Size, Qt::UserRole).toLongLong();
        if(!bytes) size /= 1048576;
    }
    return size;
}
inline bool PartMan::twitWillFormat(QTreeWidgetItem *twit)
{
    QComboBox *combo = twitComboBox(twit, Format);
    if(!combo) return true;
    return (combo->currentData(Qt::UserRole)!="PRESERVE" && !twitUseFor(twit).isEmpty());
}
inline QString PartMan::twitUseFor(QTreeWidgetItem *twit)
{
    QComboBox *combo = twitComboBox(twit, UseFor);
    if(!combo) return QString();
    return translateUse(combo->currentText());
}
inline bool PartMan::twitWillMap(const QTreeWidgetItem *twit) const
{
    if(twit->parent()==nullptr) return false;
    return !(twit->data(Device, Qt::UserRole).isNull());
}
inline QString PartMan::twitMappedDevice(const QTreeWidgetItem *twit, const bool full) const
{
    if(twitFlag(twit, TwitFlag::Subvolume)) twit = twit->parent();
    if(twitFlag(twit, TwitFlag::Partition)) {
        const QVariant &d = twit->data(Device, Qt::UserRole);
        if(!d.isNull()) {
            if(full) return "/dev/mapper/" + d.toString();
            return d.toString();
        }
    }
    if(!full) return twit->text(Device);
    return "/dev/" + twit->text(Device);
}
inline QComboBox *PartMan::twitComboBox(QTreeWidgetItem  *twit, int column)
{
    return static_cast<QComboBox *>(gui.treePartitions->itemWidget(twit, column));
}
inline QLineEdit *PartMan::twitLineEdit(QTreeWidgetItem  *twit, int column)
{
    return static_cast<QLineEdit *>(gui.treePartitions->itemWidget(twit, column));
}
