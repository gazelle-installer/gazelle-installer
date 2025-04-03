/***************************************************************************
 * Boot manager (GRUB) setup for the installer.
 *
 *   Copyright (C) 2022-2023 by AK-47, along with transplanted code:
 *    - Copyright (C) 2003-2010 by Warren Woodford
 *    - Heavily edited, with permision, by anticapitalista for antiX 2011-2014.
 *    - Heavily revised by dolphin oracle, adrian, and anticaptialista 2018.
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

#include <sys/stat.h>
#include <QDebug>
#include <QFileInfo>
#include "mprocess.h"
#include "core.h"
#include "msettings.h"
#include "partman.h"
#include "bootman.h"

using namespace Qt::Literals::StringLiterals;

BootMan::BootMan(MProcess &mproc, Core &mcore, PartMan &pman, Ui::MeInstall &ui,
    MIni &appConf, const QCommandLineParser &appArgs, QObject *parent) noexcept
    : QObject(parent), proc(mproc), core(mcore), gui(ui), partman(pman)
{
    appConf.setSection(u"Storage"_s);
    installFromRootDevice = appConf.getBoolean(u"InstallFromRootDevice"_s);
    appConf.setSection(u"OOBE"_s);
    removeNoSplash = appConf.getBoolean(u"RemoveNosplash"_s);
    appConf.setSection();
    loaderID = appConf.getString(u"ShortName"_s);
    loaderLabel = appConf.getString(u"Name"_s);
    brave = appArgs.isSet(u"brave"_s);

    connect(gui.radioBootMBR, &QRadioButton::toggled, this, &BootMan::chosenBootMBR);
    connect(gui.radioBootPBR, &QRadioButton::toggled, this, &BootMan::chosenBootPBR);
    connect(gui.radioBootESP, &QRadioButton::toggled, this, &BootMan::chosenBootESP);
}

void BootMan::manageConfig(MSettings &config) noexcept
{
    config.setSection(u"GRUB"_s, gui.pageBoot);
    config.manageGroupCheckBox(u"Install"_s, gui.boxBoot);
    static constexpr const char *grubChoices[] = {"MBR", "PBR", "ESP"};
    QRadioButton *const grubRadios[] = {gui.radioBootMBR, gui.radioBootPBR, gui.radioBootESP};
    config.manageRadios(u"TargetType"_s, 3, grubChoices, grubRadios);
    if (!gui.radioBootESP->isChecked()) {
        config.manageComboBox(u"Location"_s, gui.comboBoot, true);
    }
    config.manageCheckBox(u"HostSpecific"_s, gui.checkBootHostSpecific);
}

void BootMan::selectBootMain() noexcept
{
    const PartMan::Device *device = partman.findByMount(u"/boot"_s);
    if (!device) {
        device = partman.findByMount(u"/"_s);
    }
    assert(device != nullptr);

    if (device->origin) device = device->origin;
    while (device && device->type != PartMan::Device::PARTITION) {
        device = device->parent();
    }
    if (!gui.radioBootPBR->isChecked() && device) device = device->parent();
    if (!device) return;

    int ixsel = gui.comboBoot->findData(device->name); // Boot drive or partition
    for(int ixi = 0; ixsel < 0 && ixi < gui.comboBoot->count(); ++ixi) {
        const PartMan::NameParts &s = PartMan::splitName(gui.comboBoot->itemData(ixi).toString());
        if (s.drive == device->name) ixsel = ixi; // Partition on boot drive
    }
    if (ixsel >= 0) gui.comboBoot->setCurrentIndex(ixsel);
}

// build ESP list available to install GRUB
void BootMan::buildBootLists() noexcept
{
    // refresh lists and enable or disable options according to device presence
    chosenBootMBR();
    const bool canMBR = (gui.comboBoot->count() > 0);
    gui.radioBootMBR->setEnabled(canMBR);
    chosenBootPBR();
    const bool canPBR = (gui.comboBoot->count() > 0);
    gui.radioBootPBR->setEnabled(canPBR);
    const bool canESP = core.detectEFI();
    gui.radioBootESP->setEnabled(canESP);

    // load one as the default in preferential order: ESP, MBR, PBR
    if (canESP) gui.radioBootESP->setChecked(true);
    else if (canMBR) {
        chosenBootMBR();
        gui.radioBootMBR->setChecked(true);
    } else if (canPBR) {
        chosenBootPBR();
        gui.radioBootPBR->setChecked(true);
    }
}

// build a grub configuration and install grub
void BootMan::install(const QStringList &cmdextra)
{
    proc.log(__PRETTY_FUNCTION__, MProcess::LOG_MARKER);
    MProcess::Section sect(proc, QT_TR_NOOP("GRUB installation failed. You can reboot to"
        " the live medium and use the GRUB Rescue menu to repair the installation."));
    proc.advance(4, 4);

    sect.setExceptionStrict(false);
    // the old initrd is not valid for this hardware
    proc.shell(u"ls /mnt/antiX/boot | grep 'initrd.img-3.6'"_s, nullptr, true);
    const QString &val = proc.readOut();
    if (!val.isEmpty()) {
        proc.exec(u"rm"_s, {u"-f"_s, "/mnt/antiX/boot/"_L1 + val});
    }

    bool efivars_ismounted = false;
    if (gui.boxBoot->isChecked() && gui.radioBootESP->isChecked()) {
        const QString &efivars = u"/sys/firmware/efi/efivars"_s;
        if (QFileInfo(efivars).isDir()) {
            efivars_ismounted = proc.exec(u"mountpoint"_s, {u"-q"_s, efivars});
        }
        if (!efivars_ismounted) {
            proc.exec(u"mount"_s, {u"-t"_s, u"efivarfs"_s, u"efivarfs"_s, efivars});
        }
    }

    sect.setExceptionStrict(true);

    if (gui.boxBoot->isChecked()) {
        proc.status(tr("Installing GRUB"));

        // install new Grub now
        const int efisize = core.detectEFI(true);
        const QString &bootdev = "/dev/"_L1 + gui.comboBoot->currentData().toString();
        if (!gui.radioBootESP->isChecked()) {
            proc.exec(u"grub-install"_s, {u"--target=i386-pc"_s, u"--recheck"_s,
                u"--no-floppy"_s, u"--force"_s, u"--boot-directory=/mnt/antiX/boot"_s, bootdev});
        } else {
            // rename arch to match grub-install target
            proc.exec(u"cat"_s, {u"/sys/firmware/efi/fw_platform_size"_s}, nullptr, true);

            if (efivars_ismounted) {
                // remove any efivars-dump-entries in NVRAM
                sect.setExceptionStrict(false);
                proc.shell(u"ls /sys/firmware/efi/efivars | grep dump"_s, nullptr, true);
                const QString &dump = proc.readOut();
                sect.setExceptionStrict(true);
                if (!dump.isEmpty()) {
                    proc.shell(u"rm /sys/firmware/efi/efivars/dump*"_s, nullptr, true);
                }
            }

            sect.setRoot("/mnt/antiX");

            QStringList grubinstargs({u"--no-nvram"_s, u"--force-extra-removable"_s,
                (efisize==32 ? u"--target=i386-efi"_s : u"--target=x86_64-efi"_s),
                "--bootloader-id="_L1 + loaderID, u"--recheck"_s});
            const PartMan::Device *espdev = partman.findByMount(u"/boot/efi"_s);
            if (!espdev) espdev = partman.findByMount(u"/boot"_s);
            if (espdev != nullptr && espdev->flags.sysEFI) {
                grubinstargs << "--efi-directory="_L1 + espdev->mountPoint();
            }
            proc.exec(u"grub-install"_s, grubinstargs);

            // Update the boot NVRAM variables. Non-critial step so no need to fail.
            sect.setExceptionStrict(false);
            // Remove old boot variables of the same label.
            proc.exec(u"efibootmgr"_s, {}, nullptr, true);
            const QStringList &existing = proc.readOutLines();
            static const QRegularExpression regex(u"^Boot([0-9A-F]{4})\\*?\\s(.*)$"_s);
            for (const QString &entry : existing) {
                const QRegularExpressionMatch &match = regex.match(entry);
                if (match.hasMatch() && match.captured(2) == loaderLabel) {
                    proc.exec(u"efibootmgr"_s, {u"-qBb"_s, match.captured(1)});
                }
            }
            // Add a new NVRAM boot variable.
            if (espdev != nullptr) {
                const PartMan::NameParts &bs = PartMan::splitName(espdev->name);
                proc.exec(u"efibootmgr"_s, {u"-qcL"_s, loaderLabel, u"-d"_s, "/dev/"_L1 + bs.drive,
                    u"-p"_s, bs.partition, u"-l"_s,
                    "/EFI/"_L1 + loaderID + (efisize==32 ? "/grubia32.efi"_L1 : "/grubx64.efi"_L1)});
            }
            sect.setExceptionStrict(true);
            sect.setRoot(nullptr);
        }

        // Get non-live boot codes.
        proc.shell(u"/live/bin/non-live-cmdline"_s, nullptr, true); // Get non-live boot codes
        QStringList finalcmdline = proc.readOut().split(' ');

        {
            // Add the codes from /etc/default/grub to non-live boot codes.
            const MIni grubSettings(u"/etc/default/grub"_s, MIni::ReadOnly);
            const QString &grubDefault = grubSettings.getString(u"GRUB_CMDLINE_LINUX_DEFAULT"_s);
            qDebug() << "grubDefault is " << grubDefault;
            finalcmdline.append(grubDefault.split(' '));
        }

        // remove any leftover resume parameter
        static const QRegularExpression re(u"^(resume=|resume_offset=)"_s, QRegularExpression::CaseInsensitiveOption);
        const auto toRemove = finalcmdline.filter(re);
        for(const auto &item : toRemove) {
            finalcmdline.removeAll(item);
        }

        finalcmdline.append(cmdextra);
        qDebug() << "intermediate" << finalcmdline;

        {
            // Add built-in config_cmdline.
            proc.exec(u"uname"_s, {u"-r"_s});
            const MIni bootconf("/boot/config-"_L1 + proc.readOut(true), MIni::ReadOnly);
            const QStringList confcmdline = bootconf.getString(u"CONFIG_CMDLINE"_s).split(' ');
            for (const QString &confparameter : confcmdline) {
                finalcmdline.removeAll(confparameter);
            }
        }

        //remove any duplicate codes in list (typically splash)
        finalcmdline.removeDuplicates();

        //remove vga=ask
        finalcmdline.removeAll(u"vga=ask"_s);

        //remove toram=min and toram=store - is not yet in non-live-cmdline
        finalcmdline.removeAll(u"toram=min"_s);
        finalcmdline.removeAll(u"toram=store"_s);

        //remove boot_image code
        finalcmdline.removeAll(u"BOOT_IMAGE=/antiX/vmlinuz"_s);

        //remove nosplash boot code if configured in installer.conf
        if (removeNoSplash) {
            finalcmdline.removeAll(u"nosplash"_s);
        }

        //remove in null or empty strings that might have crept in
        finalcmdline.removeAll({});
        qDebug() << "Add cmdline options to Grub" << finalcmdline;

        //convert qstringlist back into normal qstring
        QString finalcmdlinestring = finalcmdline.join(' ');
        qDebug() << "cmdlinestring" << finalcmdlinestring;

        //get qstring boot codes read for sed command
        finalcmdlinestring.replace('\\', "\\\\"_L1);
        finalcmdlinestring.replace('|', "\\|"_L1);

        //do the replacement in /etc/default/grub
        const QString &cmd = u"sed -i -r 's|^(GRUB_CMDLINE_LINUX_DEFAULT=).*|\\1\"%1\"|' /mnt/antiX/etc/default/grub"_s;
        proc.shell(cmd.arg(finalcmdlinestring));

        //copy memtest efi files if needed
        if (efisize) {
            QString mtest = QStringLiteral("/live/to-ram/boot/uefi-mt/mtest-%1.efi").arg(efisize);
            if (!QFileInfo::exists(mtest)) {
                mtest = QStringLiteral("/live/boot-dev/boot/uefi-mt/mtest-%1.efi").arg(efisize);
                if (!QFileInfo::exists(mtest)) mtest.clear();
            }
            if (!mtest.isEmpty()) {
                core.mkpath(u"/mnt/antiX/boot/uefi-mt"_s, 0755);
                proc.exec(u"cp"_s, {mtest, u"/mnt/antiX/boot/uefi-mt"_s});
            }
        }
        proc.status();

        sect.setRoot("/mnt/antiX");
        //update grub with new config
        proc.exec(u"update-grub"_s);

        if (!gui.radioBootESP->isChecked()) {
            /* Prevent debconf pestering the user when GRUB gets updated. Non-critical. */
            MProcess::Section(proc, nullptr);
            proc.exec(u"udevadm"_s, {u"info"_s, bootdev}, nullptr, true);
            const QStringList &lines = proc.readOutLines();
            /* Obtain the best ID to use for the disk or partition. */
            QByteArray diskpath;
            static const char *types[] = {"S: disk/by-uuid", "S: disk/by-id", "S: disk/by-path", "S: "};
            int stop = sizeof(types)/sizeof(const char *);
            for (const QString &line : lines) {
                for (int i = 0; i < stop; ++i) {
                    if (line.startsWith(types[i])) {
                        diskpath = line.mid(3).toUtf8();
                        stop = i;
                    }
                }
            }
            if (!diskpath.isEmpty()) {
                /* Setup debconf to achieve the objective of silence. */
                diskpath.prepend("grub-pc grub-pc/install_devices multiselect /dev/");
                proc.exec(u"debconf-set-selections"_s, {}, &diskpath);
            }
        }
    }

    proc.status(tr("Updating initramfs"));
    sect.setExceptionMode(QT_TR_NOOP("Failed to update initramfs."));
    //if useing f2fs, then add modules to /etc/initramfs-tools/modules
    //if (rootTypeCombo->currentText() == "f2fs" || homeTypeCombo->currentText() == "f2fs") {
        //proc.shell("grep -q f2fs /etc/initramfs-tools/modules || echo f2fs >> /etc/initramfs-tools/modules");
        //proc.shell("grep -q crypto-crc32 /etc/initramfs-tools/modules || echo crypto-crc32 >> /etc/initramfs-tools/modules");
    //}

    if (gui.checkBootHostSpecific->isChecked()) {
        const QString ircfname = sect.root() + "/etc/initramfs-tools/initramfs.conf"_L1;
        QFile::copy(ircfname, ircfname+".bak"_L1);
        MIni ircfg(ircfname, MIni::ReadWrite);
        ircfg.setString(u"MODULES"_s, u"dep"_s);
        ircfg.save();
    }

    proc.exec(u"update-initramfs"_s, {u"-u"_s, u"-t"_s, u"-k"_s, u"all"_s});
    proc.status();
}

