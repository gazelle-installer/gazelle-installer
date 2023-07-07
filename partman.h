/***************************************************************************
 * Basic partition manager for the installer.
 *
 *   Copyright (C) 2019-2023 by AK-47
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

#include <map>
#include <stack>
#include <vector>
#include <QCommandLineParser>
#include <QAbstractItemModel>
#include <QStyledItemDelegate>
#include <QString>

#include "ui_meinstall.h"

#define KB 1024LL
#define MB 1048576LL
#define GB 1073741824LL
#define TB 1099511627776LL
#define PB 1125899906842624LL
#define EB 1152921504606846976LL

#define PARTMAN_SAFETY (8*MB) // 1MB at start + Compensate for rounding errors.
#define PARTMAN_MAX_PARTS 128 // Maximum number of partitions Linux supports.

class DeviceItem
{
    Q_DECLARE_TR_FUNCTIONS(DeviceItem)
    friend class PartMan;
    friend class DeviceItemIterator;
    friend class DeviceItemDelegate;
    std::vector<DeviceItem *> children;
    DeviceItem *parentItem = nullptr;
    class PartMan *partman = nullptr;
    int order = -1;
    void autoFill(unsigned int changed = 0xFFFF) noexcept;
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

    DeviceItem(enum DeviceType type, DeviceItem *parent = nullptr, DeviceItem *preceding = nullptr) noexcept;
    ~DeviceItem();
    void clear() noexcept;
    int row() const noexcept;
    DeviceItem *parent() const noexcept;
    DeviceItem *child(int row) const noexcept;
    int indexOfChild(const DeviceItem *child) const noexcept;
    int childCount() const noexcept;
    void sortChildren() noexcept;
    // Layout finishing
    DeviceItem *addPart(long long defaultSize, const QString &defaultUse, bool crypto) noexcept;
    void driveAutoSetActive() noexcept;
    void labelParts() noexcept;
    // Helpers
    static QString realUseFor(const QString &use) noexcept;
    inline QString realUseFor() const noexcept { return realUseFor(usefor); }
    static QString shownUseFor(const QString &use) noexcept;
    inline QString shownUseFor() const noexcept { return shownUseFor(realUseFor()); }
    void setActive(bool boot) noexcept;
    bool isActive() const noexcept;
    bool isLocked() const noexcept;
    bool willUseGPT() const noexcept;
    bool willFormat() const noexcept;
    bool canEncrypt() const noexcept;
    bool willEncrypt() const noexcept;
    QString assocUUID() const noexcept;
    QString mappedDevice() const noexcept;
    bool willMap() const noexcept;
    QString shownDevice() const noexcept;
    QStringList allowedUsesFor(bool real = true, bool all = true) const noexcept;
    QStringList allowedFormats() const noexcept;
    QString finalFormat() const noexcept;
    QString shownFormat(const QString &fmt) const noexcept;
    inline bool isVolume() const noexcept { return (type == Partition || type == VirtualBD); }
    bool canMount(bool pointonly = true) const noexcept;
    long long driveFreeSpace(bool inclusive = false) const noexcept;
    /* Convenience */
    struct NameParts {
        QString drive;
        QString partition;
    };
    void addToCombo(QComboBox *combo, bool warnNasty = false) const noexcept;
    static NameParts split(const QString &devname) noexcept;
    static QString join(const QString &drive, int partnum) noexcept;
};
class DeviceItemIterator
{
    DeviceItem *pos = nullptr;
    int ixPos = 0;
    std::stack<int, std::vector<int>> ixParents;
public:
    DeviceItemIterator(DeviceItem *item) noexcept : pos(item) {}
    DeviceItemIterator(const PartMan &partman) noexcept;
    inline DeviceItem *operator*() const noexcept { return pos; }
    void next() noexcept;
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
    void emitCommit() noexcept;
    void partOptionsMenu() noexcept;
};

class PartMan : public QAbstractItemModel
{
    Q_OBJECT
    friend class DeviceItem;
    friend class DeviceItemIterator;
    class MProcess &proc;
    DeviceItem root;
    DeviceItem *changing = nullptr;
    Ui::MeInstall &gui;
    bool brave, gptoverride;
    void scanVirtualDevices(bool rescan);
    void resizeColumnsToFit() noexcept;
    void preparePartitions();
    void formatPartitions();
    void prepareSubvolumes(DeviceItem *partit);
    void fixCryptoSetup();
    bool makeFstab();
    void mountPartitions();
    void treeItemChange() noexcept;
    void treeSelChange() noexcept;
    void treeMenu(const QPoint &);
    void partOptionsMenu(const QPoint &);
    void partClearClick(bool);
    void partAddClick(bool) noexcept;
    void partRemoveClick(bool) noexcept;
    void partReloadClick();
    void partManRunClick();
    void partMenuUnlock(DeviceItem *twit);
    void partMenuLock(DeviceItem *twit);
    void scanSubvolumes(DeviceItem *partit);
    bool confirmSpace(class QMessageBox &msgbox) noexcept;
    bool confirmBootable(class QMessageBox &msgbox) noexcept;
    void luksFormat();
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
    struct VolumeSpec
    {
        QString defaultLabel;
        long long image = 0;
        long long minimum = 0;
        long long preferred = 0;
    };
    std::map<QString, struct VolumeSpec> volSpecs;
    QString bootUUID;
    std::map<QString, DeviceItem *> mounts;
    class AutoPart *autopart = nullptr;
    PartMan(class MProcess &mproc, class Ui::MeInstall &ui,
        const class QSettings &appConf, const QCommandLineParser &appArgs);
    void scan(DeviceItem *drvstart = nullptr);
    bool manageConfig(class MSettings &config, bool save) noexcept;
    bool composeValidate(bool automatic, const QString &project) noexcept;
    bool checkTargetDrivesOK();
    DeviceItem *selectedDriveAuto() noexcept;
    void clearAllUses() noexcept;
    int countPrepSteps() noexcept;
    void prepStorage();
    void installTabs();
    void clearWorkArea();
    int swapCount() const noexcept;
    int isEncrypt(const QString &point) const noexcept;
    DeviceItem *findByPath(const QString &devpath) const noexcept;
    DeviceItem *findHostDev(const QString &path) const noexcept;
    struct VolumeSpec volSpecTotal(const QString &path, const QStringList &vols) const noexcept;
    struct VolumeSpec volSpecTotal(const QString &path) const noexcept;
    // Model View Controller
    QVariant data(const QModelIndex &index, int role) const noexcept override;
    bool setData(const QModelIndex &index, const QVariant &value, int role = Qt::EditRole) noexcept override;
    Qt::ItemFlags flags(const QModelIndex &index) const noexcept override;
    QVariant headerData(int section, Qt::Orientation orientation,
        int role = Qt::DisplayRole) const noexcept override;
    QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const override;
    QModelIndex index(DeviceItem *item) const noexcept;
    QModelIndex parent(const QModelIndex &index) const noexcept override;
    DeviceItem *item(const QModelIndex &index) const noexcept;
    int rowCount(const QModelIndex &parent = QModelIndex()) const noexcept override;
    inline int columnCount(const QModelIndex &) const noexcept override { return _TreeColumns_; }
    bool changeBegin(DeviceItem *item) noexcept;
    int changeEnd(bool notify = true) noexcept;
    void notifyChange(class DeviceItem *item, int first = -1, int last = -1) noexcept;
};

#endif // PARTMAN_H
