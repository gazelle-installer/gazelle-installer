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

#include <sys/stat.h>
#include <QDebug>
#include <QTimer>
#include <QLocale>
#include <QMessageBox>
#include <QTreeWidget>
#include <QSpinBox>
#include <QComboBox>
#include <QLineEdit>

#include "partman.h"

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
    connect(gui.treePartitions, &QTreeWidget::itemChanged, this, &PartMan::treeItemChange);
    connect(gui.treePartitions, &QTreeWidget::itemSelectionChanged, this, &PartMan::treeSelChange);
    connect(gui.buttonPartClear, &QToolButton::clicked, this, &PartMan::partClearClick);
    connect(gui.buttonPartAdd, &QToolButton::clicked, this, &PartMan::partAddClick);
    connect(gui.buttonPartRemove, &QToolButton::clicked, this, &PartMan::partRemoveClick);
    connect(gui.buttonPartDefault, &QToolButton::clicked, this, &PartMan::partDefaultClick);
    gui.buttonPartAdd->setEnabled(false);
    gui.buttonPartRemove->setEnabled(false);
    gui.buttonPartClear->setEnabled(false);
    gui.buttonPartDefault->setEnabled(false);
    defaultLabels["ESP"] = "EFI System";
    defaultLabels["bios_grub"] = "BIOS GRUB";
}

void PartMan::populate(QTreeWidgetItem *drvstart)
{
    gui.treePartitions->blockSignals(true);
    if(!drvstart) gui.treePartitions->clear();
    QTreeWidgetItem *curdrv = nullptr;
    for (const BlockDeviceInfo &bdinfo : listBlkDevs) {
        QTreeWidgetItem *curdev;
        if (bdinfo.isDrive) {
            if(!drvstart) curdrv = new QTreeWidgetItem(gui.treePartitions);
            else if(bdinfo.name == drvstart->text(Device)) curdrv = drvstart;
            else if(!curdrv) continue; // Skip until the drive is drvstart.
            else break; // Exit the loop early if the drive isn't drvstart.
            curdev = curdrv;
            curdrv->setData(Device, Qt::UserRole, QVariant(false)); // drive starts off "unused"
            // Model
            curdev->setText(Label, bdinfo.model);
        } else {
            if(!curdrv) continue;
            curdev = new QTreeWidgetItem(curdrv);
            setupItem(curdev, &bdinfo);
            curdrv->setData(Device, Qt::UserRole, QVariant(true)); // drive is now "used"

        }
        // Device name and size
        curdev->setText(Device, bdinfo.name);
        curdev->setText(Size, QLocale::system().formattedDataSize(bdinfo.size, 1, QLocale::DataSizeTraditionalFormat));
        curdev->setData(Size, Qt::UserRole, QVariant(bdinfo.size));
    }
    gui.treePartitions->expandAll();
    for (int ixi = gui.treePartitions->columnCount() - 1; ixi >= 0; --ixi) {
        if(ixi != Label) gui.treePartitions->resizeColumnToContents(ixi);
    }
    gui.treePartitions->blockSignals(false);
}

inline QTreeWidgetItem *PartMan::addItem(QTreeWidgetItem *parent,
    int defaultMB, const QString &defaultUse) {
    QTreeWidgetItem *twit = new QTreeWidgetItem(parent);
    setupItem(twit, nullptr, defaultMB, defaultUse);
    return twit;
}
QTreeWidgetItem *PartMan::setupItem(QTreeWidgetItem *twit, const BlockDeviceInfo *bdinfo,
    int defaultMB, const QString &defaultUse)
{
    // Size
    if(!bdinfo) {
        QSpinBox *spinSize = new QSpinBox(gui.treePartitions);
        spinSize->setAutoFillBackground(true);
        gui.treePartitions->setItemWidget(twit, Size, spinSize);
        const int maxMB = twit->parent()->data(Size, Qt::UserRole).toLongLong() / 1048576;
        spinSize->setRange(1, maxMB);
        spinSize->setValue(defaultMB);
    }
    // Label
    QLineEdit *editLabel = new QLineEdit(gui.treePartitions);
    editLabel->setAutoFillBackground(true);
    gui.treePartitions->setItemWidget(twit, Label, editLabel);
    editLabel->setEnabled(false);
    if(bdinfo) {
        twit->setText(Label, bdinfo->label);
        editLabel->setText(bdinfo->label);
    }
    // Use For
    QComboBox *comboUse = new QComboBox(gui.treePartitions);
    comboUse->setAutoFillBackground(true);
    gui.treePartitions->setItemWidget(twit, UseFor, comboUse);
    comboUse->setEditable(true);
    comboUse->setInsertPolicy(QComboBox::NoInsert);
    comboUse->addItem("");
    if(!bdinfo || bdinfo->size <= 16) comboUse->addItem("bios_grub");
    if(!bdinfo || bdinfo->size <= 4294967296) {
        comboUse->addItem("ESP");
        comboUse->addItem("boot");
    }
    comboUse->addItem("root");
    comboUse->addItem("swap");
    comboUse->addItem("home");
    comboUse->setProperty("row", QVariant::fromValue<void *>(twit));
    comboUse->lineEdit()->setPlaceholderText("----");
    connect(comboUse, &QComboBox::currentTextChanged, this, &PartMan::comboUseTextChange);
    // Type
    QComboBox *comboType = new QComboBox(gui.treePartitions);
    comboType->setAutoFillBackground(true);
    gui.treePartitions->setItemWidget(twit, Type, comboType);
    comboType->setEnabled(false);
    if(bdinfo) {
        twit->setText(Type, bdinfo->fs);
        comboType->addItem(bdinfo->fs);
    }
    connect(comboType, &QComboBox::currentTextChanged, this, &PartMan::comboTypeTextChange);
    // Mount options
    QLineEdit *editOptions = new QLineEdit(gui.treePartitions);
    editOptions->setAutoFillBackground(true);
    gui.treePartitions->setItemWidget(twit, Options, editOptions);
    editOptions->setEnabled(false);
    editOptions->setText("defaults");

    if(!defaultUse.isEmpty()) comboUse->setCurrentText(defaultUse);
    return twit;
}

