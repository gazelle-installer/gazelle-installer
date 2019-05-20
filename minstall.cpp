//
//  Copyright (C) 2003-2010 by Warren Woodford
//  Heavily edited, with permision, by anticapitalista for antiX 2011-2014.
//  Heavily revised by dolphin oracle, adrian, and anticaptialista 2018.
//  additional mount and compression oftions for btrfs by rob 2018
//  Major GUI update and user experience improvements by AK-47 2019.
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

#include <QDebug>
#include <QFileInfo>
#include <QtConcurrent/QtConcurrent>
#include <fcntl.h>
#include <sys/statvfs.h>
#include <sys/stat.h>

#include "minstall.h"
#include "mmain.h"
#include "cmd.h"

int MInstall::command(const QString &cmd)
{
    qDebug() << cmd;
    return system(cmd.toUtf8());
}

// helping function that runs a bash command in an event loop
int MInstall::runCmd(const QString &cmd)
{
    if (phase < 0) return EXIT_FAILURE;
    QEventLoop loop;
    QFutureWatcher<int> futureWatcher;
    QFuture<int> future;
    future = QtConcurrent::run(command, cmd);
    futureWatcher.setFuture(future);
    connect(&futureWatcher, SIGNAL(finished()), &loop, SLOT(quit()));
    loop.exec();
    qDebug() << "Exit code: " << future.result();
    return future.result();
}

// shell.run() doesn't distinguish between crashed and failed processes
bool MInstall::runProc(const QString &cmd, const QByteArray &input)
{
    if (phase < 0) return false;
    qDebug() << cmd;
    QEventLoop eloop;
    connect(proc, static_cast<void (QProcess::*)(int)>(&QProcess::finished), &eloop, &QEventLoop::quit);
    proc->start(cmd);
    if (!input.isEmpty()) {
        proc->write(input);
        proc->closeWriteChannel();
    }
    eloop.exec();
    disconnect(proc, SIGNAL(finished(int, QProcess::ExitStatus)), 0, 0);
    qDebug() << "Exit code: " << proc->exitCode() << ", status: " << proc->exitStatus();
    return (proc->exitStatus() == QProcess::NormalExit && proc->exitCode() == 0);
}

MInstall::MInstall(QWidget *parent, QStringList args) :
    QWidget(parent)
{
    setupUi(this);

    this->installEventFilter(this);
    this->args = args;
    installBox->hide();

    pretend = (args.contains("--pretend") || args.contains("-p"));
    // setup system variables
    QSettings settings("/usr/share/gazelle-installer-data/installer.conf", QSettings::NativeFormat);
    PROJECTNAME=settings.value("PROJECT_NAME").toString();
    PROJECTSHORTNAME=settings.value("PROJECT_SHORTNAME").toString();
    PROJECTVERSION=settings.value("VERSION").toString();
    PROJECTURL=settings.value("PROJECT_URL").toString();
    PROJECTFORUM=settings.value("FORUM_URL").toString();
    INSTALL_FROM_ROOT_DEVICE=settings.value("INSTALL_FROM_ROOT_DEVICE").toBool();
    MIN_ROOT_DEVICE_SIZE=settings.value("MIN_ROOT_DRIVE_SIZE").toString();
    MIN_BOOT_DEVICE_SIZE=settings.value("MIN_BOOT_DRIVE_SIZE", "256").toString();
    DEFAULT_HOSTNAME=settings.value("DEFAULT_HOSTNAME").toString();
    ENABLE_SERVICES=settings.value("ENABLE_SERVICES").toStringList();
    POPULATE_MEDIA_MOUNTPOINTS=settings.value("POPULATE_MEDIA_MOUNTPOINTS").toBool();
    MIN_INSTALL_SIZE=settings.value("MIN_INSTALL_SIZE").toString();
    PREFERRED_MIN_INSTALL_SIZE=settings.value("PREFERRED_MIN_INSTALL_SIZE").toString();
    REMOVE_NOSPLASH=settings.value("REMOVE_NOSPLASH", "false").toBool();

    // save config
    config = new QSettings(PROJECTNAME, "minstall", this);

    // set default host name

    computerNameEdit->setText(DEFAULT_HOSTNAME);

    // set some distro-centric text

    copyrightBrowser->setPlainText(tr("%1 is an independent Linux distribution based on Debian Stable.\n\n%1 uses some components from MEPIS Linux which are released under an Apache free license. Some MEPIS components have been modified for %1.\n\nEnjoy using %1").arg(PROJECTNAME));
    remindersBrowser->setPlainText(tr("Support %1\n\n%1 is supported by people like you. Some help others at the support forum - %2, or translate help files into different languages, or make suggestions, write documentation, or help test new software.").arg(PROJECTNAME).arg(PROJECTFORUM));

    // advanced FDE page defaults

    loadAdvancedFDE();

    setupkeyboardbutton();

    proc = new QProcess(this);
    timer = new QTimer(this);

    rootLabelEdit->setText("root" + PROJECTSHORTNAME + PROJECTVERSION);
    homeLabelEdit->setText("home" + PROJECTSHORTNAME);
    swapLabelEdit->setText("swap" + PROJECTSHORTNAME);
}

MInstall::~MInstall() {
}

/////////////////////////////////////////////////////////////////////////
// util functions

// Custom sleep
void MInstall::csleep(int msec)
{
    QTimer cstimer(this);
    QEventLoop eloop;
    connect(&cstimer, &QTimer::timeout, &eloop, &QEventLoop::quit);
    cstimer.start(msec);
    eloop.exec();
}

QString MInstall::getCmdOut(const QString &cmd)
{
    return shell.getOutput(cmd).section("\n", 0, 0);
}

QStringList MInstall::getCmdOuts(const QString &cmd)
{
    return shell.getOutput(cmd).split('\n');
}

// Check if running inside VirtualBox
bool MInstall::isInsideVB()
{
    return (shell.run("lspci -d 80ee:beef  | grep -q .") == 0);
}

bool MInstall::replaceStringInFile(const QString &oldtext, const QString &newtext, const QString &filepath)
{

    QString cmd = QString("sed -i 's/%1/%2/g' %3").arg(oldtext, newtext, filepath);
    if (shell.run(cmd) != 0) {
        return false;
    }
    return true;
}

void MInstall::updateCursor(const Qt::CursorShape shape)
{
    if (shape != Qt::ArrowCursor) {
        qApp->setOverrideCursor(QCursor(shape));
    } else {
        while (qApp->overrideCursor() != NULL) {
            qApp->restoreOverrideCursor();
        }
    }
    qApp->processEvents();
}

void MInstall::updateStatus(const QString &msg, int val)
{
    progressBar->setFormat("%p% - " + msg.toUtf8());
    if (val < 0) val = progressBar->value() + 1;
    progressBar->setValue(val);
    qApp->processEvents();
}

bool MInstall::pretendToInstall(int start, int stop, int sleep)
{
    for (int ixi = start; ixi <= stop; ++ixi) {
        updateStatus(tr("Pretending to install %1").arg(PROJECTNAME), ixi);
        csleep(sleep);
        if (phase < 0) {
            csleep(1000);
            return false;
        }
    }
    return true;
}

// write out crypttab if encrypting for auto-opening
void MInstall::writeKeyFile()
{
    if (phase < 0) return;
    // create keyfile
    // add key file to luks containers
    // get uuid of devices
    // write out crypttab
    // if encrypt-auto, add home and swap
    // if encrypt just home, just add swap
    // blkid -s UUID -o value devicename for UUID
    // containerName     /dev/disk/by-uuid/UUID_OF_PARTITION  /root/keyfile  luks >>/etc/crypttab
    // for auto install, only need to add swap
    // for root and home, need to add home and swap
    // for root only, add swap
    // for home only, add swap

    QString rngfile = "/dev/" + comboFDErandom->currentText();
    const unsigned int keylength = 4096;
    QString password = (checkBoxEncryptAuto->isChecked()) ? FDEpassword->text() : FDEpassCust->text();
    if (isRootEncrypted) { // if encrypting root
        bool newkey = (key.length() == 0);

        //create keyfile
        if (newkey) key.load(rngfile.toUtf8(), keylength);
        key.save("/mnt/antiX/root/keyfile", 0400);

        //add keyfile to container
        QString swapUUID;
        if (swapDevicePreserve != "/dev/none") {
            swapUUID = getCmdOut("blkid -s UUID -o value " + swapDevicePreserve);

            runProc("cryptsetup luksAddKey " + swapDevicePreserve + " /mnt/antiX/root/keyfile",
                    password.toUtf8() + "\n");
        }

        if (isHomeEncrypted && newkey) { // if encrypting separate /home
            runProc("cryptsetup luksAddKey " + homeDevicePreserve + " /mnt/antiX/root/keyfile",
                    password.toUtf8() + "\n");
        }
        QString rootUUID = getCmdOut("blkid -s UUID -o value " + rootDevicePreserve);
        //write crypttab keyfile entry
        QFile file("/mnt/antiX/etc/crypttab");
        if (file.open(QIODevice::WriteOnly)) {
            QTextStream out(&file);
            out << "rootfs /dev/disk/by-uuid/" + rootUUID +" none luks \n";
            if (isHomeEncrypted) {
                QString homeUUID =  getCmdOut("blkid -s UUID -o value " + homeDevicePreserve);
                out << "homefs /dev/disk/by-uuid/" + homeUUID +" /root/keyfile luks \n";
            }
            if (swapDevicePreserve != "/dev/none") {
                out << "swapfs /dev/disk/by-uuid/" + swapUUID +" /root/keyfile luks,nofail \n";
            }
        }
        file.close();
    } else if (isHomeEncrypted) { // if encrypting /home without encrypting root
        QString swapUUID;
        if (swapDevicePreserve != "/dev/none") {
            //create keyfile
            key.load(rngfile.toUtf8(), keylength);
            key.save("/mnt/antiX/home/.keyfileDONOTdelete", 0400);
            key.erase();

            //add keyfile to container
            swapUUID = getCmdOut("blkid -s UUID -o value " + swapDevicePreserve);

            runProc("cryptsetup luksAddKey " + swapDevicePreserve + " /mnt/antiX/home/.keyfileDONOTdelete",
                    password.toUtf8() + "\n");
        }
        QString homeUUID = getCmdOut("blkid -s UUID -o value " + homeDevicePreserve);
        //write crypttab keyfile entry
        QFile file("/mnt/antiX/etc/crypttab");
        if (file.open(QIODevice::WriteOnly)) {
            QTextStream out(&file);
            out << "homefs /dev/disk/by-uuid/" + homeUUID +" none luks \n";
            if (swapDevicePreserve != "/dev/none") {
                out << "swapfs /dev/disk/by-uuid/" + swapUUID +" /home/.keyfileDONOTdelete luks,nofail \n";
                shell.run("sed -i 's/^CRYPTDISKS_MOUNT.*$/CRYPTDISKS_MOUNT=\"\\/home\"/' /mnt/antiX/etc/default/cryptdisks");
            }
        }
        file.close();
    }
}

// disable hibernate when using encrypted swap
void MInstall::disablehiberanteinitramfs()
{
    if (phase < 0) return;
    QString cmd;
    if (isSwapEncrypted) {
        cmd = "touch /mnt/antiX/initramfs-tools/conf.d/resume";
        shell.run(cmd);
        QFile file("/mnt/antiX/etc/initramfs-tools/conf.d/resume");
        if (file.open(QIODevice::WriteOnly)) {
            QTextStream out(&file);
            out << "RESUME=none";
        }
        file.close();
    }
}

bool MInstall::mountPartition(const QString dev, const QString point, const QString mntops)
{
    qDebug() << "+++" << __PRETTY_FUNCTION__ << "+++";
    if (phase < 0) return -1;
    mkdir(point.toUtf8(), 0755);
    QString cmd = QString("/bin/mount %1 %2 -o %3").arg(dev).arg(point).arg(mntops);

    if (shell.run(cmd) != 0) {
        return false;
    }
    return true;
}

// checks SMART status of the selected disk, returs false if it detects errors and user chooses to abort
bool MInstall::checkDisk()
{
    qDebug() << "+++" << __PRETTY_FUNCTION__ << "+++";
    if (phase < 0) return false;
    QString msg;
    int ans;
    QString output;

    QString drv = "/dev/" + diskCombo->currentText().section(" ", 0, 0);
    output = getCmdOut("smartctl -H " + drv + "|grep -w FAILED");
    if (output.contains("FAILED")) {
        msg = output + tr("\n\nThe disk with the partition you selected for installation is failing.\n\n") +
                tr("You are strongly advised to abort.\n") +
                tr("If unsure, please exit the Installer and run GSmartControl for more information.\n\n") +
                tr("Do you want to abort the installation?");
        ans = QMessageBox::critical(this, QString::null, msg,
                                    tr("Yes"), tr("No"));
        if (ans == 0) {
            return false;
        }
    }
    else {
        output = getCmdOut("smartctl -A " + drv + "| grep -E \"^  5|^196|^197|^198\" | awk '{ if ( $10 != 0 ) { print $1,$2,$10} }'");
        if (output != "") {
            msg = tr("Smartmon tool output:\n\n") + output + "\n\n" +
                    tr("The disk with the partition you selected for installation passes the S.M.A.R.T. monitor test (smartctl)\n") +
                    tr("but the tests indicate it will have a higher than average failure rate in the upcoming year.\n") +
                    tr("If unsure, please exit the Installer and run GSmartControl for more information.\n\n") +
                    tr("Do you want to continue?");
            ans = QMessageBox::warning(this, QString::null, msg,
                                       tr("Yes"), tr("No"));
            if (ans != 0) {
                return false;
            }
        }
    }
    return true;
}

// check password length (maybe complexity)
bool MInstall::checkPassword(const QString &pass)
{
    if (pass.length() < 8) {
        QMessageBox::critical(this, QString::null,
                              tr("The password needs to be at least\n"
                                 "%1 characters long. Please select\n"
                                 "a longer password before proceeding.").arg("8"));
        return false;
    }
    return true;
}

// process the next phase of installation if possible
bool MInstall::processNextPhase()
{
    // Phase < 0 = install has been aborted (Phase -2 on close)
    if (phase < 0) return false;
    // Phase 0 = install not started yet, Phase 1 = install in progress
    // Phase 2 = waiting for operator input, Phase 3 = post-install steps
    if (phase == 0) { // no install started yet
        updateStatus(tr("Preparing to install %1").arg(PROJECTNAME), 0);
        if (!checkDisk()) return false;
        phase = 1; // installation.
        prepareToInstall();

        // these parameters are passed by reference and modified by make*Partitions()
        bool formatBoot = false;
        QByteArray encPass;
        QString rootType, homeType;

        if (!pretend) {
            if (entireDiskButton->isChecked()) {
                rootType = "ext4";
                encPass = FDEpassword->text().toUtf8();
                if (!makeDefaultPartitions(formatBoot)) {
                    // failed
                    failUI(tr("Failed to create required partitions.\nReturning to Step 1."));
                    return false;
                }
            } else {
                encPass = FDEpassCust->text().toUtf8();
                if (!makeChosenPartitions(rootType, homeType, formatBoot)) {
                    // failed
                    failUI(tr("Failed to prepare chosen partitions.\nReturning to Step 1."));
                    return false;
                }
            }
        }

        // allow the user to enter other options
        runProc("/sbin/partprobe");
        buildBootLists();
        gotoPage(5);
        iCopyBarB = grubEspButton->isChecked() ? 93 : 94;

        if (!pretend) {
            if (!formatPartitions(encPass, rootType, homeType, formatBoot)) {
                failUI(tr("Failed to format required partitions."));
                return false;
            }
            //run blkid -c /dev/null to freshen UUID cache
            runCmd("blkid -c /dev/null");
            if (!installLinux()) return false;
        } else if (!pretendToInstall(1, iCopyBarB, 100)) {
            return false;
        }
        if (!haveSysConfig) {
            progressBar->setEnabled(false);
            updateStatus(tr("Paused for required operator input"), iCopyBarB + 1);
            QApplication::beep();
            if(widgetStack->currentIndex() == 4) {
                on_nextButton_clicked();
            }
        }
        phase = 2;
    }
    if (phase == 2 && haveSysConfig) {
        phase = 3;
        progressBar->setEnabled(true);
        backButton->setEnabled(false);
        if (!pretend) {
            updateStatus(tr("Setting system configuration"), iCopyBarB + 1);
            setServices();
            if (!setComputerName()) return false;
            setLocale();
            if (haveSnapshotUserAccounts) { // skip user account creation
                QString cmd = "rsync -a /home/ /mnt/antiX/home/ --exclude '.cache' --exclude '.gvfs' --exclude '.dbus' --exclude '.Xauthority' --exclude '.ICEauthority'";
                shell.run(cmd);
            } else {
                if (!setUserInfo()) return false;
            }
            saveConfig();
            runProc("/bin/sync"); // the sync(2) system call will block the GUI
            if (!installLoader()) return false;
        } else if (!pretendToInstall(iCopyBarB + 1, 99, 1000)){
            return false;
        }
        phase = 4;
        updateStatus(tr("Installation successful"), 100);
        csleep(1000);
        gotoPage(10);
    }
    return true;
}

