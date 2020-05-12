// Basic partition manager for the installer.
//
//   Copyright (C) 2020 by AK-47
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
    gui.buttonPartAdd->setEnabled(false);
    gui.buttonPartRemove->setEnabled(false);
    gui.buttonPartClear->setEnabled(false);
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
            curdrv->setData(Device, Qt::UserRole, QVariant(true)); // drive is now "used"
            setupItem(curdev, &bdinfo);
        }
        // Device name and size
        curdev->setText(Device, bdinfo.name);
        curdev->setText(Size, QLocale::system().formattedDataSize(bdinfo.size, 1, QLocale::DataSizeTraditionalFormat));
        curdev->setData(Size, Qt::UserRole, QVariant(-bdinfo.size)); // negative = reuse partition
    }
    gui.treePartitions->expandAll();
    for (int ixi = gui.treePartitions->columnCount() - 1; ixi >= 0; --ixi) {
        if(ixi != Label) gui.treePartitions->resizeColumnToContents(ixi);
    }
    gui.treePartitions->blockSignals(false);
}

void PartMan::setupItem(QTreeWidgetItem *item, const BlockDeviceInfo *bdinfo)
{
    // Size
    if(!bdinfo) {
        QSpinBox *spinSize = new QSpinBox(gui.treePartitions);
        spinSize->setAutoFillBackground(true);
        gui.treePartitions->setItemWidget(item, Size, spinSize);
    }
    // Label
    QLineEdit *editLabel = new QLineEdit(gui.treePartitions);
    editLabel->setAutoFillBackground(true);
    gui.treePartitions->setItemWidget(item, Label, editLabel);
    editLabel->setEnabled(false);
    if(bdinfo) {
        item->setText(Label, bdinfo->label);
        editLabel->setText(bdinfo->label);
    }
    // Use For
    QComboBox *comboUse = new QComboBox(gui.treePartitions);
    comboUse->setAutoFillBackground(true);
    gui.treePartitions->setItemWidget(item, UseFor, comboUse);
    comboUse->setEditable(true);
    comboUse->setInsertPolicy(QComboBox::NoInsert);
    comboUse->addItem("");
    if(!bdinfo || bdinfo->size <= 4294967296) {
        comboUse->addItem("ESP");
        comboUse->addItem("boot");
    }
    comboUse->addItem("root");
    comboUse->addItem("swap");
    comboUse->addItem("home");
    comboUse->setProperty("row", QVariant::fromValue<void *>(item));
    comboUse->lineEdit()->setPlaceholderText("----");
    connect(comboUse, &QComboBox::currentTextChanged, this, &PartMan::comboUseTextChange);
    // Type
    QComboBox *comboType = new QComboBox(gui.treePartitions);
    comboType->setAutoFillBackground(true);
    gui.treePartitions->setItemWidget(item, Type, comboType);
    comboType->setEnabled(false);
    if(bdinfo) {
        item->setText(Type, bdinfo->fs);
        comboType->addItem(bdinfo->fs);
    }
    connect(comboType, &QComboBox::currentTextChanged, this, &PartMan::comboTypeTextChange);
    // Mount options
    QLineEdit *editOptions = new QLineEdit(gui.treePartitions);
    editOptions->setAutoFillBackground(true);
    gui.treePartitions->setItemWidget(item, Options, editOptions);
    editOptions->setEnabled(false);
    editOptions->setText("defaults");
}