/* Slots */

void BootMan::chosenBootMBR() noexcept
{
    gui.comboBoot->clear();
    for (PartMan::Iterator it(partman); PartMan::Device *device = *it; it.next()) {
        if (device->type == PartMan::Device::DRIVE && (!device->flags.bootRoot || installFromRootDevice)) {
            if (!device->flags.nasty || brave) device->addToCombo(gui.comboBoot, true);
        }
    }
    selectBootMain();
    gui.labelBoot->setText(tr("System boot disk:"));
}

void BootMan::chosenBootPBR() noexcept
{
    gui.comboBoot->clear();
    for (PartMan::Iterator it(partman); PartMan::Device *device = *it; it.next()) {
        if (device->type == PartMan::Device::PARTITION && (!device->flags.bootRoot || installFromRootDevice)) {
            const QString &ff = device->finalFormat();
            if (!device->flags.sysEFI && ff != "swap" && ff != "crypto_LUKS" && (!device->flags.nasty || brave)) {
                device->addToCombo(gui.comboBoot, true);
            }
        }
    }
    selectBootMain();
    gui.labelBoot->setText(tr("Partition to use:"));
}

void BootMan::chosenBootESP(bool checked) noexcept
{
    gui.comboBoot->clear();
    const PartMan::Device *dev = partman.findByMount(u"/boot/efi"_s);
    if (!dev) dev = partman.findByMount(u"/boot"_s);
    if (dev) dev->addToCombo(gui.comboBoot);
    gui.comboBoot->setDisabled(checked);
    gui.labelBoot->setText(tr("Partition to use:"));
}