void PartMan::labelParts(QTreeWidgetItem *drive)
{
    QString name = drive->text(Device);
    if (name.startsWith("nvme") || name.startsWith("mmcblk")) name += "p";
    name += "%1";
    for(int ixi = drive->childCount() - 1; ixi >= 0; --ixi) {
        drive->child(ixi)->setText(Device, name.arg(ixi+1));
    }
}

QString PartMan::translateUse(const QString &alias)
{
    if(alias == "root") return QStringLiteral("/");
    else if(alias == "boot") return QStringLiteral("/boot");
    else if(alias == "home") return QStringLiteral("/home");
    else return alias;
}

void PartMan::setEncryptChecks(const QString &use, enum Qt::CheckState state)
{
    QTreeWidgetItemIterator it(gui.treePartitions, QTreeWidgetItemIterator::NoChildren);
    while (*it) {
        QComboBox *comboUse = twitComboBox(*it, UseFor);
        if(comboUse && !(comboUse->currentText().isEmpty())) {
            if(translateUse(comboUse->currentText()) == use) {
                (*it)->setCheckState(Encrypt, state);
            }
        }
        ++it;
    }
}

void PartMan::comboUseTextChange(const QString &text)
{
    gui.treePartitions->blockSignals(true);
    QComboBox *combo = static_cast<QComboBox *>(sender());
    if(!combo) return;
    QTreeWidgetItem *item = static_cast<QTreeWidgetItem *>(combo->property("row").value<void *>());
    if(!item) return;
    QLineEdit *editLabel = static_cast<QLineEdit *>(gui.treePartitions->itemWidget(item, Label));

    const QString &usetext = translateUse(text);
    int useClass = -1;
    if(usetext.isEmpty()) useClass = 0;
    else if(usetext == "ESP") useClass = 1;
    else if(usetext == "bios_grub") useClass = 2;
    else if(usetext == "/boot") useClass = 3;
    else if(usetext == "swap") useClass = 4;
    else useClass = 5;
    int oldUseClass = combo->property("class").toInt();
    if(useClass != oldUseClass) {
        QComboBox *comboType = twitComboBox(item, Type);
        comboType->clear();
        const QString &curtype = item->text(Type);
        switch(useClass) {
        case 0:
            editLabel->setText(item->text(Label));
            comboType->addItem(curtype);
            break;
        case 1: comboType->addItem("FAT32"); break;
        case 2: comboType->addItem("GRUB"); break;
        case 3: comboType->addItem("ext4"); break;
        case 4: comboType->addItem("SWAP"); break;
        default:
            comboType->addItem("ext4");
            comboType->addItem("ext3");
            comboType->addItem("ext2");
            comboType->addItem("f2fs");
            comboType->addItem("jfs");
            comboType->addItem("xfs");
            comboType->addItem("btrfs");
            comboType->addItem("reiserfs");
            comboType->addItem("reiser4");
        }
        // Changing to and from a mount/use that support encryption.
        if(useClass >= 0 && useClass <= 3) {
            item->setData(Encrypt, Qt::CheckStateRole, QVariant());
        } else if(oldUseClass >= 0 && oldUseClass <= 3) {
            item->setCheckState(Encrypt, encryptCheckRoot);
        }
        if(!(item->checkState(Encrypt))
            && comboType->findText(curtype, Qt::MatchFixedString)>=0) {
            // Add an item at the start to allow preserving the existing format.
            comboType->insertItem(0, tr("%1 (keep)").arg(curtype), QVariant(true));
            comboType->insertSeparator(1);
        }
        comboType->setEnabled(comboType->count()>1);
        // Label and options
        editLabel->setEnabled(useClass!=0);
        gui.treePartitions->itemWidget(item, Options)->setEnabled(useClass!=0);
        combo->setProperty("class", QVariant(useClass));
    }
    editLabel->setText(defaultLabels.value(usetext));
    gui.treePartitions->blockSignals(false);
}