void PartMan::labelParts(QTreeWidgetItem *drive)
{
    const QString name = drive->text(Device) + "%1";
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
        QComboBox *comboUse = static_cast<QComboBox *>(gui.treePartitions->itemWidget(*it, UseFor));
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
    int oldUseClass = combo->property("class").toInt();
    int useClass = -1;
    if(text.isEmpty()) useClass = 0;
    else if(text == "ESP") useClass = 1;
    else if(text == "boot" || text == "/boot") useClass = 2;
    else if(text == "swap") useClass = 3;
    else useClass = 4;
    if(useClass != oldUseClass) {
        QTreeWidgetItem *item = static_cast<QTreeWidgetItem *>(combo->property("row").value<void *>());
        if(!item) return;
        QComboBox *comboType = static_cast<QComboBox *>(gui.treePartitions->itemWidget(item, Type));
        QLineEdit *editLabel = static_cast<QLineEdit *>(gui.treePartitions->itemWidget(item, Label));
        comboType->setEnabled(false);
        comboType->clear();
        switch(useClass) {
        case 0:
            editLabel->setText(item->text(Label));
            comboType->addItem(item->text(Type));
            break;
        case 1: comboType->addItem("FAT32"); break;
        case 2: comboType->addItem("ext4"); break;
        case 3: comboType->addItem("SWAP"); break;
        default:
            comboType->addItem("ext4");
            comboType->addItem("ext3");
            comboType->addItem("ext2");
            comboType->addItem("f2fs");
            comboType->addItem("jfs");
            comboType->addItem("xfs");
            comboType->addItem("btrfs");
            comboType->addItem("btrfs-zlib");
            comboType->addItem("btrfs-lzo");
            comboType->addItem("reiserfs");
            comboType->addItem("reiser4");
            comboType->setEnabled(true);
        }
        // Changing to and from a mount/use that support encryption.
        if(useClass >= 0 && useClass <= 2) {
            item->setData(Encrypt, Qt::CheckStateRole, QVariant());
        } else if(oldUseClass >= 0 && oldUseClass <= 2) {
            item->setCheckState(Encrypt, encryptCheckRoot);
        }
        // Label and options
        editLabel->setEnabled(useClass!=0);
        gui.treePartitions->itemWidget(item, Options)->setEnabled(useClass!=0);
        combo->setProperty("class", QVariant(useClass));
    }
    gui.treePartitions->blockSignals(false);
}

void PartMan::comboTypeTextChange(const QString &)
{
    QTreeWidgetItemIterator it(gui.treePartitions, QTreeWidgetItemIterator::NoChildren);
    bool canCheckBlocks = false;
    while (*it) {
        QComboBox *comboUse = static_cast<QComboBox *>(gui.treePartitions->itemWidget(*it, UseFor));
        if(comboUse && !(comboUse->currentText().isEmpty())) {
            QComboBox *comboType = static_cast<QComboBox *>(gui.treePartitions->itemWidget(*it, Type));
            if(!comboType) return;
            const QString &type = comboType->currentText();
            if(type.startsWith("ext") || type == "jfs") canCheckBlocks = true;
        }
        ++it;
    }
    gui.badblocksCheck->setEnabled(canCheckBlocks);
}

void PartMan::treeItemChange(QTreeWidgetItem *item, int column)
{
    if(column == Encrypt) {
        gui.treePartitions->blockSignals(true);
        QComboBox *comboUse = static_cast<QComboBox *>(gui.treePartitions->itemWidget(item, UseFor));
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
        gui.gbEncrPass->setVisible(needsCrypto);
        gui.treePartitions->blockSignals(false);
    }
}

