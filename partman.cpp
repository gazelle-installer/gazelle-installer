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
#include <QLocale>
#include <QMessageBox>
#include <QTreeWidget>
#include <QComboBox>
#include <QLineEdit>

#include "partman.h"

PartMan::PartMan(MProcess &mproc, BlockDeviceList &bdlist, Ui::MeInstall &ui, QWidget *parent)
    : QObject(parent), proc(mproc), listBlkDevs(bdlist), gui(ui), master(parent)
{
    listUsePresets << "" << "root" << "boot" << "home" << "swap";
}

void PartMan::populate()
{
    gui.treePartitions->clear();
    QTreeWidgetItem *curdrv = nullptr;
    for (const BlockDeviceInfo &bdinfo : listBlkDevs) {
        QTreeWidgetItem *curdev;
        if (bdinfo.isDrive) {
            curdrv = curdev = new QTreeWidgetItem(gui.treePartitions);
            curdrv->setData(Device, Qt::UserRole, QVariant(false)); // drive starts off "unused"
            // Model
            curdev->setText(Label, bdinfo.model);
        } else {
            curdev = new QTreeWidgetItem(curdrv);
            curdrv->setData(Device, Qt::UserRole, QVariant(true)); // drive is now "used"
            // Label
            QLineEdit *editLabel = new QLineEdit(gui.treePartitions);
            editLabel->setAutoFillBackground(true);
            gui.treePartitions->setItemWidget(curdev, Label, editLabel);
            editLabel->setEnabled(false);
            editLabel->setText(bdinfo.label);
            // Use For
            QComboBox *comboUse = new QComboBox(gui.treePartitions);
            comboUse->setAutoFillBackground(true);
            gui.treePartitions->setItemWidget(curdev, UseFor, comboUse);
            comboUse->setEditable(true);
            comboUse->setInsertPolicy(QComboBox::NoInsert);
            comboUse->addItems(listUsePresets);
            comboUse->setProperty("row", QVariant::fromValue<void *>(curdev));
            comboUse->lineEdit()->setPlaceholderText("----");
            connect(comboUse, &QComboBox::currentTextChanged, this, &PartMan::comboUseTextChange);
            // Type
            QComboBox *comboType = new QComboBox(gui.treePartitions);
            comboType->setAutoFillBackground(true);
            gui.treePartitions->setItemWidget(curdev, Type, comboType);
            comboType->setEnabled(false);
            curdev->setText(5, bdinfo.fs);
            comboType->addItem(bdinfo.fs);
            // Mount options
            QLineEdit *editOptions = new QLineEdit(gui.treePartitions);
            editOptions->setAutoFillBackground(true);
            gui.treePartitions->setItemWidget(curdev, Options, editOptions);
            editOptions->setEnabled(false);
            editOptions->setText("defaults");
        }
        // Device name and size
        curdev->setText(Device, bdinfo.name);
        curdev->setText(Size, QLocale::system().formattedDataSize(bdinfo.size, 1, QLocale::DataSizeTraditionalFormat));
        curdev->setData(Size, Qt::UserRole, QVariant(-bdinfo.size)); // negative = reuse partition
    }
    gui.treePartitions->expandAll();
    for (int ixi = gui.treePartitions->columnCount() - 2; ixi >= 0; --ixi) {
        if(ixi != Label) gui.treePartitions->resizeColumnToContents(ixi);
    }
}

void PartMan::comboUseTextChange(const QString &text)
{
    QComboBox *combo = static_cast<QComboBox *>(sender());
    if(!combo) return;
    int oldUseClass = combo->property("class").toInt();
    int useClass = -1;
    if(text.isEmpty()) useClass = 0;
    else if(text == "swap") useClass = 1;
    else if(text == "boot" || text == "/boot") useClass = 2;
    else useClass = 3;
    if(useClass != oldUseClass) {
        QTreeWidgetItem *item = static_cast<QTreeWidgetItem *>(combo->property("row").value<void *>());
        if(!item) return;
        QComboBox *comboType = static_cast<QComboBox *>(gui.treePartitions->itemWidget(item, Type));
        const QString &fs = item->text(Type);
        comboType->setEnabled(false);
        comboType->clear();
        if(useClass == 0) comboType->addItem(fs);
        else if(useClass == 1) comboType->addItem("SWAP");
        else if(useClass == 2) comboType->addItem("ext4");
        else if(useClass == 3) {
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
        if(useClass == 0 || useClass == 2) {
            // Stash and hide the encrypt setting when changing to nothing or boot.
            if(oldUseClass != 0 && oldUseClass != 2) {
                const Qt::CheckState cs = item->checkState(Encrypt);
                item->setData(Encrypt, Qt::CheckStateRole, QVariant());
                item->setData(Encrypt, Qt::UserRole, QVariant(cs));
            }
        } else if(oldUseClass == 0 || oldUseClass == 2) {
            // Load and show the encrypt setting when changing from nothing or boot.
            const int cs = item->data(Encrypt, Qt::UserRole).toInt();
            item->setCheckState(Encrypt, static_cast<Qt::CheckState>(cs));
        }
        gui.treePartitions->itemWidget(item, Label)->setDisabled(text.isEmpty());
        gui.treePartitions->itemWidget(item, Options)->setDisabled(useClass <= 1); // nothing or SWAP
        combo->setProperty("class", QVariant(useClass));
    }
}

QWidget *PartMan::composeValidate(const QString &minSizeText, bool automatic, const QString &project)
{
    QStringList msgForeignList;
    QString msgConfirm;
    QStringList msgFormatList;
    QTreeWidgetItemIterator it(gui.treePartitions, QTreeWidgetItemIterator::NoChildren);
    bool encryptAny = false;
    bool encryptRoot = false;
    while (*it) {
        QComboBox *comboUse = static_cast<QComboBox *>(gui.treePartitions->itemWidget(*it, UseFor));
        if(comboUse && !(comboUse->currentText().isEmpty())) {
            QString mount = comboUse->currentText();
            if(mount == "root") mount = "/";
            else if(mount == "boot") mount = "/boot";
            else if(mount == "home") mount = "/home";
            else if(mount == "swap") {
                swaps << *it;
                mount.clear();
            }
            // Mount description
            QString desc;
            if(mount=="/") desc = "/ (root)";
            else if(mount.isEmpty()) desc = "swap";
            else desc = mount;
            QTreeWidgetItem *twit = mounts.value(mount);

            // The mount can only be selected once.
            if(twit) {
                QMessageBox::critical(master, master->windowTitle(),
                     tr("%1 is already selected for: %2").arg(twit->text(0), desc));
                return comboUse;
            } else {
                if(!mount.isEmpty()) mounts.insert(mount, *it);
                // Warn if using a non-Linux partition (potential install of another OS).
                const QString &devname = (*it)->text(Device);
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

        // format /boot?
        twit = mounts.value("/boot");
        if (twit) {
            const QString &bootdev = twit->text(Device);
            msgFormatList << bootdev << "/boot";
            // warn if partition too big (not needed for boot, likely data or other useful partition
            const int bdindex = listBlkDevs.findDevice(bootdev);
            if (bdindex < 0 || listBlkDevs.at(bdindex).size > 2147483648) {
                // if > 2GB or not in block device list for some reason
                msg = tr("The partition you selected for /boot is larger than expected.") + "\n\n";
            }
        }

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
                msgFormatList << swapdev << "swap";
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
        const QString msgPartSel = " - " + tr("%1 for the %2 partition") + "\n";
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