// gather required information and prepare installation
void MInstall::prepareToInstall()
{
    qDebug() << "+++" << __PRETTY_FUNCTION__ << "+++";
    if (phase < 0) return;

    if (!pretend) {
        // unmount /boot/efi if mounted by previous run
        if (shell.run("mountpoint -q /mnt/antiX/boot/efi") == 0) {
            shell.run("umount /mnt/antiX/boot/efi");
        }
        // unmount /home if it exists
        shell.run("/bin/umount -l /mnt/antiX/home >/dev/null 2>&1");
        shell.run("/bin/umount -l /mnt/antiX >/dev/null 2>&1");
        // close LUKS containers
        runProc("cryptsetup luksClose /dev/mapper/rootfs");
        runProc("cryptsetup luksClose /dev/mapper/swapfs");
        runProc("cryptsetup luksClose /dev/mapper/homefs");
    }

    isRootFormatted = false;
    isHomeFormatted = false;

    // if it looks like an apple...
    if (shell.run("grub-probe -d /dev/sda2 2>/dev/null | grep hfsplus") == 0) {
        grubPbrButton->setChecked(true);
        grubMbrButton->setEnabled(false);
        localClockCheckBox->setChecked(true);
    } else if (grubMbrButton->isEnabled()){
        grubMbrButton->setChecked(true);
    }
    checkUefi();

    // timezone
    timezoneCombo->clear();
    QFile file("/usr/share/zoneinfo/zone.tab");
    if (file.open(QFile::ReadOnly | QFile::Text)) {
        while(!file.atEnd()) {
            const QString line(file.readLine().trimmed());
            if(!line.startsWith('#')) {
                timezoneCombo->addItem(line.section("\t", 2, 2));
            }
        }
        file.close();
    }
    timezoneCombo->model()->sort(0);
    file.setFileName("/etc/timezone");
    if (file.open(QFile::ReadOnly | QFile::Text)) {
        const QString line(file.readLine().trimmed());
        timezoneCombo->setCurrentIndex(timezoneCombo->findText(line));
        file.close();
    }

    // locale
    localeCombo->clear();
    QStringList loclist = getCmdOuts("locale -a | grep -Ev '^(C|POSIX)\\.?' | grep -E 'utf8|UTF-8'");
    for (QString &strloc : loclist) {
        strloc.replace("utf8", "UTF-8", Qt::CaseInsensitive);
        QLocale loc(strloc);
        localeCombo->addItem(loc.nativeCountryName() + " - " + loc.nativeLanguageName(), QVariant(strloc));
    }
    localeCombo->model()->sort(0);
    file.setFileName("/etc/default/locale");
    QString locale;
    if (file.open(QFile::ReadOnly | QFile::Text)) {
        while (!file.atEnd()) {
            const QString line(file.readLine());
            if(line.startsWith("LANG")) {
                locale = line.section('=', 1).trimmed();
            }
        }
        file.close();
    }
    int iloc = localeCombo->findData(QVariant(locale));
    if (iloc == -1) {
        // set to British English to show that the system locale was probably not picked up
        iloc = localeCombo->findData(QVariant("en_GB.UTF-8"));
    }
    localeCombo->setCurrentIndex(iloc);

    // clock 24/12 default
    if (locale == "en_US.UTF-8" || locale == "ar_EG.UTF-8" || locale == "el_GR.UTF-8" || locale == "sq_AL.UTF-8") {
        radio12h->setChecked(true);
    }

    // Detect snapshot-backup account(s)
    // test if there's another user than demo in /home, if exists, copy the /home and skip to next step, also skip account setup if demo is present on squashfs
    if (shell.run("ls /home | grep -v lost+found | grep -v demo | grep -v snapshot | grep -q [a-zA-Z0-9]") == 0 || shell.run("test -d /live/linux/home/demo") == 0) {
        haveSnapshotUserAccounts = true;
    }

    //check for samba
    QFileInfo info("/etc/init.d/smbd");
    if (!info.exists()) {
        computerGroupLabel->setEnabled(false);
        computerGroupEdit->setEnabled(false);
        computerGroupEdit->setText("");
        sambaCheckBox->setChecked(false);
        sambaCheckBox->setEnabled(false);
    }
    // check for the Samba server
    QString val = getCmdOut("dpkg -s samba | grep '^Status.*ok.*' | sed -e 's/.*ok //'");
    haveSamba = (val.compare("installed") == 0);

    buildServiceList();
}

void MInstall::addItemCombo(QComboBox *cb, const QString *part)
{
    // determine which hash to update and exit if called on same combo
    QHash<QString, int> *removedHash;
    if (cb == homeCombo) {
        if (part == prevItemHome) { // return if called to add item on the same combo
            return;
        }
        removedHash = &removedHome;
    } else if (cb == swapCombo) {
        if (part == prevItemSwap) { // return if called to add item on the same combo
            return;
        }
        removedHash = &removedSwap;
    } else if (cb == bootCombo) {
        if (part == prevItemBoot) { // return if called to add item on the same combo
            return;
        }
        removedHash = &removedBoot;
    } else { // root
        if (part == prevItemRoot) { // return if called to add item on the same combo
            return;
        }
        removedHash = &removedRoot;
    }
    // remove item from combo and update hash
    if (removedHash->contains(*part)) {
        cb->insertItem(removedHash->value(*part), *part);  //index, item
        removedHash->remove(*part); // clear removed hash
    }
}

void MInstall::removeItemCombo(QComboBox *cb, const QString *part)
{
    // determine which hash to update and exit if called on same combo
    QHash<QString, int> *removedHash;
    if (cb == homeCombo) {
        if (part == prevItemHome) { // return if called to add item on the same combo
            return;
        }
        removedHash = &removedHome;
    } else if (cb == swapCombo) {
        if (part == prevItemSwap) { // return if called to add item on the same combo
            return;
        }
        removedHash = &removedSwap;
    } else if (cb == bootCombo) {
        if (part == prevItemBoot) { // return if called to add item on the same combo
            return;
        }
        removedHash = &removedBoot;
    } else { // root
        if (part == prevItemRoot) { // return if called to add item on the same combo
            return;
        }
        removedHash = &removedRoot;
    }

    // find and remove item
    int index = cb->findText(part->section(" ", 0, 0), Qt::MatchStartsWith);
    if (index != -1) {
        cb->removeItem(index);
        removedHash->insert(*part, index);
    } else {
        return;
    }
}

// save configuration
void MInstall::saveConfig()
{
    if (phase < 0) return;
    // Disk step
    config->setValue("Disk/Disk", diskCombo->currentText().section(" ", 0, 0));
    config->setValue("Disk/Encrypted", checkBoxEncryptAuto->isChecked());
    config->setValue("Disk/EntireDisk", entireDiskButton->isChecked());
    // Partition step
    config->setValue("Partition/Root", rootCombo->currentText().section(" ", 0, 0));
    config->setValue("Partition/Home", homeCombo->currentText().section(" ", 0, 0));
    config->setValue("Partition/Swap", swapCombo->currentText().section(" ", 0, 0));
    config->setValue("Partition/Boot", bootCombo->currentText().section(" ", 0, 0));

    config->setValue("Partition/RootType", rootTypeCombo->currentText());
    config->setValue("Partition/HomeType", homeTypeCombo->currentText());

    config->setValue("Partition/RootEncrypt", checkBoxEncryptRoot->isChecked());
    config->setValue("Partition/RootEncrypt", checkBoxEncryptHome->isChecked());
    config->setValue("Partition/RootEncrypt", checkBoxEncryptSwap->isChecked());

    config->setValue("Partition/RootLabel", rootLabelEdit->text());
    config->setValue("Partition/HomeLabel", homeLabelEdit->text());
    config->setValue("Partition/SwapLabel", swapLabelEdit->text());

    config->setValue("Partition/SaveHome", saveHomeCheck->isChecked());
    config->setValue("Partition/BadBlocksCheck", badblocksCheck->isChecked());
    // AES step
    config->setValue("Encryption/Cipher", comboFDEcipher->currentText());
    config->setValue("Encryption/ChainMode", comboFDEchain->currentText());
    config->setValue("Encryption/IVgenerator", comboFDEivgen->currentText());
    config->setValue("Encryption/IVhash", comboFDEivhash->currentData().toString());
    config->setValue("Encryption/KeySize", spinFDEkeysize->cleanText());
    config->setValue("Encryption/LUKSkeyHash", comboFDEhash->currentText().toLower().remove('-'));
    config->setValue("Encryption/KernelRNG", comboFDErandom->currentText());
    config->setValue("Encryption/KDFroundTime", spinFDEroundtime->cleanText());
    // GRUB step
    if(grubCheckBox->isChecked()) {
        const char *cfgGrubInstall;
        if(grubMbrButton->isChecked()) cfgGrubInstall = "MBR";
        if(grubPbrButton->isChecked()) cfgGrubInstall = "PBR";
        if(grubEspButton->isChecked()) cfgGrubInstall = "ESP";
        config->setValue("GRUB/InstallGRUB", cfgGrubInstall);
        config->setValue("GRUB/GrubLocation", grubBootCombo->currentText().section(" ", 0, 0));
    } else {
        config->setValue("GRUB/InstallGRUB", false);
    }
    // Services step
    QTreeWidgetItemIterator it(csView, QTreeWidgetItemIterator::Checked);
    while (*it) {
        if ((*it)->parent() != NULL) {
            config->setValue("Services/" + (*it)->text(0), true);
        }
        ++it;
    }
    // Network step
    config->setValue("Network/ComputerName", computerNameEdit->text());
    config->setValue("Network/Domain", computerDomainEdit->text());
    config->setValue("Network/Workgroup", computerGroupEdit->text());
    config->setValue("Network/Samba", sambaCheckBox->isChecked());
    // Localization step
    config->setValue("Localization/Locale", localeCombo->currentData().toString());
    config->setValue("Localization/LocalClock", localClockCheckBox->isChecked());
    config->setValue("Localization/Clock24h", radio24h->isChecked());
    config->setValue("Localization/Timezone", timezoneCombo->currentText());
    // User Accounts step
    config->setValue("User/Username", userNameEdit->text());
    config->setValue("User/Autologin", autologinCheckBox->isChecked());
    config->setValue("User/SaveDesktop", saveDesktopCheckBox->isChecked());
    // copy config file to installed system
    shell.run("cp \"" + config->fileName() + "\" /mnt/antiX/var/log");
}

// update partition combos
void MInstall::updatePartCombo(QString *prevItem, const QString &part)
{
    // check if prev item selected is different or the same
    if (*prevItem == part) { // same: do nothing
        return;
    } else {
        addItemCombo(rootCombo, prevItem);
        addItemCombo(homeCombo, prevItem);
        addItemCombo(swapCombo, prevItem);
        addItemCombo(bootCombo, prevItem);
        if (part.isEmpty() || part == "root" || part == "none") {
            prevItem->clear();
        } else { // remove items from combos
            *prevItem = part; // update selection
            removeItemCombo(rootCombo, prevItem);
            removeItemCombo(homeCombo, prevItem);
            removeItemCombo(swapCombo, prevItem);
            removeItemCombo(bootCombo, prevItem);
        }
    }
}

void MInstall::loadAdvancedFDE()
{
    if (indexFDEcipher >= 0) comboFDEcipher->setCurrentIndex(indexFDEcipher);
    on_comboFDEcipher_currentIndexChanged(comboFDEcipher->currentText());
    if (indexFDEchain >= 0) comboFDEchain->setCurrentIndex(indexFDEchain);
    on_comboFDEchain_currentIndexChanged(comboFDEchain->currentText());
    if (indexFDEivgen >= 0) comboFDEivgen->setCurrentIndex(indexFDEivgen);
    on_comboFDEivgen_currentIndexChanged(comboFDEivgen->currentText());
    if (indexFDEivhash >= 0) comboFDEivhash->setCurrentIndex(indexFDEivhash);
    if (iFDEkeysize >= 0) spinFDEkeysize->setValue(iFDEkeysize);
    if (indexFDEhash >= 0) comboFDEhash->setCurrentIndex(indexFDEhash);
    if (indexFDErandom >= 0) comboFDErandom->setCurrentIndex(indexFDErandom);
    if (iFDEroundtime >= 0) spinFDEroundtime->setValue(iFDEroundtime);
}

void MInstall::saveAdvancedFDE()
{
    indexFDEcipher = comboFDEcipher->currentIndex();
    indexFDEchain = comboFDEchain->currentIndex();
    indexFDEivgen = comboFDEivgen->currentIndex();
    indexFDEivhash = comboFDEivhash->currentIndex();
    iFDEkeysize = spinFDEkeysize->value();
    indexFDEhash = comboFDEhash->currentIndex();
    indexFDErandom = comboFDErandom->currentIndex();
    iFDEroundtime = spinFDEroundtime->value();
}

// create ESP at the begining of the drive
bool MInstall::makeEsp(const QString &drv, int size)
{
    qDebug() << "+++" << __PRETTY_FUNCTION__ << "+++";
    if (phase < 0) return false;
    QString mmcnvmepartdesignator;
    if (drv.contains("nvme") || drv.contains("mmcblk" )) {
        mmcnvmepartdesignator = "p";
    }
    int err = shell.run("parted -s --align optimal " + drv + " mkpart ESP 1MiB " + QString::number(size) + "MiB");
    if (err != 0) {
        qDebug() << "Could not create ESP";
        return false;
    }

    err = shell.run("mkfs.msdos -F 32 " + drv + mmcnvmepartdesignator + "1");
    if (err != 0) {
        qDebug() << "Could not format ESP";
        return false;
    }
    shell.run("parted -s " + drv + " set 1 esp on");   // sets boot flag and esp flag
    return true;
}

bool MInstall::formatPartitions(const QByteArray &encPass, const QString &rootType, const QString &homeType, bool formatBoot)
{
    qDebug() << "+++" << __PRETTY_FUNCTION__ << "+++";
    if (phase < 0) return false;

    QString rootdev = rootDevicePreserve;
    QString swapdev = swapDevicePreserve;
    QString homedev = homeDevicePreserve;

    // set up LUKS containers
    const QString &statup = tr("Setting up LUKS encrypted containers");
    if (isSwapEncrypted) {
        if (formatSwap) {
            updateStatus(statup);
            if (!makeLuksPartition(swapdev, encPass)) return false;
        }
        updateStatus(statup);
        if (!openLuksPartition(swapdev, "swapfs", encPass)) return false;
        swapdev = "/dev/mapper/swapfs";
    }
    if (isRootEncrypted) {
        if (!rootType.isEmpty()) {
            updateStatus(statup);
            if (!makeLuksPartition(rootdev, encPass)) return false;
        }
        updateStatus(statup);
        if (!openLuksPartition(rootdev, "rootfs", encPass)) return false;
        rootdev = "/dev/mapper/rootfs";
    }
    if (isHomeEncrypted) {
        if (!homeType.isEmpty()) {
            updateStatus(statup);
            if (!makeLuksPartition(homedev, encPass)) return false;
        }
        updateStatus(statup);
        if (!openLuksPartition(homedev, "homefs", encPass)) return false;
        homedev = "/dev/mapper/homefs";
    }

    //if no swap is chosen do nothing
    if (formatSwap) {
        updateStatus(tr("Formatting swap partition"));
        const QString cmd("/sbin/mkswap %1 -L %2");
        if (!runProc(cmd.arg(swapdev, swapLabelEdit->text()))) return false;
    }

    // maybe format root (if not saving /home on root), or if using --sync option
    if (!rootType.isEmpty()) {
        updateStatus(tr("Formatting the / (root) partition"));
        if (!makeLinuxPartition(rootdev, rootType, badblocksCheck->isChecked(), rootLabelEdit->text())) {
            return false;
        }
        csleep(1000);
        isRootFormatted = true;
        root_mntops = "defaults,noatime";
    }
    if (!mountPartition(rootdev, "/mnt/antiX", root_mntops)) return false;

    // format and mount /boot if different than root
    if (formatBoot) {
        updateStatus(tr("Formatting boot partition"));
        if (!makeLinuxPartition(bootdev, "ext4", false, "boot")) {
            return false;
        }
    }

    // maybe format home
    if (!homeType.isEmpty()) {
        updateStatus(tr("Formatting the /home partition"));
        if (!makeLinuxPartition(homedev, homeType, badblocksCheck->isChecked(), homeLabelEdit->text())) {
            return false;
        }
        shell.run("/bin/rm -r /mnt/antiX/home >/dev/null 2>&1");
        csleep(1000);
        isHomeFormatted = true;
    }
    mkdir("/mnt/antiX/home", 0755);
    if (homedev != rootDevicePreserve) {
        // not on root
        updateStatus(tr("Mounting the /home partition"));
        if (!mountPartition(homedev, "/mnt/antiX/home", home_mntops)) return false;
    }

    return true;
}

bool MInstall::makeLinuxPartition(const QString &dev, const QString &type, bool bad, const QString &label)
{
    qDebug() << "+++" << __PRETTY_FUNCTION__ << "+++";
    if (phase < 0) return false;
    QString homedev = homeDevicePreserve;
    if (homedev == dev || dev == "/dev/mapper/homefs") {  // if formatting /home partition
        home_mntops = "defaults,noatime";
    } else {
        root_mntops = "defaults,noatime";
    }

    QString cmd;
    if (type == "reiserfs") {
        cmd = QString("mkfs.reiserfs -q %1 -l \"%2\"").arg(dev).arg(label);
    } else if (type == "reiser4") {
        // reiser4
        cmd = QString("mkfs.reiser4 -f -y %1 -L \"%2\"").arg(dev).arg(label);
    } else if (type == "ext3") {
        // ext3
        if (bad) {
            // do with badblocks
            cmd = QString("mkfs.ext3 -c %1 -L \"%2\"").arg(dev).arg(label);
        } else {
            // do no badblocks
            cmd = QString("mkfs.ext3 -F %1 -L \"%2\"").arg(dev).arg(label);
        }
    } else if (type == "ext2") {
        // ext2
        if (bad) {
            // do with badblocks
            cmd = QString("mkfs.ext2 -c %1 -L \"%2\"").arg(dev).arg(label);
        } else {
            // do no badblocks
            cmd = QString("mkfs.ext2 -F %1 -L \"%2\"").arg(dev).arg(label);
        }
    } else if (type == "btrfs") {
        // btrfs and set up fsck
        shell.run("/bin/cp -fp /bin/true /sbin/fsck.auto");
        // set creation options for small drives using btrfs
        csleep(1000);
        QString size_str = shell.getOutput("/sbin/sfdisk -s " + dev);
        quint64 size = size_str.toULongLong();
        size = size / 1024; // in MiB
        // if drive is smaller than 6GB, create in mixed mode
        if (size < 6000) {
            cmd = QString("mkfs.btrfs -f -M -O skinny-metadata %1 -L \"%2\"").arg(dev).arg(label);
        } else {
            cmd = QString("mkfs.btrfs -f %1 -L \"%2\"").arg(dev).arg(label);
        }
        // if compression has been selected by user, set flag
        if (type == "btrfs-zlib") {
            if (homedev == dev || dev == "/dev/mapper/homefs") { // if formatting /home partition
                home_mntops = "defaults,noatime,compress-force=zlib";
            } else {
                root_mntops = "defaults,noatime,compress-force=zlib";
            }
        } else if (type == "btrfs-lzo") {
            if (homedev == dev || dev == "/dev/mapper/homefs") {  // if formatting /home partition
                home_mntops = "defaults,noatime,compress-force=lzo";
            } else {
                root_mntops = "defaults,noatime,compress-force=lzo";
            }
        }
    } else if (type == "xfs") {
        cmd = QString("mkfs.xfs -f %1 -L \"%2\"").arg(dev).arg(label);
    } else if (type == "jfs") {
        if (bad) {
            // do with badblocks
            cmd = QString("mkfs.jfs -q -c %1 -L \"%2\"").arg(dev).arg(label);
        } else {
            // do no badblocks
            cmd = QString("mkfs.jfs -q %1 -L \"%2\"").arg(dev).arg(label);
        }
    } else { // must be ext4
        if (bad) {
            // do with badblocks
            cmd = QString("mkfs.ext4 -c %1 -L \"%2\"").arg(dev).arg(label);
        } else {
            // do no badblocks
            cmd = QString("mkfs.ext4 -F %1 -L \"%2\"").arg(dev).arg(label);
        }
    }
    if (shell.run(cmd) != 0) {
        // error
        return false;
    }
    csleep(1000);

    if (type.startsWith("ext")) {
        // ext4 tuning
        cmd = QString("/sbin/tune2fs -c0 -C0 -i1m %1").arg(dev);
        if (shell.run(cmd) != 0) {
            // error
        }
    }
    return true;
}

