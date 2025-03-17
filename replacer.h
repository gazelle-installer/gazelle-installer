/***************************************************************************
 * Operating system replacement (or reinstall or upgrade)
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
 ****************************************************************************/
#ifndef REPLACER_H
#define REPLACER_H

#include <vector>
#include <QString>
#include "ui_meinstall.h"
#include "partman.h"

class Replacer : public QObject
{
    Q_OBJECT
public:
    class RootBase;
    Replacer(class MProcess &mproc, class PartMan *pman, class Ui::MeInstall &ui, class MIni &appConf);
    void scan(bool full = false) noexcept;
    bool validate(bool automatic) const noexcept;
    bool preparePartMan() const noexcept;
private:
    std::vector<RootBase> bases;
    class MProcess &proc;
    class PartMan *partman;
    class Ui::MeInstall &gui;
    bool installFromRootDevice = false;
};

class Replacer::RootBase
{
    Q_DECLARE_TR_FUNCTIONS(Device)
public:
    struct MountEntry {
        QString fsname; /* name of mounted filesystem */
        QString dir;    /* filesystem path prefix */
        QString type;   /* mount type (see mntent.h) */
        QString opts;   /* mount options (see mntent.h) */
        int freq;       /* dump frequency in days */
        int passno;     /* pass number on parallel fsck */
        MountEntry(struct mntent *mntent);
    };
    struct CryptEntry {
        QString volume;     /* volume name */
        QString encdev;     /* encrypted device */
        QString keyfile;    /* key file */
        QString options;    /* options */
        CryptEntry(const QByteArray &line);
    };
    std::vector<struct MountEntry> mounts;
    std::vector<struct CryptEntry> crypts;
    QString devpath;
    QString release;
    bool homeSeparate = false;
    bool ok = false;
    RootBase(MProcess &proc, PartMan::Device *device) noexcept;
};

#endif // REPLACER_H
