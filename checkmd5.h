/***************************************************************************\
 * Media integrity check. This should be done at boot time instead of here.
 *
 *   Copyright (C) 2023 by AK-47
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
\***************************************************************************/
#ifndef CHECKMD5_H
#define CHECKMD5_H

#include <QCoreApplication>
#include <QLabel>
#include "mprocess.h"

class CheckMD5
{
    Q_DECLARE_TR_FUNCTIONS(CheckMD5)
    MProcess &proc;
    QLabel *labelSplash;
    bool checking = false;
public:
    CheckMD5(MProcess &mproc, QLabel *splash) noexcept;
    void check(); // Must be separate from constructor for halt() to work.
    void halt(bool silent = false) noexcept;
};

#endif // CHECKMD5_H
