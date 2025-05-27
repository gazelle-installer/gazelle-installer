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
#include "core.h"
#include "msettings.h"
#include "partman.h"
#include "swapman.h"

using namespace Qt::Literals::StringLiterals;

SwapMan::SwapMan(MProcess &mproc, Core &mcore, PartMan &pman, Ui::MeInstall &ui, QObject *parent) noexcept
    : QObject(parent), proc(mproc), core(mcore), partman(pman), gui(ui)
{
    connect(ui.textSwapFile, &QLineEdit::textEdited, this, [this](const QString &) {
        gui.pushNext->setEnabled(setupBounds());
    });
    connect(ui.spinSwapSize, QOverload<int>::of(&QSpinBox::valueChanged), this, &SwapMan::spinSizeChanged);
    connect(ui.pushSwapSizeReset, &QToolButton::clicked, this, &SwapMan::sizeResetClicked);
    connect(ui.checkHibernation, &QCheckBox::clicked, this, &SwapMan::checkHibernationClicked);
    connect(ui.boxSwapFile, &QGroupBox::toggled, this, [this](bool on) {
        if (!on) gui.pushNext->setEnabled(true);
        else gui.pushNext->setEnabled(setupBounds());
    });

    connect(ui.radioZramPercent, &QRadioButton::toggled, ui.spinZramPercent, &QSpinBox::setEnabled);
    connect(ui.radioZramPercent, &QRadioButton::toggled, ui.labelZramRecPercent, &QSpinBox::setEnabled);
    connect(ui.radioZramSize, &QRadioButton::toggled, ui.spinZramSize, &QSpinBox::setEnabled);
    connect(ui.radioZramSize, &QRadioButton::toggled, ui.labelZramRecSize, &QLabel::setEnabled);
    // Set the size spin box.
    struct sysinfo sinfo;
    if (sysinfo(&sinfo) == 0) {
        const int physram = ((long long)sinfo.totalram * sinfo.mem_unit) / MB;
        gui.spinZramSize->setMaximum(10 * physram);
        gui.spinZramSize->setValue(physram);
        gui.labelZramRecSize->setText(tr("Recommended maximum: %1 MB").arg(physram));
    } else {
        gui.radioZramSize->setEnabled(false);
        gui.labelZramRecSize->setText(tr("Unable to query RAM size."));
        gui.spinZramSize->setMaximum(0);
    }
}

void SwapMan::manageConfig(MSettings &config) noexcept
{
    config.setSection(u"Swap"_s, gui.pageBoot);
    config.manageGroupCheckBox(u"Install"_s, gui.boxSwapFile);
    config.manageLineEdit(u"File"_s, gui.textSwapFile);
    config.manageSpinBox(u"Size"_s, gui.spinSwapSize);
    config.manageCheckBox(u"Hibernation"_s, gui.checkHibernation);
}
void SwapMan::setupDefaults() noexcept
{
    gui.boxSwapFile->setChecked(partman.findByMount(u"swap"_s) == nullptr);
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
    if (gui.boxSwapFile->isChecked()) {
        installSwapFile(cmdboot_out);
    }
    if (gui.boxSwapZram->isChecked()) {
        MProcess::Section sect(proc, QT_TR_NOOP("Failed to configure zram swap."));
        sect.setRoot("/mnt/antiX");
        core.setService(u"zramswap"_s, true);
        if (!configureZRam()) {
            throw sect.failMessage();
        }
    }
}
void SwapMan::installSwapFile(QStringList &cmdboot_out) const
{
    MProcess::Section sect(proc, QT_TR_NOOP("Failed to create or install swap file."));
    const QString &swapfile = QDir::cleanPath(gui.textSwapFile->text().trimmed());
    const QString &instpath = "/mnt/antiX"_L1 + swapfile;
    const PartMan::Device *device = partman.findHostDev(swapfile);
    if (!device) throw sect.failMessage();

    proc.status(tr("Creating swap file"));
    // Create a blank swap file.
    core.mkpath(QFileInfo(instpath).path(), 0700);
    const bool btrfs = (device->type == PartMan::Device::SUBVOLUME || device->finalFormat() == "btrfs"_L1);
    const QString &strsize = QStringLiteral("%1M").arg(gui.spinSwapSize->value());
    if (btrfs) {
        proc.exec(u"btrfs"_s, {u"filesystem"_s, u"mkswapfile"_s, u"--size"_s, strsize, instpath});
    } else {
        proc.exec(u"mkswap"_s, {u"-q"_s, u"--size"_s, strsize, u"--file"_s, instpath});
    }

    proc.status(tr("Configuring swap file"));
    // Append the fstab with the swap file entry.
    QFile file(u"/mnt/antiX/etc/fstab"_s);
    if (!file.open(QIODevice::Append | QIODevice::WriteOnly)) throw sect.failMessage();
    file.write(swapfile.toUtf8());
    file.write(" swap swap defaults 0 0\n");
    file.close();
    // Hibernation.
    if (gui.checkHibernation->isChecked()) {
        proc.shell("blkid -s UUID -o value $(df -P "_L1 + instpath + " | awk 'END{print $1}')"_L1, nullptr, true);
        cmdboot_out.append("resume=UUID=" + proc.readAll().trimmed());
        if (btrfs) {
            proc.exec(u"btrfs"_s, {u"inspect-internal"_s, u"map-swapfile"_s, u"-r"_s, instpath}, nullptr, true);
        } else {
            proc.shell("filefrag -v "_L1 + instpath + " | awk 'NR==4 {print $4}' | tr -d ."_L1, nullptr, true);
        }
        cmdboot_out.append("resume_offset=" + proc.readAll().trimmed());
    }
}
bool SwapMan::configureZRam() const noexcept
{
    const QString zramfile(u"/mnt/antiX/etc/default/zramswap"_s);
    bool ok = false;
    QFile::copy(zramfile, zramfile+".bak"_L1);
    MIni zramcfg(zramfile, MIni::ReadWrite, &ok);
    if (!ok) return false;

    if (gui.radioZramPercent->isChecked()) {
        zramcfg.setInteger(u"PERCENT"_s, gui.spinZramPercent->value());
    } else {
        zramcfg.setInteger(u"SIZE"_s, gui.spinZramSize->value());
    }
    return zramcfg.save();
}

void SwapMan::setupZRam() const
{
    struct sysinfo sinfo;
    if (sysinfo(&sinfo) != 0) return;
    const long long zrsize = (long long)sinfo.totalram * sinfo.mem_unit;
    // Reported compressed data can be inaccurate if zswap is enabled, especially on Liquorix.
    proc.shell(u"echo 0 > /sys/module/zswap/parameters/enabled"_s);
    proc.exec(u"modprobe"_s, {u"zram"_s});
    if (!proc.exec(u"zramctl"_s, {u"zram0"_s, u"-a"_s,u"zstd"_s, u"-s"_s, QString::number(zrsize)})) return;
    if (!proc.exec(u"mkswap"_s, {u"-q"_s, u"/dev/zram0"_s})) return;
    proc.exec(u"swapon"_s, {u"-p"_s,u"32767"_s, u"/dev/zram0"_s});
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
