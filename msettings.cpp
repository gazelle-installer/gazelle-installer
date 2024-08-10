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

#include <QGroupBox>
#include <QComboBox>
#include <QCheckBox>
#include <QRadioButton>
#include <QLineEdit>
#include <QSpinBox>
#include <QWidget>
#include <QDebug>

#include "msettings.h"

MIni::MIni(const QString &filename, OpenMode mode, bool *loadOK) noexcept
    : file(filename), sections(&MIni::lessCaseInsensitive)
{
    bool ok = false;
    if (!filename.isEmpty()) {
        ok = file.open(QFile::Text | static_cast<QFile::OpenMode>(mode));
        if (ok && (mode & OpenMode::ReadOnly)) {
            ok = load();
        }
    }
    if (loadOK) *loadOK = ok;
}
MIni::~MIni() noexcept
{
    save();
}

void MIni::clear() noexcept
{
    curgroup.clear();
    cursection.clear();
    sections.clear();
}
bool MIni::load() noexcept
{
    if (!(file.openMode() & QFile::ReadOnly)) {
        return false;
    }
    clear();
    file.seek(0);
    while (!file.atEnd()) {
        const QByteArray &line = file.readLine().trimmed();
        if (line.isEmpty() || line.startsWith(';') || line.startsWith('#')) {
            // Empty lines or comment lines to be ignored.
            continue;
        } else if (line.startsWith('[')) {
            // Section.
            if (line.size() < 3 || !line.endsWith(']') || !curgroup.isEmpty()) {
                return false; // Incomplete bracket set, empty section, or section inside a group.
            }
            cursection = line.mid(1, line.size()-2);
        } else if (line == "}") {
            if (curgroup.isEmpty()) {
                return false; // Mismatched group ending.
            }
            endGroup();
        } else {
            const int delimpos = line.indexOf('=');
            if (delimpos > 0) {
                // Value
                setRaw(line.left(delimpos).trimmed(), line.mid(delimpos+1).trimmed());
            } else if (delimpos < 0 && line.endsWith('{')) {
                // Group
                beginGroup(line.chopped(1).trimmed());
            } else {
                return false; // Line starts with '=' or invalid group starter.
            }
        }
    }
    cursection.clear();
    return curgroup.isEmpty(); // All groups must be closed at this point.
}
bool MIni::save() noexcept
{
    if (!(file.openMode() & QFile::WriteOnly)) {
        return false;
    }
    file.resize(0);
    file.seek(0);
    for (const auto &section : sections) {
        if (!section.first.isEmpty()) {
            file.write(file.pos()>0 ? "\n[" : "[");
            file.write(section.first.toUtf8());
            file.write("]\n");
        }

        int prevgnest = -1;
        QString prevgpath;
        for (const auto &group : section.second) {
            const int depth = group.first.isEmpty() ? 0 : (1 + group.first.count('/'));
            const QString &gpath = group.first;
            // The first segment of the current and previous group that differs.
            int pivot = 0;
            for (int ixi = 0; ixi < depth; ++ixi) {
                if(prevgpath.section('/', ixi, ixi) != gpath.section('/', ixi, ixi)) {
                    pivot = ixi;
                    break;
                }
            }
            const std::vector<char> tabs(std::max(depth, prevgnest), '\t');
            // Close braces of previous groups.
            for (int ixi = prevgnest; ixi >= pivot; --ixi) {
                file.write(tabs.data(), ixi);
                file.write("}\n");
            }
            // Open braces of new groups.
            for(int ixi = pivot; ixi < depth; ++ixi) {
                file.write(tabs.data(), ixi);
                file.write(gpath.section('/', ixi, ixi).toUtf8());
                file.write(" {\n");
            }
            // Settings
            for (const auto &setting : group.second) {
                file.write(tabs.data(), depth);
                file.write(setting.first.toUtf8());
                file.write("=");
                file.write(setting.second.toUtf8());
                file.write("\n");
            }
            prevgpath = gpath;
            prevgnest = depth - 1;
        }

        // Close open groups before moving on to the next section (or the end).
        if (prevgnest >= 0) {
            const std::vector<char> tabs(prevgnest, '\t');
            for (int ixi = prevgnest; ixi >= 0; --ixi) {
                file.write(tabs.data(), ixi);
                file.write("}\n");
            }
        }
    }
    return true;
}
bool MIni::closeAndCopyTo(const QString &filename) noexcept
{
    save();
    QFile::remove(filename);
    return file.copy(filename);
}

void MIni::setSection(const QString &name) noexcept
{
    cursection = name;
    curgroup.clear();
}
void MIni::setGroup(const QString &path) noexcept
{
    curgroup = path;
}

