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

#include <QFile>
#include <QGroupBox>
#include <QComboBox>
#include <QCheckBox>
#include <QRadioButton>
#include <QLineEdit>
#include <QSpinBox>
#include <QWidget>
#include <QDebug>

#include "msettings.h"

MSettings::MSettings(const QString &filename) noexcept
    : confile(filename)
{
    clear();
    if (!confile.isEmpty()) {
        load();
    }
}

void MSettings::clear() noexcept
{
    curgroup.clear();
    cursection.clear();
    sections.clear();
}
bool MSettings::load() noexcept
{
    QFile file(confile);
    if (!file.open(QFile::ReadOnly | QFile::Text)) {
        return false;
    }
    clear();
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
                setString(line.left(delimpos).trimmed(), line.mid(delimpos+1).trimmed());
            } else if (delimpos < 0 && line.endsWith('{')) {
                // Group
                beginGroup(line.chopped(1).trimmed());
            } else {
                return false; // Line starts with '=' or not invalid group starter.
            }
        }
    }
    return true;
}
bool MSettings::save() const noexcept
{
    QFile file(confile);
    if (!file.open(QFile::ReadWrite | QFile::Truncate | QFile::Text)) {
        return false;
    }

    for (const auto &section : sections) {
        if (!section.first.isEmpty()) {
            file.write(file.pos()>0 ? "\n[" : "[");
            file.write(section.first.toUtf8());
            file.write("]\n");
        }

        int prevdepth = 0;
        QString prevgpath;
        for (const auto &group : section.second) {
            const int depth = group.first.count('/');
            const QString &gpath = group.first;
            // The first segment of the current and previous group that differs.
            int pivot = 0;
            for (int ixi = 0; ixi < depth; ++ixi) {
                if(prevgpath.section('/', ixi, ixi) != gpath.section('/', ixi, ixi)) {
                    pivot = ixi;
                    break;
                }
            }
            const std::vector<char> tabs(std::max(depth, prevdepth), '\t');
            // Close braces of previous groups.
            for (int ixi = prevdepth; ixi > pivot; --ixi) {
                file.write(tabs.data(), ixi-1);
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
            prevdepth = depth;
        }

        // Close open groups before moving on to the next section (or the end).
        if (prevdepth > 0) {
            --prevdepth;
            const std::vector<char> tabs(prevdepth, '\t');
            for (int ixi = prevdepth; ixi >= 0; --ixi) {
                file.write(tabs.data(), ixi);
                file.write("}\n");
            }
        }
    }

    return true;
}

void MSettings::setSection(const QString &name) noexcept
{
    cursection = name;
    curgroup.clear();
}

void MSettings::beginGroup(const QString &path) noexcept
{
    curgroup += path + '/';
}
void MSettings::endGroup() noexcept
{
    curgroup.chop(1);
    curgroup.truncate(curgroup.lastIndexOf('/') + 1);
}

bool MSettings::contains(const QString &key) const noexcept
{
    if (auto ssearch = sections.find(cursection); ssearch != sections.end()) {
        if (auto gsearch = ssearch->second.find(curgroup); gsearch != ssearch->second.end()) {
            return (gsearch->second.count(key) > 0);
        }
    }
    return false;
}
QString MSettings::getString(const QString &key, const QString &defaultValue) const noexcept
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
void MSettings::setString(const QString &key, const QString &value) noexcept
{
    sections[cursection][curgroup].insert_or_assign(key, value);
}

bool MSettings::getBoolean(const QString &key, bool defaultValue, enum ValState *valid) const noexcept
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
void MSettings::setBoolean(const QString &key, const bool value) noexcept
{
    setString(key, value ? "true" : "false");
}

long long MSettings::getInteger(const QString &key, long long defaultValue, enum ValState *valid, int base) const noexcept
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
void MSettings::setInteger(const QString &key, const long long value) noexcept
{
    setString(key, QString::number(value));
}

void MSettings::dumpDebug(const QRegularExpression *censor) const noexcept
{
    qDebug().noquote() << "Configuration:" << confile;
    for (const auto &section : sections) {
        const bool topsect = section.first.isEmpty(); // top-level settings (version, etc)
        if (!topsect) {
            qDebug().noquote().nospace() << " = " << section.first << ":";
        }
        for (const auto &group : section.second) {
            for (const auto &setting : group.second) {
                const QString &fullkey = group.first + setting.first;
                QString val(setting.second);
                if (censor && !val.isEmpty()) {
                    if (QString(fullkey).contains(*censor)) {
                        val = "???";
                    }
                }
                qDebug().noquote().nospace() << (topsect ? " - " : "   - ") << fullkey << ": " << val;
            }
        }
    }
    qDebug() << "End of configuration.";
}

/* Widget management */

void MSettings::setSave(bool save) noexcept
{
    saving = save;
}

void MSettings::setGroupWidget(QWidget *widget) noexcept
{
    wgroup = widget;
}
void MSettings::setSection(const QString &name, QWidget *widget) noexcept
{
    setSection(name);
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
    bad = true;
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
        const char *choices[], const int curval) noexcept
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
        const char *choices[], QRadioButton *radios[]) noexcept
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