void PartMan::comboTypeTextChange(const QString &)
{
    QTreeWidgetItemIterator it(gui.treePartitions, QTreeWidgetItemIterator::NoChildren);
    bool canCheckBlocks = false;
    while (*it) {
        QComboBox *comboUse = twitComboBox(*it, UseFor);
        if(comboUse && !(comboUse->currentText().isEmpty())) {
            QComboBox *comboType = twitComboBox(*it, Type);
            QLineEdit *editOpts = twitLineEdit(*it, Options);
            if(!comboType || !editOpts) return;
            const QString &type = comboType->currentText();
            if(type.startsWith("ext") || type == "jfs") canCheckBlocks = true;
            else if(type.startsWith("reiser")) editOpts->setText("defaults,notail");
            else editOpts->setText("defaults");
        }
        ++it;
    }
    gui.badblocksCheck->setEnabled(canCheckBlocks);
}

void PartMan::treeItemChange(QTreeWidgetItem *item, int column)
{
    if(column == Encrypt) {
        gui.treePartitions->blockSignals(true);
        QComboBox *comboUse = twitComboBox(item, UseFor);
        if(comboUse) {
            const QString use = translateUse(comboUse->currentText());
            if(use == "/") {
                encryptCheckRoot = item->checkState(column);
                if(encryptCheckRoot) {
                    setEncryptChecks("swap", Qt::Checked);
                    setEncryptChecks("/home", Qt::Checked);
                }
            } else if(use == "swap") {
                if(item->checkState(column) == Qt::Checked) setEncryptChecks("swap", Qt::Checked);
            } else if(use == "/home") {
                if(item->checkState(column) == Qt::Checked) setEncryptChecks("swap", Qt::Checked);
                else setEncryptChecks("swap", encryptCheckRoot);
            }
        }
        bool needsCrypto = false;
        QTreeWidgetItemIterator it(gui.treePartitions, QTreeWidgetItemIterator::NoChildren);
        while (*it) {
            if((*it)->checkState(Encrypt) == Qt::Checked) needsCrypto = true;
            ++it;
        }
        gui.gbEncrPass->setEnabled(needsCrypto);
        gui.treePartitions->blockSignals(false);
    }
}

void PartMan::treeSelChange()
{
    QTreeWidgetItem *twit = gui.treePartitions->selectedItems().value(0);
    QIcon iconClear = QIcon::fromTheme("star-new-symbolic");
    if(twit) {
        bool used = true;
        const bool isDrive = (twit->parent()==nullptr);
        if(isDrive) used = twit->data(Device, Qt::UserRole).toBool();
        else used = twit->parent()->data(Device, Qt::UserRole).toBool();
        gui.buttonPartClear->setEnabled(isDrive);
        if(!used) iconClear = QIcon::fromTheme("undo");
        gui.buttonPartAdd->setEnabled(!used);
        gui.buttonPartRemove->setEnabled(!used && twit->parent());
        gui.buttonPartDefault->setEnabled(isDrive);
    } else {
        gui.buttonPartAdd->setEnabled(false);
        gui.buttonPartRemove->setEnabled(false);
        gui.buttonPartDefault->setEnabled(false);
    }
    gui.buttonPartClear->setIcon(iconClear);
}

void PartMan::partClearClick(bool)
{
    QTreeWidgetItem *twit = gui.treePartitions->selectedItems().value(0);
    if(!twit || twit->parent()) return;
    while(twit->childCount()) twit->removeChild(twit->child(0));
    if(twit->data(Device, Qt::UserRole).toBool() == false) populate(twit);
    else twit->setData(Device, Qt::UserRole, QVariant(false));
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
    setupItem(part, nullptr);

    labelParts(drive);
    part->setSelected(true);
    twit->setSelected(false);
}

void PartMan::partRemoveClick(bool)
{
    QTreeWidgetItem *twit = gui.treePartitions->selectedItems().value(0);
    if(!twit || !(twit->parent())) return;
    twit->parent()->removeChild(twit);
}

void PartMan::partDefaultClick(bool)
{
    layoutDefault(gui.treePartitions->selectedItems().value(0), 40, true);
    treeSelChange();
}

