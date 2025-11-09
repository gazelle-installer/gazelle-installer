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

using namespace Qt::Literals::StringLiterals;

MIni::MIni(const QString &filename, OpenMode mode, bool *loadOK) noexcept
    : file(filename)
{
    clear();
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
    groupPath.clear();
    sections.clear();
    // Implicit default unnamed section at the beginning.
    sections.emplace_back(nullptr, u""_s);
    curSectionIndex = 0;
}
bool MIni::load() noexcept
{
    if (!(file.openMode() & QFile::ReadOnly)) {
        return false;
    }
    clear();
    if (!file.seek(0)) {
        return false;
    }
    assert(!sections.empty());
    Group *gpos = &sections[0];
    while (!file.atEnd()) {
        const QByteArray &line = file.readLine().trimmed();
        if (line.isEmpty() || line.startsWith(';') || line.startsWith('#')) {
            // Empty lines or comment lines to be ignored.
            continue;
        } else if (line.startsWith('[')) {
            // Section
            if (line.size() < 3 || !line.endsWith(']') || !groupPath.empty()) {
                return false; // Incomplete bracket set, empty section, or section inside a group.
            }
            sections.emplace_back(nullptr, line.mid(1, line.size()-2));
            ++curSectionIndex;
            gpos = &sections[curSectionIndex];
        } else if (line == "}") {
            if (groupPath.size() <= 1) {
                return false; // Mismatched group ending.
            }
            groupPath.pop_back();
            gpos = gpos->parent;
        } else {
            const int delimpos = line.indexOf('=');
            if (delimpos > 0) {
                // Setting
                gpos->settings.emplace_back(line.left(delimpos).trimmed(), line.mid(delimpos+1).trimmed());
            } else if (delimpos < 0 && line.endsWith('{')) {
                // Group
                assert(!groupPath.empty() && gpos != nullptr);
                const QString name = line.chopped(1).trimmed();
                groupPath.emplace_back(gpos->children.size()); // new index = size-1
                gpos = new Group(gpos, name);
            } else {
                return false; // Line starts with '=' or invalid group starter.
            }
        }
    }
    return groupPath.empty(); // All groups must be closed at this point.
}
bool MIni::save() noexcept
{
    if (!(file.openMode() & QFile::WriteOnly)) {
        return false;
    }
    bool ok = (file.resize(0) && file.seek(0));
    for (const Group &section : sections) {
        auto fileWriteC = [this, &ok](const char *data, qint64 size = -1) {
            if (ok) {
                if (size < 0) {
                    size = strlen(data);
                }
                ok = (file.write(data, size) == size);
            }
        };
        auto fileWriteQ = [this, &ok](const QString &data) {
            if (ok) {
                const QByteArray &bytes = data.toUtf8();
                ok = (file.write(bytes) == bytes.size());
            }
        };

        if (!section.name.isEmpty()
            && !(section.settings.empty() && section.children.empty())) {
            fileWriteC(file.pos()>0 ? "\n[" : "[");
            fileWriteQ(section.name);
            fileWriteC("]\n");
        }
        // Settings
        for (const Setting &setting : section.settings) {
            fileWriteQ(setting.key);
            fileWriteC("=");
            fileWriteQ(setting.value);
            fileWriteC("\n");
        }

        int prevdepth = -1;
        Iterator it(&section);
        for (it.next(); const Group *gpos = *it; it.next()) {
            const int depth = it.level();
            const std::vector<char> tabs(std::max(depth, prevdepth), '\t');
            // Close braces of previous groups.
            for(int i = prevdepth; i >= depth; --i) {
                fileWriteC(tabs.data(), i-1);
                fileWriteC("}\n");
            }
            // Open this group.
            fileWriteC(tabs.data(), depth-1);
            fileWriteQ(gpos->name);
            fileWriteC(" {\n");

            // Settings
            for (const Setting &setting : gpos->settings) {
                fileWriteC(tabs.data(), depth);
                fileWriteQ(setting.key);
                fileWriteC("=");
                fileWriteQ(setting.value);
                fileWriteC("\n");
            }
            prevdepth = depth;
        }

        // Close open groups before moving on to the next section (or the end).
        if (prevdepth > 0) {
            --prevdepth;
            const std::vector<char> tabs(prevdepth, '\t');
            for (int ixi = prevdepth; ixi >= 0; --ixi) {
                fileWriteC(tabs.data(), ixi);
                fileWriteC("}\n");
            }
        }
    }
    return ok;
}

bool MIni::closeAndCopyTo(const QString &filename) noexcept
{
    if (file.openMode() & QFile::WriteOnly) {
        if (!save()) return false;
    }
    QFile::remove(filename);
    return file.copy(filename); // QFile::copy() closes the file before copying.
}