// Create and open Luks partitions; return false if it cannot create one
bool MInstall::makeLuksPartition(const QString &dev, const QByteArray &password)
{
    qDebug() << "+++" << __PRETTY_FUNCTION__ << "+++";
    if (phase < 0) return false;

    // format partition
    QString strCipherSpec = comboFDEcipher->currentText() + "-" + comboFDEchain->currentText();
    if (comboFDEchain->currentText() != "ECB") {
        strCipherSpec += "-" + comboFDEivgen->currentText();
        if (comboFDEivgen->currentText() == "ESSIV") {
            strCipherSpec += ":" + comboFDEivhash->currentData().toString();
        }
    }
    QString cmd = "cryptsetup --batch-mode"
                  " --cipher " + strCipherSpec.toLower()
                  + " --key-size " + spinFDEkeysize->cleanText()
                  + " --hash " + comboFDEhash->currentText().toLower().remove('-')
                  + " --use-" + comboFDErandom->currentText()
                  + " --iter-time " + spinFDEroundtime->cleanText()
                  + " luksFormat " + dev;
    if (!runProc(cmd, password + "\n")) {
        failUI(tr("Sorry, could not create %1 LUKS partition").arg(dev));
        return false;
    }
    return true;
}

bool MInstall::openLuksPartition(const QString &dev, const QString &fs_name, const QByteArray &password, const QString &options, const bool failHard)
{
    qDebug() << "+++" << __PRETTY_FUNCTION__ << "+++";
    if (phase < 0) return false;

    // open containers, assigning container names
    QString cmd = "cryptsetup luksOpen " + dev;
    if (!fs_name.isEmpty()) cmd += " " + fs_name;
    if (!options.isEmpty()) cmd += " " + options;
    if (!runProc(cmd, password + "\n")) {
        if (failHard) failUI(tr("Sorry, could not open %1 LUKS container").arg(fs_name));
        return false;
    }
    return true;
}

// validate the partition selection and confirm it.
bool MInstall::validateChosenPartitions()
{
    qDebug() << "+++" << __PRETTY_FUNCTION__ << "+++";

    int ans;
    if (checkBoxEncryptSwap->isChecked() || checkBoxEncryptHome->isChecked() || checkBoxEncryptRoot->isChecked()) {
        if (!checkPassword(FDEpassCust->text())) {
            return false;
        }
        if (bootCombo->currentText() == "root") {
            if (checkBoxEncryptRoot->isChecked()) {
                QMessageBox::critical(this, QString::null, tr("You must choose a separate boot partition when encrypting root."));
                return false;
            }
        }
    }
    QString rootdev = "/dev/" + rootCombo->currentText().section(" -", 0, 0).trimmed();
    QString homedev = "/dev/" + homeCombo->currentText().section(" -", 0, 0).trimmed();
    QString swapdev = "/dev/" + swapCombo->currentText().section(" -", 0, 0).trimmed();
    homedev = (homedev == "/dev/root") ? rootdev : homedev;
    if (bootCombo->currentText() == "root") {
        bootdev = rootdev;
    } else {
        bootdev = "/dev/" + bootCombo->currentText().section(" -", 0, 0).trimmed();
    }

    if (rootdev == "/dev/none" || rootdev == "/dev/") {
        QMessageBox::critical(this, QString::null,
            tr("You must choose a root partition.\nThe root partition must be at least %1.").arg(MIN_INSTALL_SIZE));
        nextFocus = rootCombo;
        return false;
    }

    QStringList msgForeignList;
    QString msgConfirm;
    QStringList msgFormatList;

    // warn if using a non-Linux partition (potential Windows install)
    if (shell.run(QString("partition-info is-linux=%1").arg(rootdev)) != 0) {
        msgForeignList << rootdev << "/ (root)";
    }

    // warn on formatting or deletion
    if (!(saveHomeCheck->isChecked() && homedev == rootdev)) {
        msgFormatList << rootdev << "/ (root)";
    } else {
        msgConfirm += " - " + tr("Delete the data on %1 except for /home").arg(rootdev) + "\n";
    }

    // format /home?
    if (homedev != rootdev) {
        if (shell.run(QString("partition-info is-linux=%1").arg(homedev)) != 0) {
            msgForeignList << homedev << "/home";
        }
        if (saveHomeCheck->isChecked()) {
            msgConfirm += " - " + tr("Reuse (no reformat) %1 as the /home partition").arg(homedev) + "\n";
        } else {
            msgFormatList << homedev << "/home";
        }
    }

    // format swap? (if no swap is chosen do nothing)
    if (swapdev != "/dev/none") {
        if (shell.run(QString("partition-info is-linux=%1").arg(swapdev)) != 0) {
            msgForeignList << swapdev << "swap";
        }
        formatSwap = checkBoxEncryptSwap->isChecked() || shell.run(QString("partition-info %1 | cut -d- -f3 | grep swap").arg(swapdev)) != 0;
        if (formatSwap) {
            msgFormatList << swapdev << "swap";
        } else {
            msgConfirm += " - " + tr("Configure %1 as swap space").arg(swapdev) + "\n";
        }
    } else {
        formatSwap = false;
    }

    QString msg;

    // format /boot?
    if (bootCombo->currentText() != "root") {
        msgFormatList << bootdev << "/boot";
        // warn if using a non-Linux partition (potential Windows install)
        if (shell.run(QString("partition-info is-linux=%1").arg(bootdev)) != 0) {
            msgForeignList << bootdev << "/boot";
        }
        // warn if partition too big (not needed for boot, likely data or other useful partition
        QString size_str = shell.getOutput("lsblk -nbo SIZE " + bootdev + "|head -n1").trimmed();
        bool ok = true;
        quint64 size = size_str.toULongLong(&ok, 10);
        if (size > 2147483648 || !ok) {  // if > 2GiB or not converted properly
            msg = tr("The partition you selected for /boot is larger than expected.") + "\n\n";
        }
    }

    static const QString msgPartSel = " - " + tr("%1 for the %2 partition") + "\n";
    // message to advise of issues found.
    if (msgForeignList.count() > 0) {
        msg += tr("The following partitions you selected are not Linux partitions:") + "\n\n";
        for (QStringList::Iterator it = msgForeignList.begin(); it != msgForeignList.end(); ++it) {
            QString &s = *it;
            msg += msgPartSel.arg(s).arg((QString)*(++it));
        }
        msg += "\n";
    }
    if (!msg.isEmpty()) {
        msg += tr("Are you sure you want to reformat these partitions?");
        ans = QMessageBox::warning(this, QString::null, msg,
                                   tr("Yes"), tr("No"));
        if (ans != 0) {
            return false;
        }
    }
    // final message before the installer starts.
    msg.clear();
    if (msgFormatList.count() > 0) {
        msg += tr("The %1 installer will now format and destroy the data on the following partitions:").arg(PROJECTNAME) + "\n\n";
        for (QStringList::Iterator it = msgFormatList.begin(); it != msgFormatList.end(); ++it) {
            QString &s = *it;
            msg += msgPartSel.arg(s).arg((QString)*(++it));
        }
    }
    if (!msgConfirm.isEmpty()) {
        msg += tr("The %1 installer will now perform the following actions:").arg(PROJECTNAME);
        msg += "\n\n" + msgConfirm;
    }
    if(!msg.isEmpty()) {
        msg += "\n" + tr("These actions cannot be undone. Do you want to continue?");
        ans = QMessageBox::warning(this, QString::null, msg,
                                   tr("Yes"), tr("No"));
        if (ans != 0) {
            return false;
        }
    }

    rootDevicePreserve = rootdev;
    swapDevicePreserve = swapdev;
    homeDevicePreserve = homedev;
    return true;
}

///////////////////////////////////////////////////////////////////////////
// in this case use all of the drive

bool MInstall::makeDefaultPartitions(bool &formatBoot)
{
    qDebug() << "+++" << __PRETTY_FUNCTION__ << "+++";
    if (phase < 0) return false;

    QString mmcnvmepartdesignator;

    QString rootdev, swapdev;
    QString drv = "/dev/" + diskCombo->currentText().section(" ", 0, 0);

    if (drv.contains("nvme") || drv.contains("mmcblk" )) {
        mmcnvmepartdesignator = "p";
    }

    // entire disk, create partitions
    updateStatus(tr("Creating required partitions"));

    // unmount root part
    rootdev = drv + mmcnvmepartdesignator + "1";
    QString cmd = QString("/bin/umount -l %1 >/dev/null 2>&1").arg(rootdev);
    if (shell.run(cmd) != 0) {
        qDebug() << "could not umount: " << rootdev;
    }

    bool ok = true;
    int free = freeSpaceEdit->text().toInt(&ok,10);
    if (!ok) {
        free = 0;
    }

    // calculate new partition sizes
    // get the total disk size
    csleep(1000);
    QString size_str = shell.getOutput("/sbin/sfdisk -s " + drv);
    quint64 size = size_str.toULongLong();
    size = size / 1024; // in MiB
    // pre-compensate for rounding errors in disk geometry
    size = size - 32;
    int remaining = size;

    // allocate space for ESP
    int esp_size = 0;
    if (uefi) { // if booted from UEFI
        esp_size = 256;
        remaining -= esp_size;
    }

    // allocate space for /boot if encrypting
    int boot_size = 0;
    if (checkBoxEncryptAuto->isChecked()){ // set root and swap status encrypted
        isRootEncrypted = true;
        isSwapEncrypted = true;
        boot_size = 512;
        remaining -= boot_size;
    }

    // 2048 swap should be ample
    int swap = 2048;
    if (remaining < 2048) {
        swap = 128;
    } else if (remaining < 3096) {
        swap = 256;
    } else if (remaining < 4096) {
        swap = 512;
    } else if (remaining < 12288) {
        swap = 1024;
    }
    remaining -= swap;

    if (free > 0 && remaining > 8192) {
        // allow free_size
        // remaining is capped until free is satisfied
        if (free > remaining - 8192) {
            free = remaining - 8192;
        }
        remaining -= free;
    } else { // no free space
        free = 0;
    }

    if (uefi) { // if booted from UEFI make ESP
        // new GPT partition table
        int err = shell.run("parted -s " + drv + " mklabel gpt");
        if (err != 0 ) {
            qDebug() << "Could not create gpt partition table on " + drv;
            return false;
        }
        // switch for encrypted parts and /boot
        if (isRootEncrypted) {
            bootdev = drv + mmcnvmepartdesignator + "2";
            rootdev = drv + mmcnvmepartdesignator + "3";
            swapdev = drv + mmcnvmepartdesignator + "4";
        } else {
            rootdev = drv + mmcnvmepartdesignator + "2";
            swapdev = drv + mmcnvmepartdesignator + "3";
            updateStatus(tr("Formatting EFI System Partition (ESP)"));
        }

        if (!makeEsp(drv, esp_size)) {
            return false;
        }

    } else {
        // new msdos partition table
        cmd = QString("/bin/dd if=/dev/zero of=%1 bs=512 count=100").arg(drv);
        shell.run(cmd);
        int err = shell.run("parted -s " + drv + " mklabel msdos");
        if (err != 0 ) {
            qDebug() << "Could not create msdos partition table on " + drv;
            return false;
        }
        // switch for encrypted parts and /boot
        if (isRootEncrypted) {
            bootdev = drv + mmcnvmepartdesignator + "1";
            rootdev = drv + mmcnvmepartdesignator + "2";
            swapdev = drv + mmcnvmepartdesignator + "3";
        } else {
            rootdev = drv + mmcnvmepartdesignator + "1";
            swapdev = drv + mmcnvmepartdesignator + "2";
        }
    }
    rootDevicePreserve = rootdev;
    swapDevicePreserve = swapdev;

    // create root partition
    QString start;
    if (esp_size == 0) {
        start = "1MiB "; //use 1 MiB to aid alignment
    } else {
        start = QString::number(esp_size) + "MiB ";
    }

    // create boot partition if necessary
    if (isRootEncrypted){
        int end_boot = esp_size + boot_size;
        int err = shell.run("parted -s --align optimal " + drv + " mkpart primary " + start + QString::number(end_boot) + "MiB");
        if (err != 0) {
            qDebug() << "Could not create boot partition";
            return false;
        }
        start = QString::number(end_boot) + "MiB ";
        formatBoot = true;
    }

    // if encrypting, boot_size=512, or 0 if not  .  start is set to end_boot if encrypting
    int end_root = esp_size + boot_size + remaining;
    int err = shell.run("parted -s --align optimal " + drv + " mkpart primary ext4 " + start + QString::number(end_root) + "MiB");
    if (err != 0) {
        qDebug() << "Could not create root partition";
        return false;
    }

    // create swap partition
    err = shell.run("parted -s --align optimal " + drv + " mkpart primary " + QString::number(end_root) + "MiB " + QString::number(end_root + swap) + "MiB");
    if (err != 0) {
        qDebug() << "Could not create swap partition";
        return false;
    }

    homeDevicePreserve = rootDevicePreserve;
    formatSwap = true;
    return true;
}

///////////////////////////////////////////////////////////////////////////
// Make the chosen partitions and mount them

bool MInstall::makeChosenPartitions(QString &rootType, QString &homeType, bool &formatBoot)
{
    qDebug() << "+++" << __PRETTY_FUNCTION__ << "+++";
    if (phase < 0) return false;
    const bool saveHome = saveHomeCheck->isChecked();
    QString cmd;

    if (checkBoxEncryptRoot->isChecked()) {
        isRootEncrypted = true;
    }

    QString drv = "/dev/" + diskCombo->currentText().section(" ", 0, 0).trimmed();
    bool gpt = isGpt(drv);

    // Root
    QString rootdev = rootDevicePreserve;
    QStringList rootsplit = getCmdOut("partition-info split-device=" + rootdev).split(" ", QString::SkipEmptyParts);

    // Swap
    QString swapdev = swapDevicePreserve;
    if (checkBoxEncryptSwap->isChecked() && swapdev != "/dev/none") {
        isSwapEncrypted = true;
    }
    QStringList swapsplit = getCmdOut("partition-info split-device=" + swapdev).split(" ", QString::SkipEmptyParts);

    // Boot
    QStringList bootsplit = getCmdOut("partition-info split-device=" + bootdev).split(" ", QString::SkipEmptyParts);

    // Home
    QString homedev = homeDevicePreserve;

    if (checkBoxEncryptHome->isChecked() && homedev != rootdev) {
        isHomeEncrypted = true;
    }
    QStringList homesplit = getCmdOut("partition-info split-device=" + homedev).split(" ", QString::SkipEmptyParts);

    updateStatus(tr("Preparing required partitions"));

    // command to set the partition type
    if (gpt) {
        cmd = "/sbin/sgdisk /dev/%1 --typecode=%2:8303";
    } else {
        cmd = "/sbin/sfdisk /dev/%1 --part-type %2 83";
    }
    // maybe format swap
    if (swapdev != "/dev/none") {
        shell.run("pumount " + swapdev);
        shell.run(cmd.arg(swapsplit[0], swapsplit[1]));
    }
    // maybe format root (if not saving /home on root) // or if using --sync option
    if (!(saveHome && homedev == rootdev) && !(args.contains("--sync") || args.contains("-s"))) {
        shell.run("pumount " + rootdev);
        shell.run(cmd.arg(rootsplit[0]).arg(rootsplit[1]));
        rootType = rootTypeCombo->currentText().toUtf8();
    }
    // format and mount /boot if different than root
    if (bootCombo->currentText() != "root") {
        shell.run(cmd.arg(bootsplit[0]).arg(bootsplit[1]));
        formatBoot = true;
    }
    // prepare home if not being preserved, and on a different partition
    if (homedev != rootdev) {
        shell.run("pumount " + homedev);
        if (!saveHome) {
            shell.run(cmd.arg(homesplit[0]).arg(homesplit[1]));
            homeType = homeTypeCombo->currentText().toUtf8();
        }
    }

    return true;
}

bool MInstall::saveHomeBasic()
{
    qDebug() << "+++" << __PRETTY_FUNCTION__ << "+++";
    if (!(saveHomeCheck->isChecked())) return true;
    // if preserving /home, obtain some basic information
    bool ok = false;
    const QByteArray &pass = FDEpassCust->text().toUtf8();
    QString rootdev = rootDevicePreserve;
    QString homedev = homeDevicePreserve;
    // mount the root partition
    if (isRootEncrypted) {
        shell.run("cryptsetup luksClose rootfs");
        if (!openLuksPartition(rootdev, "rootfs", pass, "--readonly", false)) return false;
        rootdev = "/dev/mapper/rootfs";
    }
    if (!mountPartition(rootdev, "/mnt/antiX", "ro")) goto ending2;
    // mount the home partition
    if (homedev != rootDevicePreserve) {
        if (isHomeEncrypted) {
            shell.run("cryptsetup luksClose homefs");
            if (!openLuksPartition(homedev, "homefs", pass, "--readonly", false)) goto ending2;
            homedev = "/dev/mapper/homefs";
        }
        if (!mountPartition(homedev, "/mnt/antiX/home", "ro")) goto ending1;
    }

    // store a listing of /home to compare with the user name given later
    listHomes = getCmdOuts("ls -1 /mnt/antiX/home/");
    // recycle the old key for /home if possible
    key.load("/mnt/antiX/root/keyfile", -1);

    ok = true;
 ending1:
    // unmount partitions
    if (homedev != rootDevicePreserve) {
        shell.run("/bin/umount -l /mnt/antiX/home >/dev/null 2>&1");
        if (isHomeEncrypted) shell.run("cryptsetup luksClose homefs");
    }
 ending2:
    shell.run("/bin/umount -l /mnt/antiX >/dev/null 2>&1");
    if (isRootEncrypted) shell.run("cryptsetup luksClose rootfs");
    return ok;
}