void PartMan::treeSelChange()
{
    QTreeWidgetItem *twit = gui.treePartitions->selectedItems().value(0);
    QIcon iconClear = QIcon::fromTheme("star-new-symbolic");
    if(twit) {
        bool used = true;
        if(!(twit->parent())) used = twit->data(Device, Qt::UserRole).toBool();
        else used = twit->parent()->data(Device, Qt::UserRole).toBool();
        gui.buttonPartClear->setEnabled(twit->parent()==nullptr);
        if(!used) iconClear = QIcon::fromTheme("undo");
        gui.buttonPartAdd->setEnabled(!used);
        gui.buttonPartRemove->setEnabled(!used && twit->parent());
    } else {
        gui.buttonPartAdd->setEnabled(false);
        gui.buttonPartRemove->setEnabled(false);
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
    int index = 0;
    if(!drive) drive = twit;
    else index = drive->indexOfChild(twit) + 1;

    QTreeWidgetItem *part = new QTreeWidgetItem;
    drive->insertChild(index, part);
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

QWidget *PartMan::composeValidate(const QString &minSizeText, const QString &project)
{
    QStringList msgForeignList;
    QString msgConfirm;
    QStringList msgFormatList;
    bool encryptAny = false, encryptRoot = false;
    mounts.clear();
    swaps.clear();
    QTreeWidgetItemIterator it(gui.treePartitions, QTreeWidgetItemIterator::NoChildren);
    while (*it) {
        QComboBox *comboUse = static_cast<QComboBox *>
            (gui.treePartitions->itemWidget(*it, UseFor));
        if(comboUse && !(comboUse->currentText().isEmpty())) {
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
            if((*it)->checkState(Encrypt) == Qt::Checked) encryptAny = true;
        }
        ++it;
    }

    if(encryptAny) {
        // TODO: Validate encryption settings.
    }
    QTreeWidgetItem *twit = mounts.value("/");
    if(twit) {
        if(twit->checkState(Encrypt) == Qt::Checked) encryptRoot = true;
        if(!automatic) {
            // maybe format root (if not saving /home on root) // or if using --sync option
            const QString &rootdev = twit->text(Device);
            if (!(gui.saveHomeCheck->isChecked() && !mounts.contains("/home")) && !sync) {
                msgFormatList << rootdev << "/ (root)";
            } else {
                msgConfirm += " - " + tr("Delete the data on %1 except for /home").arg(rootdev) + "\n";
            }
        }
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

        // format swap if encrypting or not already swap (if no swap is chosen do nothing)
        QStringList swapEncrypt;
        QStringList swapNoEncrypt;
        for(const QTreeWidgetItem *swap : swaps) {
            const QString &swapdev = swap->text(Device);
            bool format = false;
            if (swap->checkState(Encrypt) == Qt::Checked) {
                format = true;
                swapEncrypt << swapdev;
            } else {
                const int bdindex = listBlkDevs.findDevice(swap->text(Device));
                if (bdindex >= 0 && !listBlkDevs.at(bdindex).isSwap) format = true;
                swapNoEncrypt << swapdev;
            }
            if (format) {
                msgFormatList << swapdev << tr("swap space");
            } else {
                msgConfirm += " - " + tr("Configure %1 as swap space").arg(swapdev) + "\n";
            }
        }

        // format /home?
        twit = mounts.value("/home");
        if (twit) {
            if (gui.saveHomeCheck->isChecked()) {
                msgConfirm += " - " + tr("Reuse (no reformat) %1 as the"
                    " /home partition").arg(twit->text(Device)) + "\n";
            } else {
                msgFormatList << twit->text(Device) << "/home";
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
                QComboBox *comboType = static_cast<QComboBox *>(gui.treePartitions->itemWidget(twit, Type));
                if(comboType) bdinfo.fs = comboType->currentText().toLower();
            }
            QLineEdit *editLabel = static_cast<QLineEdit *>(gui.treePartitions->itemWidget(twit, Label));
            if(editLabel) bdinfo.label = editLabel->text();
            bdinfo.isNasty = false; // future partitions are safe
            bdinfo.isFuture = bdinfo.isNative = true;
        }
        return index;
    };
    QMapIterator<QString, QTreeWidgetItem *> mi(mounts);
    while(mi.hasNext()) {
        mi.next();
        lambdaCalcBD(mi.value());
    }
    for(QTreeWidgetItem *swap : swaps) {
        const int ixswap = lambdaCalcBD(swap);
        if (ixswap >= 0) listBlkDevs[ixswap].isSwap = true;
    }

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
            QComboBox *comboUse = static_cast<QComboBox *>
                (gui.treePartitions->itemWidget(tlit->child(oxo), UseFor));
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
