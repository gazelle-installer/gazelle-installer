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
#include "msettings.h"

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
    QStringList listToUnmount;
    void setup();
    inline QTreeWidgetItem *addItem(QTreeWidgetItem *parent, int defaultMB,
        const QString &defaultUse, bool crypto);
    void setupItem(QTreeWidgetItem *twit, const BlockDeviceInfo *bdinfo,
        int defaultMB = 0, const QString &defaultUse = QString());
    void labelParts(QTreeWidgetItem *drive);
    static QString translateUse(const QString &alias);
    void clearPartitionTables(const QString &dev);
    bool formatLinuxPartition(const QString &dev, const QString &type, bool chkBadBlocks, const QString &label);
    void setEncryptChecks(const QString &use,
        enum Qt::CheckState state, QTreeWidgetItem *exclude);
    bool calculatePartBD();
    inline long long twitSize(QTreeWidgetItem *twit, bool bytes=false);
    inline bool twitWillFormat(QTreeWidgetItem *twit);
    inline QString twitUseFor(QTreeWidgetItem *twit);
    inline bool twitIsMapped(const QTreeWidgetItem * twit);
    inline QString twitMappedDevice(const QTreeWidgetItem *twit, const bool full=false) const;
    inline QComboBox *twitComboBox(QTreeWidgetItem  *twit, int column);
    inline QLineEdit *twitLineEdit(QTreeWidgetItem  *twit, int column);
    void spinSizeValueChange(int i);
    void comboUseTextChange(const QString &text);
    void comboTypeTextChange(const QString &);
    void treeItemChange(QTreeWidgetItem *item, int column);
    void treeSelChange();
    void treeMenu(const QPoint &);
    void partClearClick(bool);
    void partAddClick(bool);
    void partRemoveClick(bool);
    void partDefaultClick(bool);
public:
    bool gptoverride=false, uefi=false;
    long long rootSpaceNeeded = 0;
    long long bootSpaceNeeded = 0;
    QMap<QString, QString> defaultLabels;
    PartMan(MProcess &mproc, BlockDeviceList &bdlist, Ui::MeInstall &ui, QWidget *parent);
    void populate(QTreeWidgetItem *drvstart = nullptr);
    bool manageConfig(MSettings &config, bool save);
    QWidget *composeValidate(bool automatic,
        const QString &minSizeText, const QString &project);
    bool checkTargetDrivesOK();
    bool luksMake(const QString &dev, const QByteArray &password);
    bool luksOpen(const QString &dev, const QString &luksfs,
        const QByteArray &password, const QString &options = QString());
    QTreeWidgetItem *selectedDriveAuto();
    int layoutDefault(QTreeWidgetItem *driveitem,
        int rootPercent, bool crypto, bool updateTree=true);
    int countPrepSteps();
    bool preparePartitions();
    bool formatPartitions();
    bool fixCryptoSetup(const QString &keyfile, bool isNewKey);
    bool makeFstab(bool populateMediaMounts);
    bool mountPartitions();
    void unmount(bool all = false);
    bool willFormat(const QString &point);
    QString getMountDev(const QString &point, const bool mapped=true);
    int swapCount();
    int isEncrypt(const QString &point);
};

#endif // PARTMAN_H
