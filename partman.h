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

#define PARTMAN_SAFETY (2*MB) // 1MB at start + Compensate for rounding errors.
#define PARTMAN_MAX_PARTS 128 // Maximum number of partitions Linux supports.

class PartMan : public QAbstractItemModel
{
    Q_OBJECT
public:
    class Device;
    class Iterator;
    enum TreeColumns {
        COL_DEVICE,
        COL_SIZE,
        COL_FLAG_ACTIVE,
        COL_FLAG_ESP,
        COL_USEFOR,
        COL_LABEL,
        COL_ENCRYPT,
        COL_FORMAT,
        COL_CHECK,
        COL_OPTIONS,
        COL_DUMP,
        COL_PASS,
        TREE_COLUMNS
    };
    static constexpr struct ColumnProperties {
        bool dropdown : 1;
        bool checkbox : 1;
        bool advanced : 1;
    } colprops[TREE_COLUMNS] = {
        {false, false, false}, // COL_DEVICE
        {false, false, false}, // COL_SIZE
        {false, true, true}, // COL_FLAG_ACTIVE
        {false, true, true}, // COL_FLAG_ESP
        {true, false, false}, // COL_USEFOR
        {false, false, false}, // COL_LABEL
        {false, true, false}, // COL_ENCRYPT
        {true, false, false}, // COL_FORMAT
        {false, true, false}, // COL_CHECK
        {false, false, true}, // COL_OPTIONS
        {false, true, true}, // COL_DUMP
        {false, false, true} // COL_PASS
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
    QString bootPartPath;
    class AutoPart *autopart = nullptr;
    PartMan(class MProcess &mproc, class Core &mcore, class Ui::MeInstall &ui, class Crypto &cman,
        const class MIni &appConf, const QCommandLineParser &appArgs, QObject *parent = nullptr);
    ~PartMan();
    void scan(Device *drvstart = nullptr);
    void scanVirtualDevices(bool rescan);
    void scanSubvolumes(class Device *part, const QString &scratch = QString());
    bool loadConfig(class MSettings &config) noexcept;
    void saveConfig(class MSettings &config) const noexcept;
    bool validate(bool automatic, QTreeWidgetItem *confroot = nullptr) const noexcept;
    bool checkTargetDrivesOK() const;
    int countPrepSteps() noexcept;
    void prepStorage();
    bool installTabs() noexcept;
    void clearWorkArea();
    Device *findByPath(const QString &devpath) const noexcept;
    Device *findByMount(const QString &mount) const noexcept;
    Device *findHostDev(const QString &path) const noexcept;
    struct VolumeSpec volSpecTotal(const QString &path, const QStringList &excludes) const noexcept;
    struct VolumeSpec volSpecTotal(const QString &path, bool excludeChildMounts = true) const noexcept;
    /* Convenience */
    struct NameParts {
        QString drive;
        QString partition;
    };
    static NameParts splitName(const QString &devname) noexcept;
    static QString joinName(const QString &drive, int partnum) noexcept;

private:
    friend class Device;
    friend class Iterator;
    class MProcess &proc;
    class Core &core;
    class Device *root = nullptr;
    class Device *changing = nullptr;
    Ui::MeInstall &gui;
    class Crypto &crypto;
    bool brave;
    void resizeColumnsToFit() noexcept;
    void preparePartitions();
    void formatPartitions();
    void prepareSubvolumes(Device *part);
    bool makeFstab() noexcept;
    void mountPartitions();
    void treeItemChange() noexcept;
    void treeSelChange() noexcept;
    void treeMenu(const QPoint &);
    void showAdvancedFields(bool show) noexcept;
    void partOptionsMenu(const QPoint &);
    void partClearClick(bool);
    void partAddClick(bool) noexcept;
    void partRemoveClick(bool) noexcept;
    void partReloadClick();
    void partManRunClick();
    void partMenuUnlock(class Device *part);
    void partMenuLock(class Device *volume);
    bool confirmSpace() const noexcept;
    bool confirmBootable() const noexcept;

    // Model View Controller
    class ItemDelegate;
    QVariant data(const QModelIndex &index, int role) const noexcept override;
    bool setData(const QModelIndex &index, const QVariant &value, int role = Qt::EditRole) noexcept override;
    Qt::ItemFlags flags(const QModelIndex &index) const noexcept override;
    QVariant headerData(int section, Qt::Orientation orientation,
        int role = Qt::DisplayRole) const noexcept override;
    QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const override;
    QModelIndex parent(const QModelIndex &index) const noexcept override;
    Device *item(const QModelIndex &index) const noexcept;
    int rowCount(const QModelIndex &parent = QModelIndex()) const noexcept override;
    inline int columnCount(const QModelIndex &) const noexcept override { return TREE_COLUMNS; }
public:
    QModelIndex index(Device *device) const noexcept;
    bool removeDevice(Device *device) noexcept;
    bool newSubvolume(Device *device) noexcept;
    bool scanSubvolumesFor(Device *device) noexcept;
    Device *selectedDevice() const noexcept;
    bool changeBegin(Device *device) noexcept;
    int changeEnd(bool autofill = true, bool notify = true) noexcept;
    void notifyChange(class Device *device, int first = -1, int last = -1) noexcept;
};

class PartMan::Device
{
    Q_DECLARE_TR_FUNCTIONS(Device)
public:
    Device *active = nullptr;
    Device *origin = nullptr;
    enum DeviceType {
        UNKNOWN,
        DRIVE,
        PARTITION,
        VIRTUAL_DEVICES,
        VIRTUAL,
        SUBVOLUME
    } type = UNKNOWN;
    struct Flags {
        bool rotational : 1;
        bool nasty : 1;
        bool curEmpty : 1;
        bool oldLayout : 1;
        bool bootRoot : 1;
        bool curESP : 1;
        bool volCrypto : 1;
        bool sysEFI : 1;
    } flags = {};
    QString model, name, path, uuid, partuuid, map;
    QString label, curLabel, partlabel;
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

