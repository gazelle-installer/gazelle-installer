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

#include "blockdev.h"
#include <QComboBox>
#include <QRegularExpression>

// static function to split a device name into its drive and partition
QStringList BlockDeviceInfo::split(const QString &devname)
{
    static const QRegularExpression rxdev1("^(?:/dev/)+(mmcblk.*|nvme.*)p([0-9]*)$");
    static const QRegularExpression rxdev2("^(?:/dev/)+([a-z]*)([0-9]*)$");
    QRegularExpressionMatch rxmatch(rxdev1.match(devname));
    if (!rxmatch.hasMatch()) rxmatch = rxdev2.match(devname);
    QStringList list(rxmatch.capturedTexts());
    if (!list.isEmpty()) list.removeFirst();
    return list;
}

// return block device info that is suitable for a combo box
void BlockDeviceInfo::addToCombo(QComboBox *combo, bool warnNasty) const
{
    static const char *suffixes[] = {"B", "KB", "MB", "GB", "TB", "PB", "EB"};
    unsigned int isuffix = 0;
    qlonglong scalesize = size;
    while (scalesize >= 1024 && isuffix < sizeof(suffixes)) {
        ++isuffix;
        scalesize /= 1024;
    }
    QString strout(name + " (" + QString::number(scalesize) + suffixes[isuffix]);
    if (!fs.isEmpty()) strout += " " + fs;
    if (!label.isEmpty()) strout += " - " + label;
    if (!model.isEmpty()) strout += (label.isEmpty() ? " - " : "; ") + model;
    QString stricon;
    if (isFuture) stricon = "appointment-soon-symbolic";
    else if (isNasty && warnNasty) stricon = "dialog-warning-symbolic";
    combo->addItem(QIcon::fromTheme(stricon), strout + ")", name);
}

int BlockDeviceList::findDevice(const QString &devname) const
{
    const int cnt = count();
    for (int ixi = 0; ixi < cnt; ++ixi) {
        const BlockDeviceInfo &bdinfo = at(ixi);
        if (bdinfo.name == devname) return ixi;
    }
    return -1;
}
