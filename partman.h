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

#ifndef PARTMAN_H
#define PARTMAN_H

#include <QObject>
#include <QAbstractItemModel>
#include <QStyledItemDelegate>
#include <QString>
#include <QMap>
#include <QStack>

#include "mprocess.h"
#include "blockdev.h"
#include "ui_meinstall.h"
#include "msettings.h"

class DeviceItem
{
    QVector<DeviceItem *> children;
    DeviceItem *parentItem = nullptr;
    class PartModel *model = nullptr;
public:
    DeviceItem *active = nullptr;
    enum DeviceType {
        Unknown,
        Drive,
        Partition,
        VirtualDevices,
        VirtualBD,
        Subvolume
    } type;
    struct Flags {
        bool oldLayout : 1;
        bool cryptoV : 1;
        bool autoCrypto : 1;
        bool mapLock : 1;
    } flags = {};
    QString device, devMapper;
    long long size = 0;
    QString label, curLabel;
    QString usefor;
    bool encrypt = false;
    QString format, curFormat;
    QString options;
    bool dump = false;
    int pass = 0;
    DeviceItem(enum DeviceType type, DeviceItem *parent = nullptr, DeviceItem *preceding = nullptr);
    DeviceItem(enum DeviceType type, PartModel &container, DeviceItem *preceding = nullptr);
    ~DeviceItem();
    void clear();
    int row() const;
    DeviceItem *parent() const;
    DeviceItem *child(int row);
    int indexOfChild(DeviceItem *child);
    int childCount() const;
    void sortChildren();
    // Helpers
    static QString realUseFor(const QString &use);
    inline QString realUseFor() const { return realUseFor(usefor); }
    QString shownUseFor() const;
    void setActive(bool boot);
    bool isActive() const;
    bool isLocked() const;
    bool willFormat() const;
    bool canEncrypt() const;
    QString mappedDevice(const bool full = false) const;
    bool willMap() const;
    QString shownDevice() const;
    QStringList allowedUsesFor(bool real = true) const;
    QStringList allowedFormats() const;
    QString shownFormat(const QString &fmt) const;
    bool isVolume() const;
    bool canMount() const;
    /* Convenience */
    DeviceItem *addPart(int defaultMB, const QString &defaultUse, bool crypto);
    void driveAutoSetActive();
    void autoFill(unsigned int changed = 0xFFFF);
};
class DeviceItemIterator
{
    DeviceItem *pos;
    int ixPos = 0;
    QStack<int> ixParents;
public:
    DeviceItemIterator(DeviceItem *item) : pos(item) {}
    DeviceItemIterator(PartModel &model);
    inline DeviceItem *operator*() const { return pos; }
    void next();
};

class PartModel : public QAbstractItemModel
{
    Q_OBJECT
    DeviceItem *root = nullptr;
    DeviceItem *changing = nullptr;
    friend class DeviceItem;
    class PartMan &partman;
public:
    enum TreeColumns {
        Device,
        Size,
        UseFor,
        Label,
        Encrypt,
        Format,
        Options,
        Dump,
        Pass,
        _TreeColumns_
    };
    PartModel(PartMan &pman, QObject *parent = nullptr);
    ~PartModel();
    QVariant data(const QModelIndex &index, int role) const override;
    bool setData(const QModelIndex &index, const QVariant &value, int role = Qt::EditRole) override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;
    QVariant headerData(int section, Qt::Orientation orientation,
                        int role = Qt::DisplayRole) const override;
    QModelIndex index(int row, int column,
                      const QModelIndex &parent = QModelIndex()) const override;
    inline QModelIndex index(DeviceItem *item) const { return createIndex(item->row(), 0, item); }
    QModelIndex parent(const QModelIndex &index) const override;
    DeviceItem *item(const QModelIndex &index) const;
    DeviceItem *item(int index) const;
    int count() const;
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    inline int columnCount(const QModelIndex &) const override { return _TreeColumns_; }
    void clear();
    DeviceItem *insert(enum DeviceItem::DeviceType type, DeviceItem *parent, DeviceItem *preceeding = nullptr);
    void remove(DeviceItem *item);
    bool changeBegin(DeviceItem *item);
    int changeEnd(bool notify = true);
    void notifyChange(class DeviceItem *item, int first = -1, int last = -1);
};

class DeviceItemDelegate : public QStyledItemDelegate
{
    Q_OBJECT
    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override;
    QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &,
        const QModelIndex &index) const override;
    void setEditorData(QWidget *editor, const QModelIndex &index) const override;
    void setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const override;
    void partOptionsMenu(const QPoint &);
};

class PartMan : public QObject
{
    Q_OBJECT
    MProcess &proc;
    BlockDeviceList &listBlkDevs;
    Ui::MeInstall &gui;
    QWidget *master;
    QMap<QString, DeviceItem *> mounts;
    QStringList listToUnmount;
    PartModel model;
    void setup();
    void scanVirtualDevices(bool rescan);
    void labelParts(DeviceItem *drvit);
    void resizeColumnsToFit();
    bool formatLinuxPartition(const QString &devpath, const QString &format, bool chkBadBlocks, const QString &label);
    bool calculatePartBD();
    bool prepareSubvolumes(DeviceItem *partit);
    DeviceItem *findOrigin(const QString &vdev);
    void treeItemChange();
    void treeSelChange();
    void treeMenu(const QPoint &);
    void partOptionsMenu(const QPoint &);
    void partClearClick(bool);
    void partAddClick(bool);
    void partRemoveClick(bool);
    void partMenuUnlock(DeviceItem *twit);
    void partMenuLock(DeviceItem *twit);
    void scanSubvolumes(DeviceItem *partit);
    bool luksFormat(const QString &dev, const QByteArray &password);
    bool luksOpen(const QString &dev, const QString &luksfs,
        const QByteArray &password, const QString &options = QString());
public:
    bool gptoverride=false, uefi=false, brave=false;
    long long rootSpaceNeeded = 0;
    long long bootSpaceNeeded = 0;
    QMap<QString, QString> defaultLabels;
    PartMan(MProcess &mproc, BlockDeviceList &bdlist, Ui::MeInstall &ui, QWidget *parent);
    void populate(DeviceItem *drvstart = nullptr);
    bool manageConfig(MSettings &config, bool save);
    bool composeValidate(bool automatic, const QString &project);
    bool checkTargetDrivesOK();
    DeviceItem *selectedDriveAuto();
    void clearAllUses();
    int layoutDefault(DeviceItem *drvit, int rootPercent, bool crypto, bool updateTree=true);
    int countPrepSteps();
    bool preparePartitions();
    bool formatPartitions();
    bool fixCryptoSetup(const QString &keyfile, bool isNewKey);
    bool makeFstab(bool populateMediaMounts);
    bool mountPartitions();
    void unmount();
    bool willFormat(const QString &point);
    QString getMountDev(const QString &point, const bool mapped=true);
    int swapCount();
    int isEncrypt(const QString &point);
};

#endif // PARTMAN_H