bool MInstall::installLinux()
{
    qDebug() << "+++" << __PRETTY_FUNCTION__ << "+++";
    QString rootdev = (isRootEncrypted ? "/dev/mapper/rootfs" : rootDevicePreserve);
    if (phase < 0) return false;

    // maybe root was formatted or using --sync option
    if (isRootFormatted || args.contains("--sync") || args.contains("-s")) {
        // yes it was
        if(!copyLinux()) return false;
    } else {
        // no--it's being reused
        updateStatus(tr("Mounting the / (root) partition"));
        mountPartition(rootdev, "/mnt/antiX", root_mntops);
        // remove all folders in root except for /home
        updateStatus(tr("Deleting old system"));
        runProc("/bin/bash -c \"find /mnt/antiX -mindepth 1 -maxdepth 1 ! -name home -exec rm -r {} \\;\"");

        if (proc->exitStatus() == QProcess::NormalExit) {
            if(!copyLinux()) return false;
        } else {
            failUI(tr("Failed to delete old %1 on destination.\nReturning to Step 1.").arg(PROJECTNAME));
            return false;
        }
    }
    return true;
}

// create /etc/fstab file
void MInstall::makeFstab()
{
    if (phase < 0) return;

    // get config
    QString rootdev = rootDevicePreserve;
    QString homedev = homeDevicePreserve;
    QString swapdev = swapDevicePreserve;

    //get UUIDs
    const QString cmdBlkID("blkid -o value UUID -s UUID ");
    QString rootdevUUID = "UUID=" + getCmdOut(cmdBlkID + rootDevicePreserve);
    QString homedevUUID = "UUID=" + getCmdOut(cmdBlkID + homeDevicePreserve);
    QString swapdevUUID = "UUID=" + getCmdOut(cmdBlkID + swapDevicePreserve);
    const QString bootdevUUID = "UUID=" + getCmdOut(cmdBlkID + bootdev);

    // if encrypting, modify devices to /dev/mapper categories
    if (isRootEncrypted){
        rootdev = "/dev/mapper/rootfs";
        rootdevUUID = rootdev;
    }
    if (isHomeEncrypted) {
        homedev = "/dev/mapper/homefs";
        homedevUUID = homedev;
    }
    if (isSwapEncrypted) {
        swapdev = "/dev/mapper/swapfs";
        swapdevUUID = swapdev;
    }
    qDebug() << "Create fstab entries for:";
    qDebug() << "rootdev" << rootdev << rootdevUUID;
    qDebug() << "homedev" << homedev << homedevUUID;
    qDebug() << "swapdev" << swapdev << swapdevUUID;
    qDebug() << "bootdev" << bootdev << bootdevUUID;

    QString fstype = getPartType(rootdev);
    QString dump_pass = "1 1";

    if (fstype.startsWith("btrfs")) {
        dump_pass = "1 0";
    } else if (fstype.startsWith("reiser") ) {
        root_mntops += ",notail";
        dump_pass = "0 0";
    }

    QFile file("/mnt/antiX/etc/fstab");
    if (file.open(QIODevice::WriteOnly)) {
        QTextStream out(&file);
        out << "# Pluggable devices are handled by uDev, they are not in fstab\n";
        out << rootdevUUID + " / " + fstype + " " + root_mntops + " " + dump_pass + "\n";
        //add bootdev if present
        //only ext4 (for now) for max compatibility with other linuxes
        if (!bootdev.isEmpty() && bootdev != rootDevicePreserve) {
            out << bootdevUUID + " /boot ext4 " + root_mntops + " 1 1\n";
        }
        if (grubEspButton->isChecked()) {
            const QString espdev = "/dev/" + grubBootCombo->currentText().section(" ", 0, 0).trimmed();
            const QString espdevUUID = "UUID=" + getCmdOut(cmdBlkID + espdev);
            qDebug() << "espdev" << espdev << espdevUUID;
            out << espdevUUID + " /boot/efi vfat defaults,noatime,dmask=0002,fmask=0113 0 0\n";
        }
        if (!homedev.isEmpty() && homedev != rootDevicePreserve) {
            fstype = getPartType(homedev);
            if (isHomeFormatted) {
                dump_pass = "1 2";
                if (fstype.startsWith("btrfs")) {
                    dump_pass = "1 2";
                } else if (fstype.startsWith("reiser") ) {
                    home_mntops += ",notail";
                    dump_pass = "0 0";
                }
                out << homedevUUID + " /home " + fstype + " " + home_mntops + " " + dump_pass + "\n";
            } else { // if not formatted
                out << homedevUUID + " /home " + fstype + " defaults,noatime 1 2\n";
            }
        }
        if (!swapdev.isEmpty() && swapdev != "/dev/none") {
            out << swapdevUUID +" swap swap defaults 0 0 \n";
        }
        file.close();
    }
    // if POPULATE_MEDIA_MOUNTPOINTS is true in gazelle-installer-data, then use the --mntpnt switch
    if (POPULATE_MEDIA_MOUNTPOINTS) {
        runCmd("/sbin/make-fstab -O --install /mnt/antiX --mntpnt=/media");
    }
}

bool MInstall::copyLinux()
{
    qDebug() << "+++" << __PRETTY_FUNCTION__ << "+++";
    if (phase < 0) return false;
    // make empty dirs for opt, dev, proc, sys, run,
    // home already done

    updateStatus(tr("Creating system directories"));
    mkdir("/mnt/antiX/opt", 0755);
    mkdir("/mnt/antiX/dev", 0755);
    mkdir("/mnt/antiX/proc", 0755);
    mkdir("/mnt/antiX/sys", 0755);
    mkdir("/mnt/antiX/run", 0755);

    //if separate /boot in use, mount that to /mnt/antiX/boot
    if (!bootdev.isEmpty() && bootdev != rootDevicePreserve) {
        mkdir("/mnt/antiX/boot", 0755);
        shell.run("fsck.ext4 -y " + bootdev, QStringList("quiet")); // needed to run fsck because sfdisk --part-type can mess up the partition
        if (!mountPartition(bootdev, "/mnt/antiX/boot", root_mntops)) {
            qDebug() << "Could not mount /boot on " + bootdev;
            return false;
        }
    }

    // copy most except usr, mnt and home
    // must copy boot even if saving, the new files are required
    // media is already ok, usr will be done next, home will be done later
    updateStatus(tr("Copying new system"));
    iCopyBarA = progressBar->value();
    // setup and start the process
    QString cmd;
    cmd = "/bin/cp -a";
    if (args.contains("--sync") || args.contains("-s")) {
        cmd = "rsync -a --delete";
        if (saveHomeCheck->isChecked()) {
            cmd.append(" --filter 'protect home/*'");
        }
    }
    cmd.append(" /live/aufs/bin /live/aufs/boot /live/aufs/dev");
    cmd.append(" /live/aufs/etc /live/aufs/lib /live/aufs/lib64 /live/aufs/media /live/aufs/mnt");
    cmd.append(" /live/aufs/opt /live/aufs/root /live/aufs/sbin /live/aufs/selinux /live/aufs/usr");
    cmd.append(" /live/aufs/var /live/aufs/home /mnt/antiX");
    struct statvfs svfs;
    if (statvfs("/live/linux", &svfs) == 0) {
        iTargetInodes = svfs.f_files - svfs.f_ffree;
        if(statvfs("/mnt/antiX", &svfs) == 0) {
            iTargetInodes += svfs.f_files - svfs.f_ffree;
        }
    }

    if (!(args.contains("--nocopy") || args.contains("-n"))) {
        connect(timer, SIGNAL(timeout()), this, SLOT(copyTime()));
        timer->start(1000);
        runProc(cmd);
        if (proc->exitStatus() != QProcess::NormalExit) {
            failUI(tr("Failed to write %1 to destination.\nReturning to Step 1.").arg(PROJECTNAME));
            return false;
        }
        timer->stop();
        disconnect(timer, SIGNAL(timeout()), 0, 0);
    }

    updateStatus(tr("Fixing configuration"), 94);
    shell.run("mkdir -m 1777 /mnt/antiX/tmp");
    makeFstab();
    writeKeyFile();
    disablehiberanteinitramfs();

    // Copy live set up to install and clean up.
    //shell.run("/bin/rm -rf /mnt/antiX/etc/skel/Desktop");
    shell.run("/usr/sbin/live-to-installed /mnt/antiX");
    qDebug() << "Desktop menu";
    runCmd("chroot /mnt/antiX desktop-menu --write-out-global");
    shell.run("/bin/rm -rf /mnt/antiX/home/demo");
    shell.run("/bin/rm -rf /mnt/antiX/media/sd*");
    shell.run("/bin/rm -rf /mnt/antiX/media/hd*");
    //shell.run("/bin/mv -f /mnt/antiX/etc/X11/xorg.conf /mnt/antiX/etc/X11/xorg.conf.live >/dev/null 2>&1");

    // guess localtime vs UTC
    if (getCmdOut("guess-hwclock") == "localtime") {
        localClockCheckBox->setChecked(true);
    }

    return true;
}

///////////////////////////////////////////////////////////////////////////
// install loader

// build a grub configuration and install grub
bool MInstall::installLoader()
{
    qDebug() << "+++" << __PRETTY_FUNCTION__ << "+++";
    if (phase < 0) return false;

    const QString &statup = tr("Installing GRUB");
    updateStatus(statup);
    QString cmd;
    QString val = getCmdOut("ls /mnt/antiX/boot | grep 'initrd.img-3.6'");

    // the old initrd is not valid for this hardware
    if (!val.isEmpty()) {
        runCmd("rm -f /mnt/antiX/boot/" + val);
    }

    if (!grubCheckBox->isChecked()) {
        // skip it
        return true;
    }

    //add switch to change root partition info
    QString boot = "/dev/" + grubBootCombo->currentText().section(" ", 0, 0).trimmed();

    if (grubMbrButton->isChecked() && !isGpt(boot)) {
        QString part_num;
        if (bootdev.startsWith(boot)) part_num = bootdev;
        else if (rootDevicePreserve.startsWith(boot)) part_num = rootDevicePreserve;
        else if (homeDevicePreserve.startsWith(boot)) part_num = homeDevicePreserve;
        if (!part_num.isEmpty()) {
            // remove the non-digit part to get the number of the root partition
            part_num.remove(QRegularExpression("\\D+\\d*\\D+"));
            runProc("parted -s " + boot + " set " + part_num + " boot on");
        }
    }

    // set mounts for chroot
    runCmd("mount -o bind /dev /mnt/antiX/dev");
    runCmd("mount -o bind /sys /mnt/antiX/sys");
    runCmd("mount -o bind /proc /mnt/antiX/proc");

    QString arch;

    // install new Grub now
    if (!grubEspButton->isChecked()) {
        cmd = QString("grub-install --target=i386-pc --recheck --no-floppy --force --boot-directory=/mnt/antiX/boot %1").arg(boot);
    } else {
        runCmd("mkdir /mnt/antiX/boot/efi");
        QString mount = QString("mount %1 /mnt/antiX/boot/efi").arg(boot);
        runCmd(mount);
        // rename arch to match grub-install target
        arch = getCmdOut("cat /sys/firmware/efi/fw_platform_size");
        if (arch == "32") {
            arch = "i386";
        } else if (arch == "64") {
            arch = "x86_64";
        }

        cmd = QString("chroot /mnt/antiX grub-install --no-nvram --force-extra-removable --target=%1-efi --efi-directory=/boot/efi --bootloader-id=%2%3 --recheck").arg(arch, PROJECTSHORTNAME, PROJECTVERSION);
    }

    qDebug() << "Installing Grub";
    if (runCmd(cmd) != 0) {
        // error
        QMessageBox::critical(this, QString::null,
                              tr("GRUB installation failed. You can reboot to the live medium and use the GRUB Rescue menu to repair the installation."));
        runCmd("umount /mnt/antiX/proc; umount /mnt/antiX/sys; umount /mnt/antiX/dev");
        if (runCmd("mountpoint -q /mnt/antiX/boot/efi") == 0) {
            runCmd("umount /mnt/antiX/boot/efi");
        }
        return false;
    }

    // update NVRAM boot entries (only if installing on ESP)
    if (grubEspButton->isChecked()) {
        updateStatus(statup);
        cmd = QString("chroot /mnt/antiX grub-install --force-extra-removable --target=%1-efi --efi-directory=/boot/efi --bootloader-id=%2%3 --recheck").arg(arch, PROJECTSHORTNAME, PROJECTVERSION);
        if (runCmd(cmd) != 0) {
            QMessageBox::warning(this, QString::null, tr("NVRAM boot variable update failure. The system may not boot, but it can be repaired with the GRUB Rescue boot menu."));
        }
    }

    //added non-live boot codes to those in /etc/default/grub, remove duplicates
    //get non-live boot codes
    QString cmdline = getCmdOut("/live/bin/non-live-cmdline");

    //get /etc/default/grub codes
    QSettings grubSettings("/etc/default/grub", QSettings::NativeFormat);
    QString grubDefault=grubSettings.value("GRUB_CMDLINE_LINUX_DEFAULT").toString();
    qDebug() << "grubDefault is " << grubDefault;

    //covert qstrings to qstringlists and join the default and non-live lists together
    QStringList finalcmdline=cmdline.split(" ");
    finalcmdline.append(grubDefault.split(" "));
    qDebug() << "intermediate" << finalcmdline;

    //remove any duplicate codes in list (typically splash)
    finalcmdline.removeDuplicates();

    //remove vga=ask
    finalcmdline.removeAll("vga=ask");

    //remove boot_image code
    finalcmdline.removeAll("BOOT_IMAGE=/antiX/vmlinuz");

    //remove nosplash boot code if configured in installer.conf
    if (REMOVE_NOSPLASH) {
        finalcmdline.removeAll("nosplash");
    }
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
    cmd = QString("sed -i -r 's|^(GRUB_CMDLINE_LINUX_DEFAULT=).*|\\1\"%1\"|' /mnt/antiX/etc/default/grub").arg(finalcmdlinestring);
    runCmd(cmd);

    //copy memtest efi files if needed

    if (uefi) {
        if (arch == "i386") {
            runCmd("mkdir -p /mnt/antiX/boot/uefi-mt");
            runCmd("cp /live/boot-dev/boot/uefi-mt/mtest-32.efi /mnt/antiX/boot/uefi-mt");
        } else {
            runCmd("mkdir -p /mnt/antiX/boot/uefi-mt");
            runCmd("cp /live/boot-dev/boot/uefi-mt/mtest-64.efi /mnt/antiX/boot/uefi-mt");
        }
    }
    updateStatus(statup);

    //update grub with new config

    qDebug() << "Update Grub";
    runCmd("chroot /mnt/antiX update-grub");
    updateStatus(statup);

    qDebug() << "Update initramfs";
    runCmd("chroot /mnt/antiX update-initramfs -u -t -k all");
    updateStatus(statup);
    qDebug() << "clear chroot env";
    runCmd("umount /mnt/antiX/proc");
    runCmd("umount /mnt/antiX/sys");
    runCmd("umount /mnt/antiX/dev");
    if (runCmd("mountpoint -q /mnt/antiX/boot/efi") == 0) {
        runCmd("umount /mnt/antiX/boot/efi");
    }

    return true;
}

bool MInstall::isGpt(const QString &drv)
{
    QString cmd = QString("blkid %1 | grep -q PTTYPE=\\\"gpt\\\"").arg(drv);
    return (shell.run(cmd) == 0);
}

void MInstall::checkUefi()
{
    // return false if not uefi, or if a bad combination, like 32 bit iso and 64 bit uefi)
    if (shell.run("uname -m | grep -q i686") == 0 && shell.run("grep -q 64 /sys/firmware/efi/fw_platform_size") == 0) {
        uefi = false;
    } else {
        uefi = (shell.run("test -d /sys/firmware/efi") == 0);
    }
    qDebug() << "uefi =" << uefi;
}

/////////////////////////////////////////////////////////////////////////
// create the user, can not be rerun

