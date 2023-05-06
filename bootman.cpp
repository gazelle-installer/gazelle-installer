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
    const QSettings &appConf, const QCommandLineParser &appArgs)
    : QObject(ui.boxMain), proc(mproc), gui(ui), partman(pman)
{
    loaderID = appConf.value("PROJECT_SHORTNAME").toString() + appConf.value("VERSION").toString();
    installFromRootDevice = appConf.value("INSTALL_FROM_ROOT_DEVICE").toBool();
    removeNoSplash = appConf.value("REMOVE_NOSPLASH").toBool();
    brave = appArgs.isSet("brave");

    connect(gui.radioBootMBR, &QRadioButton::toggled, this, &BootMan::chosenBootMBR);
    connect(gui.radioBootPBR, &QRadioButton::toggled, this, &BootMan::chosenBootPBR);
    connect(gui.radioBootESP, &QRadioButton::toggled, this, &BootMan::chosenBootESP);
}

void BootMan::manageConfig(MSettings &config)
{
    config.startGroup("GRUB", gui.pageBoot);
    config.manageGroupCheckBox("Install", gui.boxBoot);
    const char *grubChoices[] = {"MBR", "PBR", "ESP"};
    QRadioButton *grubRadios[] = {gui.radioBootMBR, gui.radioBootPBR, gui.radioBootESP};
    config.manageRadios("TargetType", 3, grubChoices, grubRadios);
    config.manageComboBox("Location", gui.comboBoot, true);
    config.endGroup();
}

