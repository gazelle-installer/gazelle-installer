/***************************************************************************
 * Automatic partition layout builder for the installer.
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
 ***************************************************************************/

#ifndef AUTOPART_H
#define AUTOPART_H

#include <QObject>
#include <ui_meinstall.h>

class AutoPart : public QObject
{
    Q_OBJECT
    class MProcess &proc;
    class Ui::MeInstall &gui;
    class PartMan *partman = nullptr;
    class DeviceItem *drvitem = nullptr;
    QString strRoot, strHome, strNone;
    long long available = 0;
    long long prefRoot = 0, prefHome = 0;
    long long minRoot = 0, recRoot = 0, sizeRoot = 0;
    long long minHome = 0, recHome = 0, sizeHome = 0;
    int recPortionMin = 0, recPortionMax = 0;
    bool crypto = false;
    bool snapToRec = false;
    // Slots
    void sliderPressed();
    void actionTriggered(int action);
    void valueChanged(int value);
public:
    AutoPart(class MProcess &mproc, class PartMan *pman, Ui::MeInstall &ui,
        const class QSettings &appConf, long long homeNeeded);
    void refresh();
    void setDrive(class DeviceItem *drive, bool swapfile, bool encrypt, bool hibernation, bool snapshot);
    enum Part { Root, Home };
    void setPartSize(Part part, long long nbytes);
    long long partSize(Part part = Root);
    // Layout Builder
    void builderGUI(class DeviceItem *drive);
    long long buildLayout(long long rootFormatSize, bool crypto, bool updateTree=true);
    // Helpers
    long long recommended(bool encrypt, bool hibernation, bool snapshots);
    static QString sizeString(long long size);
signals:
    void partsChanged();
};

// Calculate a percentage without compromising the range of long long.
static inline long long portion(long long range, int percent, long round = -1)
{
    const bool roundUp = (round > 0);
    if (roundUp) percent = 100 - percent;
    else round = -round;
    long long r = ((range / 100) * percent) + (((range % 100) * percent) / 100);
    if (roundUp) {
        r = ((range-r) + (round-1LL)) / round;
        r *= round;
        if (r < 0 || r > range) return range;
    } else {
        r /= round;
        r *= round;
    }
    return r;
}
static inline int percent(long long portion, long long range, bool roundUp = false)
{
    if (roundUp) portion = range - portion;
    const int percent = portion / (double)(range / 100.0L);
    return roundUp ? 100-percent : percent;
}
#endif // AUTOPART_H
