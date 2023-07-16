/***************************************************************************
 * Boot manager (GRUB) setup for the installer.
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
#ifndef BOOTMAN_H
#define BOOTMAN_H

#include <QObject>
#include <QCommandLineParser>
#include "ui_meinstall.h"

class BootMan : public QObject
{
    Q_OBJECT
    class MProcess &proc;
    Ui::MeInstall &gui;
    QWidget *master;
    class PartMan &partman;
    QString loaderID, loaderLabel;
    bool installFromRootDevice, removeNoSplash;
    bool brave;
    void selectBootMain() noexcept;
    // Slots
    void chosenBootMBR() noexcept;
    void chosenBootPBR() noexcept;
    void chosenBootESP(bool checked) noexcept;
public:
    BootMan(class MProcess &mproc, class PartMan &pman, Ui::MeInstall &ui,
        const class QSettings &appConf, const QCommandLineParser &appArgs) noexcept;
    void manageConfig(class MSettings &config) noexcept;
    void buildBootLists() noexcept;
    void install(const QStringList &cmdextra = {});
};

#endif // BOOTMAN_H