QWidget *PartMan::composeValidate(const QString &minSizeText, const QString &project)
{
    QStringList msgForeignList;
    QString msgConfirm;
    QStringList msgFormatList;
    bool encryptAny = false, encryptRoot = false;
    mounts.clear();
    swaps.clear();
    QTreeWidgetItemIterator it(gui.treePartitions, QTreeWidgetItemIterator::NoChildren);
    for(; *it; ++it) {
        QComboBox *comboUse = twitComboBox(*it, UseFor);
        if(!comboUse || comboUse->currentText().isEmpty()) continue;
        QString mount = translateUse(comboUse->currentText());
        const QString &devname = (*it)->text(Device);
        if(!mount.startsWith("/") && comboUse->findText(mount)<0) {
            QMessageBox::critical(master, master->windowTitle(),
                tr("Invalid use for %1: %2").arg(devname, mount));
            return comboUse;
        }
        if(mount == "swap") {
            swaps << *it;
            mount.clear();
        }
        // Mount description
        QString desc;
        if(mount == "/") desc = "/ (root)";
        else if(mount.isEmpty()) desc = tr("swap space");
        else {
            if(mount == "ESP") desc = tr("EFI System Partition");
            else desc = mount;
            if(mount != "/home") msgFormatList << devname << desc;
        }
        QTreeWidgetItem *twit = mounts.value(mount);

        // The mount can only be selected once.
        if(twit) {
            QMessageBox::critical(master, master->windowTitle(),
                tr("%1 is already selected for: %2").arg(twit->text(Device), desc));
            return comboUse;
        } else {
            if(!mount.isEmpty()) mounts.insert(mount, *it);
            // Warn if using a non-Linux partition (potential install of another OS).
            const int bdindex = listBlkDevs.findDevice(devname);
            if (bdindex >= 0 && !listBlkDevs.at(bdindex).isNative) {
                msgForeignList << devname << desc;
            }
        }
        QVariant mapperData;
        if((*it)->checkState(Encrypt) == Qt::Checked) {
            if(mount.isEmpty()) mapperData = QString("fs.swap%1").arg(swaps.count()-1);
            else if(mount == "/") mapperData = "fs.root";
            else mapperData = QString("fs%1").arg(mounts.count()) + mount.replace('/', '.');
            encryptAny = true;
        }
        (*it)->setData(Device, Qt::UserRole, mapperData);
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
        const bool formatRoot = twitWillFormat(rootitem);
        if(!formatRoot && mounts.contains("/home")) {
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
        // Format (vs just configuring) swap partitions?
        for(QTreeWidgetItem *swap : swaps) {
            const QString &dev = swap->text(Device);
            if (twitWillFormat(swap)) msgFormatList << dev << tr("swap space");
            else msgConfirm += " - " + tr("Configure %1 as swap space").arg(dev) + "\n";
        }
        // Format (vs just mounting) other partitions?
        for(const auto &it : mounts.toStdMap()) {
            const QString &dev = it.second->text(Device);
            if(twitWillFormat(it.second)) msgFormatList << dev << it.first;
            else if(it.first == "/") {
                msgConfirm += " - " + tr("Delete the data on %1"
                    " except for /home").arg(dev) + "\n";
            } else {
                msgConfirm += " - " + tr("Reuse (no reformat) %1 as the"
                    " %2 partition").arg(dev, it.first) + "\n";
            }
        }

        int ans;
        const QString msgPartSel = " - " + tr("%1, to be used for %2") + "\n";
        // message to advise of issues found.
        if (msgForeignList.count() > 0) {
            msg += tr("The following partitions you selected are not Linux partitions:") + "\n\n";
            for (QStringList::Iterator it = msgForeignList.begin(); it != msgForeignList.end(); ++it) {
                QString &s = *it;
                msg += msgPartSel.arg(s).arg((QString)*(++it));
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
                msg += msgPartSel.arg(s).arg((QString)*(++it));
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

    // calculate the future partitions here
    auto lambdaCalcBD = [this](QTreeWidgetItem *twit) -> int {
        if(!twit) return -1;
        int index = twit->data(Size, Qt::UserRole).toInt() ? listBlkDevs.findDevice(twit->text(Device)) : -1;
        if (index >= 0) {
            BlockDeviceInfo &bdinfo = listBlkDevs[index];
            if(twit->checkState(Encrypt) == Qt::Checked) bdinfo.fs = QStringLiteral("crypt_LUKS");
            else {
                QComboBox *comboType = twitComboBox(twit, Type);
                if(comboType) bdinfo.fs = comboType->currentText().toLower();
            }
            QLineEdit *editLabel = static_cast<QLineEdit *>(gui.treePartitions->itemWidget(twit, Label));
            if(editLabel) bdinfo.label = editLabel->text();
            bdinfo.isNasty = false; // future partitions are safe
            bdinfo.isFuture = bdinfo.isNative = true;
        }
        return index;
    };
    for(const auto &it : mounts.toStdMap()) lambdaCalcBD(it.second);
    for(QTreeWidgetItem *swap : swaps) lambdaCalcBD(swap);

    return nullptr;
}

// Checks SMART status of the selected drives.
// Returns false if it detects errors and user chooses to abort.
bool PartMan::checkTargetDrivesOK()
{
    QString smartFail, smartWarn;
    for(int ixi = 0; ixi < gui.treePartitions->topLevelItemCount(); ++ixi) {
        QStringList purposes;
        QTreeWidgetItem *tlit =  gui.treePartitions->topLevelItem(ixi);
        for(int oxo = 0; oxo < tlit->childCount(); ++oxo) {
            QComboBox *comboUse = twitComboBox(tlit->child(oxo), UseFor);
            if(comboUse) {
                const QString &text = comboUse->currentText();
                if(!text.isEmpty()) purposes << text;
            }
        }
        // If any partitions are selected run the SMART tests.
        if (!purposes.isEmpty()) {
            const QString &drive = tlit->text(Device);
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
        QTreeWidgetItem *twit = gui.treePartitions->topLevelItem(ixi);
        if(twit->text(Device) == drv) return twit;
    }
    return nullptr;
}
int PartMan::layoutDefault(QTreeWidgetItem *drivetree, int rootPercent, bool updateTree)
{
    if(updateTree) {
        // Clear the existing layout from the target device.
        while(drivetree->childCount()) drivetree->removeChild(drivetree->child(0));
        // Mark the drive as "unused" as its layout will be replaced.
        drivetree->setData(Device, Qt::UserRole, QVariant(false));
    }
    // Drive geometry basics.
    bool ok = true;
    const QString &fsText = gui.freeSpaceEdit->text().trimmed();
    int free = fsText.isEmpty() ? 0 : fsText.toInt(&ok,10);
    if (!ok) return 0;
    const int driveSize = drivetree->data(Size, Qt::UserRole).toLongLong() / 1048576;
    int rootFormatSize = driveSize - 32; // Compensate for rounding errors.
    // Boot partitions.
    if(uefi) {
        if(updateTree) addItem(drivetree, 256, "ESP");
        rootFormatSize -= 256;
    } else if(driveSize >= 2097152 || gptoverride) {
        if(updateTree) addItem(drivetree, 1, "bios_grub");
        rootFormatSize -= 1;
    }
    if (gui.checkBoxEncryptAuto->isChecked()){
        int bootFormatSize = 512;
        while(bootFormatSize < (bootSpaceNeeded/1048576)) bootFormatSize*=2;
        if(updateTree) addItem(drivetree, bootFormatSize, "boot");
        rootFormatSize -= bootFormatSize;
    }
    // Operating system.
    int swapFormatSize = 2048;
    if (rootFormatSize < 2048) swapFormatSize = 128;
    else if (rootFormatSize < 3096) swapFormatSize = 256;
    else if (rootFormatSize < 4096) swapFormatSize = 512;
    else if (rootFormatSize < 12288) swapFormatSize = 1024;
    rootFormatSize -= swapFormatSize;
    if (free > 0 && rootFormatSize > 8192) {
        if (free > (rootFormatSize - 8192)) free = rootFormatSize - 8192;
        rootFormatSize -= free;
    }
    // Home
    int homeFormatSize = rootFormatSize;
    rootFormatSize = (rootFormatSize * rootPercent) / 100;
    const int rootMinMB = rootSpaceNeeded / 1048576;
    if(rootFormatSize < rootMinMB) rootFormatSize = rootMinMB;
    homeFormatSize -= rootFormatSize;
    if(updateTree) {
        addItem(drivetree, rootFormatSize, "root");
        addItem(drivetree, swapFormatSize, "swap");
        if(homeFormatSize>0) addItem(drivetree, homeFormatSize, "home");
        labelParts(drivetree);
    }
    return rootFormatSize;
}

int PartMan::countPrepSteps()
{
    int pcount = 0;
    // Creation of new partitions
    for(int ixi = gui.treePartitions->topLevelItemCount(); ixi>=0; --ixi) {
        QTreeWidgetItem *drvit = gui.treePartitions->topLevelItem(ixi);
        if(twitComboBox(drvit, Type)) pcount += drvit->childCount() + 1; // Partition table creation included.
    }
    // Formatting partitions
    pcount += mounts.count();
    QMapIterator<QString, QTreeWidgetItem *> mi(mounts);
    while(mi.hasNext()) {
        mi.next();
        QTreeWidgetItem *twit = mi.value();
        if(twit->checkState(Encrypt)) ++pcount; //LUKS format.
    }
    // Formatting swap space
    pcount += swaps.count();
    for(QTreeWidgetItem *twit : swaps) {
        if(twit->checkState(Encrypt)) ++pcount; //LUKS format.
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
    QStringList listUnmount;
    for(QTreeWidgetItem *twit : swaps) listUnmount << twit->text(Device);
    for(const auto &it : mounts.toStdMap()) listUnmount << it.second->text(Device);
    for (const QString &strdev : listUnmount) {
        proc.exec("swapoff /dev/" + strdev, true);
        proc.exec("/bin/umount /dev/" + strdev, true);
    }
    listUnmount.clear();

    qDebug() << " ---- PARTITION FORMAT SCHEDULE ----";
    for(const auto &it : mounts.toStdMap()) {
        const QTreeWidgetItem *twit = it.second;
        qDebug().nospace().noquote() << twit->text(UseFor) << ": " << twit->text(Device)
            << " (" << twit->data(Size, Qt::UserRole).toLongLong() / 1048576 << "MB)";
    }
    for(int ixi = swaps.count()-1; ixi>=0; --ixi) {
        const QTreeWidgetItem *twit = swaps.at(ixi);
        qDebug().nospace().noquote() << "swap" << ixi << ": " << twit->text(Device)
            << " (" << twit->data(Size, Qt::UserRole).toLongLong() / 1048576 << "MB)";
    }

    // Clear the existing partition tables on devices which will have a new layout.
    for(int ixi = gui.treePartitions->topLevelItemCount()-1; ixi>=0; --ixi) {
        QTreeWidgetItem *twit = gui.treePartitions->topLevelItem(ixi);
        if(twit->data(Device, Qt::UserRole).toBool()) continue;
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
        QTreeWidgetItem *drvitem = gui.treePartitions->topLevelItem(ixi);
        const QString &drvdev = drvitem->text(Device);
        const int devCount = drvitem->childCount();
        if(drvitem->data(Device, Qt::UserRole).toBool()) {
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
                QTreeWidgetItem *twit = drvitem->child(ixdev);
                QComboBox *comboUseFor = twitComboBox(twit, UseFor);
                if(!comboUseFor || comboUseFor->currentText().isEmpty()) continue;
                const QStringList devsplit(BlockDeviceInfo::split("/dev/" + twit->text(Device)));
                if(!proc.exec(cmd.arg(devsplit.at(0), devsplit.at(1)))) return false;
                proc.sleep(1000);
            }
        } else {
            // Creating new partitions.
            const QString cmdParted("parted -s --align optimal /dev/" + drvdev);
            long long start = 1; // start with 1 MB to aid alignment
            for(int ixdev=0; ixdev<devCount; ++ixdev) {
                QTreeWidgetItem *twit = drvitem->child(ixdev);
                QComboBox *comboUseFor = twitComboBox(twit, UseFor);
                if(!comboUseFor) continue;
                QWidget *twSize = gui.treePartitions->itemWidget(twit, Size);
                const QString &useFor = comboUseFor->currentText();
                const QString type(useFor.compare("ESP", Qt::CaseInsensitive)
                    ? " mkpart primary " : " mkpart ESP ");
                const long long end = start + static_cast<QSpinBox *>(twSize)->value();
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
    // Swap partitions.
    for(QTreeWidgetItem *twit : swaps) {
        const QString dev = "/dev/" + twit->text(Device);
        if(!(twit->checkState(Encrypt))) continue;
        if(twitWillFormat(twit)) {
            proc.status(statup);
            if(!luksMake(dev, encPass)) return false;
        }
        proc.status(statup);
        if (!luksOpen(dev, twitMappedDevice(twit), encPass)) return false;
    }
    // Other partitions.
    for(QTreeWidgetItem *twit : mounts) {
        const QString dev = "/dev/" + twit->text(Device);
        if(!(twit->checkState(Encrypt))) continue;
        if(twitWillFormat(twit)) {
            proc.status(statup);
            if (!luksMake(dev, encPass)) return false;
        }
        proc.status(statup);
        if (!luksOpen(dev, twitMappedDevice(twit), encPass)) return false;
    }

    // Format all swaps.
    for(int ixi = swaps.count()-1; ixi>=0; --ixi) {
        QTreeWidgetItem *twit = swaps.at(ixi);
        if(!twitWillFormat(twit)) continue;
        proc.status(tr("Formatting swap partitions"));
        QString cmd("/sbin/mkswap " + twitMappedDevice(twit, true));
        const QString &label = twitLineEdit(twit, Label)->text();
        if(!label.isEmpty()) cmd.append(" -L \"" + label + "\"");
        if(!proc.exec(cmd, true)) return false;
    }
    // Format other partitions.
    const bool badblocks = gui.badblocksCheck->isChecked();
    for(const auto &it : mounts.toStdMap()) {
        QTreeWidgetItem *twit = it.second;
        if(!twitWillFormat(twit)) continue;
        proc.status(tr("Formatting: %1").arg(it.first));
        const QString &dev = twitMappedDevice(twit, true);
        if(twitComboBox(twit, UseFor)->currentText().compare("ESP", Qt::CaseInsensitive) == 0) {
            proc.status(tr("Formatting EFI System Partition"));
            if (!proc.exec("mkfs.msdos -F 32 " + dev)) return false;
            // Sets boot flag and ESP flag.
            proc.exec("parted -s /dev/" + BlockDeviceInfo::split(dev).at(0) + " set 1 esp on");
        } else {
            if(!formatLinuxPartition(dev, twitComboBox(twit, Type)->currentText(),
                badblocks, twitLineEdit(twit, Label)->text())) return false;
        }
        proc.sleep(1000);
    }

    return true;
}

// Transplanted straight from minstall.cpp
bool PartMan::formatLinuxPartition(const QString &dev, const QString &type, bool chkBadBlocks, const QString &label)
{
    proc.log(__PRETTY_FUNCTION__);

    QString cmd;
    if (type == "reiserfs") {
        cmd = "mkfs.reiserfs -q";
    } else if (type == "reiser4") {
        cmd = "mkfs.reiser4 -f -y";
    } else if (type.startsWith("btrfs")) {
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
    } else if (type == "xfs" || type == "f2fs") {
        cmd = "mkfs." + type + " -f";
    } else { // jfs, ext2, ext3, ext4
        cmd = "mkfs." + type;
        if (type == "jfs") cmd.append(" -q");
        else cmd.append(" -F");
        if (chkBadBlocks) cmd.append(" -c");
    }

    cmd.append(" " + dev);
    if (!label.isEmpty()) {
        if (type == "reiserfs" || type == "f2fs") cmd.append(" -l \"");
        else cmd.append(" -L \"");
        cmd.append(label + "\"");
    }
    if (!proc.exec(cmd)) return false;

    if (type.startsWith("ext")) {
        // ext4 tuning
        proc.exec("/sbin/tune2fs -c0 -C0 -i1m " + dev);
    }
    return true;
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
    const QString cmdAddKey("cryptsetup luksAddKey /dev/%1 " + keyfile);
    const QString cmdBlkID("blkid -s UUID -o value /dev/");
    // Find the file system device which contains the key
    QString keyMount;
    const bool noKey = keyfile.isEmpty();
    if(!noKey) {
        for(const auto &it : mounts.toStdMap()) {
            if(keyfile.startsWith("/mnt/antiX"+it.first+'/')) keyMount = it.first;
        }
    }
    // File systems
    for(auto &it : mounts.toStdMap()) {
        if(it.second->checkState(Encrypt)==Qt::Unchecked) continue;
        const QString &dev = it.second->text(Device);
        QString uuid = proc.execOut(cmdBlkID + dev);
        if(isNewKey) {
            if(!proc.exec(cmdAddKey.arg(dev), true, &password)) return false;
        }
        out << twitMappedDevice(it.second, false) << " /dev/disk/by-uuid/" << uuid;
        if(noKey || it.first==keyMount) out << " none luks \n";
        else out << ' ' << keyfile << "luks \n";
    }
    // Swap spaces
    for(QTreeWidgetItem *twit : swaps) {
        if(twit->checkState(Encrypt)==Qt::Unchecked) continue;
        const QString &dev = twit->text(Device);
        QString uuid = proc.execOut(cmdBlkID + dev);
        if(!proc.exec(cmdAddKey.arg(dev), true, &password)) return false;
        out << twitMappedDevice(twit, false) << " /dev/disk/by-uuid/" << uuid;
        if(noKey) out << " none luks,nofail \n";
        else out << ' ' << keyfile << "luks,nofail \n";
    }
    // Update cryptdisks if the key is not in the root file system.
    if(!keyMount.isEmpty() && keyMount!='/') {
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
    // File systems
    for(auto &it : mounts.toStdMap()) {
        if(it.first == "EFI") continue; // EFI will be dealt with later.
        const QString &dev = twitMappedDevice(it.second, true);
        qDebug() << "Creating fstab entry for:" << it.first << dev;
        // Device ID or UUID
        if(twitIsMapped(it.second)) out << dev;
        else out << "UUID=" + proc.execOut(cmdBlkID + dev);
        // Mount point, file system
        QString fstype = proc.execOut("blkid " + dev + " -o value -s TYPE");
        out << " " << it.first << " " << fstype;
        // Options
        out << " " << twitLineEdit(it.second, Options)->text();
        // Dump, pass
        if(fstype.startsWith("reiser")) out << " 0 0";
        else if(it.first=="/boot") out << " 1 1\n";
        else if(it.first=="/") {
            if(fstype.startsWith("btrfs")) out << " 1 0\n";
            else out << " 1 1\n";
        } else out << " 1 2\n";
    }
    // Swap spaces
    for(QTreeWidgetItem *twit : swaps) {
        const QString &dev = twitMappedDevice(twit, true);
        if(twitIsMapped(twit)) out << dev;
        else out << "UUID=" + proc.execOut(cmdBlkID + dev);
        out << " swap swap " << twitLineEdit(twit, Options)->text() << " 0 0\n";
    }
    // EFI System Partition
    if (gui.grubEspButton->isChecked()) {
        const QString espdev = "/dev/" + gui.grubBootCombo->currentData().toString();
        const QString espdevUUID = "UUID=" + proc.execOut(cmdBlkID + espdev);
        qDebug() << "espdev" << espdev << espdevUUID;
        out << espdevUUID + " /boot/efi vfat defaults,noatime,dmask=0002,fmask=0113 0 0\n";
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
        mkdir(point.toUtf8(), 0755);
        if(it.first=="/boot") {
             // needed to run fsck because sfdisk --part-type can mess up the partition
            if(!proc.exec("fsck.ext4 -y " + dev)) return false;
        }
        const QString &cmd = QString("/bin/mount %1 %2 -o %3").arg(dev,
            point, twitLineEdit(it.second, Options)->text());
        if(!proc.exec(cmd)) return false;
    }
    return true;
}

void PartMan::unmount(bool all)
{
    for(QTreeWidgetItem *twit : swaps) {
        if(!(all || twit->checkState(Encrypt))) continue;
        QString cmd("cryptsetup close swap%1");
        proc.exec(cmd.arg(twitMappedDevice(twit)), true);
    }
    QMapIterator<QString, QTreeWidgetItem *> it(mounts);
    it.toBack();
    while(it.hasPrevious()) {
        it.previous();
        if(it.key().at(0)!='/') continue;
        proc.exec("/bin/umount -l /mnt/antiX" + it.key(), true);
        QTreeWidgetItem *twit = it.value();
        if(!(all || twit->checkState(Encrypt))) continue;
        QString cmd("cryptsetup close %1");
        proc.exec(cmd.arg(twitMappedDevice(twit)), true);
    }
}

// Public properties
bool PartMan::willFormatRoot()
{
    QTreeWidgetItem *rootitem = mounts.value("/");
    if(rootitem) return twitWillFormat(rootitem);
    return false;
}
QString PartMan::getMountDev(const QString &point, const bool mapped)
{
    const QTreeWidgetItem *twit = mounts.value(point);
    if(!twit) return QString();
    if(mapped) return twitMappedDevice(twit, true);
    return QString("/dev/" + twit->text(Device));
}
int PartMan::swapCount()
{
    return swaps.count();
}
int PartMan::isEncrypt(const QString &point)
{
    int count = 0;
    if(point.isEmpty() || point=="SWAP") {
        for(QTreeWidgetItem *twit : swaps) {
            if(twit->checkState(Encrypt)) ++count;
        }
    }
    if(point.isEmpty()) {
        for(QTreeWidgetItem *twit : mounts) {
            if(twit->checkState(Encrypt)) ++count;
        }
    }
    const QTreeWidgetItem *twit = mounts.value(point);
    if(twit && twit->checkState(Encrypt)) ++count;
    return count;
}

// Helpers
inline bool PartMan::twitWillFormat(QTreeWidgetItem *twit)
{
    QComboBox *combo = twitComboBox(twit, Type);
    return !(combo->currentData(Qt::UserRole).toBool());
}
inline bool PartMan::twitIsMapped(const QTreeWidgetItem * twit)
{
    if(twit->parent()!=nullptr) return false;
    return !(twit->data(Device, Qt::UserRole).isNull());
}
inline QString PartMan::twitMappedDevice(const QTreeWidgetItem *twit, const bool full) const
{
    if(twit->parent()==nullptr) {
        const QVariant &d = twit->data(Device, Qt::UserRole);
        if(!d.isNull()) {
            if(full) return "/dev/mapper/" + d.toString();
            return d.toString();
        }
    }
    if(!full) return twit->text(Device);
    else return "/dev/" + twit->text(Device);
}
inline QComboBox *PartMan::twitComboBox(QTreeWidgetItem  *twit, int column)
{
    return static_cast<QComboBox *>(gui.treePartitions->itemWidget(twit, column));
}
inline QLineEdit *PartMan::twitLineEdit(QTreeWidgetItem  *twit, int column)
{
    return static_cast<QLineEdit *>(gui.treePartitions->itemWidget(twit, column));
}