void BootMan::selectBootMain()
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
void BootMan::buildBootLists()
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
    proc.log(__PRETTY_FUNCTION__, MProcess::Section);
    if (proc.halted()) return;
    proc.advance(4, 4);

    // the old initrd is not valid for this hardware
    proc.shell("/bin/ls /mnt/antiX/boot | grep 'initrd.img-3.6'", nullptr, true);
    const QString &val = proc.readOut();
    if (!val.isEmpty()) proc.exec("/bin/rm", {"-f", "/mnt/antiX/boot/" + val});

    bool efivars_ismounted = false;
    if (gui.boxBoot->isChecked() && gui.radioBootESP->isChecked()) {
        QString efivars = QStringLiteral("/sys/firmware/efi/efivars");
        if (QFileInfo(efivars).isDir()) {
            efivars_ismounted = proc.exec("/bin/mountpoint", {"-q", efivars});
        }
        if (!efivars_ismounted) proc.exec("/bin/mount", {"-t", "efivarfs", "efivarfs", efivars});
    }

    if (gui.radioBootESP->isChecked()) mkdir("/mnt/antiX/boot/efi", 0755);

    // set mounts for chroot
    proc.exec("/bin/mount", {"--rbind", "--make-rslave", "/dev", "/mnt/antiX/dev"});
    proc.exec("/bin/mount", {"--rbind", "--make-rslave", "/sys", "/mnt/antiX/sys"});
    proc.exec("/bin/mount", {"--rbind", "/proc", "/mnt/antiX/proc"});
    proc.exec("/bin/mount", {"-t", "tmpfs", "-o", "size=100m,nodev,mode=755", "tmpfs", "/mnt/antiX/run"});
    proc.exec("/bin/mkdir", {"/mnt/antiX/run/udev"});
    proc.exec("/bin/mount", {"--rbind", "/run/udev", "/mnt/antiX/run/udev"});

    // Trap exceptions here, and re-throw them once the local cleanup is done.
    const char *failed = nullptr;
    try {
        installMain(efivars_ismounted);
    } catch (const char *msg) {
        failed = msg;
    }

    proc.exec("/bin/umount", {"-R", "/mnt/antiX/run"});
    proc.exec("/bin/umount", {"-R", "/mnt/antiX/proc"});
    proc.exec("/bin/umount", {"-R", "/mnt/antiX/sys"});
    proc.exec("/bin/umount", {"-R", "/mnt/antiX/dev"});
    if (proc.exec("mountpoint", {"-q", "/mnt/antiX/boot/efi"})) {
        proc.exec("/bin/umount", {"/mnt/antiX/boot/efi"});
    }
    if (failed) throw failed;
}
void BootMan::installMain(bool efivars_ismounted)
{
    if (gui.boxBoot->isChecked()) {
        proc.status(tr("Installing GRUB"));
        const char *failGrub = QT_TR_NOOP("GRUB installation failed. You can reboot to"
            " the live medium and use the GRUB Rescue menu to repair the installation.");
        proc.setExceptionMode(failGrub);

        // install new Grub now
        QString arch;
        const QString &boot = "/dev/" + gui.comboBoot->currentData().toString();
        if (!gui.radioBootESP->isChecked()) {
            proc.exec("grub-install", {"--target=i386-pc", "--recheck",
                "--no-floppy", "--force", "--boot-directory=/mnt/antiX/boot", boot});
        } else {
            proc.exec("/bin/mount", {boot, "/mnt/antiX/boot/efi"});
            // rename arch to match grub-install target
            proc.exec("cat", {"/sys/firmware/efi/fw_platform_size"}, nullptr, true);
            arch = proc.readOut();
            arch = (arch == "32") ? "i386" : "x86_64";  // fix arch name for 32bit

            if (efivars_ismounted) {
                // remove any efivars-dump-entries in NVRAM
                proc.setExceptionMode(nullptr);
                proc.shell("/bin/ls /sys/firmware/efi/efivars | grep dump", nullptr, true);
                const QString &dump = proc.readOut();
                proc.setExceptionMode(failGrub);
                if (!dump.isEmpty()) proc.shell("/bin/rm /sys/firmware/efi/efivars/dump*", nullptr, true);
            }

            proc.exec("chroot", {"/mnt/antiX", "grub-install", "--force-extra-removable",
                "--target=" + arch + "-efi", "--efi-directory=/boot/efi",
                "--bootloader-id=" + loaderID, "--recheck"});
        }

        //get /etc/default/grub codes
        QSettings grubSettings("/etc/default/grub", QSettings::NativeFormat);
        QString grubDefault=grubSettings.value("GRUB_CMDLINE_LINUX_DEFAULT").toString();
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

        if (proc.detectEFI()) {
            mkdir("/mnt/antiX/boot/uefi-mt", 0755);
            QString mtest;
            QString mtest_dev;
            QString mtest_ram;
            if (arch == "i386") {
                mtest_dev = "/live/boot-dev/boot/uefi-mt/mtest-32.efi";
                mtest_ram = "/live/to-ram/boot/uefi-mt/mtest-32.efi";
            } else {
                mtest_dev = "/live/boot-dev/boot/uefi-mt/mtest-64.efi";
                mtest_ram = "/live/to-ram/boot/uefi-mt/mtest-64.efi";
            }
            if (QFileInfo::exists(mtest_ram)) {
                mtest = mtest_ram;
            } else if (QFileInfo::exists(mtest_dev)) {
                mtest = mtest_dev;
            }
            if (!mtest.isNull()) {
                proc.exec("/bin/cp", {mtest, "/mnt/antiX/boot/uefi-mt"});
            }
        }
        proc.status();

        //update grub with new config

        qDebug() << "Update Grub";
        proc.exec("chroot", {"/mnt/antiX", "update-grub"});
    }

    proc.status(tr("Updating initramfs"));
    proc.setExceptionMode(QT_TR_NOOP("Failed to update initramfs."));
    //if useing f2fs, then add modules to /etc/initramfs-tools/modules
    //if (rootTypeCombo->currentText() == "f2fs" || homeTypeCombo->currentText() == "f2fs") {
        //proc.shell("grep -q f2fs /mnt/antiX/etc/initramfs-tools/modules || echo f2fs >> /mnt/antiX/etc/initramfs-tools/modules");
        //proc.shell("grep -q crypto-crc32 /mnt/antiX/etc/initramfs-tools/modules || echo crypto-crc32 >> /mnt/antiX/etc/initramfs-tools/modules");
    //}

    // Use MODULES=dep to trim the initrd, often results in faster boot.
    proc.exec("sed", {"-i", "-r", "s/MODULES=.*/MODULES=dep/", "/mnt/antiX/etc/initramfs-tools/initramfs.conf"});

    proc.exec("chroot", {"/mnt/antiX", "update-initramfs", "-u", "-t", "-k", "all"});
    proc.status();
    proc.setExceptionMode(nullptr);
}

/* Slots */

void BootMan::chosenBootMBR()
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

void BootMan::chosenBootPBR()
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

void BootMan::chosenBootESP()
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
