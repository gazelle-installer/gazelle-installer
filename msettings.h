// MSettings class - Installer-specific extensions to QSettings.
//
//   Copyright (C) 2019 by AK-47
//   Licensed under the Apache License, Version 2.0 (the "License");
//   you may not use this file except in compliance with the License.
//   You may obtain a copy of the License at
//
//       http://www.apache.org/licenses/LICENSE-2.0
//
//   Unless required by applicable law or agreed to in writing, software
//   distributed under the License is distributed on an "AS IS" BASIS,
//   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//   See the License for the specific language governing permissions and
//   limitations under the License.
//
// This file is part of the gazelle-installer.

#ifndef MSETTINGS_H
#define MSETTINGS_H

#include <QComboBox>
#include <QCheckBox>
#include <QRadioButton>
#include <QLineEdit>
#include <QSpinBox>
#include <QSettings>
#include <QWidget>

class MSettings : public QSettings
{
    bool saving = false;
    QWidget *group = nullptr;
public:
    MSettings(const QString &fileName, QObject *parent = Q_NULLPTR);
    bool bad = false;
    void dumpDebug();
    void setSave(bool save);
    void startGroup(const QString &prefix, QWidget *wgroup);
    void setGroupWidget(QWidget *wgroup);
    // widget management
    void markBadWidget(QWidget *widget);
    static bool isBadWidget(QWidget *widget);
    void manageComboBox(const QString &key, QComboBox *combo, const bool useData);
    void manageCheckBox(const QString &key, QCheckBox *checkbox);
    void manageLineEdit(const QString &key, QLineEdit *lineedit);
    void manageSpinBox(const QString &key, QSpinBox *spinbox);
    int manageEnum(const QString &key, const int nchoices,
            const char *choices[], const int curval);
    void manageRadios(const QString &key, const int nchoices,
            const char *choices[], QRadioButton *radios[]);
};

#endif // MSETTINGS_H
