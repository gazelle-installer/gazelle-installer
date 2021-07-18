/****************************************************************************
 * Block device enumeration for the installer.
 ****************************************************************************
 *   Copyright (C) 2019 by AK-47
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

#ifndef BLOCKDEV_H
#define BLOCKDEV_H

#include <QComboBox>
#include "mprocess.h"

struct BlockDeviceInfo
{
    QString name;
    QString fs;
    QString label;
    QString model;
    long long size;
    int physec;
    bool isFuture = false;
    bool isNasty = false;
    bool isDrive = false;
    bool isGPT = false;
    bool isBoot = false;
    bool isESP = false;
    bool isNative = false;
    int mapCount = 0;
    void addToCombo(QComboBox *combo, bool warnNasty = false) const;
    static QStringList split(const QString &devname);
    static QString join(const QString &drive, int partnum);
};

class BlockDeviceList : public QList<BlockDeviceInfo>
{
public:
    void build(MProcess &proc);
    int findDevice(const QString &devname) const;
};

#endif // BLOCKDEV_H