void MIni::beginGroup(const QString &path) noexcept
{
    if (curgroup.isEmpty()) curgroup = path;
    else curgroup += '/' + path;
}
void MIni::endGroup() noexcept
{
    curgroup.truncate(curgroup.lastIndexOf('/'));
}

QStringList MIni::getSections() const noexcept
{
    QStringList list;
    for (auto &section : sections) {
        list.append(section.first);
    }
    return list;
}
QStringList MIni::getGroups() const noexcept
{
    QStringList list;
    if (auto ssearch = sections.find(cursection); ssearch != sections.end()) {
        for (auto &group : ssearch->second) {
            list.append(group.first);
        }
    }
    return list;
}
QStringList MIni::getKeys() const noexcept
{
    QStringList list;
    if (auto ssearch = sections.find(cursection); ssearch != sections.end()) {
        if (auto gsearch = ssearch->second.find(curgroup); gsearch != ssearch->second.end()) {
            for (auto &setting : gsearch->second) {
                list.append(setting.first);
            }
        }
    }
    return list;
}

bool MIni::contains(const QString &key) const noexcept
{
    if (auto ssearch = sections.find(cursection); ssearch != sections.end()) {
        if (auto gsearch = ssearch->second.find(curgroup); gsearch != ssearch->second.end()) {
            return (gsearch->second.count(key) > 0);
        }
    }
    return false;
}
QString MIni::getRaw(const QString &key, const QString &defaultValue) const noexcept
{
    if (auto ssearch = sections.find(cursection); ssearch != sections.end()) {
        if (auto gsearch = ssearch->second.find(curgroup); gsearch != ssearch->second.end()) {
            if (auto vsearch = gsearch->second.find(key); vsearch != gsearch->second.end()) {
                return vsearch->second;
            }
        }
    }
    return defaultValue;
}
void MIni::setRaw(const QString &key, const QString &value) noexcept
{
    const auto &snew = sections.try_emplace(cursection, &MIni::lessCaseInsensitive).first;
    const auto &gnew = snew->second.try_emplace(curgroup, &MIni::lessCaseInsensitive).first;
    gnew->second.insert_or_assign(key, value);
}

QString MIni::getString(const QString &key, const QString &defaultValue) const noexcept
{
    const QString &val = getRaw(key, defaultValue);
    if (val.size()>=2) {
        if (const QChar c = val.front(); c == val.back() && c == '"') {
            return val.mid(1, val.size()-2);
        }
    }
    return val;
}
void MIni::setString(const QString &key, const QString &value) noexcept
{
    setRaw(key, value);
}

bool MIni::getBoolean(const QString &key, bool defaultValue, enum ValState *valid) const noexcept
{
    const QString &val = getString(key);
    if (valid) *valid = VAL_NOTFOUND;
    if (!val.isNull()) {
        bool ok = true;
        if (!val.compare("true", Qt::CaseInsensitive)) defaultValue = true;
        else if (!val.compare("false", Qt::CaseInsensitive)) defaultValue = false;
        else defaultValue = (val.toInt(&ok) != 0);
        if (valid) *valid = ok ? VAL_OK : VAL_INVALID;
    }
    return defaultValue;
}
void MIni::setBoolean(const QString &key, const bool value) noexcept
{
    setRaw(key, value ? "true" : "false");
}

long long MIni::getInteger(const QString &key, long long defaultValue, enum ValState *valid, int base) const noexcept
{
    const QString &val = getString(key);
    if (valid) *valid = VAL_NOTFOUND;
    if (!val.isNull()) {
        bool ok = false;
        defaultValue = val.toLongLong(&ok, base);
        if (valid) *valid = ok ? VAL_OK : VAL_INVALID;
    }
    return defaultValue;
}
void MIni::setInteger(const QString &key, const long long value) noexcept
{
    setRaw(key, QString::number(value));
}

/* Case-insensitive key comparison for maps. */
bool MIni::lessCaseInsensitive(const QString &a, const QString &b) noexcept
{
    return (a.compare(b, Qt::CaseInsensitive) < 0);
}

/* Print to console and debug log. */
void MSettings::addFilter(const QString &key) noexcept
{
    filter.append(cursection + '/' + curgroup + '/' + key);
}
void MSettings::dumpDebug() const noexcept
{
    qDebug().noquote() << "Configuration:" << fileName();
    for (const auto &section : sections) {
        const char *bullet = " - ";
        if (!section.first.isEmpty()) {
            qDebug().noquote().nospace() << " = " << section.first << ':';
            bullet = "   - ";
        }
        for (const auto &group : section.second) {
            if (!group.first.isEmpty()) {
                qDebug().noquote().nospace() << "   + " << group.first << ':';
                bullet = "     - ";
            }
            for (const auto &setting : group.second) {
                const bool show = (setting.second.isEmpty()
                    || !filter.contains(section.first + '/' + group.first + '/' + setting.first));
                qDebug().noquote().nospace() << bullet << setting.first
                    << ": " << (show ? setting.second : "<filter>");
            }
        }
    }
    qDebug() << "End of configuration.";
}