QString MIni::section() const noexcept
{
    assert(curSectionIndex < sections.size());
    return sections[curSectionIndex].name;
}
bool MIni::setSection(const QString &name, bool create) noexcept
{
    groupPath.clear();
    for (size_t ixi = 0; ixi < sections.size(); ++ixi) {
        if (name.compare(sections[ixi].name, Qt::CaseInsensitive) == 0) {
            curSectionIndex = ixi;
            return true;
        }
    }
    // Section not found at this point.
    if (create) {
        sections.emplace_back(nullptr, name);
        curSectionIndex = sections.size() - 1;
    }
    return false;
}

QString MIni::group() const noexcept
{
    assert(curSectionIndex < sections.size());
    const Group *pos = &sections[curSectionIndex];
    QString path;
    for (const int index : groupPath) {
        assert(index < pos->children.size());
        if (!path.isEmpty()) {
            path.append('/');
        }
        path += pos->name;
        pos = pos->children[index];
    }
    return path;
}
bool MIni::setGroup(const QString &path, bool create) noexcept
{
    bool found = false;
    groupPath.clear();
    assert(curSectionIndex < sections.size());
    Group *pgroup = &sections[curSectionIndex];
    const QStringList &segments = path.split('/', Qt::SkipEmptyParts);
    for (const QString &name : segments) {
        found = false;
        for (size_t ixi = 0; ixi < pgroup->children.size(); ++ixi) {
            if (name.compare(pgroup->name, Qt::CaseInsensitive) == 0) {
                groupPath.emplace_back(ixi);
                pgroup = pgroup->children[ixi];
                found = true;
                break;
            }
        }
        // Group not found at this point.
        if (!found && create) {
            new Group(pgroup, name);
            groupPath.emplace_back(pgroup->children.size() - 1);
        }
    }
    return found;
}
bool MIni::beginGroup(const QString &name, bool create) noexcept
{
    Group *pgroup = currentGroup();
    for (size_t ixi = 0; ixi < pgroup->children.size(); ++ixi) {
        const Group *g = pgroup->children[ixi];
        assert (g != nullptr);
        if (name.compare(g->name, Qt::CaseInsensitive) == 0) {
            groupPath.emplace_back(ixi);
            return true;
        }
    }
    // Group not found at this point.
    if (create) {
        new Group(pgroup, name);
        groupPath.push_back(pgroup->children.size() - 1);
    }
    return false;
}
void MIni::endGroup() noexcept
{
    groupPath.pop_back();
}

QStringList MIni::getSections() const noexcept
{
    QStringList list;
    for (const Group &section : sections) {
        list.append(section.name);
    }
    return list;
}
QStringList MIni::getGroups() const noexcept
{
    const Group *gp = currentGroup();
    assert(gp != nullptr);
    QStringList list;
    for (const Group *g : gp->children) {
        list.append(g->name);
    }
    return list;
}
QStringList MIni::getKeys() const noexcept
{
    const Group *g = currentGroup();
    assert(g != nullptr);
    QStringList list;
    for (const Setting &setting : g->settings) {
        list.append(setting.key);
    }
    return list;
}

