/***************************************************************************
 * Base operating system software installation and configuration.
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
 ****************************************************************************/

#ifndef BASE_H
#define BASE_H

#include <QCommandLineParser>
#include "ui_meinstall.h"
#include "mprocess.h"
#include "msettings.h"
#include "partman.h"

class Base
{
    Q_DECLARE_TR_FUNCTIONS(Base)
    MProcess &proc;
    Ui::MeInstall &gui;
    QWidget *master;
    PartMan &partman;
    long long sourceInodes = 1;
    long long bufferRoot = 0, bufferHome = 0;
    bool pretend = false;
    bool nocopy = false, sync = false;
    bool populateMediaMounts = false;
    bool mediacheck = false, nomediacheck = false;
    bool checking = false;
    void checkMediaMD5(const QString &path, const QString &sqfs);
    void copyLinux();
public:
    // source medium
    QString bootSource;
    QStringList rootSources;

    Base(MProcess &mproc, PartMan &pman, Ui::MeInstall &ui,
        const QSettings &appConf, const QCommandLineParser &appArgs);
    void scanMedia();
    void haltCheck(bool silent);
    void install();
};

#endif // BASE_H
