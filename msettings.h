/***************************************************************************
 * MSettings class - Installer-specific extensions to QSettings.
 *
 *   Copyright (C) 2019 by AK-47
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

#include <map>
#include <QFile>

class MIni
{
    QFile file;
    typedef std::map<QString, QString> SettingsMap;
    typedef std::map<QString, SettingsMap> SettingsGroup;
    std::map<QString, SettingsGroup> sections;
    QString cursection, curgroup;
public:
    MIni(const QString &filename, bool readOnly = false) noexcept;
    ~MIni() noexcept;
    void clear() noexcept;
    QString fileName() const noexcept { return file.fileName(); }
    bool load() noexcept;
    bool sync() noexcept;
    bool closeAndCopyTo(const QString &filename) noexcept;
    void setSection(const QString &name) noexcept;
    QString section() const noexcept { return cursection; }
    void beginGroup(const QString &path) noexcept;
    void endGroup() noexcept;
    bool contains(const QString &key) const noexcept;
    enum ValState {
        VAL_NOTFOUND,
        VAL_OK,
        VAL_INVALID
    };
    QString getString(const QString &key, const QString &defaultValue = QString()) const noexcept;
    void setString(const QString &key, const QString &value) noexcept;
    bool getBoolean(const QString &key, bool defaultValue = false, enum ValState *valid = nullptr) const noexcept;
    void setBoolean(const QString &key, const bool value) noexcept;
    long long getInteger(const QString &key, long long defaultValue = 0,
        enum ValState *valid = nullptr, int base = 10) const noexcept;
    void setInteger(const QString &key, const long long value) noexcept;
    void dumpDebug(const class QRegularExpression *censor = nullptr) const noexcept;
};

class MSettings: public MIni
{
public:
    MSettings(const QString &filename, bool readOnly = false) noexcept : MIni(filename, readOnly) {}
    bool bad = false;
    void setSave(bool save) noexcept;
    bool isSave() const noexcept { return saving; }
    void setGroupWidget(class QWidget *widget) noexcept;
    void setSection(const QString &name, class QWidget *widget) noexcept;
    void markBadWidget(class QWidget *widget) noexcept;
    static bool isBadWidget(class QWidget *widget) noexcept;
    void manageComboBox(const QString &key, class QComboBox *combo, const bool useData) noexcept;
    void manageCheckBox(const QString &key, class QCheckBox *checkbox) noexcept;
    void manageGroupCheckBox(const QString &key, class QGroupBox *groupbox) noexcept;
    void manageLineEdit(const QString &key, class QLineEdit *lineedit) noexcept;
    void manageSpinBox(const QString &key, class QSpinBox *spinbox) noexcept;
    int manageEnum(const QString &key, const int nchoices,
            const char *choices[], const int curval) noexcept;
    void manageRadios(const QString &key, const int nchoices,
            const char *choices[], class QRadioButton *radios[]) noexcept;
private:
    bool saving = false;
    class QWidget *wgroup = nullptr;
};

#endif // MSETTINGS_H