bool MInstall::setUserName()
{
    qDebug() << "+++" << __PRETTY_FUNCTION__ << "+++";
    if (phase < 0) return false;
    if (args.contains("--nocopy") || args.contains("-n")) {
        return true;
    }
    DIR *dir;
    QString cmd;

    // see if user directory already exists
    QString dpath = QString("/mnt/antiX/home/%1").arg(userNameEdit->text());
    if ((dir = opendir(dpath.toUtf8())) != NULL) {
        // already exists
        closedir(dir);
        if (oldHomeAction == OldHomeSave) {
            // save the old directory
            int rexit = -1;
            cmd = QString("mv -f %1 %1.00%2").arg(dpath);
            for (int ixi = 1; ixi < 10 && rexit != 0; ++ixi) {
                rexit = shell.run(cmd.arg(ixi));
            }
            if (rexit != 0) {
                failUI(tr("Sorry, failed to save old home directory. Before proceeding,\nyou'll have to select a different username or\ndelete a previously saved copy of your home directory."));
                return false;
            }
        } else if (oldHomeAction == OldHomeDelete) {
            // delete the directory
            cmd = QString("rm -rf %1").arg(dpath);
            if (shell.run(cmd) != 0) {
                failUI(tr("Sorry, failed to delete old home directory. Before proceeding, \nyou'll have to select a different username."));
                return false;
            }
        }
    }

    if ((dir = opendir(dpath.toUtf8())) == NULL) {
        // dir does not exist, must create it
        // copy skel to demo
        if (shell.run("cp -a /mnt/antiX/etc/skel /mnt/antiX/home") != 0) {
            failUI(tr("Sorry, failed to create user directory."));
            return false;
        }
        cmd = QString("mv -f /mnt/antiX/home/skel %1").arg(dpath);
        if (shell.run(cmd) != 0) {
            failUI(tr("Sorry, failed to name user directory."));
            return false;
        }
    } else {
        // dir does exist, clean it up
        cmd = QString("cp -n /mnt/antiX/etc/skel/.bash_profile %1").arg(dpath);
        shell.run(cmd);
        cmd = QString("cp -n /mnt/antiX/etc/skel/.bashrc %1").arg(dpath);
        shell.run(cmd);
        cmd = QString("cp -n /mnt/antiX/etc/skel/.gtkrc %1").arg(dpath);
        shell.run(cmd);
        cmd = QString("cp -n /mnt/antiX/etc/skel/.gtkrc-2.0 %1").arg(dpath);
        shell.run(cmd);
        cmd = QString("cp -Rn /mnt/antiX/etc/skel/.config %1").arg(dpath);
        shell.run(cmd);
        cmd = QString("cp -Rn /mnt/antiX/etc/skel/.local %1").arg(dpath);
        shell.run(cmd);
    }
    // saving Desktop changes
    if (saveDesktopCheckBox->isChecked()) {
        shell.run("su -c 'dconf reset /org/blueman/transfer/shared-path' demo"); //reset blueman path
        cmd = QString("rsync -a /home/demo/ %1 --exclude '.cache' --exclude '.gvfs' --exclude '.dbus' --exclude '.Xauthority' --exclude '.ICEauthority' --exclude '.mozilla' --exclude 'Installer.desktop' --exclude 'minstall.desktop' --exclude 'Desktop/antixsources.desktop' --exclude '.jwm/menu' --exclude '.icewm/menu' --exclude '.fluxbox/menu' --exclude '.config/rox.sourceforge.net/ROX-Filer/pb_antiX-fluxbox' --exclude '.config/rox.sourceforge.net/ROX-Filer/pb_antiX-icewm' --exclude '.config/rox.sourceforge.net/ROX-Filer/pb_antiX-jwm'").arg(dpath);
        if (shell.run(cmd) != 0) {
            QMessageBox::warning(this, QString::null,
                                  tr("Sorry, failed to save desktop changes."));
        } else {
            cmd = QString("grep -rl \"home/demo\" " + dpath + "| xargs sed -i 's|home/demo|home/" + userNameEdit->text() + "|g'");
            shell.run(cmd);
        }
    }
    // fix the ownership, demo=newuser
    cmd = QString("chown -R demo:demo %1").arg(dpath);
    if (shell.run(cmd) != 0) {
        failUI(tr("Sorry, failed to set ownership of user directory."));
        return false;
    }

    // change in files
    replaceStringInFile("demo", userNameEdit->text(), "/mnt/antiX/etc/group");
    replaceStringInFile("demo", userNameEdit->text(), "/mnt/antiX/etc/gshadow");
    replaceStringInFile("demo", userNameEdit->text(), "/mnt/antiX/etc/passwd");
    replaceStringInFile("demo", userNameEdit->text(), "/mnt/antiX/etc/shadow");
    replaceStringInFile("demo", userNameEdit->text(), "/mnt/antiX/etc/slim.conf");
    replaceStringInFile("demo", userNameEdit->text(), "/mnt/antiX/etc/lightdm/lightdm.conf");
    if (autologinCheckBox->isChecked()) {
        replaceStringInFile("#auto_login", "auto_login", "/mnt/antiX/etc/slim.conf");
        replaceStringInFile("#default_user ", "default_user ", "/mnt/antiX/etc/slim.conf");
    }
    else {
        replaceStringInFile("auto_login", "#auto_login", "/mnt/antiX/etc/slim.conf");
        replaceStringInFile("default_user ", "#default_user ", "/mnt/antiX/etc/slim.conf");
        replaceStringInFile("autologin-user=", "#autologin-user=", "/mnt/antiX/etc/lightdm/lightdm.conf");
    }
    cmd = QString("touch /mnt/antiX/var/mail/%1").arg(userNameEdit->text());
    shell.run(cmd);

    return true;
}

// get the type of the partition
QString MInstall::getPartType(const QString &dev)
{
    qDebug() << "+++" << __PRETTY_FUNCTION__ << "+++";
    return shell.getOutput("blkid " + dev + " -o value -s TYPE");
}

bool MInstall::setPasswords()
{
    qDebug() << "+++" << __PRETTY_FUNCTION__ << "+++";
    if (phase < 0) return false;
    if (args.contains("--nocopy") || args.contains("-n")) {
        return true;
    }

    const QString cmd = "chroot /mnt/antiX chpasswd";

    if (!runProc(cmd, QString("root:" + rootPasswordEdit->text() + "\n").toUtf8())) {
        failUI(tr("Sorry, unable to set root password."));
        return false;
    }
    if (!runProc(cmd, QString("demo:" + userPasswordEdit->text() + "\n").toUtf8())) {
        failUI(tr("Sorry, unable to set user password."));
        return false;
    }

    return true;
}

bool MInstall::validateUserInfo()
{
    nextFocus = userNameEdit;
    // see if username is reasonable length
    if (userNameEdit->text().isEmpty()) {
        QMessageBox::critical(this, QString::null,
                              tr("Please enter a user name."));
        return false;
    } else if (!userNameEdit->text().contains(QRegExp("^[a-zA-Z_][a-zA-Z0-9_-]*[$]?$"))) {
        QMessageBox::critical(this, QString::null,
                              tr("The user name cannot contain special characters or spaces.\n"
                                 "Please choose another name before proceeding."));
        return false;
    }
    // check that user name is not already used
    QFile file("/etc/passwd");
    if (file.open(QFile::ReadOnly | QFile::Text)) {
        const QByteArray &match = QString("%1:").arg(userNameEdit->text()).toUtf8();
        while (!file.atEnd()) {
            if (file.readLine().startsWith(match)) {
                QMessageBox::critical(this, QString::null,
                                      tr("Sorry, that name is in use.\n"
                                         "Please select a different name."));
                return false;
            }
        }
    }

    if (userPasswordEdit->text().isEmpty()) {
        QMessageBox::critical(this, QString::null,
                              tr("Please enter the user password."));
        nextFocus = userPasswordEdit;
        return false;
    }
    if (rootPasswordEdit->text().isEmpty()) {
        QMessageBox::critical(this, QString::null,
                              tr("Please enter the root password."));
        nextFocus = rootPasswordEdit;
        return false;
    }
    if (userPasswordEdit->text() != userPasswordEdit2->text()) {
        QMessageBox::critical(this, QString::null,
                              tr("The user password entries do not match.\n"
                                 "Please try again."));
        nextFocus = userPasswordEdit;
        return false;
    }
    if (rootPasswordEdit->text() != rootPasswordEdit2->text()) {
        QMessageBox::critical(this, QString::null,
                              tr("The root password entries do not match.\n"
                                 "Please try again."));
        nextFocus = rootPasswordEdit;
        return false;
    }

    // Check for pre-existing /home directory
    // see if user directory already exists
    if (listHomes.contains(userNameEdit->text())) {
        // already exists
        int ans;
        QString msg;
        msg = tr("The home directory for %1 already exists.Would you like to reuse the old home directory?").arg(userNameEdit->text());
        ans = QMessageBox::information(this, QString::null, msg,
                                       tr("Yes"), tr("No"));
        if (ans == 0) {
            // use the old home
            oldHomeAction = OldHomeUse;
        } else {
            // don't reuse -- maybe save the old home
            msg = tr("Would you like to save the old home directory\nand create a new home directory?");
            ans = QMessageBox::information(this, QString::null, msg,
                                           tr("Yes"), tr("No"));
            if (ans == 0) {
                // save the old directory
                oldHomeAction = OldHomeSave;
            } else {
                // don't save and don't reuse -- delete?
                msg = tr("Would you like to delete the old home directory for %1?").arg(userNameEdit->text());
                ans = QMessageBox::information(this, QString::null, msg,
                                               tr("Yes"), tr("No"));
                if (ans == 0) {
                    // delete the directory
                    oldHomeAction = OldHomeDelete;
                } else {
                    // don't save, reuse or delete -- can't proceed
                    QMessageBox::critical(this, QString::null,
                                          tr("You've chosen to not use, save or delete the old home directory.\n"
                                             "Before proceeding, you'll have to select a different username."));
                    nextFocus = userNameEdit;
                    return false;
                }
            }
        }
    }
    nextFocus = NULL;
    return true;
}

bool MInstall::setUserInfo()
{
    if (phase < 0) return false;
    if (!setPasswords()) {
        return false;
    }
    return setUserName();
}

/////////////////////////////////////////////////////////////////////////
// computer name functions

bool MInstall::validateComputerName()
{
    qDebug() << "+++" << __PRETTY_FUNCTION__ << "+++";
    // see if name is reasonable
    nextFocus = computerNameEdit;
    if (computerNameEdit->text().length() < 2) {
        QMessageBox::critical(this, QString::null,
                              tr("Sorry, your computer name needs to be\nat least 2 characters long. You'll have to\nselect a different name before proceeding."));
        return false;
    } else if (computerNameEdit->text().contains(QRegExp("[^0-9a-zA-Z-.]|^[.-]|[.-]$|\\.\\."))) {
        QMessageBox::critical(this, QString::null,
                              tr("Sorry, your computer name contains invalid characters.\nYou'll have to select a different\nname before proceeding."));
        return false;
    }
    // see if name is reasonable
    nextFocus = computerDomainEdit;
    if (computerDomainEdit->text().length() < 2) {
        QMessageBox::critical(this, QString::null,
                              tr("Sorry, your computer domain needs to be at least\n2 characters long. You'll have to select a different\nname before proceeding."));
        return false;
    } else if (computerDomainEdit->text().contains(QRegExp("[^0-9a-zA-Z-.]|^[.-]|[.-]$|\\.\\."))) {
        QMessageBox::critical(this, QString::null,
                              tr("Sorry, your computer domain contains invalid characters.\nYou'll have to select a different\nname before proceeding."));
        return false;
    }

    if (haveSamba) {
        // see if name is reasonable
        if (computerGroupEdit->text().length() < 2) {
            QMessageBox::critical(this, QString::null,
                                  tr("Sorry, your workgroup needs to be at least\n2 characters long. You'll have to select a different\nname before proceeding."));
            nextFocus = computerGroupEdit;
            return false;
        }
    } else {
        computerGroupEdit->clear();
    }

    nextFocus = NULL;
    return true;
}

// set the computer name, can not be rerun
bool MInstall::setComputerName()
{
    if (phase < 0) return false;
    if (haveSamba) {
        //replaceStringInFile(PROJECTSHORTNAME + "1", computerNameEdit->text(), "/mnt/antiX/etc/samba/smb.conf");
        replaceStringInFile("WORKGROUP", computerGroupEdit->text(), "/mnt/antiX/etc/samba/smb.conf");
    }
    if (sambaCheckBox->isChecked()) {
        shell.run("mv -f /mnt/antiX/etc/rc5.d/K*smbd /mnt/antiX/etc/rc5.d/S06smbd >/dev/null 2>&1");
        shell.run("mv -f /mnt/antiX/etc/rc4.d/K*smbd /mnt/antiX/etc/rc4.d/S06smbd >/dev/null 2>&1");
        shell.run("mv -f /mnt/antiX/etc/rc3.d/K*smbd /mnt/antiX/etc/rc3.d/S06smbd >/dev/null 2>&1");
        shell.run("mv -f /mnt/antiX/etc/rc2.d/K*smbd /mnt/antiX/etc/rc2.d/S06smbd >/dev/null 2>&1");
        shell.run("mv -f /mnt/antiX/etc/rc5.d/K*samba-ad-dc /mnt/antiX/etc/rc5.d/S01samba-ad-dc >/dev/null 2>&1");
        shell.run("mv -f /mnt/antiX/etc/rc4.d/K*samba-ad-dc /mnt/antiX/etc/rc4.d/S01samba-ad-dc >/dev/null 2>&1");
        shell.run("mv -f /mnt/antiX/etc/rc3.d/K*samba-ad-dc /mnt/antiX/etc/rc3.d/S01samba-ad-dc >/dev/null 2>&1");
        shell.run("mv -f /mnt/antiX/etc/rc2.d/K*samba-ad-dc /mnt/antiX/etc/rc2.d/S01samba-ad-dc >/dev/null 2>&1");
        shell.run("mv -f /mnt/antiX/etc/rc5.d/K*nmbd /mnt/antiX/etc/rc5.d/S01nmbd >/dev/null 2>&1");
        shell.run("mv -f /mnt/antiX/etc/rc4.d/K*nmbd /mnt/antiX/etc/rc4.d/S01nmbd >/dev/null 2>&1");
        shell.run("mv -f /mnt/antiX/etc/rc3.d/K*nmbd /mnt/antiX/etc/rc3.d/S01nmbd >/dev/null 2>&1");
        shell.run("mv -f /mnt/antiX/etc/rc2.d/K*nmbd /mnt/antiX/etc/rc2.d/S01nmbd >/dev/null 2>&1");
    } else {
        shell.run("mv -f /mnt/antiX/etc/rc5.d/S*smbd /mnt/antiX/etc/rc5.d/K01smbd >/dev/null 2>&1");
        shell.run("mv -f /mnt/antiX/etc/rc4.d/S*smbd /mnt/antiX/etc/rc4.d/K01smbd >/dev/null 2>&1");
        shell.run("mv -f /mnt/antiX/etc/rc3.d/S*smbd /mnt/antiX/etc/rc3.d/K01smbd >/dev/null 2>&1");
        shell.run("mv -f /mnt/antiX/etc/rc2.d/S*smbd /mnt/antiX/etc/rc2.d/K01smbd >/dev/null 2>&1");
        shell.run("mv -f /mnt/antiX/etc/rc5.d/S*samba-ad-dc /mnt/antiX/etc/rc5.d/K01samba-ad-dc >/dev/null 2>&1");
        shell.run("mv -f /mnt/antiX/etc/rc4.d/S*samba-ad-dc /mnt/antiX/etc/rc4.d/K01samba-ad-dc >/dev/null 2>&1");
        shell.run("mv -f /mnt/antiX/etc/rc3.d/S*samba-ad-dc /mnt/antiX/etc/rc3.d/K01samba-ad-dc >/dev/null 2>&1");
        shell.run("mv -f /mnt/antiX/etc/rc2.d/S*samba-ad-dc /mnt/antiX/etc/rc2.d/K01samba-ad-dc >/dev/null 2>&1");
        shell.run("mv -f /mnt/antiX/etc/rc5.d/S*nmbd /mnt/antiX/etc/rc5.d/K01nmbd >/dev/null 2>&1");
        shell.run("mv -f /mnt/antiX/etc/rc4.d/S*nmbd /mnt/antiX/etc/rc4.d/K01nmbd >/dev/null 2>&1");
        shell.run("mv -f /mnt/antiX/etc/rc3.d/S*nmbd /mnt/antiX/etc/rc3.d/K01nmbd >/dev/null 2>&1");
        shell.run("mv -f /mnt/antiX/etc/rc2.d/S*nmbd /mnt/antiX/etc/rc2.d/K01nmbd >/dev/null 2>&1");
    }

    char rbuf[4];
    if (readlink("/mnt/antiX/sbin/init", rbuf, sizeof(rbuf)) >= 0) { // systemd check
        if (!sambaCheckBox->isChecked()) {
            runCmd("chroot /mnt/antiX systemctl disable smbd");
            runCmd("chroot /mnt/antiX systemctl disable nmbd");
            runCmd("chroot /mnt/antiX systemctl disable samba-ad-dc");
            runCmd("chroot /mnt/antiX systemctl mask smbd");
            runCmd("chroot /mnt/antiX systemctl mask nmbd");
            runCmd("chroot /mnt/antiX systemctl mask samba-ad-dc");
        }
    }

    //replaceStringInFile(PROJECTSHORTNAME + "1", computerNameEdit->text(), "/mnt/antiX/etc/hosts");
    QString cmd;
    cmd = QString("sed -i 's/'\"$(grep 127.0.0.1 /etc/hosts | grep -v localhost | head -1 | awk '{print $2}')\"'/" + computerNameEdit->text() + "/' /mnt/antiX/etc/hosts");
    shell.run(cmd);
    cmd = QString("echo \"%1\" | cat > /mnt/antiX/etc/hostname").arg(computerNameEdit->text());
    shell.run(cmd);
    cmd = QString("echo \"%1\" | cat > /mnt/antiX/etc/mailname").arg(computerNameEdit->text());
    shell.run(cmd);
    cmd = QString("sed -i 's/.*send host-name.*/send host-name \"%1\";/g' /mnt/antiX/etc/dhcp/dhclient.conf").arg(computerNameEdit->text());
    shell.run(cmd);
    cmd = QString("echo \"%1\" | cat > /mnt/antiX/etc/defaultdomain").arg(computerDomainEdit->text());
    shell.run(cmd);
    return true;
}

void MInstall::setLocale()
{
    qDebug() << "+++" << __PRETTY_FUNCTION__ << "+++";
    if (phase < 0) return;
    QString cmd2;
    QString cmd;

    //locale
    cmd = QString("chroot /mnt/antiX /usr/sbin/update-locale \"LANG=%1\"").arg(localeCombo->currentData().toString());
    qDebug() << "Update locale";
    runCmd(cmd);
    cmd = QString("Language=%1").arg(localeCombo->currentData().toString());

    // /etc/localtime is either a file or a symlink to a file in /usr/share/zoneinfo. Use the one selected by the user.
    //replace with link
    cmd = QString("ln -nfs /usr/share/zoneinfo/%1 /mnt/antiX/etc/localtime").arg(timezoneCombo->currentText());
    shell.run(cmd);
    cmd = QString("ln -nfs /usr/share/zoneinfo/%1 /etc/localtime").arg(timezoneCombo->currentText());
    shell.run(cmd);
    // /etc/timezone is text file with the timezone written in it. Write the user-selected timezone in it now.
    cmd = QString("echo %1 > /mnt/antiX/etc/timezone").arg(timezoneCombo->currentText());
    shell.run(cmd);
    cmd = QString("echo %1 > /etc/timezone").arg(timezoneCombo->currentText());
    shell.run(cmd);

    // timezone
    shell.run("cp -f /etc/default/rcS /mnt/antiX/etc/default");
    // Set clock to use LOCAL
    if (localClockCheckBox->isChecked()) {
        shell.run("echo '0.0 0 0.0\n0\nLOCAL' > /etc/adjtime");
    } else {
        shell.run("echo '0.0 0 0.0\n0\nUTC' > /etc/adjtime");
    }
    shell.run("hwclock --hctosys");
    shell.run("cp -f /etc/adjtime /mnt/antiX/etc/");

    // Set clock format
    if (radio12h->isChecked()) {
        //mx systems
        shell.run("sed -i '/data0=/c\\data0=%l:%M' /home/demo/.config/xfce4/panel/xfce4-orageclock-plugin-1.rc");
        shell.run("sed -i '/data0=/c\\data0=%l:%M' /mnt/antiX/etc/skel/.config/xfce4/panel/xfce4-orageclock-plugin-1.rc");
        //antix systems
        shell.run("sed -i 's/%H:%M/%l:%M/g' /mnt/antiX/etc/skel/.icewm/preferences");
        shell.run("sed -i 's/%k:%M/%l:%M/g' /mnt/antiX/etc/skel/.fluxbox/init");
        shell.run("sed -i 's/%k:%M/%l:%M/g' /mnt/antiX/etc/skel/.jwm/tray");
    } else {
        //mx systems
        shell.run("sed -i '/data0=/c\\data0=%H:%M' /home/demo/.config/xfce4/panel/xfce4-orageclock-plugin-1.rc");
        shell.run("sed -i '/data0=/c\\data0=%H:%M' /mnt/antiX/etc/skel/.config/xfce4/panel/xfce4-orageclock-plugin-1.rc");
        //antix systems
        shell.run("sed -i 's/%H:%M/%H:%M/g' /mnt/antiX/etc/skel/.icewm/preferences");
        shell.run("sed -i 's/%k:%M/%k:%M/g' /mnt/antiX/etc/skel/.fluxbox/init");
        shell.run("sed -i 's/%k:%M/%k:%M/g' /mnt/antiX/etc/skel/.jwm/tray");
    }

    // localize repo
    qDebug() << "Localize repo";
    runCmd("chroot /mnt/antiX localize-repo default");
}

