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
#include <QMessageBox>
#include <QFileInfo>
#include "mprocess.h"
#include "msettings.h"
#include "partman.h"
#include "bootman.h"

BootMan::BootMan(MProcess &mproc, PartMan &pman, Ui::MeInstall &ui,
    const QSettings &appConf, const QCommandLineParser &appArgs) noexcept
    : QObject(ui.boxMain), proc(mproc), gui(ui), partman(pman)
{
    loaderID = appConf.value("PROJECT_SHORTNAME").toString();
    loaderLabel = appConf.value("PROJECT_NAME").toString();
    installFromRootDevice = appConf.value("INSTALL_FROM_ROOT_DEVICE").toBool();
    removeNoSplash = appConf.value("REMOVE_NOSPLASH").toBool();
    brave = appArgs.isSet("brave");

    connect(gui.radioBootMBR, &QRadioButton::toggled, this, &BootMan::chosenBootMBR);
    connect(gui.radioBootPBR, &QRadioButton::toggled, this, &BootMan::chosenBootPBR);
    connect(gui.radioBootESP, &QRadioButton::toggled, this, &BootMan::chosenBootESP);
}

void BootMan::manageConfig(MSettings &config) noexcept
{
    config.startGroup("GRUB", gui.pageBoot);
    config.manageGroupCheckBox("Install", gui.boxBoot);
    const char *grubChoices[] = {"MBR", "PBR", "ESP"};
    QRadioButton *grubRadios[] = {gui.radioBootMBR, gui.radioBootPBR, gui.radioBootESP};
    config.manageRadios("TargetType", 3, grubChoices, grubRadios);
    config.manageComboBox("Location", gui.comboBoot, true);
    config.endGroup();
}

