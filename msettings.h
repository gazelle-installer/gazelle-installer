/***************************************************************************
 * MIni and MSettings class - INI format parser and settings management.
 *
 *   Copyright (C) 2019-2024 by AK-47
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
#ifndef MSETTINGS_H
#define MSETTINGS_H

#include <vector>
#include <QFile>

class MIni
{
    friend class MSettings;
    static bool lessCaseInsensitive(const QString &a, const QString &b) noexcept;
    QFile file;
    struct Setting {
        QString key;
        QString value;
        Setting(const QString &key, const QString &value) noexcept : key(key), value(value) {}
    };
    class Group {
    public:
        QString name;
        std::vector<Setting> settings;
        std::vector<Group *> children;
        Group *parent;
        Group(Group *parent, const QString &name) noexcept;
        Group(Group&& other) noexcept; // Move constructor
        ~Group() noexcept;
        QString path() const noexcept;
        // Delete copy constructors because they cause problems.
        Group(const Group &) = delete;
        Group &operator=(const Group &) = delete;
    };
    class Iterator
    {
        const Group *start = nullptr;
        const Group *pos = nullptr;
        size_t ixPos = 0;
        std::vector<int> ixParents;
    public:
        Iterator(const Group *group) noexcept;
        inline const Group *operator*() const noexcept { return pos; }
        void next() noexcept;
        int level() noexcept;
    };
    std::vector<Group> sections;
    std::vector<int> groupPath;
    int curSectionIndex = 0;

    const Group *currentGroup() const noexcept;
    Group *currentGroup() noexcept;
public:
    enum OpenMode {
        NotOpen = QIODeviceBase::NotOpen, // 0x0000
        ReadOnly = QIODeviceBase::ReadOnly, // 0x0001
        WriteOnly = QIODeviceBase::WriteOnly, // 0x0002
        ReadWrite = QIODeviceBase::ReadWrite, // 0x0003 (ReadOnly | WriteOnly)
        NewOnly = QIODeviceBase::NewOnly, // 0x0040
        ExistingOnly = QIODeviceBase::ExistingOnly // 0x0080
    };
    MIni(const QString &filename, OpenMode mode = OpenMode::ReadWrite, bool *loadOK = nullptr) noexcept;
    ~MIni() noexcept;
    void clear() noexcept;
    QString fileName() const noexcept { return file.fileName(); }
    bool load() noexcept;
    bool save() noexcept;
    bool closeAndCopyTo(const QString &filename) noexcept;
    QString section() const noexcept;
    bool setSection(const QString &name = QString(), bool create = true) noexcept;
    QString group() const noexcept;
    bool setGroup(const QString &path = QString(), bool create = true) noexcept;
    bool beginGroup(const QString &name, bool create = true) noexcept;
    void endGroup() noexcept;
    QStringList getSections() const noexcept;
    QStringList getGroups() const noexcept;
    QStringList getKeys() const noexcept;
    bool contains(const QString &key) const noexcept;
    enum ValState {
        VAL_NOTFOUND,
        VAL_OK,
        VAL_INVALID
    };
    QString getRaw(const QString &key, const QString &defaultValue = QString()) const noexcept;
    void setRaw(const QString &key, const QString &value) noexcept;
    QString getString(const QString &key, const QString &defaultValue = QString()) const noexcept;
    void setString(const QString &key, const QString &value) noexcept;
    bool getBoolean(const QString &key, bool defaultValue = false, enum ValState *valid = nullptr) const noexcept;
    void setBoolean(const QString &key, const bool value) noexcept;
    long long getInteger(const QString &key, long long defaultValue = 0,
        enum ValState *valid = nullptr, int base = 10) const noexcept;
    void setInteger(const QString &key, const long long value) noexcept;
    double getFloat(const QString &key, double defaultValue = 0, enum ValState *valid = nullptr) const noexcept;
    void setFloat(const QString &key, const double value, const char format = 'G', const int precision = 6) noexcept;
};

class MSettings: public MIni
{
    QStringList filter;
public:
    bool good;
    MSettings(const QString &filename, bool saveMode) noexcept;
    void addFilter(const QString &key) noexcept;
    void dumpDebug() const noexcept;
    bool isSave() const noexcept { return saving; }
    void setGroupWidget(class QWidget *widget) noexcept;
    void setSection(const QString &name, class QWidget *widget) noexcept;
    void markBadWidget(class QWidget *widget) noexcept;
    static bool isBadWidget(class QWidget *widget) noexcept;
    void manageComboBox(const QString &key, class QComboBox *combo, const bool useData) noexcept;
    void manageCheckBox(const QString &key, class QCheckBox *checkbox) noexcept;
    void manageGroupCheckBox(const QString &key, class QGroupBox *groupbox) noexcept;
    void manageLineEdit(const QString &key, class QLineEdit *lineedit) noexcept;
    void managePassEdit(const QString &key, class QLineEdit *master, class QLineEdit *slave, bool force = false) noexcept;
    void manageSpinBox(const QString &key, class QSpinBox *spinbox) noexcept;
    int manageEnum(const QString &key, const int nchoices,
            const char *const choices[], const int curval) noexcept;
    void manageRadios(const QString &key, const int nchoices,
            const char *const choices[], class QRadioButton *const radios[]) noexcept;
private:
    bool saving = false;
    class QWidget *wgroup = nullptr;
};

#endif // MSETTINGS_H
