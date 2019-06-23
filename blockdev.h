//
//   Copyright (C) 2019 by AK-47
//   Licensed under the Apache License, Version 2.0 (the "License");
//   you may not use this file except in compliance with the License.
//   You may obtain a copy of the License at
//
//       http://www.apache.org/licenses/LICENSE-2.0
//
//   Unless required by applicable law or agreed to in writing, software
//   distributed under the License is distributed on an "AS IS" BASIS,
//   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//   See the License for the specific language governing permissions and
//   limitations under the License.
//

#ifndef BLOCKDEV_H
#define BLOCKDEV_H

#include <QComboBox>

struct BlockDeviceInfo
{
    QString name;
    QString fs;
    QString label;
    QString model;
    qint64 size;
    bool isFuture = false;
    bool isNasty = false;
    bool isDisk = false;
    bool isGPT = false;
    bool isBoot = false;
    bool isESP = false;
    bool isNative = false;
    bool isSwap = false;
    void addToCombo(QComboBox *combo, bool warnNasty = false) const;
    static QStringList split(const QString &devname);
};

class BlockDeviceList : public QList<BlockDeviceInfo>
{
public:
    int findDevice(const QString &devname) const;
};

#endif // BLOCKDEV_H
