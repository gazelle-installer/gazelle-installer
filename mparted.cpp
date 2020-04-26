// Basic partition editor for the installer.
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

#include <QLocale>
#include <QTreeWidget>
#include <QLineEdit>
#include <QComboBox>

#include "mparted.h"

MParted::MParted(MProcess &mproc, BlockDeviceList &bdlist, QObject *parent)
    : QObject(parent), proc(mproc), listBlkDevs(bdlist)
{

}

void MParted::setup(QTreeWidget *twParts)
{
    treePartitions = twParts;
    listUsePresets << "" << "/" << "/boot" << "/home" << "swap";
}

void MParted::populate()
{
    treePartitions->clear();
    QTreeWidgetItem *curdrv = nullptr;
    for (const BlockDeviceInfo &bdinfo : listBlkDevs) {
        QTreeWidgetItem *curdev;
        if (bdinfo.isDrive) {
            curdrv = curdev = new QTreeWidgetItem(treePartitions);
            curdrv->setData(0, Qt::UserRole, QVariant(false)); // drive starts off "unused"
            // Model
            curdev->setText(2, bdinfo.model);
        } else {
            curdev = new QTreeWidgetItem(curdrv);
            curdrv->setData(0, Qt::UserRole, QVariant(true)); // drive is now "used"
            curdev->setCheckState(4, Qt::Unchecked);
            // Label
            QLineEdit *editLabel = new QLineEdit(treePartitions);
            editLabel->setAutoFillBackground(true);
            treePartitions->setItemWidget(curdev, 2, editLabel);
            editLabel->setEnabled(false);
            editLabel->setText(bdinfo.label);
            // Use For
            QComboBox *comboUse = new QComboBox(treePartitions);
            comboUse->setAutoFillBackground(true);
            treePartitions->setItemWidget(curdev, 3, comboUse);
            comboUse->setEditable(true);
            comboUse->addItems(listUsePresets);
            const QLineEdit *editUse = comboUse->lineEdit();
            connect(editUse, &QLineEdit::editingFinished, this, &MParted::comboUseEditFinish);
            // Type
            QComboBox *comboType = new QComboBox(treePartitions);
            comboType->setAutoFillBackground(true);
            treePartitions->setItemWidget(curdev, 5, comboType);
            comboType->setEnabled(false);
            curdev->setText(5, bdinfo.fs);
            comboType->addItem(bdinfo.fs);
        }
        curdev->setText(0, bdinfo.name);
        curdev->setText(1, QLocale::system().formattedDataSize(bdinfo.size, 1, QLocale::DataSizeTraditionalFormat));
    }
    treePartitions->expandAll();
    for (int ixi = treePartitions->columnCount() - 1; ixi >= 0; --ixi) {
        if(ixi != 2) treePartitions->resizeColumnToContents(ixi);
    }
}

void MParted::comboUseEditFinish()
{
    QLineEdit *comboUseEdit = static_cast<QLineEdit *>(sender());
    QComboBox *combo = static_cast<QComboBox *>(comboUseEdit->parent());
    if(!combo) return;
    const QString &text = combo->currentText();
    if(text == combo->itemData(0).toString()) return;

    QTreeWidgetItemIterator it(treePartitions);
    while (*it) {
        if ((*it)->parent() != nullptr) {
            QComboBox *comboUse = static_cast<QComboBox *>(treePartitions->itemWidget(*it, 3));
            QString ctext = comboUse->currentText();
            if(comboUse == combo) {
                QComboBox *comboType = static_cast<QComboBox *>(treePartitions->itemWidget(*it, 5));
                const QString &fs = (*it)->text(5);
                comboType->setEnabled(false);
                comboType->clear();
                if(ctext.isEmpty()) comboType->addItem(fs);
                else if(ctext == "swap") comboType->addItem("SWAP");
                else if(ctext == "/boot") comboType->addItem("ext4");
                else {
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
                    int ixFS = comboType->findText(fs, Qt::MatchExactly);
                    if(ixFS >= 0) comboType->setCurrentIndex(ixFS);
                    comboType->setEnabled(true);
                }
                treePartitions->itemWidget(*it, 2)->setDisabled(text.isEmpty());
            } else if(comboUse->findText(text) >= 0) {
                comboUse->blockSignals(true);
                comboUse->clear();
                for(const QString &preset : listUsePresets) {
                    if(text.isEmpty() || text != preset) comboUse->addItem(preset);
                }
                comboUse->blockSignals(false);
                if(text != ctext) comboUse->setCurrentText(ctext);
            }
        }
        ++it;
    }
    combo->setItemData(0, text);
}
