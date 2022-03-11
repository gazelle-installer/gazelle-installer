/***************************************************************************
 * Boot manager (GRUB) setup for the installer.
 ***************************************************************************
 *
 *   Copyright (C) 2022 by AK-47, along with transplanted code:
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
#include "bootman.h"

BootMan::BootMan(MProcess &mproc, PartMan &pman, Ui::MeInstall &ui,
    const QSettings &appConf, const QCommandLineParser &appArgs)
    : QObject(ui.boxMain), proc(mproc), gui(ui), partman(pman)
{
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
bool BootMan::install(const QString &loaderID)
{
    proc.log(__PRETTY_FUNCTION__);
    if (proc.halted()) return false;
    proc.advance(4, 4);

    // the old initrd is not valid for this hardware
    proc.shell("/bin/ls /mnt/antiX/boot | grep 'initrd.img-3.6'", nullptr, true);
    const QString &val = proc.readOut();
    if (!val.isEmpty()) proc.exec("/bin/rm", {"-f", "/mnt/antiX/boot/" + val});

    bool efivarfs = QFileInfo("/sys/firmware/efi/efivars").isDir();
    bool efivarfs_mounted = false;
    if (efivarfs) {
        QFile file("/proc/self/mounts");
        if (file.open(QFile::ReadOnly | QFile::Text)) {
            while (!file.atEnd() && !efivarfs_mounted) {
                if (file.readLine().startsWith("efivarfs")) efivarfs_mounted = true;
            }
            file.close();
        }
    }
    if (efivarfs && !efivarfs_mounted) {
        proc.exec("/bin/mount", {"-t", "efivarfs", "efivarfs", "/sys/firmware/efi/efivars"});
    }

    if (!gui.boxBoot->isChecked()) {
        // skip it
        proc.status(tr("Updating initramfs"));
        //if useing f2fs, then add modules to /etc/initramfs-tools/modules
        qDebug() << "Update initramfs";
        //if (rootTypeCombo->currentText() == "f2fs" || homeTypeCombo->currentText() == "f2fs") {
            //proc.shell("grep -q f2fs /mnt/antiX/etc/initramfs-tools/modules || echo f2fs >> /mnt/antiX/etc/initramfs-tools/modules");
            //proc.shell("grep -q crypto-crc32 /mnt/antiX/etc/initramfs-tools/modules || echo crypto-crc32 >> /mnt/antiX/etc/initramfs-tools/modules");
        //}
        return proc.exec("chroot", {"/mnt/antiX", "update-initramfs", "-u", "-t", "-k", "all"});
    }

    proc.status(tr("Installing GRUB"));

    // set mounts for chroot
    proc.exec("/bin/mount", {"--rbind", "--make-rslave", "/dev", "/mnt/antiX/dev"});
    proc.exec("/bin/mount", {"--rbind", "--make-rslave", "/sys", "/mnt/antiX/sys"});
    proc.exec("/bin/mount", {"--rbind", "/proc", "/mnt/antiX/proc"});
    proc.exec("/bin/mount", {"-t", "tmpfs", "-o", "size=100m,nodev,mode=755", "tmpfs", "/mnt/antiX/run"});
    proc.exec("/bin/mkdir", {"/mnt/antiX/run/udev"});
    proc.exec("/bin/mount", {"--rbind", "/run/udev", "/mnt/antiX/run/udev"});

    // install new Grub now
    bool isOK = false;
    QString arch;
    const QString &boot = "/dev/" + gui.comboBoot->currentData().toString();
    if (!gui.radioBootESP->isChecked()) {
        isOK = proc.exec("grub-install", {"--target=i386-pc", "--recheck",
            "--no-floppy", "--force", "--boot-directory=/mnt/antiX/boot", boot});
    } else {
        mkdir("/mnt/antiX/boot/efi", 0755);
        proc.exec("/bin/mount", {boot, "/mnt/antiX/boot/efi"});
        // rename arch to match grub-install target
        proc.exec("cat", {"/sys/firmware/efi/fw_platform_size"}, nullptr, true);
        arch = proc.readOut();
        arch = (arch == "32") ? "i386" : "x86_64";  // fix arch name for 32bit

        isOK = proc.exec("chroot", {"/mnt/antiX", "grub-install", "--force-extra-removable",
            "--target=" + arch + "-efi", "--efi-directory=/boot/efi",
            "--bootloader-id=" + loaderID, "--recheck"});
    }
    if (!isOK) {
        QMessageBox::critical(master, master->windowTitle(), tr("GRUB installation failed."
            " You can reboot to the live medium and use the GRUB Rescue menu to repair the installation."));
        proc.exec("/bin/umount", {"-R", "/mnt/antiX/run"});
        proc.exec("/bin/umount", {"-R", "/mnt/antiX/proc"});
        proc.exec("/bin/umount", {"-R", "/mnt/antiX/sys"});
        proc.exec("/bin/umount", {"-R", "/mnt/antiX/dev"});
        if (proc.exec("mountpoint", {"-q", "/mnt/antiX/boot/efi"})) {
            proc.exec("/bin/umount", {"/mnt/antiX/boot/efi"});
        }
        return false;
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

    //remove any duplicate codes in list (typically splash)
    finalcmdline.removeDuplicates();

    //remove vga=ask
    finalcmdline.removeAll("vga=ask");

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
    qDebug() << "Add cmdline options to Grub";
    const QString cmd = "sed -i -r 's|^(GRUB_CMDLINE_LINUX_DEFAULT=).*|\\1\"%1\"|' /mnt/antiX/etc/default/grub";
    proc.shell(cmd.arg(finalcmdlinestring));

    //copy memtest efi files if needed

    if (proc.detectEFI()) {
        mkdir("/mnt/antiX/boot/uefi-mt", 0755);
        if (arch == "i386") {
            proc.exec("/bin/cp", {"/live/boot-dev/boot/uefi-mt/mtest-32.efi", "/mnt/antiX/boot/uefi-mt"});
        } else {
            proc.exec("/bin/cp", {"/live/boot-dev/boot/uefi-mt/mtest-64.efi", "/mnt/antiX/boot/uefi-mt"});
        }
    }
    proc.status();

    //update grub with new config

    qDebug() << "Update Grub";
    proc.exec("chroot", {"/mnt/antiX", "update-grub"});

    proc.status(tr("Updating initramfs"));
    //if useing f2fs, then add modules to /etc/initramfs-tools/modules
    //if (rootTypeCombo->currentText() == "f2fs" || homeTypeCombo->currentText() == "f2fs") {
        //proc.shell("grep -q f2fs /mnt/antiX/etc/initramfs-tools/modules || echo f2fs >> /mnt/antiX/etc/initramfs-tools/modules");
        //proc.shell("grep -q crypto-crc32 /mnt/antiX/etc/initramfs-tools/modules || echo crypto-crc32 >> /mnt/antiX/etc/initramfs-tools/modules");
    //}
    proc.exec("chroot", {"/mnt/antiX", "update-initramfs", "-u", "-t", "-k", "all"});
    proc.status();
    qDebug() << "clear chroot env";
    proc.exec("/bin/umount", {"-R", "/mnt/antiX/run"});
    proc.exec("/bin/umount", {"-R", "/mnt/antiX/proc"});
    proc.exec("/bin/umount", {"-R", "/mnt/antiX/sys"});
    proc.exec("/bin/umount", {"-R", "/mnt/antiX/dev"});
    if (proc.exec("mountpoint", {"-q", "/mnt/antiX/boot/efi"})) {
        proc.exec("/bin/umount", {"/mnt/antiX/boot/efi"});
    }
    return true;
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
