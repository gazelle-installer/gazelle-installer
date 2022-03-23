/***************************************************************************
 * Out-of-Box Experience - GUI and related functions of the installer.
 ***************************************************************************
 *
 *   Copyright (C) 2022 by AK-47, along with transplanted code:
 *    - Copyright (C) 2003-2010 by Warren Woodford
 *    - Heavily edited, with permision, by anticapitalista for antiX 2011-2014.
 *    - Heavily revised by dolphin oracle, adrian, and anticaptialista 2018.
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
#ifndef OOBE_H
#define OOBE_H

#include <QObject>
#include <QStringList>
#include "ui_meinstall.h"
#include "mprocess.h"
#include "msettings.h"

class Oobe : public QObject
{
    Q_OBJECT
    MProcess &proc;
    Ui::MeInstall &gui;
    QWidget *master;
    bool containsSystemD = false;
    bool containsRunit = false;
    bool haveSamba = false;
    QStringList timeZones; // cached time zone list
    QStringList enableServices;
    void buildServiceList();
    int selectTimeZone(const QString &zone);
    void resetBlueman();
    // Slots
    void localeIndexChanged(int index);
    void timeAreaIndexChanged(int index);

public:
    bool online = false;
    bool haveSnapshotUserAccounts = false;
    Oobe(MProcess &mproc, Ui::MeInstall &ui, QWidget *parent, const QSettings &appConf);
    void manageConfig(MSettings &config, bool save);
    void enable();
    void process();
    void stashServices(bool save);
    void setService(const QString &service, bool enabled);
    QWidget *validateComputerName();
    QWidget *validateUserInfo(bool automatic);
    void setComputerName();
    void setLocale();
    void setUserInfo();
    bool replaceStringInFile(const QString &oldtext, const QString &newtext, const QString &filepath);
    // Slots
    void userPassValidationChanged();
    void oldHomeToggled();
};

#endif // OOBE_H