    Device(enum DeviceType type, Device *parent, Device *preceding = nullptr) noexcept;
    ~Device();
    // Copy and move constructors will cause dangling pointers here.
    Device(const Device &) = delete;
    Device &operator=(const Device &) = delete;
    Device(Device &&) = delete;

    void clear() noexcept;
    int row() const noexcept;
    Device *parent() const noexcept;
    Device *child(int row) const noexcept;
    int indexOfChild(const Device *device) const noexcept;
    void sortChildren() noexcept;
    // Layout finishing
    Device *addPart(long long defaultSize, const QString &defaultUse, bool crypto) noexcept;
    void driveAutoSetActive() noexcept;
    void labelParts() noexcept;
    // Helpers
    void setActive(bool on) noexcept;
    bool isActive() const noexcept;
    bool isLocked() const noexcept;
    bool willFormat() const noexcept;
    bool canEncrypt() const noexcept;
    bool willEncrypt() const noexcept;
    QString assocUUID() const noexcept;
    QString mappedDevice() const noexcept;
    bool willMap() const noexcept;
    QString shownDevice() const noexcept;
    QStringList allowedUsesFor(bool all = true) const noexcept;
    QStringList allowedFormats() const noexcept;
    QString finalFormat() const noexcept;
    QString finalLabel() const noexcept;
    QString shownFormat(const QString &fmt) const noexcept;
    inline QString shownFormat() const noexcept { return shownFormat(format); }
    inline bool isVolume() const noexcept { return (type == PARTITION || type == VIRTUAL); }
    QString mountPoint() const noexcept;
    bool canMount(bool fsonly = true) const noexcept;
    long long driveFreeSpace(bool inclusive = false) const noexcept;
    /* Convenience */
    QString friendlyName(bool mappedFormat = false) const noexcept;
    void addToCombo(QComboBox *combo, bool warnNasty = false) const noexcept;

private:
    friend class PartMan;
    friend class Iterator;
    friend class ItemDelegate;
    std::vector<Device *> children;
    Device *parentItem = nullptr;
    class PartMan &partman;
    int order = -1;
    inline Device(class PartMan &pman) : partman(pman) {}
    void autoFill(unsigned int changed = 0xFFFF) noexcept;
};

class PartMan::Iterator
{
    Device *start = nullptr;
    Device *pos = nullptr;
    int ixPos = 0;
    std::stack<int, std::vector<int>> ixParents;
public:
    Iterator(Device *device) noexcept : start(device), pos(device) {}
    Iterator(const PartMan &partman) noexcept;
    inline Device *operator*() const noexcept { return pos; }
    void next() noexcept;
};

class PartMan::ItemDelegate : public QStyledItemDelegate
{
    Q_OBJECT
public:
    ItemDelegate(QObject *parent = nullptr) noexcept : QStyledItemDelegate(parent) {}
private:
    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override;
    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override;
    QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &,
        const QModelIndex &index) const override;
    void setEditorData(QWidget *editor, const QModelIndex &index) const override;
    void setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const override;
    void partOptionsMenu() noexcept;
};

#endif // PARTMAN_H
