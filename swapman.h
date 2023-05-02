/***************************************************************************
 * Swap space management and setup for the installer.
 *
 *   Copyright (C) 2023 by AK-47.
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
#ifndef SWAPMAN_H
#define SWAPMAN_H

#include <QObject>
#include "ui_meinstall.h"
#include "mprocess.h"
#include "partman.h"

class SwapMan : public QObject
{
    Q_OBJECT
    MProcess &proc;
    Ui::MeInstall &gui;
    PartMan &partman;
    // Slots
    void swapFileEdited(const QString &text);
    void sizeResetClicked();
    void spinSizeChanged(int i);
    void checkHibernationClicked(bool checked);
public:
    SwapMan(MProcess &mproc, PartMan &pman, Ui::MeInstall &ui);
    void manageConfig(MSettings &config, bool advanced);
    void setupDefaults();
    void install();
    static long long recommended(bool hibernation);
};

#endif // SWAPMAN_H
