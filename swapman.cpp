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

#include <math.h>
#include <sys/sysinfo.h>
#include <sys/stat.h>
#include <QFileInfo>
#include <QDir>
#include "mprocess.h"
#include "msettings.h"
#include "partman.h"
#include "swapman.h"

SwapMan::SwapMan(MProcess &mproc, PartMan &pman, Ui::MeInstall &ui) noexcept
    : QObject(ui.boxMain), proc(mproc), partman(pman), gui(ui)
{
    connect(ui.textSwapFile, &QLineEdit::textEdited, this, [this](const QString &) {
        gui.pushNext->setEnabled(setupBounds());
    });
    connect(ui.spinSwapSize, QOverload<int>::of(&QSpinBox::valueChanged), this, &SwapMan::spinSizeChanged);
    connect(ui.pushSwapSizeReset, &QToolButton::clicked, this, &SwapMan::sizeResetClicked);
    connect(ui.checkHibernation, &QCheckBox::clicked, this, &SwapMan::checkHibernationClicked);
    connect(ui.boxSwap, &QGroupBox::toggled, this, [=](bool on) {
        if (on) ui.pushNext->setEnabled(true);
        else ui.pushNext->setEnabled(setupBounds());
    });
}

void SwapMan::manageConfig(MSettings &config, bool advanced) noexcept
{
    config.startGroup("Swap", gui.pageBoot);
    if (advanced || !config.isSave()) {
        config.manageGroupCheckBox("Install", gui.boxSwap);
        config.manageLineEdit("File", gui.textSwapFile);
        config.manageSpinBox("Size", gui.spinSwapSize);
    }
    config.manageCheckBox("Hibernation", gui.checkHibernation);
    config.endGroup();
}
void SwapMan::setupDefaults() noexcept
{
    gui.boxSwap->setChecked(partman.swapCount() <= 0);
    setupBounds();
    sizeResetClicked();
}

bool SwapMan::setupBounds() noexcept
{
    const PartMan::Device *device = partman.findHostDev(gui.textSwapFile->text());
    int max = 0;
    if (!device) gui.labelSwapMax->setText(tr("Invalid location"));
    else {
        max = (int)((device->size - partman.volSpecTotal(device->usefor).minimum) / MB);
        gui.labelSwapMax->setText(tr("Maximum: %1 MB").arg(max));
    }
    gui.spinSwapSize->setMaximum(max);
    const bool canHibernate = (max >= (recommended(true) / MB));
    gui.checkHibernation->setEnabled(canHibernate);
    if (!canHibernate) gui.checkHibernation->setChecked(false);
    return (device != nullptr);
}

void SwapMan::install(QStringList &cmdboot_out)
{
    proc.log(__PRETTY_FUNCTION__, MProcess::LOG_MARKER);
    proc.advance(1, 2);
    const int size = gui.spinSwapSize->value();
    if (!gui.boxSwap->isChecked() || size <= 0) return;
    MProcess::Section sect(proc, QT_TR_NOOP("Failed to create or install swap file."));
    const QString &swapfile = QDir::cleanPath(gui.textSwapFile->text().trimmed());
    const QString &instpath = "/mnt/antiX" + swapfile;
    const PartMan::Device *device = partman.findHostDev(swapfile);
    if (!device) throw sect.failMessage();

    proc.status(tr("Creating swap file"));
    // Create a blank swap file.
    proc.mkpath(QFileInfo(instpath).path(), 0700);
    const bool btrfs = (device->type == PartMan::Device::SUBVOLUME || device->finalFormat() == "btrfs");
    if (btrfs) {
        proc.exec("truncate", {"--size=0", instpath});
        proc.exec("chattr", {"+C", instpath});
    }
    proc.exec("fallocate", {"-l", QStringLiteral("%1M").arg(size), instpath});
    chmod(instpath.toUtf8().constData(), 0600);
    proc.exec("mkswap", {"-q", instpath});

    proc.status(tr("Configuring swap file"));
    // Append the fstab with the swap file entry.
    QFile file("/mnt/antiX/etc/fstab");
    if (!file.open(QIODevice::Append | QIODevice::WriteOnly)) throw sect.failMessage();
    file.write(swapfile.toUtf8());
    file.write(" swap swap defaults 0 0\n");
    file.close();
    // Hibernation.
    if (gui.checkHibernation->isChecked()) {
        proc.shell("blkid -s UUID -o value $(df -P " + instpath + " | awk 'END{print $1}')", nullptr, true);
        cmdboot_out.append("resume=UUID=" + proc.readAll().trimmed());
        if (btrfs) {
            proc.exec("btrfs", {"inspect-internal", "map-swapfile", "-r", instpath}, nullptr, true);
        } else {
            proc.shell("filefrag -v " + instpath + " | awk 'NR==4 {print $4}' | tr -d .", nullptr, true);
        }
        cmdboot_out.append("resume_offset=" + proc.readAll().trimmed());
    }
}

void SwapMan::setupZRam() const
{
    struct sysinfo sinfo;
    if (sysinfo(&sinfo) != 0) return;
    const long long zrsize = (long long)sinfo.totalram * sinfo.mem_unit;
    // Reported compressed data can be inaccurate if zswap is enabled, especially on Liquorix.
    proc.shell("echo 0 > /sys/module/zswap/parameters/enabled");
    proc.exec("modprobe", {"zram"});
    if (!proc.exec("zramctl", {"zram0", "-a","zstd", "-s", QString::number(zrsize)})) return;
    if (!proc.exec("mkswap", {"-q", "/dev/zram0"})) return;
    proc.exec("swapon", {"-p","32767", "/dev/zram0"});
}

long long SwapMan::recommended(bool hibernation) noexcept
{
    struct sysinfo sinfo;
    if (sysinfo(&sinfo) != 0) return 0;
    long long physram = (long long)sinfo.totalram * sinfo.mem_unit;
    long long irec = hibernation ? physram : 0;
    if (physram < 1*GB) irec += physram;
    else irec += (long long)roundl(sqrtl((long double)physram / GB)) * GB;
    return irec;
}

/* Slots */

void SwapMan::sizeResetClicked() noexcept
{
    const int irec = (int)(recommended(gui.checkHibernation->isChecked()) / MB);
    gui.spinSwapSize->setValue(irec);
    spinSizeChanged(gui.spinSwapSize->value());
}

void SwapMan::spinSizeChanged(int i) noexcept
{
    if (i < (recommended(true) / MB)) gui.checkHibernation->setChecked(false);
}

void SwapMan::checkHibernationClicked(bool checked) noexcept
{
    if (checked) {
        const int irec = (int)(recommended(gui.checkHibernation->isChecked()) / MB);
        if(gui.spinSwapSize->value() < irec) gui.spinSwapSize->setValue(irec);
    }
}