void MInstall::stashServices(bool save)
{
    QTreeWidgetItemIterator it(csView);
    while (*it) {
        if ((*it)->parent() != NULL) {
            (*it)->setCheckState(save?2:0, (*it)->checkState(save?0:2));
        }
        ++it;
    }
}

void MInstall::setServices()
{
    qDebug() << "+++" << __PRETTY_FUNCTION__ << "+++";
    if (phase < 0) return;

    // systemd check
    char rbuf[4];
    bool systemd = (readlink("/mnt/antiX/sbin/init", rbuf, sizeof(rbuf)) >= 0);

    QTreeWidgetItemIterator it(csView);
    while (*it) {
        if ((*it)->parent() != NULL) {
            QString service = (*it)->text(0);
            qDebug() << "Service: " << service;
            if ((*it)->checkState(0) == Qt::Checked) {
                if (!systemd) {
                    runCmd("chroot /mnt/antiX update-rc.d " + service + " enable");
                } else {
                    runCmd("chroot /mnt/antiX systemctl enable " + service);
                }
            } else {
                if (!systemd) {
                    runCmd("chroot /mnt/antiX update-rc.d " + service + " disable");
                } else {
                    runCmd("chroot /mnt/antiX systemctl disable " + service);
                    runCmd("chroot /mnt/antiX systemctl mask " + service);
                }
            }
        }
        ++it;
    }

    if (!isInsideVB()) {
        shell.run("mv -f /mnt/antiX/etc/rc5.d/S*virtualbox-guest-utils /mnt/antiX/etc/rc5.d/K01virtualbox-guest-utils >/dev/null 2>&1");
        shell.run("mv -f /mnt/antiX/etc/rc4.d/S*virtualbox-guest-utils /mnt/antiX/etc/rc4.d/K01virtualbox-guest-utils >/dev/null 2>&1");
        shell.run("mv -f /mnt/antiX/etc/rc3.d/S*virtualbox-guest-utils /mnt/antiX/etc/rc3.d/K01virtualbox-guest-utils >/dev/null 2>&1");
        shell.run("mv -f /mnt/antiX/etc/rc2.d/S*virtualbox-guest-utils /mnt/antiX/etc/rc2.d/K01virtualbox-guest-utils >/dev/null 2>&1");
        shell.run("mv -f /mnt/antiX/etc/rcS.d/S*virtualbox-guest-x11 /mnt/antiX/etc/rcS.d/K21virtualbox-guest-x11 >/dev/null 2>&1");
    }
}

void MInstall::failUI(const QString &msg)
{
    if (phase >= 0) {
        this->setEnabled(false);
        QMessageBox::critical(this, QString::null, msg);
        updateCursor(Qt::WaitCursor);
    }
}


// logic displaying pages
int MInstall::showPage(int curr, int next)
{
    if (next == 3 && ixPageRefAdvancedFDE != 0) { // at Step_FDE
        return next;
    }

    if (next == 2 && curr == 1) { // at Step_Disk (forward)
        if (entireDiskButton->isChecked()) {
            if (checkBoxEncryptAuto->isChecked() && !checkPassword(FDEpassword->text())) {
                return curr;
            }
            QString drv = "/dev/" + diskCombo->currentText().section(" ", 0, 0);
            QString msg = tr("OK to format and use the entire disk (%1) for %2?").arg(drv).arg(PROJECTNAME);
            int ans = QMessageBox::warning(this, QString::null, msg,
                                           tr("Yes"), tr("No"));
            if (ans != 0) { // don't format - stop install
                return curr;
            }
            return 4; // Go to Step_Progress
        }
    } else if (next == 3 && curr == 2) { // at Step_Partition (fwd)
        if (!validateChosenPartitions()) {
            return curr;
        }
        if (!saveHomeBasic()) {
            const QString &msg = tr("The data in /home cannot be preserved because the required information could not be obtained.") + "\n"
                    + tr("If the partition containing /home is encrypted, please ensure the correct \"Encrypt\" boxes are selected, and that the entered password is correct.") + "\n"
                    + tr("The installer cannot encrypt an existing /home directory or partition.");
            QMessageBox::critical(this, QString::null, msg);
            return curr;
        }
        return 4; // Go to Step_Progress
    } else if (curr == 3) { // at Step_FDE
        if (next == 4) { // Forward
            saveAdvancedFDE();
        } else { // Backward
            loadAdvancedFDE();
        }
        next = ixPageRefAdvancedFDE;
        ixPageRefAdvancedFDE = 0;
        return next;
    } else if (next == 3 && curr == 4) { // at Step_Progress (backward)
        if (haveSnapshotUserAccounts) {
            return 8; // skip Step_User_Accounts and go to Step_Localization
        }
        return 9; // go to Step_Users
    } else if (next == 6 && curr == 5) { // at Step_Boot screen (forward)
        return next + 1; // skip Services screen
    } else if (next == 10 && curr == 9) { // at Step_User_Accounts (forward)
        if (!validateUserInfo()) {
            return curr;
        }
        haveSysConfig = true;
        next = 4;
    } else if (next == 8 && curr == 7) { // at Step_Network (forward)
        if (!validateComputerName()) {
            return curr;
        }
    } else if (next == 6 && curr == 7) { // at Step_Network (backward)
       return next - 1; // skip Services screen
    } else if (next == 9 && curr == 8) { // at Step_Localization (forward)
        if (haveSnapshotUserAccounts) {
            haveSysConfig = true;
            next = 4; // Continue
        }
    } else if (curr == 6) { // at Step_Services
        stashServices(next == 7);
        return 8; // goes back to the screen that called Services screen
    }
    return next;
}

void MInstall::pageDisplayed(int next)
{
    QString val;

    // progress bar shown only for install and configuration pages.
    installBox->setVisible(next >= 4 && next <= 9);

    if(next >= 5 && next <= 9) {
        haveSysConfig = false; // (re)editing configuration
        ixTipStart = ixTip;
    }

    switch (next) {
    case 1: // choose disk
        ((MMain *)mmn)->setHelpText(tr("<p><b>General Instructions</b><br/>BEFORE PROCEEDING, CLOSE ALL OTHER APPLICATIONS.</p>"
                                       "<p>On each page, please read the instructions, make your selections, and then click on Next when you are ready to proceed. "
                                       "You will be prompted for confirmation before any destructive actions are performed.</p>"
                                       "<p>Installation requires about %1 of space. %2 or more is preferred. "
                                       "You can use the entire disk or you can put the installation on existing partitions. </p>"
                                       "<p>If you are running Mac OS or Windows OS (from Vista onwards), you may have to use that system's software to set up partitions and boot manager before installing.</p>"
                                       "<p>The ext2, ext3, ext4, jfs, xfs, btrfs and reiserfs Linux filesystems are supported and ext4 is recommended.</p>").arg(MIN_INSTALL_SIZE).arg(PREFERRED_MIN_INSTALL_SIZE) + tr(""
                                       "<p>Autoinstall will place home on the root partition.</p>") + tr(""
                                       "<p><b>Encryption</b><br/>Encryption is possible via LUKS.  A password is required (8 characters minimum length)</p>") + tr(""
                                       "<p>A separate unencrypted boot partition is required. For additional settings including cipher selection, use the <b>Edit advanced encryption settings</b> button.</p>") + tr(""
                                       "<p>When encryption is used with autoinstall, the separate boot partition will be automatically created</p>"));
        if (diskCombo->count() == 0 || phase < 0) {
            updateCursor(Qt::WaitCursor);
            updateDiskInfo();
        }
        phase = 0;
        this->setEnabled(true);
        updateCursor();
        break;

    case 2:  // choose partition
        updateCursor(Qt::WaitCursor);
        ((MMain *)mmn)->setHelpText(tr("<p><b>Limitations</b><br/>Remember, this software is provided AS-IS with no warranty what-so-ever. "
                                       "It's solely your responsibility to backup your data before proceeding.</p>"
                                       "<p><b>Choose Partitions</b><br/>%1 requires a root partition. The swap partition is optional but highly recommended. If you want to use the Suspend-to-Disk feature of %1, you will need a swap partition that is larger than your physical memory size.</p>"
                                       "<p>If you choose a separate /home partition it will be easier for you to upgrade in the future, but this will not be possible if you are upgrading from an installation that does not have a separate home partition.</p>"
                                       "<p><b>Upgrading</b><br/>To upgrade from an existing Linux installation, select the same home partition as before and check the preference to preserve data in /home.</p>"
                                       "<p>If you are preserving an existing /home directory tree located on your root partition, the installer will not reformat the root partition. "
                                       "As a result, the installation will take much longer than usual.</p>"
                                       "<p><b>Preferred Filesystem Type</b><br/>For %1, you may choose to format the partitions as ext2, ext3, ext4, jfs, xfs, btrfs or reiser. </p>"
                                       "<p>Additional compression options are available for drives using btrfs. "
                                       "Lzo is fast, but the compression is lower. Zlib is slower, with higher compression.</p>"
                                       "<p><b>Bad Blocks</b><br/>If you choose ext2, ext3 or ext4 as the format type, you have the option of checking and correcting for bad blocks on the drive. "
                                       "The badblock check is very time consuming, so you may want to skip this step unless you suspect that your drive has bad blocks.</p>").arg(PROJECTNAME)+ tr(""
                                       "<p><b>Encryption</b><br/>Encryption is possible via LUKS.  A password is required (8 characters minimum length)</p>") + tr(""
                                       "<p>A separate unencrypted boot partition is required. For additional settings including cipher selection, use the <b>Edit advanced encryption settings</b> button.</p>"));
        updatePartInfo();
        updateCursor();
        break;

    case 3: // advanced encryption settings
        ((MMain *)mmn)->setHelpText("<p><b>"
                                    + tr("Advanced Encryption Settings") + "</b><br/>" + tr("This page allows fine-tuning of LUKS encrypted partitions.") + "<br/>"
                                    + tr("In most cases, the defaults provide a practical balance between security and performance that is suitable for sensitive applications.")
                                    + "</p><p>"
                                    + tr("This text covers the basics of the parameters used with LUKS, but is not meant to be a comprehensive guide to cryptography.") + "<br/>"
                                    + tr("Altering any of these settings without a sound knowledge in cryptography may result in weak encryption being used.") + "<br/>"
                                    + tr("Editing a field will often affect the available options below it. The fields below may be automatically changed to recommended values.") + "<br/>"
                                    + tr("Whilst better performance or higher security may be obtained by changing settings from their recommended values, you do so entirely at your own risk.")
                                    + "</p><p>"
                                    + tr("You can use the <b>Benchmark</b> button (which runs <i>cryptsetup benchmark</i> in its own terminal window) to compare the performance of common combinations of hashes, ciphers and chain modes.") + "<br/>"
                                    + tr("Please note that <i>cryptsetup benchmark</i> does not cover all the combinations or selections possible, and generally covers the most commonly used selections.")
                                    + "</p><p>"
                                    + "<b>" + tr("Cipher") + "</b><br/>" + tr("A variety of ciphers are available.") + "<br/>"
                                    + "<b>Serpent</b> " + tr("was one of the five AES finalists. It is considered to have a higher security margin than Rijndael and all the other AES finalists. It performs better on some 64-bit CPUs.") + "<br/>"
                                    + "<b>AES</b> " + tr("(also known as <i>Rijndael</i>) is a very common cipher, and many modern CPUs include instructions specifically for AES, due to its ubiquity. Although Rijndael was selected over Serpent for its performance, no attacks are currently expected to be practical.") + "<br/>"
                                    + "<b>Twofish</b> " + tr("is the successor to Blowfish. It became one of the five AES finalists, although it was not selected for the standard.") + "<br/>"
                                    + "<b>CAST6</b> " + tr("(CAST-256) was a candidate in the AES contest, however it did not become a finalist.") + "<br/>"
                                    + "<b>Blowfish</b> " + tr("is a 64-bit block cipher created by Bruce Schneier. It is not recommended for sensitive applications as only CBC and ECB modes are supported. Blowfish supports key sizes between 32 and 448 bits that are multiples of 8.")
                                    + "</p><p>"
                                    + "<b>" + tr("Chain mode") + "</b><br/>" + tr("If blocks were all encrypted using the same key, a pattern may emerge and be able to predict the plain text.") + "<br />"
                                    + "<b>XTS</b> " + tr("XEX-based Tweaked codebook with ciphertext Stealing) is a modern chain mode, which supersedes CBC and EBC. It is the default (and recommended) chain mode. Using ESSIV over Plain64 will incur a performance penalty, with negligble known security gain.") + "<br />"
                                    + "<b>CBC</b> " + tr("(Cipher Block Chaining) is simpler than XTS, but vulnerable to a padding oracle attack (somewhat mitigated by ESSIV) and is not recommended for sensitive applications.") + "<br />"
                                    + "<b>ECB</b> " + tr("(Electronic CodeBook) is less secure than CBC and should not be used for sensitive applications.")
                                    + "</p><p>"
                                    + "<b>" + tr("IV generator") + "</b><br/>" + tr("For XTS and CBC, this selects how the <b>i</b>nitialisation <b>v</b>ector is generated. <b>ESSIV</b> requires a hash function, and for that reason, a second drop-down box will be available if this is selected. The hashes available depend on the selected cipher.") + "<br/>"
                                    + tr("ECB mode does not use an IV, so these fields will all be disabled if ECB is selected for the chain mode.")
                                    + "</p><p>"
                                    + "<b>" + tr("Key size") + "</b><br/>" + tr("Sets the key size in bits. Available key sizes are limited by the cipher and chain mode.") + "<br/>"
                                    + tr("The XTS cipher chain mode splits the key in half (for example, AES-256 in XTS mode requires a 512-bit key size).")
                                    + "</p><p>"
                                    + "<b>" + tr("LUKS key hash") + "</b><br/>" + tr("The hash used for PBKDF2 and for the AF splitter.") + " <br/>"
                                    + tr("SHA-1 and RIPEMD-160 are no longer recommended for sensitive applications as they have been found to be broken.")
                                    + "</p><p>"
                                    + "<b>" + tr("Kernel RNG") + "</b><br/>" + tr("Sets which kernel random number generator will be used to create the master key volume key (which is a long-term key).") + "<br/>"
                                    + tr("Two options are available: /dev/<b>random</b> which blocks until sufficient entropy is obtained (can take a long time in low-entropy situations), and /dev/<b>urandom</b> which will not block even if there is insufficient entropy (possibly weaker keys).")
                                    + "</p><p>"
                                    + "<b>" + tr("KDF round time</b><br/>The amount of time (in milliseconds) to spend with PBKDF2 passphrase processing.") + "<br/>"
                                    + tr("A value of 0 selects the compiled-in default (run <i>cryptsetup --help</i> for details).") + "<br/>"
                                    + tr("If you have a slow machine, you may wish to increase this value for extra security, in exchange for time taken to unlock a volume after a passphrase is entered.")
                                    + "</p>");
        break;

    case 4: // installation step
        if (phase == 0) {
            updateCursor(Qt::BusyCursor); // restored after entering boot config screen
            tipsEdit->setText("<p><b>" + tr("Additional information required") + "</b><br/>"
                              + tr("The %1 installer is about to request more information from you. Please wait.").arg(PROJECTNAME)
                              + "</p>");
        } else if(ixTipStart >= 0) {
            iLastProgress = progressBar->value();
            on_progressBar_valueChanged(iLastProgress);
        }
        ((MMain *)mmn)->setHelpText("<p><b>" + tr("Installation in Progress") + "</b><br/>"
                                    + tr("%1 is installing.  For a fresh install, this will probably take 3-20 minutes, depending on the speed of your system and the size of any partitions you are reformatting.").arg(PROJECTNAME)
                                    + "</p><p>"
                                    + tr("If you click the Abort button, the installation will be stopped as soon as possible.")
                                    + "</p><p>"
                                    + "<b>" + tr("Change settings while you wait") + "</b><br/>"
                                    + tr("While %1 is being installed, you can click on the <b>Next</b> or <b>Back</b> buttons to enter other information required for the installation.").arg(PROJECTNAME)
                                    + "</p><p>"
                                    + tr("Complete these steps at your own pace. The installer will wait for your input if necessary.")
                                    + "</p>");
        widgetStack->setEnabled(true);
        if (phase > 0 && phase < 4) {
            backButton->setEnabled(haveSysConfig);
            nextButton->setEnabled(!haveSysConfig);
        }
        if (!processNextPhase() && phase > -2) {
            cleanup(false);
            gotoPage(1);
        }
        return; // avoid enabling both Back and Next buttons at the end
        break;
    case 5: // set bootloader
        ((MMain *)mmn)->setHelpText(tr("<p><b>Select Boot Method</b><br/> %1 uses the GRUB bootloader to boot %1 and MS-Windows. "
                                       "<p>By default GRUB2 is installed in the Master Boot Record (MBR) or ESP (EFI System Partition for 64-bit UEFI boot systems) of your boot drive and replaces the boot loader you were using before. This is normal.</p>"
                                       "<p>If you choose to install GRUB2 to Partition Boot Record (PBR) instead, then GRUB2 will be installed at the beginning of the specified partition. This option is for experts only.</p>"
                                       "<p>If you uncheck the Install GRUB box, GRUB will not be installed at this time. This option is for experts only.</p>").arg(PROJECTNAME));

        updateCursor(); // restore wait cursor set in install screen
        break;

    case 6: // set services
        ((MMain *)mmn)->setHelpText(tr("<p><b>Common Services to Enable</b><br/>Select any of these common services that you might need with your system configuration and the services will be started automatically when you start %1.</p>").arg(PROJECTNAME));
        break;

    case 7: // set computer name
        ((MMain *)mmn)->setHelpText(tr("<p><b>Computer Identity</b><br/>The computer name is a common unique name which will identify your computer if it is on a network. "
                                       "The computer domain is unlikely to be used unless your ISP or local network requires it.</p>"
                                       "<p>The computer and domain names can contain only alphanumeric characters, dots, hyphens. They cannot contain blank spaces, start or end with hyphens</p>"
                                       "<p>The SaMBa Server needs to be activated if you want to use it to share some of your directories or printer "
                                       "with a local computer that is running MS-Windows or Mac OSX.</p>"));
        break;

    case 8: // set localization, clock, services button
        ((MMain *)mmn)->setHelpText(tr("<p><b>Localization Defaults</b><br/>Set the default keyboard and locale. These will apply unless they are overridden later by the user.</p>"
                                       "<p><b>Configure Clock</b><br/>If you have an Apple or a pure Unix computer, by default the system clock is set to GMT or Universal Time. To change, check the box for 'System clock uses LOCAL.'</p>"
                                       "<p><b>Timezone Settings</b><br/>The system boots with the timezone preset to GMT/UTC. To change the timezone, after you reboot into the new installation, right click on the clock in the Panel and select Properties.</p>"
                                       "<p><b>Service Settings</b><br/>Most users should not change the defaults. Users with low-resource computers sometimes want to disable unneeded services in order to keep the RAM usage as low as possible. Make sure you know what you are doing! "));
        break;

    case 9: // set username and passwords
        ((MMain *)mmn)->setHelpText(tr("<p><b>Default User Login</b><br/>The root user is similar to the Administrator user in some other operating systems. "
                                       "You should not use the root user as your daily user account. "
                                       "Please enter the name for a new (default) user account that you will use on a daily basis. "
                                       "If needed, you can add other user accounts later with %1 User Manager. </p>"
                                       "<p><b>Passwords</b><br/>Enter a new password for your default user account and for the root account. "
                                       "Each password must be entered twice.</p>").arg(PROJECTNAME));
        if (!nextFocus) nextFocus = userNameEdit;
        break;

    case 10: // done
        closeButton->setEnabled(false);
        ((MMain *)mmn)->setHelpText(tr("<p><b>Congratulations!</b><br/>You have completed the installation of %1</p>"
                                       "<p><b>Finding Applications</b><br/>There are hundreds of excellent applications installed with %1 "
                                       "The best way to learn about them is to browse through the Menu and try them. "
                                       "Many of the apps were developed specifically for the %1 project. "
                                       "These are shown in the main menus. "
                                       "<p>In addition %1 includes many standard Linux applications that are run only from the command line and therefore do not show up in the Menu.</p>").arg(PROJECTNAME));
        break;

    default:
        // case 0 or any other
        ((MMain *)mmn)->setHelpText("<p><b>" + tr("Enjoy using %1</b></p>").arg(PROJECTNAME) + "\n\n " + tr("<p><b>Support %1</b><br/>"
                                                                                                  "%1 is supported by people like you. Some help others at the "
                                                                                                  "support forum - %2 - or translate help files into different "
                                                                                                  "languages, or make suggestions, write documentation, or help test new software.</p>").arg(PROJECTNAME).arg(PROJECTFORUM));
        nextButton->setDefault(true);
        break;
    }

    backButton->setEnabled(true);
    nextButton->setEnabled(true);
}

