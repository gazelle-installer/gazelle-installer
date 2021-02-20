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

#ifndef PARTMAN_H
#define PARTMAN_H

#include <QObject>
#include <QTreeWidget>
#include <QString>
#include <QMap>

#include "mprocess.h"
#include "blockdev.h"
#include "ui_meinstall.h"

class PartMan : public QObject
{
    Q_OBJECT
    enum TreeColumns {
        Device,
        Size,
        Label,
        UseFor,
        Encrypt,
        Type,
        Options
    };
    MProcess &proc;
    BlockDeviceList &listBlkDevs;
    Ui::MeInstall &gui;
    QWidget *master;
    QMap<QString, QTreeWidgetItem *> mounts;
    QList<QTreeWidgetItem *> swaps;
    enum Qt::CheckState encryptCheckRoot = Qt::Unchecked;
    void setup();
    inline QTreeWidgetItem *addItem(QTreeWidgetItem *parent,
        int defaultMB = 1, const QString &defaultUse = QString());
    QTreeWidgetItem *setupItem(QTreeWidgetItem *twit, const BlockDeviceInfo *bdinfo,
        int defaultMB = 1, const QString &defaultUse = QString());
    void labelParts(QTreeWidgetItem *drive);
    static QString translateUse(const QString &alias);
    void setEncryptChecks(const QString &use, enum Qt::CheckState state);
    void comboUseTextChange(const QString &text);
    void comboTypeTextChange(const QString &);
    void treeItemChange(QTreeWidgetItem *item, int column);
    void treeSelChange();
    void partClearClick(bool);
    void partAddClick(bool);
    void partRemoveClick(bool);
    void partDefaultClick(bool);
public:
    bool automatic, sync;
    bool gptoverride, uefi;
    QMap<QString, QString> defaultLabels;
    PartMan(MProcess &mproc, BlockDeviceList &bdlist, Ui::MeInstall &ui, QWidget *parent);
    void populate(QTreeWidgetItem *drvstart = nullptr);
    QWidget *composeValidate(const QString &minSizeText, const QString &project);
    bool checkTargetDrivesOK();
    bool luksMake(const QString &dev, const QByteArray &password);
    bool luksOpen(const QString &dev, const QString &luksfs,
        const QByteArray &password, const QString &options = QString());
    QString mapperName(const QString &mount);
    bool layoutDefault();
    int countPrepSteps();
    bool prepareParts();
    void unmount(bool all = false);
};

#endif // PARTMAN_H