void BootMan::selectBootMain() noexcept
{
    const DeviceItem *twit = partman.mounts.value("/boot");
    if (!twit) twit = partman.mounts.value("/");
    if (twit->origin) twit = twit->origin;
    while (twit && twit->type != DeviceItem::Partition) twit = twit->parent();
    if (!gui.radioBootPBR->isChecked() && twit) twit = twit->parent();
    if (!twit) return;
    int ixsel = gui.comboBoot->findData(twit->device); // Boot drive or partition
    for(int ixi = 0; ixsel < 0 && ixi < gui.comboBoot->count(); ++ixi) {
        const QStringList &s = DeviceItem::split(gui.comboBoot->itemData(ixi).toString());
        if (s.at(0) == twit->device) ixsel = ixi; // Partition on boot drive
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
    chosenBootESP();
    const bool canESP = (proc.detectEFI() && gui.comboBoot->count() > 0);
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
void BootMan::install()
{
    proc.log(__PRETTY_FUNCTION__, MProcess::LogFunction);
    if (proc.halted()) return;
    proc.advance(4, 4);

    // the old initrd is not valid for this hardware
    proc.shell("ls /mnt/antiX/boot | grep 'initrd.img-3.6'", nullptr, true);
    const QString &val = proc.readOut();
    if (!val.isEmpty()) proc.exec("rm", {"-f", "/mnt/antiX/boot/" + val});

    bool efivars_ismounted = false;
    if (gui.boxBoot->isChecked() && gui.radioBootESP->isChecked()) {
        QString efivars = QStringLiteral("/sys/firmware/efi/efivars");
        if (QFileInfo(efivars).isDir()) {
            efivars_ismounted = proc.exec("mountpoint", {"-q", efivars});
        }
        if (!efivars_ismounted) proc.exec("mount", {"-t", "efivarfs", "efivarfs", efivars});
    }

    if (gui.radioBootESP->isChecked()) mkdir("/mnt/antiX/boot/efi", 0755);

    // set mounts for chroot
    proc.exec("mount", {"--mkdir", "--rbind", "--make-rslave", "/dev", "/mnt/antiX/dev"});
    proc.exec("mount", {"--mkdir", "--rbind", "--make-rslave", "/sys", "/mnt/antiX/sys"});
    proc.exec("mount", {"--mkdir", "--rbind", "/proc", "/mnt/antiX/proc"});
    proc.exec("mount", {"-t", "tmpfs", "--mkdir", "-o", "size=100m,nodev,mode=755", "tmpfs", "/mnt/antiX/run"});
    proc.exec("mount", {"--mkdir", "--rbind", "/run/udev", "/mnt/antiX/run/udev"});

    // Trap exceptions here, and re-throw them once the local cleanup is done.
    const char *failed = nullptr;
    try {
        installMain(efivars_ismounted);
    } catch (const char *msg) {
        failed = msg;
    }

    proc.exec("umount", {"-R", "/mnt/antiX/run"});
    proc.exec("umount", {"-R", "/mnt/antiX/proc"});
    proc.exec("umount", {"-R", "/mnt/antiX/sys"});
    proc.exec("umount", {"-R", "/mnt/antiX/dev"});
    if (proc.exec("mountpoint", {"-q", "/mnt/antiX/boot/efi"})) {
        proc.exec("umount", {"/mnt/antiX/boot/efi"});
    }
    if (failed) throw failed;
}
void BootMan::installMain(bool efivars_ismounted)
{
    MProcess::Section sect(proc, QT_TR_NOOP("GRUB installation failed. You can"
        " reboot to the live medium and use the GRUB Rescue menu to repair the installation."));
    if (gui.boxBoot->isChecked()) {
        proc.status(tr("Installing GRUB"));

        // install new Grub now
        const int efisize = proc.detectEFI(true);
        const QString &bootdev = "/dev/" + gui.comboBoot->currentData().toString();
        if (!gui.radioBootESP->isChecked()) {
            proc.exec("grub-install", {"--target=i386-pc", "--recheck",
                "--no-floppy", "--force", "--boot-directory=/mnt/antiX/boot", bootdev});
        } else {
            proc.exec("mount", {bootdev, "/mnt/antiX/boot/efi"});
            // rename arch to match grub-install target
            proc.exec("cat", {"/sys/firmware/efi/fw_platform_size"}, nullptr, true);

            if (efivars_ismounted) {
                // remove any efivars-dump-entries in NVRAM
                MProcess::Section sect2(proc, nullptr);
                proc.shell("ls /sys/firmware/efi/efivars | grep dump", nullptr, true);
                const QString &dump = proc.readOut();
                sect2.end();
                if (!dump.isEmpty()) proc.shell("rm /sys/firmware/efi/efivars/dump*", nullptr, true);
            }

            MProcess::Section sect2(proc);
            sect2.setRoot("/mnt/antiX");
            proc.exec("grub-install", {"--no-nvram", "--force-extra-removable",
                (efisize==32 ? "--target=i386-efi" : "--target=x86_64-efi"),
                "--efi-directory=/boot/efi", "--bootloader-id=" + loaderID, "--recheck"});

            // Update the boot NVRAM variables. Non-critial step so no need to fail.
            sect2.setExceptionMode(nullptr);
            // Remove old boot variables of the same label.
            proc.exec("efibootmgr", {}, nullptr, true);
            const QStringList &existing = proc.readOutLines();
            const QRegularExpression regex("^Boot([0-9A-F]{4})\\*?\\s(.*)$");
            for (const QString &entry : existing) {
                const QRegularExpressionMatch &match = regex.match(entry);
                if (match.hasMatch() && match.captured(2) == loaderLabel) {
                    proc.exec("efibootmgr", {"-qBb", match.captured(1)});
                }
            }
            // Add a new NVRAM boot variable.
            proc.exec("efibootmgr", {"-qcL", loaderLabel, "-d", bootdev,
                "-l", "/EFI/" + loaderID + (efisize==32 ? "/grubia32.efi" : "/grubx64.efi")});
        }

        //get /etc/default/grub codes
        const QSettings grubSettings("/etc/default/grub", QSettings::NativeFormat);
        const QString &grubDefault = grubSettings.value("GRUB_CMDLINE_LINUX_DEFAULT").toString();
        qDebug() << "grubDefault is " << grubDefault;

        //added non-live boot codes to those in /etc/default/grub, remove duplicates
        proc.shell("/live/bin/non-live-cmdline", nullptr, true); // Get non-live boot codes
        QStringList finalcmdline = proc.readOut().split(" ");
        finalcmdline.append(grubDefault.split(" "));
        qDebug() << "intermediate" << finalcmdline;

        //get built-in config_cmdline
        proc.shell("grep ^CONFIG_CMDLINE= /boot/config-$(uname -r) | cut -d '\"' -f2", nullptr, true);
        const QStringList confcmdline = proc.readOut().split(" ");

        for (const QString &confparameter : confcmdline) {
            finalcmdline.removeAll(confparameter);
        }

        //remove any duplicate codes in list (typically splash)
        finalcmdline.removeDuplicates();

        //remove vga=ask
        finalcmdline.removeAll("vga=ask");

        //remove toram=min and toram=store - is not yet in non-live-cmdline
        finalcmdline.removeAll("toram=min");
        finalcmdline.removeAll("toram=store");

        //remove boot_image code
        finalcmdline.removeAll("BOOT_IMAGE=/antiX/vmlinuz");

        //remove nosplash boot code if configured in installer.conf
        if (removeNoSplash) finalcmdline.removeAll("nosplash");
        //remove in null or empty strings that might have crept in
        finalcmdline.removeAll({});
        qDebug() << "Add cmdline options to Grub" << finalcmdline;

        //convert qstringlist back into normal qstring
        QString finalcmdlinestring = finalcmdline.join(" ");
        qDebug() << "cmdlinestring" << finalcmdlinestring;

        //get qstring boot codes read for sed command
        finalcmdlinestring.replace('\\', "\\\\");
        finalcmdlinestring.replace('|', "\\|");

        //do the replacement in /etc/default/grub
        const QString cmd = "sed -i -r 's|^(GRUB_CMDLINE_LINUX_DEFAULT=).*|\\1\"%1\"|' /mnt/antiX/etc/default/grub";
        proc.shell(cmd.arg(finalcmdlinestring));

        //copy memtest efi files if needed
        if (efisize) {
            QString mtest = QString("/live/to-ram/boot/uefi-mt/mtest-%1.efi").arg(efisize);
            if (!QFileInfo::exists(mtest)) {
                mtest = QString("/live/boot-dev/boot/uefi-mt/mtest-%1.efi").arg(efisize);
                if (!QFileInfo::exists(mtest)) mtest.clear();
            }
            if (!mtest.isEmpty()) {
                proc.mkpath("/mnt/antiX/boot/uefi-mt", 0755);
                proc.exec("cp", {mtest, "/mnt/antiX/boot/uefi-mt"});
            }
        }
        proc.status();

        //update grub with new config
        proc.exec("chroot", {"/mnt/antiX", "update-grub"});

        if (!gui.radioBootESP->isChecked()) {
            /* Prevent debconf pestering the user when GRUB gets updated. */
            proc.exec("udevadm", {"info", bootdev}, nullptr, true);
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
            if (diskpath.isEmpty()) throw sect.failMessage();
            /* Setup debconf to achieve the objective of silence. */
            diskpath.prepend("grub-pc grub-pc/install_devices multiselect /dev/");
            proc.exec("chroot", {"/mnt/antiX", "debconf-set-selections"}, &diskpath);
        }
    }

    proc.status(tr("Updating initramfs"));
    sect.setExceptionMode(QT_TR_NOOP("Failed to update initramfs."));
    //if useing f2fs, then add modules to /etc/initramfs-tools/modules
    //if (rootTypeCombo->currentText() == "f2fs" || homeTypeCombo->currentText() == "f2fs") {
        //proc.shell("grep -q f2fs /mnt/antiX/etc/initramfs-tools/modules || echo f2fs >> /mnt/antiX/etc/initramfs-tools/modules");
        //proc.shell("grep -q crypto-crc32 /mnt/antiX/etc/initramfs-tools/modules || echo crypto-crc32 >> /mnt/antiX/etc/initramfs-tools/modules");
    //}

    // Use MODULES=dep to trim the initrd, often results in faster boot.
    proc.exec("sed", {"-i", "-r", "s/MODULES=.*/MODULES=dep/", "/mnt/antiX/etc/initramfs-tools/initramfs.conf"});

    proc.exec("chroot", {"/mnt/antiX", "update-initramfs", "-u", "-t", "-k", "all"});
    proc.status();
}

/* Slots */

void BootMan::chosenBootMBR() noexcept
{
    gui.comboBoot->clear();
    for (DeviceItemIterator it(partman); DeviceItem *item = *it; it.next()) {
        if (item->type == DeviceItem::Drive && (!item->flags.bootRoot || installFromRootDevice)) {
            if (!item->flags.nasty || brave) item->addToCombo(gui.comboBoot, true);
        }
    }
    selectBootMain();
    gui.labelBoot->setText(tr("System boot disk:"));
}

void BootMan::chosenBootPBR() noexcept
{
    gui.comboBoot->clear();
    for (DeviceItemIterator it(partman); DeviceItem *item = *it; it.next()) {
        if (item->type == DeviceItem::Partition && (!item->flags.bootRoot || installFromRootDevice)) {
            if (item->realUseFor() == "ESP") continue;
            else if (!item->format.compare("SWAP", Qt::CaseInsensitive)) continue;
            else if (item->format == "crypto_LUKS") continue;
            else if (item->format.isEmpty() || item->format == "PRESERVE") {
                if (item->curFormat == "crypto_LUKS") continue;
                if (!item->curFormat.compare("SWAP", Qt::CaseInsensitive)) continue;
            }
            if (!item->flags.nasty || brave) item->addToCombo(gui.comboBoot, true);
        }
    }
    selectBootMain();
    gui.labelBoot->setText(tr("Partition to use:"));
}

void BootMan::chosenBootESP() noexcept
{
    gui.comboBoot->clear();
    for (DeviceItemIterator it(partman); DeviceItem *item = *it; it.next()) {
        if (item->realUseFor() == "ESP" && (!item->flags.bootRoot || installFromRootDevice)) {
            item->addToCombo(gui.comboBoot);
        }
    }
    selectBootMain();
    gui.labelBoot->setText(tr("Partition to use:"));
}