void MInstall::gotoPage(int next)
{
    backButton->setEnabled(false);
    nextButton->setEnabled(false);
    widgetStack->setEnabled(false);
    int curr = widgetStack->currentIndex();
    next = showPage(curr, next);

    // modify ui for standard cases
    if (next == 0) {
        // entering first page
        backButton->hide();
    } else {
        // default
        backButton->show();
    }

    int c = widgetStack->count();
    QSize isize = nextButton->iconSize();
    isize.setWidth(isize.height());
    if (next >= c-1) {
        // entering the last page
        backButton->hide();
        nextButton->setText(tr("Finish"));
    } else if (next == 3 || next == 6){
        // Advanced Encryption Settings and Services pages
        isize.setWidth(0);
        nextButton->setText(tr("OK"));
    } else {
        nextButton->setText(tr("Next"));
    }
    nextButton->setIconSize(isize);
    if (next > c-1) {
        // finished
        updateCursor(Qt::WaitCursor);
        cleanup();
        if (checkBoxExitReboot->isChecked()) {
            shell.run("/usr/local/bin/persist-config --shutdown --command reboot &");
        }
        qApp->exit(0);
        return;
    }
    // display the next page
    widgetStack->setCurrentIndex(next);
    qApp->processEvents();

    // anything to do after displaying the page
    pageDisplayed(next);
    widgetStack->setEnabled(true);
    if (nextFocus) {
        nextFocus->setFocus();
        nextFocus = NULL;
    }
}

void MInstall::firstRefresh(QDialog *main)
{
    qDebug() << "+++" << __PRETTY_FUNCTION__ << "+++";
    mmn = main;
    if (!pretend) {
        // disable automounting in Thunar
        auto_mount = shell.getOutput("command -v xfconf-query >/dev/null && su $(logname) -c 'xfconf-query --channel thunar-volman --property /automount-drives/enabled'");
        shell.run("command -v xfconf-query >/dev/null && su $(logname) -c 'xfconf-query --channel thunar-volman --property /automount-drives/enabled --set false'");
    }

    rootTypeCombo->setEnabled(false);
    homeTypeCombo->setEnabled(false);
    checkBoxEncryptRoot->setEnabled(false);
    checkBoxEncryptHome->setEnabled(false);
    rootLabelEdit->setEnabled(false);
    homeLabelEdit->setEnabled(false);
    swapLabelEdit->setEnabled(false);

    FDEpassword->hide();
    FDEpassword2->hide();
    labelFDEpass->hide();
    labelFDEpass2->hide();
    buttonAdvancedFDE->hide();
    gbEncrPass->hide();
    existing_partitionsButton->hide();

    gotoPage(0);
}

void MInstall::updatePartitionWidgets()
{
    qDebug() << "+++" << __PRETTY_FUNCTION__ << "+++";

    // see if there are any partitions in the system
    bool foundPartitions = false;
    QFile file("/proc/partitions");
    if (file.open(QFile::ReadOnly | QFile::Text)) {
        QRegularExpression regexp("((x?[h,s,v]d[a-z]*)|((mmcblk|nvme).*p))[0-9]");
        while (!foundPartitions) {
            QString line(file.readLine());
            if (line.contains(regexp)) foundPartitions = true;
            else if (line.isEmpty()) break;
        }
        file.close();
    }

    if (foundPartitions) {
        existing_partitionsButton->show();
        existing_partitionsButton->setChecked(true);
    } else {
        existing_partitionsButton->hide();
        entireDiskButton->setChecked(true);
    }
}

// widget being shown
void MInstall::updateDiskInfo()
{
    qDebug() << "+++" << __PRETTY_FUNCTION__ << "+++";

    diskCombo->setEnabled(false);
    diskCombo->clear();
    diskCombo->addItem(tr("Loading..."));

    if (!pretend) runProc("/sbin/swapoff -a"); // kludge - live boot automatically activates swap
    runProc("/sbin/partprobe");
    updatePartitionWidgets();
    //  shell.run("umount -a 2>/dev/null");
    QString exclude = " --exclude=boot";
    if (INSTALL_FROM_ROOT_DEVICE) {
        exclude.clear();
    }
    listBootDrives = getCmdOuts("partition-info" + exclude + " --min-size=" + MIN_ROOT_DEVICE_SIZE + " -n drives");
    indexPartInfoDisk = -1;
    diskCombo->clear();
    diskCombo->addItems(listBootDrives);
    diskCombo->setCurrentIndex(0);
    diskCombo->setEnabled(true);
}

void MInstall::buildServiceList()
{
    qDebug() << "+++" << __PRETTY_FUNCTION__ << "+++";
    QLocale locale;
    QString lang = locale.bcp47Name().toUpper();

    //setup csView
    csView->header()->setMinimumSectionSize(150);
    csView->header()->resizeSection(0,150);

    QSettings services_desc("/usr/share/gazelle-installer-data/services.list", QSettings::NativeFormat);

    for (const QString &service : qAsConst(ENABLE_SERVICES)) {
        QString lang_str = (lang == "EN")? "" : "_" + lang;
        QStringList list = services_desc.value(service + lang_str).toStringList();
        if (list.size() != 2) {
            list = services_desc.value(service).toStringList(); // Use English definition
            if (list.size() != 2) {
                continue;
            }
        }
        QString category, description;
        category = list.at(0);
        description = list.at(1);

        if (QFile("/etc/init.d/" + service).exists()) {
            QList<QTreeWidgetItem *> found_items = csView->findItems(category, Qt::MatchExactly, 0);
            QTreeWidgetItem *top_item;
            QTreeWidgetItem *item;
            QTreeWidgetItem *parent;
            if (found_items.size() == 0) { // add top item if no top items found
                top_item = new QTreeWidgetItem(csView);
                top_item->setText(0, category);
                parent = top_item;
            } else {
                parent = found_items.last();
            }
            item = new QTreeWidgetItem(parent);
            item->setText(0, service);
            item->setText(1, description);
            item->setCheckState(0, Qt::Checked);
        }
    }
    csView->expandAll();
    csView->resizeColumnToContents(0);
    csView->resizeColumnToContents(1);
    stashServices(true);
}

/////////////////////////////////////////////////////////////////////////
// slots

void MInstall::on_passwordCheckBox_stateChanged(int state)
{
    if (state == Qt::Unchecked) {
        // don't show
        userPasswordEdit->setEchoMode(QLineEdit::Password);
        userPasswordEdit2->setEchoMode(QLineEdit::Password);
        rootPasswordEdit->setEchoMode(QLineEdit::Password);
        rootPasswordEdit2->setEchoMode(QLineEdit::Password);
    } else {
        // show
        userPasswordEdit->setEchoMode(QLineEdit::Normal);
        userPasswordEdit2->setEchoMode(QLineEdit::Normal);
        rootPasswordEdit->setEchoMode(QLineEdit::Normal);
        rootPasswordEdit2->setEchoMode(QLineEdit::Normal);
    }
}

void MInstall::on_nextButton_clicked()
{
    gotoPage(widgetStack->currentIndex() + 1);
}

void MInstall::on_backButton_clicked()
{
    gotoPage(widgetStack->currentIndex() - 1);
}

void MInstall::on_abortInstallButton_clicked()
{
    abort(false);
    QApplication::beep();
}

// clicking advanced button to go to Services page
void MInstall::on_viewServicesButton_clicked()
{
    gotoPage(6);
}

void MInstall::on_qtpartedButton_clicked()
{
    updateCursor(Qt::WaitCursor);
    nextButton->setEnabled(false);
    qtpartedButton->setEnabled(false);
    shell.run("[ -f /usr/sbin/gparted ] && /usr/sbin/gparted || /usr/bin/partitionmanager");
    runProc("/sbin/partprobe");
    updatePartitionWidgets();
    indexPartInfoDisk = -1; // invalidate existing partition info
    qtpartedButton->setEnabled(true);
    nextButton->setEnabled(true);
    updateCursor();
}

void MInstall::on_buttonBenchmarkFDE_clicked()
{
    shell.run("x-terminal-emulator -e bash -c \"/sbin/cryptsetup benchmark"
              " && echo && read -n 1 -srp 'Press any key to close the benchmark window.'\"");
}

// disk selection changed, rebuild dropdown menus
void MInstall::updatePartInfo()
{
    if (indexPartInfoDisk == diskCombo->currentIndex()) return;
    indexPartInfoDisk = diskCombo->currentIndex();

    if (phase < 1) {
        const QString &sloading = tr("Loading...");
        rootCombo->clear();
        swapCombo->clear();
        homeCombo->clear();
        bootCombo->clear();
        rootCombo->addItem(sloading);
        swapCombo->addItem(sloading);
        homeCombo->addItem(sloading);
        bootCombo->addItem(sloading);
    }

    QString drv = "/dev/" + diskCombo->currentText().section(" ", 0, 0);

    // build rootCombo
    QString exclude;
    if (!INSTALL_FROM_ROOT_DEVICE) {
        exclude = "boot,";
    }
    QStringList partitions = getCmdOuts(QString("partition-info -n --exclude=" + exclude + "swap --min-size=" + MIN_ROOT_DEVICE_SIZE + " %1").arg(drv));
    rootCombo->clear();
    if (partitions.size() > 0) {
        rootCombo->addItems(partitions);
    } else {
        rootCombo->addItem("none");
    }

    // build homeCombo for all disks
    partitions = getCmdOuts("partition-info all -n --exclude=" + exclude + "swap --min-size=1000");
    homeCombo->clear();
    homeCombo->addItem("root");
    homeCombo->addItems(partitions);

    // build swapCombo for all disks
    partitions = getCmdOuts("partition-info all -n --exclude=" + exclude);
    swapCombo->clear();
    swapCombo->addItem("none");
    swapCombo->addItems(partitions);

    // build bootCombo for all disks, exclude ESP (EFI)
    partitions = getCmdOuts("partition-info all -n --exclude=" + exclude + "efi --min-size=" + MIN_BOOT_DEVICE_SIZE);
    bootCombo->clear();
    bootCombo->addItem("root");
    bootCombo->addItems(partitions);

    on_rootCombo_activated(rootCombo->currentText());
}

// root partition changed, rebuild home, swap, boot combo boxes
void MInstall::on_rootCombo_activated(const QString &arg1)
{
    updatePartCombo(&prevItemRoot, arg1);
    rootLabelEdit->setEnabled(!arg1.isEmpty());
    rootTypeCombo->setEnabled(!arg1.isEmpty());
    checkBoxEncryptRoot->setEnabled(!arg1.isEmpty());
}

void MInstall::on_rootTypeCombo_activated(QString)
{
    if (rootTypeCombo->currentText().startsWith("ext") || rootTypeCombo->currentText() == "jfs") {
        badblocksCheck->setEnabled(true);
    } else {
        badblocksCheck->setEnabled(false);
    }
}

bool MInstall::abort(bool onclose)
{
    this->setEnabled(false);
    qDebug() << "+++" << __PRETTY_FUNCTION__ << "+++";
    // ask for confirmation when installing (except for some steps that don't need confirmation)
    if (phase > 0 && phase < 4) {
        if(QMessageBox::warning(this, tr("Confirmation"),
                                tr("The installation and configuration is incomplete.\n"
                                   "Do you really want to stop now?"),
                                QMessageBox::Yes | QMessageBox::No) == QMessageBox::No) {
            this->setEnabled(true);
            return false;
        }
    }
    updateCursor(Qt::WaitCursor);
    proc->terminate();
    QTimer::singleShot(5000, proc, SLOT(kill()));
    shell.terminate();
    QTimer::singleShot(1000, &shell, SLOT(kill()));
    // help the installer if it was stuck at the config pages
    if (onclose) {
        phase = -2;
    } else if (phase == 2 && !haveSysConfig) {
        phase = -1;
        gotoPage(1);
    } else {
        phase = -1;
    }
    return true;
}

bool MInstall::eventFilter(QObject* obj, QEvent* event)
{
    if (event->type() == QEvent::KeyPress) {
        QKeyEvent *keyEvent = static_cast<QKeyEvent*>(event);
        if (keyEvent->key() == Qt::Key_Escape) {
            if (installBox->isHidden()) { // don't close on installation by mistake
                on_closeButton_clicked();
            }
            return true;
        }
    }
    return QObject::eventFilter(obj, event);
}

// run before closing the app, do some cleanup
void MInstall::cleanup(bool endclean)
{
    qDebug() << "+++" << __PRETTY_FUNCTION__ << "+++";
    if (pretend) return;

    if (endclean) {
        shell.run("command -v xfconf-query >/dev/null && su $(logname) -c 'xfconf-query --channel thunar-volman --property /automount-drives/enabled --set " + auto_mount.toUtf8() + "'");
        shell.run("cp /var/log/minstall.log /mnt/antiX/var/log >/dev/null 2>&1");
        shell.run("rm -rf /mnt/antiX/mnt/antiX >/dev/null 2>&1");
    }
    shell.run("umount -l /mnt/antiX/proc >/dev/null 2>&1; umount -l /mnt/antiX/sys >/dev/null 2>&1; umount -l /mnt/antiX/dev/shm >/dev/null 2>&1; umount -l /mnt/antiX/dev >/dev/null 2>&1");
    shell.run("umount -lR /mnt/antiX >/dev/null 2>&1");

    if (isRootEncrypted) {
        shell.run("cryptsetup luksClose rootfs");
    }
    if (isHomeEncrypted) {
        shell.run("cryptsetup luksClose homefs");
    }
    if (isSwapEncrypted) {
        shell.run("cryptsetup luksClose swapfs");
    }
}

/////////////////////////////////////////////////////////////////////////
// copy process events

void MInstall::copyTime()
{
    struct statvfs svfs;
    const int progspace = iCopyBarB - iCopyBarA;
    int i = 0;
    if(iTargetInodes > 0 && statvfs("/mnt/antiX", &svfs) == 0) {
        i = (int)((svfs.f_files - svfs.f_ffree) / (iTargetInodes / progspace));
        if (i > progspace) {
            i = progspace;
        }
    } else {
        updateStatus(tr("Copy progress unknown. No file system statistics."), 0);
    }
    progressBar->setValue(i + iCopyBarA);
}

