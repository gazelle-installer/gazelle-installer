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

#ifndef PARTMAN_H
#define PARTMAN_H

#include <QCommandLineParser>
#include <QAbstractItemModel>
#include <QStyledItemDelegate>
#include <QString>
#include <QMap>
#include <QStack>

#include "ui_meinstall.h"
#include "mprocess.h"
#include "msettings.h"
#include "safecache.h"

class DeviceItem
{
    Q_DECLARE_TR_FUNCTIONS(DeviceItem)
    friend class PartMan;
    friend class DeviceItemIterator;
    friend class DeviceItemDelegate;
    QVector<DeviceItem *> children;
    DeviceItem *parentItem = nullptr;
    class PartMan *partman = nullptr;
    int order = -1;
    DeviceItem *addPart(int defaultMB, const QString &defaultUse, bool crypto);
    void driveAutoSetActive();
    void autoFill(unsigned int changed = 0xFFFF);
    void labelParts();
public:
    DeviceItem *active = nullptr;
    DeviceItem *origin = nullptr;
    enum DeviceType {
        Unknown,
        Drive,
        Partition,
        VirtualDevices,
        VirtualBD,
        Subvolume
    } type;
    struct Flags {
        bool rotational : 1;
        bool nasty : 1;
        bool curEmpty : 1;
        bool oldLayout : 1;
        bool bootRoot : 1;
        bool curMBR : 1;
        bool curGPT : 1;
        bool curESP : 1;
        bool volCrypto : 1;
    } flags = {};
    QString model, device, path, uuid, devMapper;
    QString label, curLabel;
    QString usefor;
    QString format, curFormat;
    QString options;
    long long size = 0;
    int mapCount = 0;
    int physec;
    int pass = 0;
    int discgran = 0;
    bool encrypt = false;
    bool chkbadblk = false;
    bool dump = false;
    bool addToCrypttab = false;

    DeviceItem(enum DeviceType type, DeviceItem *parent = nullptr, DeviceItem *preceding = nullptr);
    ~DeviceItem();
    void clear();
    int row() const;
    DeviceItem *parent() const;
    DeviceItem *child(int row) const;
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
    bool willUseGPT() const;
    bool willFormat() const;
    bool canEncrypt() const;
    bool willEncrypt() const;
    QString mappedDevice() const;
    bool willMap() const;
    QString shownDevice() const;
    QStringList allowedUsesFor(bool real = true) const;
    QStringList allowedFormats() const;
    QString shownFormat(const QString &fmt) const;
    inline bool isVolume() const { return (type == Partition || type == VirtualBD); }
    bool canMount() const;
    long long driveFreeSpace(bool inclusive = false) const;
    /* Convenience */
    int layoutDefault(int rootPercent, bool crypto, bool updateTree=true);
    void addToCombo(QComboBox *combo, bool warnNasty = false) const;
    static QStringList split(const QString &devname);
    static QString join(const QString &drive, int partnum);
};
class DeviceItemIterator
{
    DeviceItem *pos = nullptr;
    int ixPos = 0;
    QStack<int> ixParents;
public:
    DeviceItemIterator(DeviceItem *item) : pos(item) {}
    DeviceItemIterator(const PartMan &partman);
    inline DeviceItem *operator*() const { return pos; }
    void next();
};

class DeviceItemDelegate : public QStyledItemDelegate
{
    Q_OBJECT
    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override;
    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override;
    QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &,
        const QModelIndex &index) const override;
    void setEditorData(QWidget *editor, const QModelIndex &index) const override;
    void setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const override;
    void emitCommit();
    void partOptionsMenu();
};

class PartMan : public QAbstractItemModel
{
    Q_OBJECT
    friend class DeviceItem;
    friend class DeviceItemIterator;
    MProcess &proc;
    DeviceItem root;
    DeviceItem *changing = nullptr;
    Ui::MeInstall &gui;
    QWidget *master;
    SafeCache key;
    QMap<QString, QString> defaultLabels;
    bool brave, gptoverride;
    void scanVirtualDevices(bool rescan);
    void resizeColumnsToFit();
    void preparePartitions();
    void formatPartitions();
    void prepareSubvolumes(DeviceItem *partit);
    bool fixCryptoSetup();
    bool makeFstab();
    void mountPartitions();
    void treeItemChange();
    void treeSelChange();
    void treeMenu(const QPoint &);
    void partOptionsMenu(const QPoint &);
    void partClearClick(bool);
    void partAddClick(bool);
    void partRemoveClick(bool);
    void partReloadClick();
    void partManRunClick();
    void partMenuUnlock(DeviceItem *twit);
    void partMenuLock(DeviceItem *twit);
    void scanSubvolumes(DeviceItem *partit);
    void luksFormat(DeviceItem *partit, const QByteArray &password);
    void luksOpen(DeviceItem *partit, const QString &luksfs, const QByteArray &password);
public:
    enum TreeColumns {
        Device,
        Size,
        UseFor,
        Label,
        Encrypt,
        Format,
        Check,
        Options,
        Dump,
        Pass,
        _TreeColumns_
    };
    QString bootUUID;
    long long rootSpaceNeeded = 0;
    long long bootSpaceNeeded = 0;
    QMap<QString, DeviceItem *> mounts;
    PartMan(MProcess &mproc, Ui::MeInstall &ui, const QSettings &appConf, const QCommandLineParser &appArgs);
    void scan(DeviceItem *drvstart = nullptr);
    bool manageConfig(MSettings &config, bool save);
    bool composeValidate(bool automatic, const QString &project);
    bool checkTargetDrivesOK();
    DeviceItem *selectedDriveAuto();
    void clearAllUses();
    int countPrepSteps();
    void prepStorage();
    void installTabs();
    void loadKeyMaterial(const QString &keyfile);
    void unmount();
    bool willFormat(const QString &point);
    QString getMountDev(const QString &point, const bool mapped=true);
    int swapCount();
    int isEncrypt(const QString &point);
    DeviceItem *findByPath(const QString &devpath) const;
    DeviceItem *findHostDev(const QString &path) const;
    // Model View Controller
    QVariant data(const QModelIndex &index, int role) const override;
    bool setData(const QModelIndex &index, const QVariant &value, int role = Qt::EditRole) override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
    QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const override;
    QModelIndex index(DeviceItem *item) const;
    QModelIndex parent(const QModelIndex &index) const override;
    DeviceItem *item(const QModelIndex &index) const;
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    inline int columnCount(const QModelIndex &) const override { return _TreeColumns_; }
    bool changeBegin(DeviceItem *item);
    int changeEnd(bool notify = true);
    void notifyChange(class DeviceItem *item, int first = -1, int last = -1);
};

#endif // PARTMAN_H
