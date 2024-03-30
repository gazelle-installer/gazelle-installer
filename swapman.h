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

class SwapMan : public QObject
{
    Q_OBJECT
    class MProcess &proc;
    class PartMan &partman;
    Ui::MeInstall &gui;
    bool setupBounds() noexcept;
    // Slots
    void sizeResetClicked() noexcept;
    void spinSizeChanged(int i) noexcept;
    void checkHibernationClicked(bool checked) noexcept;
public:
    SwapMan(class MProcess &mproc, class PartMan &pman, Ui::MeInstall &ui) noexcept;
    void manageConfig(class MSettings &config, bool advanced) noexcept;
    void setupDefaults() noexcept;
    void install(QStringList &cmdboot_out);
    void setupZRam() const;
    static long long recommended(bool hibernation) noexcept;
};

#endif // SWAPMAN_H