bool MIni::contains(const QString &key) const noexcept
{
    const Group *g = currentGroup();
    assert(g != nullptr);
    for (const Setting &setting : g->settings) {
        if (key.compare(setting.key, Qt::CaseInsensitive) == 0) {
            return true;
        }
    }
    return false;
}
QString MIni::getRaw(const QString &key, const QString &defaultValue) const noexcept
{
    const Group *g = currentGroup();
    assert(g != nullptr);
    for (const Setting &setting : g->settings) {
        if (key.compare(setting.key, Qt::CaseInsensitive) == 0) {
            return setting.value;
        }
    }
    return defaultValue;
}
void MIni::setRaw(const QString &key, const QString &value) noexcept
{
    Group *g = currentGroup();
    assert(g != nullptr);
    for (Setting &setting : g->settings) {
        if (key.compare(setting.key, Qt::CaseInsensitive) == 0) {
            setting.value = value;
            return;
        }
    }
    // Setting not found at this point.
    g->settings.emplace_back(key, value);
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
        if (!val.compare("true"_L1, Qt::CaseInsensitive)) {
            defaultValue = true;
        } else if (!val.compare("false"_L1, Qt::CaseInsensitive)) {
            defaultValue = false;
        } else {
            defaultValue = (val.toInt(&ok) != 0);
        }
        if (valid) *valid = ok ? VAL_OK : VAL_INVALID;
    }
    return defaultValue;
}
void MIni::setBoolean(const QString &key, const bool value) noexcept
{
    setRaw(key, value ? u"true"_s : u"false"_s);
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

double MIni::getFloat(const QString &key, double defaultValue, enum ValState *valid) const noexcept
{
    const QString &val = getString(key);
    if (valid) *valid = VAL_NOTFOUND;
    if (!val.isNull()) {
        bool ok = false;
        defaultValue = val.toDouble(&ok);
        if (valid) *valid = ok ? VAL_OK : VAL_INVALID;
    }
    return defaultValue;
}
void MIni::setFloat(const QString &key, const double value, const char format, const int precision) noexcept
{
    setRaw(key, QString::number(value, format, precision));
}

/* Case-insensitive key comparison for maps. */
bool MIni::lessCaseInsensitive(const QString &a, const QString &b) noexcept
{
    return (a.compare(b, Qt::CaseInsensitive) < 0);
}

/* Const and non-const versions, not ideal but that's C++ for you. */
const MIni::Group *MIni::currentGroup() const noexcept
{
    assert(curSectionIndex < sections.size());
    const Group *pos = &sections[curSectionIndex];
    for (const int index : groupPath) {
        assert(index < pos->children.size());
        pos = pos->children[index];
    }
    return pos;
}
MIni::Group *MIni::currentGroup() noexcept
{
    assert(curSectionIndex < sections.size());
    Group *pos = &sections[curSectionIndex];
    for (const int index : groupPath) {
        assert(index < pos->children.size());
        pos = pos->children[index];
    }
    return pos;
}

/* Group management */
MIni::Group::Group(Group *parent, const QString &name) noexcept
    : name(name), parent(parent)
{
    if (parent) {
        parent->children.emplace_back(this);
    }
}
MIni::Group::Group(Group&& other) noexcept
    : name(other.name), parent(other.parent)
{
    settings.swap(other.settings);
    children.swap(other.children);
    // Update the parent pointer of all children.
    for (Group *child : children) {
        child->parent = this;
    }
}
MIni::Group::~Group() noexcept
{
    for (Group *g : children) {
        if (g) delete g;
    }
}
QString MIni::Group::path() const noexcept
{
    QString result(name);
    for (const Group *gp = parent; gp != nullptr; gp = gp->parent) {
        result.prepend(gp->name + '/');
    }
    return result;
}

/* A very slimmed down and non-standard one-way tree iterator. */
MIni::Iterator::Iterator(const Group *group) noexcept
    : start(group), pos(group)
{
}
void MIni::Iterator::next() noexcept
{
    if (!pos) return;
    if (!pos->children.empty()) {
        ixParents.push_back(ixPos);
        ixPos = 0;
        pos = pos->children[0];
    } else {
        Group *parent = pos->parent;
        if (!parent) {
            pos = nullptr;
            return;
        }
        Group *chnext = nullptr;
        if ((ixPos + 1) < parent->children.size()) {
            chnext = parent->children[ixPos+1];
        }
        while (!chnext && parent && parent != start) {
            parent = parent->parent;
            if (!parent) break;
            ixPos = ixParents.back();
            ixParents.pop_back();
            if ((ixPos + 1) < parent->children.size()) {
                chnext = parent->children[ixPos+1];
            } else {
                chnext = nullptr;
                break;
            }
        }
        if (chnext) ++ixPos;
        pos = chnext;
    }
    // If back to the start then stop the search.
    if (pos == start) {
        pos = nullptr;
    }
}
int MIni::Iterator::level() noexcept
{
    return ixParents.size();
}

/* Print to console and debug log. */
void MSettings::addFilter(const QString &key) noexcept
{
    filter.append(sections[curSectionIndex].name + '/' + group() + '/' + key);
}
void MSettings::dumpDebug() const noexcept
{
    qDebug().noquote() << "Configuration:" << fileName();
    for (const Group &section : sections) {
        QString bullet(" - "_L1);
        if (!section.name.isEmpty()) {
            qDebug().noquote().nospace() << " = " << section.name << ':';
            bullet = "   - "_L1;
        }
        for (const Setting &setting : section.settings) {
            const bool show = (setting.key.isEmpty()
                || !filter.contains(section.name + '/' + setting.key));
            qDebug().noquote().nospace() << bullet << setting.key
                << ": " << (show ? setting.value : u"<filter>"_s);
        }

        MIni::Iterator it(&section);
        for (it.next(); const Group *gpos = *it; it.next()) {
            QString indent;
            indent.fill(' ', it.level() * 2);
            qDebug().noquote().nospace() << indent << " + " << gpos->name;

            indent.append("  "_L1);
            for (const Setting &setting : gpos->settings) {
                const bool show = (setting.key.isEmpty()
                    || !filter.contains(gpos->path() + '/' + setting.key));
                qDebug().noquote().nospace() << indent << " - " << setting.key
                    << ": " << (show ? setting.value : u"<filter>"_s);
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
        widget->setStyleSheet(u"QWidget { background: maroon; border: 2px inset red; }\n"
          "QPushButton:!pressed { border-style: outset; }\n"
          "QRadioButton { border-style: dotted; }"_s);
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
