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

#include <QSettings>

class MSettings : public QSettings
{
    bool saving = false;
    class QWidget *group = nullptr;
public:
    MSettings(const QString &fileName, QObject *parent = Q_NULLPTR);
    bool bad = false;
    void dumpDebug();
    void setSave(bool save);
    bool isSave() { return saving; }
    void startGroup(const QString &prefix, class QWidget *wgroup);
    void setGroupWidget(class QWidget *wgroup);
    // widget management
    void markBadWidget(class QWidget *widget);
    static bool isBadWidget(class QWidget *widget);
    void manageComboBox(const QString &key, class QComboBox *combo, const bool useData);
    void manageCheckBox(const QString &key, class QCheckBox *checkbox);
    void manageGroupCheckBox(const QString &key, class QGroupBox *groupbox);
    void manageLineEdit(const QString &key, class QLineEdit *lineedit);
    void manageSpinBox(const QString &key, class QSpinBox *spinbox);
    int manageEnum(const QString &key, const int nchoices,
            const char *choices[], const int curval);
    void manageRadios(const QString &key, const int nchoices,
            const char *choices[], class QRadioButton *radios[]);
};

#endif // MSETTINGS_H
