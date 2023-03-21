/***************************************************************************
 * Swap space management and setup for the installer.
 ***************************************************************************
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
#include <math.h>
#include <sys/sysinfo.h>
#include <QFileInfo>
#include <QDir>
#include "swapman.h"

SwapMan::SwapMan(MProcess &mproc, PartMan &pman, Ui::MeInstall &ui)
    : QObject(ui.boxMain), proc(mproc), gui(ui), partman(pman)
{
    connect(ui.textSwapFile, &QLineEdit::textEdited, this, &SwapMan::swapFileEdited);
    connect(ui.pushSwapSizeReset, &QToolButton::clicked, this, &SwapMan::sizeResetClick);
}

void SwapMan::manageConfig(MSettings &config)
{
    config.startGroup("Swap", gui.pageBoot);
    config.manageGroupCheckBox("Install", gui.boxSwap);
    config.manageLineEdit("File", gui.textSwapFile);
    config.manageSpinBox("Size", gui.spinSwapSize);
    config.endGroup();
}
void SwapMan::setupDefaults()
{
    gui.boxSwap->setChecked(partman.swapCount() <= 0);
    swapFileEdited(gui.textSwapFile->text());
    sizeResetClick();
}

void SwapMan::install()
{
    proc.log(__PRETTY_FUNCTION__, MProcess::Section);
    if (proc.halted()) return;
    proc.advance(1, 2);
    const int size = gui.spinSwapSize->value();
    if(!gui.boxSwap->isChecked() || size <= 0) return;
    static const char *const msgfail = QT_TR_NOOP("Failed to create or install swap file.");
    const QString &instpath = gui.textSwapFile->text().trimmed();
    const QString &realpath = "/mnt/antiX" + instpath;

    proc.status(tr("Creating swap file"));
    // Create a blank swap file.
    bool ok = proc.exec("fallocate", {"-l", QStringLiteral("%1M").arg(size), realpath});
    if(ok) chmod(instpath.toUtf8().constData(), 0600);
    if(ok) ok = proc.exec("mkswap", {realpath});
    if(!ok) throw msgfail;
    // Append the fstab with the swap file entry.
    proc.status(tr("Configuring swap file"));
    QFile file("/mnt/antiX/etc/fstab");
    if (!file.open(QIODevice::Append | QIODevice::WriteOnly)) throw msgfail;
    file.write(instpath.toUtf8());
    file.write(" swap swap defaults 0 0\n");
    file.close();
}

unsigned long SwapMan::recommendedMB(bool hibernation)
{
    struct sysinfo sinfo;
    if (sysinfo(&sinfo) == 0) {
        sinfo.totalram /= 1048576;
        unsigned long irec = sinfo.totalram;
        if (sinfo.totalram > 1024) {
            irec = (unsigned long)round(sqrt((float)sinfo.totalram/1024.0));
            irec *= 1024;
        }
        if (hibernation) irec += sinfo.totalram;
        return irec;
    }
    return 0;
}

/* Slots */

void SwapMan::swapFileEdited(const QString &text)
{
    QString spath = text;
    const DeviceItem *devit = nullptr;
    do {
        spath = QDir(QFileInfo(spath).absolutePath()).path();
        devit = partman.mounts.value(spath);
    } while(!devit && !spath.isEmpty() && spath != '/');

    int max = 0;
    if (!devit) gui.labelSwapMax->setText(tr("Invalid location"));
    else {
        max = devit->size / 2097152; // Max swap = half device size
        gui.labelSwapMax->setText(tr("Maximum: %1 MB").arg(max));
    }
    gui.spinSwapSize->setMaximum(max);
}

void SwapMan::sizeResetClick()
{
    int irec = (int)recommendedMB(false);
    const int imax = gui.spinSwapSize->maximum() / 2;
    if (irec > imax) irec = imax;
    gui.spinSwapSize->setValue(irec);
}
