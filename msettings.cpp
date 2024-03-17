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

#include <QIODevice>
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

MSettings::MSettings() noexcept
{
    clear();
}

void MSettings::clear() noexcept
{
    curprefix.clear();
    sectname.clear();
    sections.clear();
    psection = &sections[""];
}
bool MSettings::load(const QString &filename) noexcept
{
    QFile file(filename);
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
            if (line.size() < 3 || !line.endsWith(']') || !curprefix.isEmpty()) {
                return false; // Incomplete bracket set, empty section, or section inside a group.
            }
            sectname = line.mid(1, line.size()-2);
            psection = &sections[sectname];
        } else if (line == "}") {
            if (curprefix.isEmpty()) {
                return false; // Mismatched group ending.
            }
            endGroup();
        } else {
            const int delimpos = line.indexOf('=');
            if (delimpos > 0) {
                setValue(line.left(delimpos).trimmed(), line.mid(delimpos+1).trimmed());
            } else if (delimpos < 0 && line.endsWith('{')) {
                // Group (prefix)
                beginGroup(line.chopped(1).trimmed());
            } else {
                return false; // Line starts with '=' or not invalid group starter.
            }
        }
    }
    return true;
}
bool MSettings::save(const QString &filename) noexcept
{
    QFile file(filename);
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
        QString prevprefix;
        for (const auto &group : section.second) {
            const int depth = group.first.count('/');
            const QString &prefix = group.first;
            // The first segment of the current and previous group that differs.
            int pivot = 0;
            for (int ixi = 0; ixi < depth; ++ixi) {
                if(prevprefix.section('/', ixi, ixi) != prefix.section('/', ixi, ixi)) {
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
                file.write(prefix.section('/', ixi, ixi).toUtf8());
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
            prevprefix = prefix;
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

void MSettings::dumpDebug(const QRegularExpression *censor) noexcept
{
    qDebug().noquote() << "Configuration";
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

bool MSettings::contains(const QString &key) noexcept
{
    assert(psection != nullptr);
    const QString &fullkey = curprefix + key;
    const int pivot = fullkey.lastIndexOf('/') + 1;
    if (auto gsearch = psection->find(fullkey.left(pivot)); gsearch != psection->end()) {
        return gsearch->second.count(fullkey.mid(pivot));
    }
    return false;
}
QVariant MSettings::value(const QString &key, const QVariant &defaultValue) const noexcept
{
    assert(psection != nullptr);
    const QString &fullkey = curprefix + key;
    const int pivot = fullkey.lastIndexOf('/') + 1;
    if (auto gsearch = psection->find(fullkey.left(pivot)); gsearch != psection->end()) {
        if (auto ssearch = gsearch->second.find(fullkey.mid(pivot)); ssearch != gsearch->second.end()) {
            return QVariant(ssearch->second);
        }
    }
    return defaultValue;
}
void MSettings::setValue(const QString &key, const QVariant &value) noexcept
{
    assert(psection != nullptr);
    const QString &fullkey = curprefix + key;
    const int pivot = fullkey.lastIndexOf('/') + 1;
    (*psection)[fullkey.left(pivot)][fullkey.mid(pivot)] = value.toString();
}

void MSettings::setSave(bool save) noexcept
{
    saving = save;
}

void MSettings::setSection(const QString &name, QWidget *wgroup) noexcept
{
    psection = &sections[name];
    sectname = name;
    group = wgroup;
}

void MSettings::startGroup(const QString &prefix, QWidget *wgroup) noexcept
{
    beginGroup(prefix);
    group = wgroup;
}
void MSettings::beginGroup(const QString &prefix) noexcept
{
    curprefix += prefix + '/';
}
void MSettings::endGroup() noexcept
{
    curprefix.chop(1);
    curprefix.truncate(curprefix.lastIndexOf('/') + 1);
}

void MSettings::setGroupWidget(QWidget *wgroup) noexcept
{
    group = wgroup;
}

void MSettings::markBadWidget(QWidget *widget) noexcept
{
    if (widget) {
        widget->setStyleSheet("QWidget { background: maroon; border: 2px inset red; }\n"
                              "QPushButton:!pressed { border-style: outset; }\n"
                              "QRadioButton { border-style: dotted; }");
    }
    if (group) group->setProperty("BAD", true);
    bad = true;
}
bool MSettings::isBadWidget(QWidget *widget) noexcept
{
    return widget->property("BAD").toBool();
}

void MSettings::manageComboBox(const QString &key, QComboBox *combo, const bool useData) noexcept
{
    const QVariant &comboval = useData ? combo->currentData() : QVariant(combo->currentText());
    if (saving) setValue(key, comboval);
    else if (contains(key)) {
        const QVariant &val = value(key, comboval);
        const int icombo = useData ? combo->findData(val, Qt::UserRole, Qt::MatchFixedString)
                         : combo->findText(val.toString(), Qt::MatchFixedString);
        if (icombo >= 0) combo->setCurrentIndex(icombo);
        else markBadWidget(combo);
    }
}

void MSettings::manageCheckBox(const QString &key, QCheckBox *checkbox) noexcept
{
    const QVariant state(checkbox->isChecked());
    if (saving) setValue(key, state);
    else if (contains(key)) checkbox->setChecked(value(key, state).toBool());
}

void MSettings::manageGroupCheckBox(const QString &key, QGroupBox *groupbox) noexcept
{
    const QVariant state(groupbox->isChecked());
    if (saving) setValue(key, state);
    else if (contains(key)) groupbox->setChecked(value(key, state).toBool());
}

void MSettings::manageLineEdit(const QString &key, QLineEdit *lineedit) noexcept
{
    const QString &text = lineedit->text();
    if (saving) setValue(key, text);
    else if (contains(key)) lineedit->setText(value(key, text).toString());
}

void MSettings::manageSpinBox(const QString &key, QSpinBox *spinbox) noexcept
{
    const QVariant spinval(spinbox->value());
    if (saving) setValue(key, spinval);
    else if (contains(key)) {
        const int val = value(key, spinval).toInt();
        spinbox->setValue(val);
        if (val != spinbox->value()) markBadWidget(spinbox);
    }
}

int MSettings::manageEnum(const QString &key, const int nchoices,
        const char *choices[], const int curval) noexcept
{
    QVariant choice(curval >= 0 ? choices[curval] : "");
    if (saving) {
        if (curval >= 0) setValue(key, choice);
    } else {
        const QString &val = value(key, choice).toString();
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