void MInstall::on_progressBar_valueChanged(int value)
{
    if (ixTipStart < 0 || widgetStack->currentIndex() != 4) {
        return; // no point displaying a new hint if it will be invisible
    }

    const int tipcount = 6;
    ixTip = tipcount;
    if (ixTipStart < tipcount) {
        int imax = (progressBar->maximum() - iLastProgress) / (tipcount - ixTipStart);
        if (imax != 0) {
            ixTip = ixTipStart + (value - iLastProgress) / imax;
        }
    }

    switch(ixTip)
    {
    case 0:
        tipsEdit->setText(tr("<p><b>Getting Help</b><br/>"
                             "Basic information about %1 is at %2.</p><p>"
                             "There are volunteers to help you at the %3 forum, %4</p>"
                             "<p>If you ask for help, please remember to describe your problem and your computer "
                             "in some detail. Usually statements like 'it didn't work' are not helpful.</p>").arg(PROJECTNAME).arg(PROJECTURL).arg(PROJECTSHORTNAME).arg(PROJECTFORUM));
        break;

    case 1:
        tipsEdit->setText(tr("<p><b>Repairing Your Installation</b><br/>"
                             "If %1 stops working from the hard drive, sometimes it's possible to fix the problem by booting from LiveDVD or LiveUSB and running one of the included utilities in %1 or by using one of the regular Linux tools to repair the system.</p>"
                             "<p>You can also use your %1 LiveDVD or LiveUSB to recover data from MS-Windows systems!</p>").arg(PROJECTNAME));
        break;

    case 2:
        tipsEdit->setText(tr("<p><b>Support %1</b><br/>"
                             "%1 is supported by people like you. Some help others at the "
                             "support forum - %2 - or translate help files into different "
                             "languages, or make suggestions, write documentation, or help test new software.</p>").arg(PROJECTNAME).arg(PROJECTFORUM));
        break;

    case 3:
        tipsEdit->setText(tr("<p><b>Adjusting Your Sound Mixer</b><br/>"
                             " %1 attempts to configure the sound mixer for you but sometimes it will be "
                             "necessary for you to turn up volumes and unmute channels in the mixer "
                             "in order to hear sound.</p> "
                             "<p>The mixer shortcut is located in the menu. Click on it to open the mixer. </p>").arg(PROJECTNAME));
        break;

    case 4:
        tipsEdit->setText(tr("<p><b>Keep Your Copy of %1 up-to-date</b><br/>"
                             "For more information and updates please visit</p><p> %2</p>").arg(PROJECTNAME).arg(PROJECTFORUM));
        break;

    default:
        tipsEdit->setText(tr("<p><b>Special Thanks</b><br/>Thanks to everyone who has chosen to support %1 with their time, money, suggestions, work, praise, ideas, promotion, and/or encouragement.</p>"
                             "<p>Without you there would be no %1.</p>"
                             "<p>%2 Dev Team</p>").arg(PROJECTNAME).arg(PROJECTSHORTNAME));
        break;
    }
}

void MInstall::on_closeButton_clicked()
{
    ((MMain *)mmn)->close();
}

void MInstall::setupkeyboardbutton()
{
    qDebug() << "+++" << __PRETTY_FUNCTION__ << "+++";
    QFile file("/etc/default/keyboard");
    if (file.open(QFile::ReadOnly | QFile::Text)) {
        while (!file.atEnd()) {
            QString line(file.readLine().trimmed());
            QLabel *plabel = NULL;
            if (line.startsWith("XKBMODEL")) plabel = labelModel;
            else if (line.startsWith("XKBLAYOUT")) plabel = labelLayout;
            else if (line.startsWith("XKBVARIANT")) plabel = labelVariant;
            if (plabel != NULL) {
                line = line.section('=', 1);
                line.replace(",", " ");
                line.remove(QChar('"'));
                plabel->setText(line);
            }
        }
        file.close();
    }
}

void MInstall::on_buttonSetKeyboard_clicked()
{
    mmn->hide();
    shell.run("fskbsetting");
    mmn->show();
    setupkeyboardbutton();
}

void MInstall::on_homeCombo_currentIndexChanged(const QString &arg1)
{
    if (!homeCombo->isEnabled() || arg1.isEmpty()) {
        return;
    }
    homeLabelEdit->setEnabled(arg1 != "root");
    homeTypeCombo->setEnabled(arg1 != "root");
    checkBoxEncryptHome->setEnabled(arg1 != "root");
    checkBoxEncryptHome->setChecked(checkBoxEncryptRoot->isChecked() && arg1 == "root");
    if (arg1 == "root") {
        homeTypeCombo->setCurrentIndex(rootTypeCombo->currentIndex());
    }
}

void MInstall::on_userPasswordEdit2_textChanged(const QString &arg1)
{
    QPalette pal = userPasswordEdit->palette();
    if (arg1 != userPasswordEdit->text()) {
        pal.setColor(QPalette::Base, QColor(255, 0, 0, 70));
    } else {
        pal.setColor(QPalette::Base, QColor(0, 255, 0, 40));
    }
    userPasswordEdit->setPalette(pal);
    userPasswordEdit2->setPalette(pal);
}

void MInstall::on_rootPasswordEdit2_textChanged(const QString &arg1)
{
    QPalette pal = rootPasswordEdit->palette();
    if (arg1 != rootPasswordEdit->text()) {
        pal.setColor(QPalette::Base, QColor(255, 0, 0, 70));
    } else {
        pal.setColor(QPalette::Base, QColor(0, 255, 0, 40));
    }
    rootPasswordEdit->setPalette(pal);
    rootPasswordEdit2->setPalette(pal);
}

void MInstall::on_userPasswordEdit_textChanged()
{
    userPasswordEdit2->clear();
    userPasswordEdit->setPalette(QApplication::palette());
    userPasswordEdit2->setPalette(QApplication::palette());
}

void MInstall::on_rootPasswordEdit_textChanged()
{
    rootPasswordEdit2->clear();
    rootPasswordEdit->setPalette(QApplication::palette());
    rootPasswordEdit2->setPalette(QApplication::palette());
}

void MInstall::on_checkBoxEncryptAuto_toggled(bool checked)
{
    FDEpassword->clear();
    FDEpassword2->clear();
    nextButton->setDisabled(checked);
    FDEpassword->setVisible(checked);
    FDEpassword2->setVisible(checked);
    labelFDEpass->setVisible(checked);
    labelFDEpass2->setVisible(checked);
    buttonAdvancedFDE->setVisible(checked);
    grubPbrButton->setDisabled(checked);

    if (checked) {
        FDEpassword->setFocus();
    }
}

void MInstall::on_existing_partitionsButton_clicked(bool checked)
{
    checkBoxEncryptAuto->setChecked(!checked);
}

void MInstall::on_FDEpassword_textChanged()
{
    FDEpassword2->clear();
    FDEpassword->setPalette(QApplication::palette());
    FDEpassword2->setPalette(QApplication::palette());
    nextButton->setDisabled(true);
}

void MInstall::on_FDEpassword2_textChanged(const QString &arg1)
{
    QPalette pal = FDEpassword->palette();
    if (arg1 != FDEpassword->text()) {
        pal.setColor(QPalette::Base, QColor(255, 0, 0, 70));
        nextButton->setDisabled(true);
    } else {
        pal.setColor(QPalette::Base, QColor(0, 255, 0, 40));
        nextButton->setEnabled(true);
    }
    FDEpassword->setPalette(pal);
    FDEpassword2->setPalette(pal);
}

void MInstall::on_FDEpassCust_textChanged()
{
    FDEpassCust2->clear();
    FDEpassCust->setPalette(QApplication::palette());
    FDEpassCust2->setPalette(QApplication::palette());
    nextButton->setDisabled(true);
}

void MInstall::on_FDEpassCust2_textChanged(const QString &arg1)
{
    QPalette pal = FDEpassCust->palette();
    if (arg1 != FDEpassCust->text()) {
        pal.setColor(QPalette::Base, QColor(255, 0, 0, 70));
        nextButton->setDisabled(true);
    } else {
        pal.setColor(QPalette::Base, QColor(0, 255, 0, 40));
        nextButton->setEnabled(true);
    }
    FDEpassCust->setPalette(pal);
    FDEpassCust2->setPalette(pal);
}

void MInstall::on_checkBoxEncryptRoot_toggled(bool checked)
{
    if (homeCombo->currentText() == "root") { // if home on root set disable home encryption checkbox and set same encryption option
        checkBoxEncryptHome->setEnabled(false);
        checkBoxEncryptHome->setChecked(checked);
    }

    if (checked) {
        gbEncrPass->setVisible(true);
        checkBoxEncryptSwap->setChecked(true);
    } else {
        gbEncrPass->setVisible(checkBoxEncryptHome->isChecked());
        checkBoxEncryptSwap->setChecked(checkBoxEncryptHome->isChecked());
    }
}

void MInstall::on_checkBoxEncryptHome_toggled(bool checked)
{
    if (checked) {
        gbEncrPass->setVisible(true);
        checkBoxEncryptSwap->setChecked(true);
    } else {
        gbEncrPass->setVisible(checkBoxEncryptRoot->isChecked());
        checkBoxEncryptSwap->setChecked(checkBoxEncryptRoot->isChecked());
    }
}

void MInstall::on_checkBoxEncryptSwap_toggled(bool checked)
{
    nextButton->setDisabled(checked);
    if (checked) {
        FDEpassCust2->clear();
        FDEpassCust->clear();
        FDEpassCust->setFocus();
        QMessageBox::warning(this, QString::null,
                             tr("This option also encrypts swap partition if selected, which will render the swap partition unable to be shared with other installed operating systems."),
                             tr("OK"));
    }
}

void MInstall::on_buttonAdvancedFDE_clicked()
{
    ixPageRefAdvancedFDE = widgetStack->currentIndex();
    gotoPage(3);
}

void MInstall::on_buttonAdvancedFDECust_clicked()
{
    ixPageRefAdvancedFDE = widgetStack->currentIndex();
    gotoPage(3);
}

void MInstall::on_comboFDEcipher_currentIndexChanged(const QString &arg1)
{
    int hashgroup = 1;
    if (arg1 == "Blowfish") {
        hashgroup = 7;
        comboFDEchain->clear();
        comboFDEchain->addItem("CBC");
        comboFDEchain->addItem("ECB");
    } else {
        if (arg1 == "Serpent" || arg1 == "CAST6") hashgroup = 3;
        comboFDEchain->clear();
        comboFDEchain->addItem("XTS");
        comboFDEchain->addItem("CBC");
        comboFDEchain->addItem("ECB");
    }
    on_comboFDEchain_currentIndexChanged(comboFDEchain->currentText());

    comboFDEivhash->clear();
    if (hashgroup & 4) comboFDEivhash->addItem("SHA-384", QVariant("sha384"));
    if (hashgroup & 1) comboFDEivhash->addItem("SHA-256", QVariant("sha256"));
    if (hashgroup & 2) comboFDEivhash->addItem("SHA-224", QVariant("sha224"));
    if (hashgroup & 4) comboFDEivhash->addItem("Whirlpool-384", QVariant("wp384"));
    if (hashgroup & 1) comboFDEivhash->addItem("Whirlpool-256", QVariant("wp256"));
    if (hashgroup & 1) comboFDEivhash->addItem("Tiger", QVariant("tgr192"));
    if (hashgroup & 2) comboFDEivhash->addItem("Tiger/160", QVariant("tgr160"));
    if (hashgroup & 1) comboFDEivhash->addItem("Tiger/128", QVariant("tgr128"));
    if (hashgroup & 4) comboFDEivhash->addItem("RIPEMD-320", QVariant("rmd320"));
    if (hashgroup & 1) comboFDEivhash->addItem("RIPEMD-256", QVariant("rmd256"));
    comboFDEivhash->insertSeparator(100);
    if (hashgroup & 2) comboFDEivhash->addItem("RIPEMD-160", QVariant("rmd160"));
    if (hashgroup & 1) comboFDEivhash->addItem("RIPEMD-128", QVariant("rmd128"));
    if (hashgroup & 2) comboFDEivhash->addItem("SHA-1", QVariant("sha1"));
    if (hashgroup & 1) comboFDEivhash->addItem("MD5", QVariant("md5"));
    if (hashgroup & 1) comboFDEivhash->addItem("MD4", QVariant("md4"));
}

void MInstall::on_comboFDEchain_currentIndexChanged(const QString &arg1)
{
    int multKey = 1; // Multiplier for key sizes.

    if (arg1 == "ECB") {
        labelFDEivgen->setEnabled(false);
        comboFDEivgen->setEnabled(false);
        comboFDEivhash->setEnabled(false);
        comboFDEivgen->setCurrentIndex(-1);
    } else {
        int ixIVGen = -1;
        if (arg1 == "XTS") {
            multKey = 2;
            ixIVGen = comboFDEivgen->findText("Plain64");
        } else if (arg1 == "CBC") {
            ixIVGen = comboFDEivgen->findText("ESSIV");
        }
        if (ixIVGen >= 0) comboFDEivgen->setCurrentIndex(ixIVGen);
        labelFDEivgen->setEnabled(true);
        comboFDEivgen->setEnabled(true);
        comboFDEivhash->setEnabled(true);
    }

    const QString &strCipher = comboFDEcipher->currentText();
    if (strCipher == "Blowfish") {
        spinFDEkeysize->setSingleStep(multKey*8);
        spinFDEkeysize->setRange(multKey*64, multKey*448);
    } else if (strCipher == "Twofish" || strCipher == "AES") {
        spinFDEkeysize->setSingleStep(multKey*64);
        spinFDEkeysize->setRange(multKey*128, multKey*256);
    } else {
        spinFDEkeysize->setSingleStep(multKey*16);
        spinFDEkeysize->setRange(multKey*128, multKey*256);
    }
    spinFDEkeysize->setValue(65536);
}

void MInstall::on_comboFDEivgen_currentIndexChanged(const QString &arg1)
{
    if (arg1 == "ESSIV") {
        comboFDEivhash->show();
        comboFDEivhash->setCurrentIndex(0);
    } else {
        comboFDEivhash->hide();
    }
}

void MInstall::on_spinFDEkeysize_valueChanged(int i)
{
    bool entered = false;
    if (!entered) {
        entered = true;
        int iSingleStep = spinFDEkeysize->singleStep();
        int iMod = i % iSingleStep;
        if (iMod) spinFDEkeysize->setValue(i + (iSingleStep-iMod));
    }
}

void MInstall::on_homeCombo_activated(const QString &arg1)
{
    updatePartCombo(&prevItemHome, arg1);
}

void MInstall::on_swapCombo_activated(const QString &arg1)
{
    updatePartCombo(&prevItemSwap, arg1);
    swapLabelEdit->setEnabled(swapCombo->currentText() != "none");
}

void MInstall::on_bootCombo_activated(const QString &arg1)
{
    updatePartCombo(&prevItemBoot, arg1);
}

void MInstall::on_grubCheckBox_toggled(bool checked)
{
    if(checked) {
        if(listBootESP.count() > 0) {
            grubEspButton->setEnabled(true);
        }
        if(listBootDrives.count() > 0) {
            grubMbrButton->setEnabled(true);
        }
        if(listBootPart.count() > 0) {
            grubPbrButton->setDisabled(checkBoxEncryptRoot->isChecked());
        }
    } else {
        grubEspButton->setEnabled(false);
        grubMbrButton->setEnabled(false);
        grubPbrButton->setEnabled(false);
    }
    grubInsLabel->setEnabled(checked);
    grubBootLabel->setEnabled(checked);
    grubBootCombo->setEnabled(checked);
}

void MInstall::on_grubMbrButton_toggled()
{
    grubBootCombo->clear();
    grubBootCombo->addItems(listBootDrives);
    grubBootLabel->setText(tr("System boot disk:"));
}

void MInstall::on_grubPbrButton_toggled()
{
    grubBootCombo->clear();
    grubBootCombo->addItems(listBootPart);
    grubBootLabel->setText(tr("Partition to use:"));
}

void MInstall::on_grubEspButton_toggled()
{
    grubBootCombo->clear();
    grubBootCombo->addItems(listBootESP);
    grubBootLabel->setText(tr("Partition to use:"));
}

// build ESP list available to install GRUB
void MInstall::buildBootLists()
{
    // check if booted UEFI and if ESP(s) available
    grubEspButton->setEnabled(false);
    if (uefi) {
        const QStringList drives = shell.getOutput("partition-info drives --noheadings --exclude=boot | awk '{print $1}'").split("\n");

        // find ESP for all partitions on all drives
        listBootESP.clear();
        for (const QString &drive : drives) {
            if (isGpt("/dev/" + drive)) {
                QString esps = shell.getOutput("lsblk -nlo name,parttype /dev/" + drive + " | egrep '(c12a7328-f81f-11d2-ba4b-00a0c93ec93b|0xef)$' | awk '{print $1}'");
                if (!esps.isEmpty()) {
                    listBootESP << esps.split("\n");
                }
                // backup detection for drives that don't have UUID for ESP
                const QStringList backup_list = shell.getOutput("fdisk -l -o DEVICE,TYPE /dev/" + drive + " |grep 'EFI System' |cut -d\\  -f1 | cut -d/ -f3").split("\n");
                for (const QString &part : backup_list) {
                    if (!listBootESP.contains(part)) {
                        listBootESP << part;
                    }
                }
            }
        }

        // if GPT, and ESP exists
        if (listBootESP.count() > 0) {
            grubEspButton->setEnabled(true);
            grubEspButton->click();
        }
    }

    // build partition list available to install GRUB (in PBR)
    const QStringList part_list = shell.getOutput("partition-info all --noheadings --exclude=swap,boot,efi").split("\n");
    listBootPart.clear();
    for (const QString &part : part_list) {
        if (shell.run("partition-info is-linux=" + part.section(" ", 0, 0)) == 0) { // list only Linux partitions
            if (shell.getOutput("blkid /dev/" + part.section(" ", 0, 0) + " -s TYPE -o value") != "crypto_LUKS") { // exclude crypto_LUKS partitions
                listBootPart << part;
            }
        }
    }
}

/////////////////////////////////////////////////////////////////////////
// SafeCache Class - for temporarily caching sensitive files

SafeCache::SafeCache()
{
    reserve(16384);
}

SafeCache::~SafeCache()
{
    erase();
}

// to completely free the key use parameters NULL, 0
bool SafeCache::load(const char *filename, int length)
{
    bool ok = false;
    erase();

    // open and stat the file if specified
    int fd = open(filename, O_RDONLY);
    if (fd == -1) return false;
    struct stat statbuf;
    if (fstat(fd, &statbuf) == 0) {
        if (statbuf.st_size > 0 && (length < 0 || length > statbuf.st_size)) {
            if (statbuf.st_size > capacity()) length = capacity();
            else length = statbuf.st_size;
        }
    }
    if (length >= 0) resize(length);

    // read the the file (if specified) into the buffer
    length = size();
    int remain = length;
    while (remain > 0) {
        ssize_t chunk = read(fd, data() + (length - remain), remain);
        if (chunk < 0) goto ending;
        remain -= chunk;
        fsync(fd);
    }
    ok = true;

 ending:
    close(fd);
    return ok;
}

bool SafeCache::save(const char *filename, mode_t mode)
{
    bool ok = false;
    int fd = open(filename, O_CREAT|O_TRUNC|O_WRONLY, mode);
    if (fd == -1) return false;
    if (write(fd, constData(), size()) == size()) goto ending;
    if (fchmod(fd, mode) != 0) goto ending;
    ok = true;

 ending:
    close(fd);
    return ok;
}

void SafeCache::erase()
{
    fill(0xAA);
    fill(0x55);
}