/* Widget management */

MSettings::MSettings(const QString &filename, bool saveMode) noexcept
    : MIni(filename, saveMode ? MIni::WriteOnly : MIni::ReadOnly, &good), saving(saveMode)
{
    if (filename.isEmpty()) {
        good = true; // File-less configuration buffer.
    }
}

void MSettings::setGroupWidget(QWidget *widget) noexcept
{
    wgroup = widget;
}
void MSettings::setSection(const QString &name, QWidget *widget) noexcept
{
    MIni::setSection(name);
    setGroupWidget(widget);
}

void MSettings::markBadWidget(QWidget *widget) noexcept
{
    if (widget) {
        widget->setStyleSheet("QWidget { background: maroon; border: 2px inset red; }\n"
                              "QPushButton:!pressed { border-style: outset; }\n"
                              "QRadioButton { border-style: dotted; }");
    }
    if (wgroup) wgroup->setProperty("BAD", true);
    good = false;
}
bool MSettings::isBadWidget(QWidget *widget) noexcept
{
    return widget->property("BAD").toBool();
}

void MSettings::manageComboBox(const QString &key, QComboBox *combo, const bool useData) noexcept
{
    if (saving) {
        setString(key, useData ? combo->currentData().toString() : combo->currentText());
    } else {
        const QString &val = getString(key);
        if (!val.isNull()) {
            const int icombo = useData ? combo->findData(val, Qt::UserRole, Qt::MatchFixedString)
                             : combo->findText(val, Qt::MatchFixedString);
            if (icombo >= 0) combo->setCurrentIndex(icombo);
            else markBadWidget(combo);
        }
    }
}

void MSettings::manageCheckBox(const QString &key, QCheckBox *checkbox) noexcept
{
    if (saving) setBoolean(key, checkbox->isChecked());
    else {
        ValState vstate = VAL_NOTFOUND;
        const bool val = getBoolean(key, false, &vstate);
        if (vstate == VAL_OK) checkbox->setChecked(val);
    }
}

void MSettings::manageGroupCheckBox(const QString &key, QGroupBox *groupbox) noexcept
{
    if (saving) setBoolean(key, groupbox->isChecked());
    else {
        ValState vstate = VAL_NOTFOUND;
        const bool val = getBoolean(key, false, &vstate);
        if (vstate == VAL_OK) groupbox->setChecked(val);
    }
}

void MSettings::manageLineEdit(const QString &key, QLineEdit *lineedit) noexcept
{
    if (saving) setString(key, lineedit->text());
    else {
        const QString &val = getString(key);
        if (!val.isNull()) lineedit->setText(val);
    }
}

void MSettings::manageSpinBox(const QString &key, QSpinBox *spinbox) noexcept
{
    if (saving) setInteger(key, spinbox->value());
    else {
        ValState vstate = VAL_NOTFOUND;
        const int spinval = getInteger(key, 0, &vstate);
        if (vstate != VAL_NOTFOUND) {
            spinbox->setValue(spinval);
            if (vstate != VAL_OK || spinval != spinbox->value()) {
                markBadWidget(spinbox);
            }
        }
    }
}

int MSettings::manageEnum(const QString &key, const int nchoices,
        const char *const choices[], const int curval) noexcept
{
    const char *choice = (curval >= 0 ? choices[curval] : "");
    if (saving) {
        if (curval >= 0) setString(key, choice);
    } else {
        const QString &val = getString(key, choice);
        for (int ixi = 0; ixi < nchoices; ++ixi) {
            if (!val.compare(QString(choices[ixi]), Qt::CaseInsensitive)) {
                return ixi;
            }
        }
        return -1;
    }
    return curval;
}

void MSettings::manageRadios(const QString &key, const int nchoices,
        const char *const choices[], QRadioButton *const radios[]) noexcept
{
    // obtain the current choice
    int ixradio = -1;
    for (int ixi = 0; ixradio < 0 && ixi < nchoices; ++ixi) {
        if (radios[ixi]->isChecked()) ixradio = ixi;
    }
    // select the corresponding radio button
    ixradio = manageEnum(key, nchoices, choices, ixradio);
    if (ixradio >= 0) radios[ixradio]->setChecked(true);
    else if (contains(key)) {
        for (int ixi = 0; ixi < nchoices; ++ixi) {
            markBadWidget(radios[ixi]);
        }
    }
}
