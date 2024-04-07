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

#include <QGroupBox>
#include <QComboBox>
#include <QCheckBox>
#include <QRadioButton>
#include <QLineEdit>
#include <QSpinBox>
#include <QWidget>
#include <QDebug>

#include "msettings.h"

MSettings::MSettings(const QString &fileName, QObject *parent) noexcept
    : QSettings(fileName, QSettings::IniFormat, parent)
{
}

void MSettings::dumpDebug(const QRegularExpression *censor) noexcept
{
    qDebug().noquote() << "Configuration:" << fileName();
    // top-level settings (version, etc)
    QStringList chkeys = childKeys();
    for (QString &strkey : chkeys) {
        qDebug().noquote().nospace() << " - " << strkey
                                     << ": " << value(strkey).toString();
    }
    // iterate through groups
    for (const QString &strgroup : childGroups()) {
        beginGroup(strgroup);
        qDebug().noquote().nospace() << " = " << strgroup << ":";
        QStringList chkeys = childKeys();
        for (QString &strkey : chkeys) {
            QString val(value(strkey).toString());
            if (censor && !val.isEmpty()) {
                if (QString(strgroup+'/'+strkey).contains(*censor)) val = "???";
            }
            qDebug().noquote().nospace() << "   - " << strkey << ": " << val;
        }
        endGroup();
    }
    qDebug() << "End of configuration.";
}

void MSettings::setSave(bool save) noexcept
{
    saving = save;
}

void MSettings::startGroup(const QString &prefix, QWidget *wgroup) noexcept
{
    beginGroup(prefix);
    group = wgroup;
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
        const char *const choices[], const int curval) noexcept
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
