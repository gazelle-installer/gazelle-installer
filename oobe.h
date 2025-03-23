/***************************************************************************
 * Out-of-Box Experience - GUI and related functions of the installer.
 *
 *   Copyright (C) 2022-2023 by AK-47, along with transplanted code:
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
#include "passedit.h"

class Oobe : public QObject
{
    Q_OBJECT
public:
    bool haveSnapshotUserAccounts = false;
    Oobe(class MProcess &mproc, Ui::MeInstall &ui, QWidget *parent,
         class MIni &appConf, bool oem, bool modeOOBE);
    void manageConfig(class MSettings &config) const noexcept;
    void enable() const;
    void process() const;
    void stashServices(bool save) const noexcept;
    void setService(const QString &service, bool enabled) const;
    QWidget *validateComputerName() const noexcept;
    QWidget *validateUserInfo(bool automatic) const noexcept;
    // Slots
    void userPassValidationChanged() const noexcept;
    void oldHomeToggled() const noexcept;

private:
    class MProcess &proc;
    Ui::MeInstall &gui;
    QWidget *master;
    bool containsSystemD = false;
    bool containsRunit = false;
    bool haveSamba = false;
    bool oem = false;
    bool online = false;
    QStringList timeZones; // cached time zone list
    PassEdit passUser, passRoot;
    QString curUser, curHome;

    void buildServiceList(class MIni &appconf) noexcept;
    int selectTimeZone(const QString &zone) const noexcept;
    void resetBlueman() const;
    void setComputerName() const;
    void setLocale() const;
    void setUserClockFormat(const QString &skelpath) const noexcept;
    void setUserInfo() const;
    bool replaceStringInFile(const QString &oldtext, const QString &newtext, const QString &filepath) const noexcept;
    bool containsAnySubstring(const QString& mainString, const QStringList& substrings) const noexcept;
    // Slots
    void localeIndexChanged(int index) const noexcept;
    void timeAreaIndexChanged(int index) const noexcept;
};

#endif // OOBE_H
