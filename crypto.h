/***************************************************************************\
 * Drive encryption.
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
\***************************************************************************/
#ifndef CRYPTO_H
#define CRYPTO_H

#include <QObject>
#include "ui_meinstall.h"
#include "passedit.h"
#include "partman.h"

class Crypto : public QObject
{
    Q_OBJECT
    class MProcess &proc;
    class Ui::MeInstall &gui;
    PassEdit pass;
    bool cryptsupport;
public:
    Crypto(class MProcess &mproc, Ui::MeInstall &ui, QObject *parent = nullptr);
    bool manageConfig(class MSettings &config) noexcept;
    bool valid() const noexcept;
    inline bool supported() const noexcept { return cryptsupport; }
    void formatAll(const class PartMan &partman);
    void open(class PartMan::Device *part, const class QByteArray &password);
    bool makeCrypttab(const class PartMan &partman) noexcept;
};

#endif // CRYPTO_H
