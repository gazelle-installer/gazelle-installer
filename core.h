/***************************************************************************
 * Core class - Common module for various important utilities.
 *
 *   Copyright (C) 2025 by AK-47
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
#ifndef CORE_H
#define CORE_H

#include <QObject>

class Core
{
public:
    Core(class MProcess &mproc);
    // Common functions that are traditionally carried out by processes.
    void sleep(const int msec, const bool silent = false) noexcept;
    bool mkpath(const QString &path, mode_t mode = 0, bool force = false) const;
    // Operating system
    const QString &detectArch();
    int detectEFI(bool noTest = false);
    bool detectVirtualBox();
    // Services
    void setService(const QString &service, bool enabled) const;

private:
    class MProcess &proc;
    int sleepcount = 0;
    // System detection results
    QString testArch;
    int testEFI = -1;
    int testVirtualBox = -1;
    // Init systems
    bool containsSysVinit = false;
    bool containsSystemd = false;
    bool containsRunit = false;
};

#endif // CORE_H
