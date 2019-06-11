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
#include <QTimer>
#include <QProcess>
#include <fcntl.h>
#include <sys/statvfs.h>
#include <sys/stat.h>

#include "version.h"
#include "minstall.h"

MInstall::MInstall(const QStringList &args, const QString &cfgfile)
{
    setupUi(this);
    updateCursor(Qt::WaitCursor);
    this->setEnabled(false);
    setWindowFlags(Qt::Window); // for the close, min and max buttons
    installBox->hide();
    gotoPage(0);
    proc = new QProcess(this);

    brave = (args.contains("--brave"));
    pretend = (args.contains("--pretend") || args.contains("-p"));
    automatic = (args.contains("--auto") || args.contains("-a"));
    nocopy = (args.contains("--nocopy") || args.contains("-n"));
    sync = (args.contains("--sync") || args.contains("-s"));
    if (pretend) listHomes = args; // dummy existing homes

    // setup system variables
    QSettings settings("/usr/share/gazelle-installer-data/installer.conf", QSettings::NativeFormat);
    PROJECTNAME=settings.value("PROJECT_NAME").toString();
    PROJECTSHORTNAME=settings.value("PROJECT_SHORTNAME").toString();
    PROJECTVERSION=settings.value("VERSION").toString();
    PROJECTURL=settings.value("PROJECT_URL").toString();
    PROJECTFORUM=settings.value("FORUM_URL").toString();
    INSTALL_FROM_ROOT_DEVICE=settings.value("INSTALL_FROM_ROOT_DEVICE").toBool();
    MIN_ROOT_DEVICE_SIZE=settings.value("MIN_ROOT_DRIVE_SIZE").toLongLong() * 1048576;
    MIN_BOOT_DEVICE_SIZE=settings.value("MIN_BOOT_DRIVE_SIZE", "256").toLongLong() * 1048576;
    DEFAULT_HOSTNAME=settings.value("DEFAULT_HOSTNAME").toString();
    ENABLE_SERVICES=settings.value("ENABLE_SERVICES").toStringList();
    POPULATE_MEDIA_MOUNTPOINTS=settings.value("POPULATE_MEDIA_MOUNTPOINTS").toBool();
    MIN_INSTALL_SIZE=settings.value("MIN_INSTALL_SIZE").toString();
    PREFERRED_MIN_INSTALL_SIZE=settings.value("PREFERRED_MIN_INSTALL_SIZE").toString();
    REMOVE_NOSPLASH=settings.value("REMOVE_NOSPLASH", "false").toBool();
    setWindowTitle(tr("%1 Installer").arg(PROJECTNAME));

    // config file
    if (QFile::exists(cfgfile)) config = new QSettings(cfgfile, QSettings::NativeFormat, this);

    // set some distro-centric text
    copyrightBrowser->setPlainText(tr("%1 is an independent Linux distribution based on Debian Stable.\n\n%1 uses some components from MEPIS Linux which are released under an Apache free license. Some MEPIS components have been modified for %1.\n\nEnjoy using %1").arg(PROJECTNAME));
    remindersBrowser->setPlainText(tr("Support %1\n\n%1 is supported by people like you. Some help others at the support forum - %2, or translate help files into different languages, or make suggestions, write documentation, or help test new software.").arg(PROJECTNAME).arg(PROJECTFORUM));

    // ensure the help widgets are displayed correctly when started
    // Qt will delete the heap-allocated event object when posted
    qApp->postEvent(this, new QEvent(QEvent::PaletteChange));

    QTimer::singleShot(0, this, &MInstall::startup);
}

MInstall::~MInstall() {
}

// meant to be run after the installer becomes visible
void MInstall::startup()
{
    qDebug() << "+++" << __PRETTY_FUNCTION__ << "+++";

    // set default host name
    computerNameEdit->setText(DEFAULT_HOSTNAME);

    // advanced encryption settings page defaults
    on_comboFDEcipher_currentIndexChanged(comboFDEcipher->currentText());
    on_comboFDEchain_currentIndexChanged(comboFDEchain->currentText());
    on_comboFDEivgen_currentIndexChanged(comboFDEivgen->currentText());

    rootLabelEdit->setText("root" + PROJECTSHORTNAME + PROJECTVERSION);
    homeLabelEdit->setText("home" + PROJECTSHORTNAME);
    swapLabelEdit->setText("swap" + PROJECTSHORTNAME);

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

    setupkeyboardbutton();
    updatePartitionWidgets();
    manageConfig(ConfigLoadA);
    stashAdvancedFDE(true);

    if (!pretend) {
        // disable automounting in Thunar
        auto_mount = getCmdOut("command -v xfconf-query >/dev/null && su $(logname) -c 'xfconf-query --channel thunar-volman --property /automount-drives/enabled'");
        execute("command -v xfconf-query >/dev/null && su $(logname) -c 'xfconf-query --channel thunar-volman --property /automount-drives/enabled --set false'", false);
    }
    this->setEnabled(true);
    updateCursor();
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

bool MInstall::execute(const QString &cmd, const bool rawexec, const QByteArray &input)
{
    if (phase < 0) return false;
    qDebug() << cmd;
    QEventLoop eloop;
    connect(proc, static_cast<void (QProcess::*)(int)>(&QProcess::finished), &eloop, &QEventLoop::quit);
    if (rawexec) proc->start(cmd);
    else proc->start("/bin/bash", QStringList() << "-c" << cmd);
    if (!input.isEmpty()) proc->write(input);
    proc->closeWriteChannel();
    eloop.exec();
    disconnect(proc, SIGNAL(finished(int, QProcess::ExitStatus)), 0, 0);
    qDebug() << "Exit:" << proc->exitCode() << proc->exitStatus();
    return (proc->exitStatus() == QProcess::NormalExit && proc->exitCode() == 0);
}

QString MInstall::getCmdOut(const QString &cmd, bool everything)
{
    execute(cmd);
    QString strout(proc->readAll().trimmed());
    if (everything) return strout;
    return strout.section("\n", 0, 0);
}

QStringList MInstall::getCmdOuts(const QString &cmd)
{
    execute(cmd);
    return QString(proc->readAll().trimmed()).split('\n');
}

// Check if running inside VirtualBox
bool MInstall::isInsideVB()
{
    return execute("lspci -d 80ee:beef  | grep -q .", false);
}

bool MInstall::replaceStringInFile(const QString &oldtext, const QString &newtext, const QString &filepath)
{
    QString cmd = QString("sed -i 's/%1/%2/g' %3").arg(oldtext, newtext, filepath);
    return execute(cmd);
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
        if (!swapDevice.isEmpty()) {
            swapUUID = getCmdOut("blkid -s UUID -o value " + swapDevice);

            execute("cryptsetup luksAddKey " + swapDevice + " /mnt/antiX/root/keyfile",
                    true, password.toUtf8() + "\n");
        }

        if (isHomeEncrypted && newkey) { // if encrypting separate /home
            execute("cryptsetup luksAddKey " + homeDevice + " /mnt/antiX/root/keyfile",
                    true, password.toUtf8() + "\n");
        }
        QString rootUUID = getCmdOut("blkid -s UUID -o value " + rootDevice);
        //write crypttab keyfile entry
        QFile file("/mnt/antiX/etc/crypttab");
        if (file.open(QIODevice::WriteOnly)) {
            QTextStream out(&file);
            out << "rootfs /dev/disk/by-uuid/" + rootUUID +" none luks \n";
            if (isHomeEncrypted) {
                QString homeUUID =  getCmdOut("blkid -s UUID -o value " + homeDevice);
                out << "homefs /dev/disk/by-uuid/" + homeUUID +" /root/keyfile luks \n";
            }
            if (!swapDevice.isEmpty()) {
                out << "swapfs /dev/disk/by-uuid/" + swapUUID +" /root/keyfile luks,nofail \n";
            }
        }
        file.close();
    } else if (isHomeEncrypted) { // if encrypting /home without encrypting root
        QString swapUUID;
        if (!swapDevice.isEmpty()) {
            //create keyfile
            key.load(rngfile.toUtf8(), keylength);
            key.save("/mnt/antiX/home/.keyfileDONOTdelete", 0400);
            key.erase();

            //add keyfile to container
            swapUUID = getCmdOut("blkid -s UUID -o value " + swapDevice);

            execute("cryptsetup luksAddKey " + swapDevice + " /mnt/antiX/home/.keyfileDONOTdelete",
                    true, password.toUtf8() + "\n");
        }
        QString homeUUID = getCmdOut("blkid -s UUID -o value " + homeDevice);
        //write crypttab keyfile entry
        QFile file("/mnt/antiX/etc/crypttab");
        if (file.open(QIODevice::WriteOnly)) {
            QTextStream out(&file);
            out << "homefs /dev/disk/by-uuid/" + homeUUID +" none luks \n";
            if (!swapDevice.isEmpty()) {
                out << "swapfs /dev/disk/by-uuid/" + swapUUID +" /home/.keyfileDONOTdelete luks,nofail \n";
                execute("sed -i 's/^CRYPTDISKS_MOUNT.*$/CRYPTDISKS_MOUNT=\"\\/home\"/' /mnt/antiX/etc/default/cryptdisks", false);
            }
        }
        file.close();
    }
}

// disable hibernate when using encrypted swap
void MInstall::disablehiberanteinitramfs()
{
    if (phase < 0) return;
    if (isSwapEncrypted) {
        execute("touch /mnt/antiX/initramfs-tools/conf.d/resume");
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
    return execute(cmd);
}

// checks SMART status of the selected disk, returs false if it detects errors and user chooses to abort
bool MInstall::checkDisk()
{
    qDebug() << "+++" << __PRETTY_FUNCTION__ << "+++";
    if (phase < 0) return false;
    QString msg;
    int ans;
    QString output;

    QString drv = "/dev/" + (entireDiskButton->isChecked() ? diskCombo : rootCombo)->currentData().toString();
    output = getCmdOut("smartctl -H " + drv + "|grep -w FAILED");
    if (output.contains("FAILED")) {
        msg = output + tr("\n\nThe disk with the partition you selected for installation is failing.\n\n") +
                tr("You are strongly advised to abort.\n") +
                tr("If unsure, please exit the Installer and run GSmartControl for more information.\n\n") +
                tr("Do you want to abort the installation?");
        ans = QMessageBox::critical(this, windowTitle(), msg,
                                    QMessageBox::Yes, QMessageBox::No);
        if (ans == QMessageBox::Yes) return false;
    }
    else {
        output = getCmdOut("smartctl -A " + drv + "| grep -E \"^  5|^196|^197|^198\" | awk '{ if ( $10 != 0 ) { print $1,$2,$10} }'");
        if (!output.isEmpty()) {
            msg = tr("Smartmon tool output:\n\n") + output + "\n\n" +
                    tr("The disk with the partition you selected for installation passes the S.M.A.R.T. monitor test (smartctl)\n") +
                    tr("but the tests indicate it will have a higher than average failure rate in the upcoming year.\n") +
                    tr("If unsure, please exit the Installer and run GSmartControl for more information.\n\n") +
                    tr("Do you want to continue?");
            ans = QMessageBox::warning(this, windowTitle(), msg,
                                       QMessageBox::Yes, QMessageBox::No);
            if (ans != QMessageBox::Yes) return false;
        }
    }
    return true;
}

// check password length (maybe complexity)
bool MInstall::checkPassword(const QString &pass)
{
    if (pass.length() < 8) {
        QMessageBox::critical(this, windowTitle(),
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
    static const int progPhase23 = 94; // start of Phase 2/3 progress bar space
    // Phase < 0 = install has been aborted (Phase -2 on close)
    if (phase < 0) return false;
    // Phase 0 = install not started yet, Phase 1 = install in progress
    // Phase 2 = waiting for operator input, Phase 3 = post-install steps
    if (phase == 0) { // no install started yet
        updateStatus(tr("Preparing to install %1").arg(PROJECTNAME), 0);
        if (!checkDisk()) return false;
        phase = 1; // installation.

        // preparation
        prepareToInstall();

        // allow the user to enter other options
        buildBootLists();
        manageConfig(ConfigLoadB);
        gotoPage(5);

        // the core of the installation
        if (!pretend) {
            bool ok = makePartitions();
            if (ok) ok = formatPartitions();
            if (!ok) {
                failUI(tr("Failed to format required partitions."));
                return false;
            }
            //run blkid -c /dev/null to freshen UUID cache
            execute("blkid -c /dev/null", true);
            if (!installLinux(progPhase23 - 1)) return false;
        } else if (!pretendToInstall(1, progPhase23 - 1, 100)) {
            return false;
        }
        if (!haveSysConfig) {
            progressBar->setEnabled(false);
            updateStatus(tr("Paused for required operator input"), progPhase23);
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
            updateStatus(tr("Setting system configuration"), progPhase23);
            setServices();
            if (!setComputerName()) return false;
            setLocale();
            if (haveSnapshotUserAccounts) { // skip user account creation
                QString cmd = "rsync -a /home/ /mnt/antiX/home/ --exclude '.cache' --exclude '.gvfs' --exclude '.dbus' --exclude '.Xauthority' --exclude '.ICEauthority'";
                execute(cmd);
            } else {
                if (!setUserInfo()) return false;
            }
            manageConfig(ConfigSave);
            execute("/bin/sync", true); // the sync(2) system call will block the GUI
            if (!installLoader()) return false;
        } else if (!pretendToInstall(progPhase23, 99, 1000)){
            return false;
        }
        phase = 4;
        updateStatus(tr("Installation successful"), 100);
        execute("sleep 1", true);
        gotoPage(10);
    }
    return true;
}

// gather required information and prepare installation
void MInstall::prepareToInstall()
{
    qDebug() << "+++" << __PRETTY_FUNCTION__ << "+++";
    if (phase < 0) return;

    // cleanup previous mounts
    cleanup(false);

    isRootFormatted = false;
    isHomeFormatted = false;

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
    if (execute("ls /home | grep -Ev '(lost\\+found|demo|snapshot)' | grep -q [a-zA-Z0-9]", false)
        || execute("test -d /live/linux/home/demo", true)) {
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

int MInstall::manageConfig(enum ConfigAction mode)
{
    configStuck = 0;
    if (mode == ConfigSave) {
        if (config) delete config;
        config = new QSettings("/mnt/antiX/var/log/minstall.conf", QSettings::NativeFormat);
        config->setValue("Version", VERSION);
    }
    if (!config) return 0;

    auto lambdaSetComboBox = [this, mode](const QString &key, QComboBox *combo, const bool useData) -> void {
        const QVariant &comboval = useData ? combo->currentData() : QVariant(combo->currentText());
        if (mode == ConfigSave) config->setValue(key, comboval);
        else {
            const QVariant &val = config->value(key, comboval);
            const int icombo = useData ? combo->findData(val, Qt::UserRole, Qt::MatchFixedString)
                             : combo->findText(val.toString(), Qt::MatchFixedString);
            if (icombo >= 0) combo->setCurrentIndex(icombo);
            else if (!configStuck) configStuck = -1;
        }
    };
    auto lambdaSetCheckBox = [this, mode](const QString &key, QCheckBox *checkbox) -> void {
        const QVariant state(checkbox->isChecked());
        if (mode == ConfigSave) config->setValue(key, state);
        else checkbox->setChecked(config->value(key, state).toBool());
    };
    auto lambdaSetLineEdit = [this, mode](const QString &key, QLineEdit *lineedit) -> void {
        const QString &text = lineedit->text();
        if (mode == ConfigSave) config->setValue(key, text);
        else lineedit->setText(config->value(key, text).toString());
    };
    auto lambdaSetSpinBox = [this, mode](const QString &key, QSpinBox *spinbox) -> void {
        const QVariant spinval(spinbox->value());
        if (mode == ConfigSave) config->setValue(key, spinval);
        else {
            const int val = config->value(key, spinval).toInt();
            spinbox->setValue(val);
            if (val != spinbox->value() && !configStuck) configStuck = -1;
        }
    };
    auto lambdaSetEnum = [this, mode](const QString &key, const int nchoices,
            const char *choices[], const int curval) -> int {
        QVariant choice(curval >= 0 ? choices[curval] : "");
        if (mode == ConfigSave) config->setValue(key, choice);
        else {
            const QString &val = config->value(key, choice).toString();
            for (int ixi = 0; ixi < nchoices; ++ixi) {
                if (!val.compare(QString(choices[ixi]), Qt::CaseInsensitive)) return ixi;
            }
            if (!configStuck) configStuck = -1;
        }
        return curval;
    };
    auto lambdaSetRadios = [lambdaSetEnum](const QString &key, const int nchoices,
            const char *choices[], QRadioButton *radios[]) -> void {
        // obtain the current choice
        int ixradio = -1;
        for (int ixi = 0; ixradio < 0 && ixi < nchoices; ++ixi) {
            if (radios[ixi]->isChecked()) ixradio = ixi;
        }
        // select the corresponding radio button
        ixradio = lambdaSetEnum(key, nchoices, choices, ixradio);
        if (ixradio >= 0) radios[ixradio]->setChecked(true);
    };

    if (mode == ConfigSave || mode == ConfigLoadA) {
        config->beginGroup("Setup");
        const char *diskChoices[] = {"Drive", "Partitions"};
        QRadioButton *diskRadios[] = {entireDiskButton, existing_partitionsButton};
        lambdaSetRadios("Destination", 2, diskChoices, diskRadios);
        config->endGroup();
        if (configStuck < 0) configStuck = 1;

        if (entireDiskButton->isChecked()) {
            // Disk drive setup
            config->beginGroup("Drive");
            lambdaSetComboBox("Device", diskCombo, true);
            lambdaSetCheckBox("Encrypted", checkBoxEncryptAuto);
            config->endGroup();
            if (configStuck < 0) configStuck = 1;
        } else {
            // Partition step
            config->beginGroup("Partitions");
            lambdaSetComboBox("Root", rootCombo, true);
            lambdaSetComboBox("Home", homeCombo, true);
            lambdaSetComboBox("Swap", swapCombo, true);
            lambdaSetComboBox("Boot", bootCombo, true);
            lambdaSetComboBox("RootType", rootTypeCombo, false);
            lambdaSetComboBox("HomeType", homeTypeCombo, false);
            lambdaSetCheckBox("RootEncrypt", checkBoxEncryptRoot);
            lambdaSetCheckBox("HomeEncrypt", checkBoxEncryptHome);
            lambdaSetCheckBox("SwapEncrypt", checkBoxEncryptSwap);
            lambdaSetLineEdit("RootLabel", rootLabelEdit);
            lambdaSetLineEdit("HomeLabel", homeLabelEdit);
            lambdaSetLineEdit("SwapLabel", swapLabelEdit);
            lambdaSetCheckBox("SaveHome", saveHomeCheck);
            lambdaSetCheckBox("BadBlocksCheck", badblocksCheck);
            config->endGroup();
            if (configStuck < 0) configStuck = 2;
        }

        // AES step
        config->beginGroup("Encryption");
        if (mode != ConfigSave) {
            const QString &epass = config->value("Pass").toString();
            if (entireDiskButton->isChecked()) {
                FDEpassword->setText(epass);
                FDEpassword2->setText(epass);
            } else {
                FDEpassCust->setText(epass);
                FDEpassCust2->setText(epass);
            }
        }
        lambdaSetComboBox("Cipher", comboFDEcipher, false);
        lambdaSetComboBox("ChainMode", comboFDEchain, false);
        lambdaSetComboBox("IVgenerator", comboFDEivgen, false);
        lambdaSetComboBox("IVhash", comboFDEivhash, false);
        lambdaSetSpinBox("KeySize", spinFDEkeysize);
        lambdaSetComboBox("LUKSkeyHash", comboFDEhash, false);
        lambdaSetComboBox("KernelRNG", comboFDErandom, false);
        lambdaSetSpinBox("KDFroundTime", spinFDEroundtime);
        config->endGroup();
        if (configStuck < 0) configStuck = 3;
    }

    if (mode == ConfigSave || mode == ConfigLoadB) {
        // GRUB step
        config->beginGroup("GRUB");
        if(grubCheckBox->isChecked()) {
            const char *grubChoices[] = {"MBR", "PBR", "ESP"};
            QRadioButton *grubRadios[] = {grubMbrButton, grubPbrButton, grubEspButton};
            lambdaSetRadios("Install", 3, grubChoices, grubRadios);
        }
        lambdaSetComboBox("Location", grubBootCombo, true);
        config->endGroup();
        if (configStuck < 0) configStuck = 5;

        // Services step
        config->beginGroup("Services");
        QTreeWidgetItemIterator it(csView);
        while (*it) {
            if ((*it)->parent() != NULL) {
                const QString &itext = (*it)->text(0);
                const QVariant checkval((*it)->checkState(0) == Qt::Checked);
                if (mode == ConfigSave) config->setValue(itext, checkval);
                else {
                    const bool val = config->value(itext, checkval).toBool();
                    (*it)->setCheckState(0, val ? Qt::Checked : Qt::Unchecked);
                }
            }
            ++it;
        }
        config->endGroup();
        if (configStuck < 0) configStuck = 6;

        // Network step
        config->beginGroup("Network");
        lambdaSetLineEdit("ComputerName", computerNameEdit);
        lambdaSetLineEdit("Domain", computerDomainEdit);
        lambdaSetLineEdit("Workgroup", computerGroupEdit);
        lambdaSetCheckBox("Samba", sambaCheckBox);
        config->endGroup();
        if (configStuck < 0) configStuck = 7;

        // Localization step
        config->beginGroup("Localization");
        lambdaSetComboBox("Locale", localeCombo, true);
        lambdaSetCheckBox("LocalClock", localClockCheckBox);
        const char *clockChoices[] = {"24", "12"};
        QRadioButton *clockRadios[] = {radio24h, radio12h};
        lambdaSetRadios("ClockHours", 2, clockChoices, clockRadios);
        lambdaSetComboBox("Timezone", timezoneCombo, false);
        config->endGroup();
        if (configStuck < 0) configStuck = 8;

        // User Accounts step
        config->beginGroup("User");
        lambdaSetLineEdit("Username", userNameEdit);
        lambdaSetCheckBox("Autologin", autologinCheckBox);
        lambdaSetCheckBox("SaveDesktop", saveDesktopCheckBox);
        const char *oldHomeActionChoices[] = {"Nothing", "Use", "Save", "Delete"};
        oldHomeAction = static_cast<OldHomeAction>(lambdaSetEnum("OldHomeAction", 4, oldHomeActionChoices, oldHomeAction));
        if (mode != ConfigSave) {
            const QString &upass = config->value("UserPass").toString();
            userPasswordEdit->setText(upass);
            userPasswordEdit2->setText(upass);
            const QString &rpass = config->value("RootPass").toString();
            rootPasswordEdit->setText(rpass);
            rootPasswordEdit2->setText(rpass);
        }
        config->endGroup();
        if (configStuck < 0) configStuck = 9;
    }

    return configStuck;
}

void MInstall::stashAdvancedFDE(bool save)
{
    if (save) {
        indexFDEcipher = comboFDEcipher->currentIndex();
        indexFDEchain = comboFDEchain->currentIndex();
        indexFDEivgen = comboFDEivgen->currentIndex();
        indexFDEivhash = comboFDEivhash->currentIndex();
        iFDEkeysize = spinFDEkeysize->value();
        indexFDEhash = comboFDEhash->currentIndex();
        indexFDErandom = comboFDErandom->currentIndex();
        iFDEroundtime = spinFDEroundtime->value();
    } else {
        comboFDEcipher->setCurrentIndex(indexFDEcipher);
        comboFDEchain->setCurrentIndex(indexFDEchain);
        comboFDEivgen->setCurrentIndex(indexFDEivgen);
        comboFDEivhash->setCurrentIndex(indexFDEivhash);
        spinFDEkeysize->setValue(iFDEkeysize);
        comboFDEhash->setCurrentIndex(indexFDEhash);
        comboFDErandom->setCurrentIndex(indexFDErandom);
        spinFDEroundtime->setValue(iFDEroundtime);
    }
}

bool MInstall::formatPartitions()
{
    qDebug() << "+++" << __PRETTY_FUNCTION__ << "+++";
    if (phase < 0) return false;
    QString rootdev = rootDevice;
    QString swapdev = swapDevice;
    QString homedev = homeDevice;

    // set up LUKS containers
    const QByteArray &encPass = (entireDiskButton->isChecked()
                                 ? FDEpassword : FDEpassCust)->text().toUtf8();
    const QString &statup = tr("Setting up LUKS encrypted containers");
    if (isSwapEncrypted) {
        if (swapFormatSize) {
            updateStatus(statup);
            if (!makeLuksPartition(swapdev, encPass)) return false;
        }
        updateStatus(statup);
        if (!openLuksPartition(swapdev, "swapfs", encPass)) return false;
        swapdev = "/dev/mapper/swapfs";
    }
    if (isRootEncrypted) {
        if (rootFormatSize) {
            updateStatus(statup);
            if (!makeLuksPartition(rootdev, encPass)) return false;
        }
        updateStatus(statup);
        if (!openLuksPartition(rootdev, "rootfs", encPass)) return false;
        rootdev = "/dev/mapper/rootfs";
    }
    if (isHomeEncrypted) {
        if (homeFormatSize) {
            updateStatus(statup);
            if (!makeLuksPartition(homedev, encPass)) return false;
        }
        updateStatus(statup);
        if (!openLuksPartition(homedev, "homefs", encPass)) return false;
        homedev = "/dev/mapper/homefs";
    }

    //if no swap is chosen do nothing
    if (swapFormatSize) {
        updateStatus(tr("Formatting swap partition"));
        const QString cmd("/sbin/mkswap %1 -L %2");
        if (!execute(cmd.arg(swapdev, swapLabelEdit->text()))) return false;
    }

    // maybe format root (if not saving /home on root), or if using --sync option
    if (swapFormatSize) {
        updateStatus(tr("Formatting the / (root) partition"));
        if (!makeLinuxPartition(rootdev, rootTypeCombo->currentText(),
                                badblocksCheck->isChecked(), rootLabelEdit->text())) {
            return false;
        }
        isRootFormatted = true;
        root_mntops = "defaults,noatime";
    }
    if (!mountPartition(rootdev, "/mnt/antiX", root_mntops)) return false;

    // format and mount /boot if different than root
    if (bootFormatSize) {
        updateStatus(tr("Formatting boot partition"));
        if (!makeLinuxPartition(bootDevice, "ext4", false, "boot")) return false;
    }

    // format ESP if necessary
    if (espFormatSize) {
        updateStatus(tr("Formatting EFI System Partition"));
        if (!execute("mkfs.msdos -F 32 " + espDevice)) return false;
        execute("parted -s " + splitDevice(espDevice)[0] + " set 1 esp on"); // sets boot flag and esp flag
    }
    // maybe format home
    if (homeFormatSize) {
        updateStatus(tr("Formatting the /home partition"));
        if (!makeLinuxPartition(homedev, homeTypeCombo->currentText(),
                                badblocksCheck->isChecked(), homeLabelEdit->text())) {
            return false;
        }
        execute("/bin/rm -r /mnt/antiX/home", true);
        isHomeFormatted = true;
    }
    mkdir("/mnt/antiX/home", 0755);
    if (homedev != rootDevice) {
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
    QString homedev = homeDevice;
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
        execute("/bin/cp -fp /bin/true /sbin/fsck.auto");
        // set creation options for small drives using btrfs
        QString size_str = getCmdOut("/sbin/sfdisk -s " + dev);
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
    if (!execute(cmd)) {
        // error
        return false;
    }

    if (type.startsWith("ext")) {
        // ext4 tuning
        cmd = QString("/sbin/tune2fs -c0 -C0 -i1m %1").arg(dev);
        if (!execute(cmd)) {
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
    if (!execute(cmd, true, password + "\n")) {
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
    if (!execute(cmd, true, password + "\n")) {
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
                QMessageBox::critical(this, windowTitle(), tr("You must choose a separate boot partition when encrypting root."));
                return false;
            }
        }
    }
    QString rootdev(rootCombo->currentData().toString());
    QString homedev(homeCombo->currentData().toString());
    QString swapdev(swapCombo->currentData().toString());
    QString bootdev(bootCombo->currentData().toString());
    if (homedev == "root") homedev = rootdev;
    if (bootdev == "root") bootdev = rootdev;

    if (rootdev.isEmpty()) {
        QMessageBox::critical(this, windowTitle(),
            tr("You must choose a root partition.\nThe root partition must be at least %1.").arg(MIN_INSTALL_SIZE));
        nextFocus = rootCombo;
        return false;
    }

    QStringList msgForeignList;
    QString msgConfirm;
    QStringList msgFormatList;

    // warn if using a non-Linux partition (potential install of another OS)
    auto lambdaForeignTrap = [this,&msgForeignList](const QString &devname, const QString &desc) -> void {
        const int bdindex = listBlkDevs.findDevice(devname);
        if (bdindex >= 0 && !listBlkDevs[bdindex].isNative) {
            msgForeignList << devname << desc;
        }
    };

    lambdaForeignTrap(rootdev, "/ (root)");

    // warn on formatting or deletion
    if (!(saveHomeCheck->isChecked() && homedev == rootdev)) {
        msgFormatList << rootdev << "/ (root)";
    } else {
        msgConfirm += " - " + tr("Delete the data on %1 except for /home").arg(rootdev) + "\n";
    }

    // format /home?
    if (homedev != rootdev) {
        lambdaForeignTrap(homedev, "/home");
        if (saveHomeCheck->isChecked()) {
            msgConfirm += " - " + tr("Reuse (no reformat) %1 as the /home partition").arg(homedev) + "\n";
        } else {
            msgFormatList << homedev << "/home";
        }
    }

    // format swap? (if no swap is chosen do nothing)
    if (!swapdev.isEmpty()) {
        lambdaForeignTrap(swapdev, "swap");
        formatSwap = checkBoxEncryptSwap->isChecked();
        const int bdindex = listBlkDevs.findDevice(swapdev);
        if (bdindex >= 0 && listBlkDevs[bdindex].isSwap) formatSwap = true;
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
        lambdaForeignTrap(bootdev, "/boot");
        // warn if partition too big (not needed for boot, likely data or other useful partition
        QString size_str = getCmdOut("lsblk -nbo SIZE /dev/" + bootdev + "|head -n1").trimmed();
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
        ans = QMessageBox::warning(this, windowTitle(), msg,
                                   QMessageBox::Yes, QMessageBox::No);
        if (ans != QMessageBox::Yes) {
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
        if (!msgConfirm.isEmpty()) msg += "\n";
    }
    if (!msgConfirm.isEmpty()) {
        msg += tr("The %1 installer will now perform the following actions:").arg(PROJECTNAME);
        msg += "\n\n" + msgConfirm;
    }
    if(!msg.isEmpty()) {
        msg += "\n" + tr("These actions cannot be undone. Do you want to continue?");
        ans = QMessageBox::warning(this, windowTitle(), msg,
                                   QMessageBox::Yes, QMessageBox::No);
        if (ans != QMessageBox::Yes) {
            return false;
        }
    }

    rootDevice = "/dev/" + rootdev;
    if (!bootdev.isEmpty()) bootDevice = "/dev/" + bootdev;
    if (!swapdev.isEmpty()) swapDevice = "/dev/" + swapdev;
    if (!homedev.isEmpty()) homeDevice = "/dev/" + homedev;
    return true;
}

bool MInstall::calculateDefaultPartitions()
{
    QString drv(diskCombo->currentData().toString());
    int bdindex = listBlkDevs.findDevice(drv);
    if (bdindex < 0) return false;
    QString mmcnvmepartdesignator;
    if (drv.startsWith("nvme") || drv.startsWith("mmcblk")) {
        mmcnvmepartdesignator = "p";
    }

    // remove partitions from the list that belong to this drive
    const int ixRemoveBD = bdindex + 1;
    while (ixRemoveBD < listBlkDevs.size() && !listBlkDevs[ixRemoveBD].isDisk) {
        listBlkDevs.removeAt(ixRemoveBD);
    }

    bool ok = true;
    int free = freeSpaceEdit->text().toInt(&ok,10);
    if (!ok) free = 0;

    // calculate new partition sizes
    // get the total disk size
    rootFormatSize = listBlkDevs[bdindex].size / 1048576; // in MB
    rootFormatSize -= 32; // pre-compensate for rounding errors in disk geometry

    // allocate space for ESP if booted from UEFI
    espFormatSize = uefi ? 256 : 0;
    rootFormatSize -= espFormatSize;

    // allocate space for /boot if encrypting
    bootFormatSize = 0;
    if (checkBoxEncryptAuto->isChecked()){ // set root and swap status encrypted
        isRootEncrypted = true;
        isSwapEncrypted = true;
        bootFormatSize = 512;
        rootFormatSize -= bootFormatSize;
    }

    // no default separate /home just yet
    homeFormatSize = 0;

    // 2048 swap should be ample
    swapFormatSize = 2048;
    if (rootFormatSize < 2048) swapFormatSize = 128;
    else if (rootFormatSize < 3096) swapFormatSize = 256;
    else if (rootFormatSize < 4096) swapFormatSize = 512;
    else if (rootFormatSize < 12288) swapFormatSize = 1024;
    rootFormatSize -= swapFormatSize;

    if (free > 0 && rootFormatSize > 8192) {
        // allow free_size
        // remaining is capped until free is satisfied
        if (free > rootFormatSize - 8192) {
            free = rootFormatSize - 8192;
        }
        rootFormatSize -= free;
    } else { // no free space
        free = 0;
    }

    // code for adding future partitions to the list
    int ixAddBD = bdindex;
    int ixpart = 1;
    auto lambdaAddFutureBD = [this, bdindex, &ixpart, &ixAddBD, &drv, &mmcnvmepartdesignator]
            (qint64 size, const QString &fs) -> BlockDeviceInfo &  {
        BlockDeviceInfo bdinfo;
        bdinfo.name = drv + mmcnvmepartdesignator + QString::number(ixpart);
        bdinfo.fs = fs;
        bdinfo.size = size * 1048576; // back into bytes
        bdinfo.isFuture = bdinfo.isNative = true;
        bdinfo.isGPT = listBlkDevs[bdindex].isGPT;
        ++ixAddBD;
        listBlkDevs.insert(ixAddBD, bdinfo);
        ++ixpart;
        return listBlkDevs[ixAddBD];
    };
    // add future partitions to the block device list and store new names
    if (uefi) {
        BlockDeviceInfo &bdinfo = lambdaAddFutureBD(espFormatSize, "vfat");
        espDevice = bdinfo.name;
        bdinfo.isNative = false;
        bdinfo.isESP = true;
    }
    if (!isRootEncrypted) bootDevice.clear();
    else bootDevice = "/dev/" + lambdaAddFutureBD(bootFormatSize, "ext4").name;
    rootDevice = "/dev/" + lambdaAddFutureBD(rootFormatSize,
                          isRootEncrypted ? "crypto_LUKS" : "ext4").name;
    BlockDeviceInfo &bdinfo = lambdaAddFutureBD(swapFormatSize, "swap");
    bdinfo.isSwap = true;
    swapDevice = "/dev/" + bdinfo.name;
    homeDevice = rootDevice;
    rootTypeCombo->setCurrentIndex(rootTypeCombo->findText("ext4"));
    return true;
}

bool MInstall::calculateChosenPartitions()
{
    auto lambdaSetFutureBD = [this](QComboBox *combo, const QString &label,
            const QString &fs, const bool isEncrypted) -> int {
        int index = listBlkDevs.findDevice(combo->currentData().toString());
        if (index >= 0) {
            BlockDeviceInfo &bdinfo = listBlkDevs[index];
            bdinfo.fs = isEncrypted ? QStringLiteral("crypt_LUKS") : fs;
            bdinfo.label = label;
            bdinfo.isNasty = false; // future partitions are safe
            bdinfo.isFuture = bdinfo.isNative = true;
        }
        return index;
    };
    const bool saveHome = saveHomeCheck->isChecked();

    // format swap if encrypting or not already swap
    swapFormatSize = 0;
    if (swapCombo->currentData().isValid()) {
        if (checkBoxEncryptSwap->isChecked()) {
            isSwapEncrypted = true;
            swapFormatSize = -1;
        } else {
            int index = listBlkDevs.findDevice(swapCombo->currentData().toString());
            if (index >= 0 && !listBlkDevs[index].isSwap) swapFormatSize = -1;
        }
        if (swapFormatSize) {
            const int bdindex = lambdaSetFutureBD(swapCombo, swapLabelEdit->text(),
                                                  "swap", isSwapEncrypted);
            if (bdindex < 0) return false;
            listBlkDevs[bdindex].isSwap = true;
        }
    }

    if (checkBoxEncryptRoot->isChecked()) isRootEncrypted = true;
    // maybe format root (if not saving /home on root) // or if using --sync option
    rootFormatSize = 0;
    if (!(saveHome && homeDevice == rootDevice) && !sync) {
        rootFormatSize = -1;
        lambdaSetFutureBD(rootCombo, rootLabelEdit->text(),
                          rootTypeCombo->currentData().toString(), isRootEncrypted);
    }

    // format and mount /boot if different than root
    bootFormatSize = espFormatSize = 0; // no new ESP partition this time
    if (bootCombo->currentText() != "root") {
        lambdaSetFutureBD(bootCombo, "boot", "ext4", false);
        bootFormatSize = -1;
    }

    // prepare home if not being preserved, and on a different partition
    homeFormatSize = 0;
    if (homeDevice != rootDevice) {
        if (checkBoxEncryptHome->isChecked()) isHomeEncrypted = true;
        if (!saveHome) {
            homeFormatSize = -1;
            lambdaSetFutureBD(homeCombo, homeLabelEdit->text(),
                              homeTypeCombo->currentText(), isHomeEncrypted);
        }
    }

    return true;
}

///////////////////////////////////////////////////////////////////////////
// Make the chosen partitions and mount them

bool MInstall::makePartitions()
{
    qDebug() << "+++" << __PRETTY_FUNCTION__ << "+++";
    if (phase < 0) return false;

    qint64 start = 1; // start with 1 MB to aid alignment

    auto lambdaDetachPart = [this](const QString &strdev) -> void {
        execute("swapoff " + strdev, true);
        execute("umount " + strdev, true);
    };
    auto lambdaPreparePart = [this, &start, lambdaDetachPart]
            (const QString &strdev, qint64 size, const QString &type) -> bool {
        const QStringList devsplit = splitDevice(strdev);
        bool rc = true;
        // size=0 = nothing, size>0 = creation, size<0 = allocation.
        if (size > 0) {
            const qint64 end = start + size;
            rc = execute("parted -s --align optimal /dev/" + devsplit[0] + " mkpart " + type
                         + " " + QString::number(start) + "MiB " + QString::number(end) + "MiB");
            start += end;
        } else if (size < 0){
            lambdaDetachPart(strdev);
            // command to set the partition type
            QString cmd;
            if (isGpt("/dev/" + devsplit[0])) {
                cmd = "/sbin/sgdisk /dev/%1 --typecode=%2:8303";
            } else {
                cmd = "/sbin/sfdisk /dev/%1 --part-type %2 83";
            }
            rc = execute(cmd.arg(devsplit[0], devsplit[1]));
        }
        return rc;
    };

    qDebug() << " ---- PARTITION FORMAT SCHEDULE ----";
    qDebug() << rootDevice << rootFormatSize;
    qDebug() << homeDevice << homeFormatSize;
    qDebug() << swapDevice << swapFormatSize;
    qDebug() << bootDevice << bootFormatSize;

    if (entireDiskButton->isChecked()) {
        QString drv(diskCombo->currentData().toString());
        // detach all existing partitions on the selected drive
        for (const BlockDeviceInfo &bdinfo : listBlkDevsBackup) {
            if (!bdinfo.isDisk && bdinfo.name.startsWith(drv)) {
                lambdaDetachPart("/dev/" + bdinfo.name);
            }
        }
        updateStatus(tr("Creating required partitions"));
        execute(QStringLiteral("/bin/dd if=/dev/zero of=/dev/%1 bs=512 count=100").arg(drv));
        if (!execute("parted -s /dev/" + drv + " mklabel " + (uefi ? "gpt" : "msdos"))) return false;
    } else {
        updateStatus(tr("Preparing required partitions"));
        // prepare home if not being preserved, and on a different partition
        if (saveHomeCheck->isChecked()) lambdaDetachPart(homeDevice);
    }

    // any new partitions they will appear in this order on the disk
    if (!lambdaPreparePart(espDevice, espFormatSize, "ESP")) return false;
    if (!lambdaPreparePart(bootDevice, bootFormatSize, "primary")) return false;
    if (!lambdaPreparePart(rootDevice, rootFormatSize, "primary ext4")) return false;
    if (!lambdaPreparePart(homeDevice, homeFormatSize, "primary")) return false;
    if (!lambdaPreparePart(swapDevice, swapFormatSize, "primary")) return false;
    execute("partprobe", true);
    return true;
}

bool MInstall::saveHomeBasic()
{
    qDebug() << "+++" << __PRETTY_FUNCTION__ << "+++";
    if (!(saveHomeCheck->isChecked())) return true;
    cleanup(false); // cleanup previous mounts
    // if preserving /home, obtain some basic information
    bool ok = false;
    const QByteArray &pass = FDEpassCust->text().toUtf8();
    QString rootdev = rootDevice;
    QString homedev = homeDevice;
    // mount the root partition
    if (isRootEncrypted) {
        if (!openLuksPartition(rootdev, "rootfs", pass, "--readonly", false)) return false;
        rootdev = "/dev/mapper/rootfs";
    }
    if (!mountPartition(rootdev, "/mnt/antiX", "ro")) goto ending2;
    // mount the home partition
    if (homedev != rootDevice) {
        if (isHomeEncrypted) {
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
    if (homedev != rootDevice) {
        execute("/bin/umount -l /mnt/antiX/home", false);
        if (isHomeEncrypted) execute("cryptsetup luksClose homefs", true);
    }
 ending2:
    execute("/bin/umount -l /mnt/antiX", false);
    if (isRootEncrypted) execute("cryptsetup luksClose rootfs", true);
    return ok;
}

bool MInstall::installLinux(const int progend)
{
    qDebug() << "+++" << __PRETTY_FUNCTION__ << "+++";
    QString rootdev = (isRootEncrypted ? "/dev/mapper/rootfs" : rootDevice);
    if (phase < 0) return false;

    if (!(isRootFormatted || sync)) {
        // if root was not formatted and not using --sync option then re-use it
        updateStatus(tr("Mounting the / (root) partition"));
        mountPartition(rootdev, "/mnt/antiX", root_mntops);
        // remove all folders in root except for /home
        updateStatus(tr("Deleting old system"));
        execute("find /mnt/antiX -mindepth 1 -maxdepth 1 ! -name home -exec rm -r {} \\;", false);

        if (proc->exitStatus() != QProcess::NormalExit) {
            failUI(tr("Failed to delete old %1 on destination.\nReturning to Step 1.").arg(PROJECTNAME));
            return false;
        }
    }

    // make empty dirs for opt, dev, proc, sys, run,
    // home already done
    updateStatus(tr("Creating system directories"));
    mkdir("/mnt/antiX/opt", 0755);
    mkdir("/mnt/antiX/dev", 0755);
    mkdir("/mnt/antiX/proc", 0755);
    mkdir("/mnt/antiX/sys", 0755);
    mkdir("/mnt/antiX/run", 0755);

    //if separate /boot in use, mount that to /mnt/antiX/boot
    if (!bootDevice.isEmpty() && bootDevice != rootDevice) {
        mkdir("/mnt/antiX/boot", 0755);
        execute("fsck.ext4 -y " + bootDevice); // needed to run fsck because sfdisk --part-type can mess up the partition
        if (!mountPartition(bootDevice, "/mnt/antiX/boot", root_mntops)) {
            qDebug() << "Could not mount /boot on " + bootDevice;
            return false;
        }
    }

    if(!copyLinux(progend - 1)) return false;

    updateStatus(tr("Fixing configuration"), progend);
    mkdir("/mnt/antiX/tmp", 01777);
    chmod("/mnt/antiX/tmp", 01777);
    makeFstab();
    writeKeyFile();
    disablehiberanteinitramfs();

    // Copy live set up to install and clean up.
    execute("/usr/sbin/live-to-installed /mnt/antiX", false);
    qDebug() << "Desktop menu";
    execute("chroot /mnt/antiX desktop-menu --write-out-global", false);
    execute("/bin/rm -rf /mnt/antiX/home/demo");
    execute("/bin/rm -rf /mnt/antiX/media/sd*", false);
    execute("/bin/rm -rf /mnt/antiX/media/hd*", false);

    // guess localtime vs UTC
    if (getCmdOut("guess-hwclock") == "localtime") {
        localClockCheckBox->setChecked(true);
    }

    return true;
}

// create /etc/fstab file
void MInstall::makeFstab()
{
    if (phase < 0) return;

    // get config
    QString rootdev = rootDevice;
    QString homedev = homeDevice;
    QString swapdev = swapDevice;

    //get UUIDs
    const QString cmdBlkID("blkid -o value UUID -s UUID ");
    QString rootdevUUID = "UUID=" + getCmdOut(cmdBlkID + rootDevice);
    QString homedevUUID = "UUID=" + getCmdOut(cmdBlkID + homeDevice);
    QString swapdevUUID = "UUID=" + getCmdOut(cmdBlkID + swapDevice);
    const QString bootdevUUID = "UUID=" + getCmdOut(cmdBlkID + bootDevice);

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
    qDebug() << "bootdev" << bootDevice << bootdevUUID;

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
        if (!bootDevice.isEmpty() && bootDevice != rootDevice) {
            out << bootdevUUID + " /boot ext4 " + root_mntops + " 1 1\n";
        }
        if (grubEspButton->isChecked()) {
            const QString espdev = "/dev/" + grubBootCombo->currentData().toString();
            const QString espdevUUID = "UUID=" + getCmdOut(cmdBlkID + espdev);
            qDebug() << "espdev" << espdev << espdevUUID;
            out << espdevUUID + " /boot/efi vfat defaults,noatime,dmask=0002,fmask=0113 0 0\n";
        }
        if (!homedev.isEmpty() && homedev != rootDevice) {
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
        if (!swapdev.isEmpty()) {
            out << swapdevUUID +" swap swap defaults 0 0 \n";
        }
        file.close();
    }
    // if POPULATE_MEDIA_MOUNTPOINTS is true in gazelle-installer-data, then use the --mntpnt switch
    if (POPULATE_MEDIA_MOUNTPOINTS) {
        execute("/sbin/make-fstab -O --install /mnt/antiX --mntpnt=/media");
    }
}

bool MInstall::copyLinux(const int progend)
{
    qDebug() << "+++" << __PRETTY_FUNCTION__ << "+++";
    if (phase < 0) return false;

    // copy most except usr, mnt and home
    // must copy boot even if saving, the new files are required
    // media is already ok, usr will be done next, home will be done later
    updateStatus(tr("Copying new system"));
    int progstart = progressBar->value();
    // setup and start the process
    QString cmd;
    cmd = "/bin/cp -av";
    if (sync) {
        cmd = "rsync -av --delete";
        if (saveHomeCheck->isChecked()) {
            cmd.append(" --filter 'protect home/*'");
        }
    }
    cmd.append(" /live/aufs/bin /live/aufs/boot /live/aufs/dev");
    cmd.append(" /live/aufs/etc /live/aufs/lib /live/aufs/lib64 /live/aufs/media /live/aufs/mnt");
    cmd.append(" /live/aufs/opt /live/aufs/root /live/aufs/sbin /live/aufs/selinux /live/aufs/usr");
    cmd.append(" /live/aufs/var /live/aufs/home /mnt/antiX");
    struct statvfs svfs;

    fsfilcnt_t sourceInodes = 1;
    fsfilcnt_t targetInodes = 1;
    if (statvfs("/live/linux", &svfs) == 0) {
        sourceInodes = svfs.f_files - svfs.f_ffree;
        if(statvfs("/mnt/antiX", &svfs) == 0) {
            targetInodes = svfs.f_files - svfs.f_ffree;
        }
    }

    if (!nocopy) {
        if (phase < 0) return false;
        QEventLoop eloop;
        connect(proc, static_cast<void (QProcess::*)(int)>(&QProcess::finished), &eloop, &QEventLoop::quit);
        connect(proc, static_cast<void (QProcess::*)()>(&QProcess::readyRead), &eloop, &QEventLoop::quit);
        qDebug() << cmd;
        proc->start(cmd);
        const int progspace = progend - progstart;
        const int progdiv = (progspace != 0) ? (sourceInodes / progspace) : 0;
        while (proc->state() != QProcess::NotRunning) {
            eloop.exec();
            proc->readAll();
            if(statvfs("/mnt/antiX", &svfs) == 0 && progdiv != 0) {
                int i = (svfs.f_files - svfs.f_ffree - targetInodes) / progdiv;
                if (i > progspace) i = progspace;
                progressBar->setValue(i + progstart);
            } else {
                updateStatus(tr("Copy progress unknown. No file system statistics."));
            }
        }
        disconnect(proc, SIGNAL(readyRead()), 0, 0);
        disconnect(proc, SIGNAL(finished(int, QProcess::ExitStatus)), 0, 0);

        if (proc->exitStatus() != QProcess::NormalExit) {
            failUI(tr("Failed to write %1 to destination.\nReturning to Step 1.").arg(PROJECTNAME));
            return false;
        }
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
        execute("rm -f /mnt/antiX/boot/" + val);
    }

    if (!grubCheckBox->isChecked()) {
        // skip it
        return true;
    }

    //add switch to change root partition info
    QString boot = "/dev/" + grubBootCombo->currentData().toString();

    if (grubMbrButton->isChecked() && !isGpt(boot)) {
        QString part_num;
        if (bootDevice.startsWith(boot)) part_num = bootDevice;
        else if (rootDevice.startsWith(boot)) part_num = rootDevice;
        else if (homeDevice.startsWith(boot)) part_num = homeDevice;
        if (!part_num.isEmpty()) {
            // remove the non-digit part to get the number of the root partition
            part_num.remove(QRegularExpression("\\D+\\d*\\D+"));
            execute("parted -s " + boot + " set " + part_num + " boot on", true);
        }
    }

    // set mounts for chroot
    execute("mount -o bind /dev /mnt/antiX/dev", true);
    execute("mount -o bind /sys /mnt/antiX/sys", true);
    execute("mount -o bind /proc /mnt/antiX/proc", true);

    QString arch;

    // install new Grub now
    if (!grubEspButton->isChecked()) {
        cmd = QString("grub-install --target=i386-pc --recheck --no-floppy --force --boot-directory=/mnt/antiX/boot %1").arg(boot);
    } else {
        mkdir("/mnt/antiX/boot/efi", 0755);
        QString mount = QString("mount %1 /mnt/antiX/boot/efi").arg(boot);
        execute(mount);
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
    if (!execute(cmd)) {
        // error
        QMessageBox::critical(this, windowTitle(),
                              tr("GRUB installation failed. You can reboot to the live medium and use the GRUB Rescue menu to repair the installation."));
        execute("umount /mnt/antiX/proc", true);
        execute("umount /mnt/antiX/sys", true);
        execute("umount /mnt/antiX/dev", true);
        if (execute("mountpoint -q /mnt/antiX/boot/efi", true)) {
            execute("umount /mnt/antiX/boot/efi", true);
        }
        return false;
    }

    // update NVRAM boot entries (only if installing on ESP)
    updateStatus(statup);
    if (grubEspButton->isChecked()) {
        cmd = QString("chroot /mnt/antiX grub-install --force-extra-removable --target=%1-efi --efi-directory=/boot/efi --bootloader-id=%2%3 --recheck").arg(arch, PROJECTSHORTNAME, PROJECTVERSION);
        if (!execute(cmd)) {
            QMessageBox::warning(this, windowTitle(), tr("NVRAM boot variable update failure. The system may not boot, but it can be repaired with the GRUB Rescue boot menu."));
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
    execute(cmd, false);

    //copy memtest efi files if needed

    if (uefi) {
        mkdir("/mnt/antiX/boot/uefi-mt", 0755);
        if (arch == "i386") {
            execute("cp /live/boot-dev/boot/uefi-mt/mtest-32.efi /mnt/antiX/boot/uefi-mt", true);
        } else {
            execute("cp /live/boot-dev/boot/uefi-mt/mtest-64.efi /mnt/antiX/boot/uefi-mt", true);
        }
    }
    updateStatus(statup);

    //update grub with new config

    qDebug() << "Update Grub";
    execute("chroot /mnt/antiX update-grub");
    updateStatus(statup);

    qDebug() << "Update initramfs";
    execute("chroot /mnt/antiX update-initramfs -u -t -k all");
    updateStatus(statup);
    qDebug() << "clear chroot env";
    execute("umount /mnt/antiX/proc", true);
    execute("umount /mnt/antiX/sys", true);
    execute("umount /mnt/antiX/dev", true);
    if (execute("mountpoint -q /mnt/antiX/boot/efi", true)) {
        execute("umount /mnt/antiX/boot/efi", true);
    }

    return true;
}

bool MInstall::isGpt(const QString &drv)
{
    QString cmd = QString("blkid %1 | grep -q PTTYPE=\\\"gpt\\\"").arg(drv);
    return execute(cmd, false);
}

void MInstall::checkUefi()
{
    // return false if not uefi, or if a bad combination, like 32 bit iso and 64 bit uefi)
    if (execute("uname -m | grep -q i686", false) && execute("grep -q 64 /sys/firmware/efi/fw_platform_size")) {
        uefi = false;
    } else {
        uefi = execute("test -d /sys/firmware/efi", true);
    }
    qDebug() << "uefi =" << uefi;
}

// get the type of the partition
QString MInstall::getPartType(const QString &dev)
{
    qDebug() << "+++" << __PRETTY_FUNCTION__ << "+++";
    return getCmdOut("blkid " + dev + " -o value -s TYPE");
}

/////////////////////////////////////////////////////////////////////////
// user account functions

bool MInstall::validateUserInfo()
{
    nextFocus = userNameEdit;
    // see if username is reasonable length
    if (userNameEdit->text().isEmpty()) {
        QMessageBox::critical(this, windowTitle(),
                              tr("Please enter a user name."));
        return false;
    } else if (!userNameEdit->text().contains(QRegExp("^[a-zA-Z_][a-zA-Z0-9_-]*[$]?$"))) {
        QMessageBox::critical(this, windowTitle(),
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
                QMessageBox::critical(this, windowTitle(),
                                      tr("Sorry, that name is in use.\n"
                                         "Please select a different name."));
                return false;
            }
        }
    }

    if (userPasswordEdit->text().isEmpty()) {
        QMessageBox::critical(this, windowTitle(),
                              tr("Please enter the user password."));
        nextFocus = userPasswordEdit;
        return false;
    }
    if (rootPasswordEdit->text().isEmpty()) {
        QMessageBox::critical(this, windowTitle(),
                              tr("Please enter the root password."));
        nextFocus = rootPasswordEdit;
        return false;
    }
    if (userPasswordEdit->text() != userPasswordEdit2->text()) {
        QMessageBox::critical(this, windowTitle(),
                              tr("The user password entries do not match.\n"
                                 "Please try again."));
        nextFocus = userPasswordEdit;
        return false;
    }
    if (rootPasswordEdit->text() != rootPasswordEdit2->text()) {
        QMessageBox::critical(this, windowTitle(),
                              tr("The root password entries do not match.\n"
                                 "Please try again."));
        nextFocus = rootPasswordEdit;
        return false;
    }

    // Check for pre-existing /home directory
    // see if user directory already exists
    if (oldHomeAction == OldHomeNothing && listHomes.contains(userNameEdit->text())) {
        QMessageBox msgbox(this);
        msgbox.setWindowTitle(windowTitle());
        msgbox.setText(tr("The home directory for %1 already exists.").arg(userNameEdit->text()));
        msgbox.setInformativeText(tr("What would you like to do with the old directory?"));
        QPushButton *msgbtnUse = msgbox.addButton(tr("Reuse"), QMessageBox::ActionRole);
        QPushButton *msgbtnSave = msgbox.addButton(tr("Save"), QMessageBox::ActionRole);
        QPushButton *msgbtnDelete = msgbox.addButton(tr("Delete"), QMessageBox::ActionRole);
        msgbox.setDefaultButton(msgbox.addButton(QMessageBox::Cancel));
        msgbox.exec();
        QAbstractButton *msgbtn = msgbox.clickedButton();
        if (msgbtn == msgbtnDelete) oldHomeAction = OldHomeDelete; // delete the directory
        else if (msgbtn == msgbtnSave) oldHomeAction = OldHomeSave; // save the old directory
        else if (msgbtn == msgbtnUse) oldHomeAction = OldHomeUse; // use the old home
        else return false; // don't save, reuse or delete -- can't proceed
    }
    nextFocus = NULL;
    return true;
}

// setup the user, cannot be rerun
bool MInstall::setUserInfo()
{
    if (nocopy) return true;
    if (phase < 0) return false;

    // set the user passwords first
    if (!execute("chroot /mnt/antiX chpasswd", true,
                 QString("root:" + rootPasswordEdit->text() + "\n"
                         "demo:" + userPasswordEdit->text()).toUtf8())) {
        failUI(tr("Failed to set user account passwords."));
        return false;
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
            bool ok = false;
            cmd = QString("mv -f %1 %1.00%2").arg(dpath);
            for (int ixi = 1; ixi < 10 && !ok; ++ixi) {
                ok = execute(cmd.arg(ixi));
            }
            if (!ok) {
                failUI(tr("Failed to save old home directory."));
                return false;
            }
        } else if (oldHomeAction == OldHomeDelete) {
            // delete the directory
            cmd = QString("rm -rf %1").arg(dpath);
            if (!execute(cmd)) {
                failUI(tr("Failed to delete old home directory."));
                return false;
            }
        }
    }

    if ((dir = opendir(dpath.toUtf8())) == NULL) {
        // dir does not exist, must create it
        // copy skel to demo
        if (!execute("cp -a /mnt/antiX/etc/skel /mnt/antiX/home")) {
            failUI(tr("Sorry, failed to create user directory."));
            return false;
        }
        cmd = QString("mv -f /mnt/antiX/home/skel %1").arg(dpath);
        if (!execute(cmd)) {
            failUI(tr("Sorry, failed to name user directory."));
            return false;
        }
    } else {
        // dir does exist, clean it up
        cmd = QString("cp -n /mnt/antiX/etc/skel/.bash_profile %1").arg(dpath);
        execute(cmd);
        cmd = QString("cp -n /mnt/antiX/etc/skel/.bashrc %1").arg(dpath);
        execute(cmd);
        cmd = QString("cp -n /mnt/antiX/etc/skel/.gtkrc %1").arg(dpath);
        execute(cmd);
        cmd = QString("cp -n /mnt/antiX/etc/skel/.gtkrc-2.0 %1").arg(dpath);
        execute(cmd);
        cmd = QString("cp -Rn /mnt/antiX/etc/skel/.config %1").arg(dpath);
        execute(cmd);
        cmd = QString("cp -Rn /mnt/antiX/etc/skel/.local %1").arg(dpath);
        execute(cmd);
    }
    // saving Desktop changes
    if (saveDesktopCheckBox->isChecked()) {
        execute("su -c 'dconf reset /org/blueman/transfer/shared-path' demo"); //reset blueman path
        cmd = QString("rsync -a --info=name1 /home/demo/ %1"
                      " --exclude '.cache' --exclude '.gvfs' --exclude '.dbus' --exclude '.Xauthority' --exclude '.ICEauthority'"
                      " --exclude '.mozilla' --exclude 'Installer.desktop' --exclude 'minstall.desktop' --exclude 'Desktop/antixsources.desktop'"
                      " --exclude '.jwm/menu' --exclude '.icewm/menu' --exclude '.fluxbox/menu'"
                      " --exclude '.config/rox.sourceforge.net/ROX-Filer/pb_antiX-fluxbox' --exclude '.config/rox.sourceforge.net/ROX-Filer/pb_antiX-icewm'"
                      " --exclude '.config/rox.sourceforge.net/ROX-Filer/pb_antiX-jwm' | xargs -I '$' sed -i 's|home/demo|home/" + userNameEdit->text() + "|g' %1/$").arg(dpath);
        execute(cmd);
    }
    // fix the ownership, demo=newuser
    cmd = QString("chown -R demo:demo %1").arg(dpath);
    if (!execute(cmd)) {
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
    replaceStringInFile("demo", userNameEdit->text(), "/mnt/antiX/home/*/.gtkrc-2.0");
    replaceStringInFile("demo", userNameEdit->text(), "/mnt/antiX/root/.gtkrc-2.0");
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
    execute(cmd);

    return true;
}

/////////////////////////////////////////////////////////////////////////
// computer name functions

bool MInstall::validateComputerName()
{
    qDebug() << "+++" << __PRETTY_FUNCTION__ << "+++";
    // see if name is reasonable
    nextFocus = computerNameEdit;
    if (computerNameEdit->text().isEmpty()) {
        QMessageBox::critical(this, windowTitle(), tr("Please enter a computer name."));
        return false;
    } else if (computerNameEdit->text().contains(QRegExp("[^0-9a-zA-Z-.]|^[.-]|[.-]$|\\.\\."))) {
        QMessageBox::critical(this, windowTitle(),
                              tr("Sorry, your computer name contains invalid characters.\nYou'll have to select a different\nname before proceeding."));
        return false;
    }
    // see if name is reasonable
    nextFocus = computerDomainEdit;
    if (computerDomainEdit->text().isEmpty()) {
        QMessageBox::critical(this, windowTitle(), tr("Please enter a domain name."));
        return false;
    } else if (computerDomainEdit->text().contains(QRegExp("[^0-9a-zA-Z-.]|^[.-]|[.-]$|\\.\\."))) {
        QMessageBox::critical(this, windowTitle(),
                              tr("Sorry, your computer domain contains invalid characters.\nYou'll have to select a different\nname before proceeding."));
        return false;
    }

    if (haveSamba) {
        // see if name is reasonable
        if (computerGroupEdit->text().isEmpty()) {
            QMessageBox::critical(this, windowTitle(), tr("Please enter a workgroup."));
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
        execute("mv -f /mnt/antiX/etc/rc5.d/K*smbd /mnt/antiX/etc/rc5.d/S06smbd >/dev/null 2>&1", false);
        execute("mv -f /mnt/antiX/etc/rc4.d/K*smbd /mnt/antiX/etc/rc4.d/S06smbd >/dev/null 2>&1", false);
        execute("mv -f /mnt/antiX/etc/rc3.d/K*smbd /mnt/antiX/etc/rc3.d/S06smbd >/dev/null 2>&1", false);
        execute("mv -f /mnt/antiX/etc/rc2.d/K*smbd /mnt/antiX/etc/rc2.d/S06smbd >/dev/null 2>&1", false);
        execute("mv -f /mnt/antiX/etc/rc5.d/K*samba-ad-dc /mnt/antiX/etc/rc5.d/S01samba-ad-dc >/dev/null 2>&1", false);
        execute("mv -f /mnt/antiX/etc/rc4.d/K*samba-ad-dc /mnt/antiX/etc/rc4.d/S01samba-ad-dc >/dev/null 2>&1", false);
        execute("mv -f /mnt/antiX/etc/rc3.d/K*samba-ad-dc /mnt/antiX/etc/rc3.d/S01samba-ad-dc >/dev/null 2>&1", false);
        execute("mv -f /mnt/antiX/etc/rc2.d/K*samba-ad-dc /mnt/antiX/etc/rc2.d/S01samba-ad-dc >/dev/null 2>&1", false);
        execute("mv -f /mnt/antiX/etc/rc5.d/K*nmbd /mnt/antiX/etc/rc5.d/S01nmbd >/dev/null 2>&1", false);
        execute("mv -f /mnt/antiX/etc/rc4.d/K*nmbd /mnt/antiX/etc/rc4.d/S01nmbd >/dev/null 2>&1", false);
        execute("mv -f /mnt/antiX/etc/rc3.d/K*nmbd /mnt/antiX/etc/rc3.d/S01nmbd >/dev/null 2>&1", false);
        execute("mv -f /mnt/antiX/etc/rc2.d/K*nmbd /mnt/antiX/etc/rc2.d/S01nmbd >/dev/null 2>&1", false);
    } else {
        execute("mv -f /mnt/antiX/etc/rc5.d/S*smbd /mnt/antiX/etc/rc5.d/K01smbd >/dev/null 2>&1", false);
        execute("mv -f /mnt/antiX/etc/rc4.d/S*smbd /mnt/antiX/etc/rc4.d/K01smbd >/dev/null 2>&1", false);
        execute("mv -f /mnt/antiX/etc/rc3.d/S*smbd /mnt/antiX/etc/rc3.d/K01smbd >/dev/null 2>&1", false);
        execute("mv -f /mnt/antiX/etc/rc2.d/S*smbd /mnt/antiX/etc/rc2.d/K01smbd >/dev/null 2>&1", false);
        execute("mv -f /mnt/antiX/etc/rc5.d/S*samba-ad-dc /mnt/antiX/etc/rc5.d/K01samba-ad-dc >/dev/null 2>&1", false);
        execute("mv -f /mnt/antiX/etc/rc4.d/S*samba-ad-dc /mnt/antiX/etc/rc4.d/K01samba-ad-dc >/dev/null 2>&1", false);
        execute("mv -f /mnt/antiX/etc/rc3.d/S*samba-ad-dc /mnt/antiX/etc/rc3.d/K01samba-ad-dc >/dev/null 2>&1", false);
        execute("mv -f /mnt/antiX/etc/rc2.d/S*samba-ad-dc /mnt/antiX/etc/rc2.d/K01samba-ad-dc >/dev/null 2>&1", false);
        execute("mv -f /mnt/antiX/etc/rc5.d/S*nmbd /mnt/antiX/etc/rc5.d/K01nmbd >/dev/null 2>&1", false);
        execute("mv -f /mnt/antiX/etc/rc4.d/S*nmbd /mnt/antiX/etc/rc4.d/K01nmbd >/dev/null 2>&1", false);
        execute("mv -f /mnt/antiX/etc/rc3.d/S*nmbd /mnt/antiX/etc/rc3.d/K01nmbd >/dev/null 2>&1", false);
        execute("mv -f /mnt/antiX/etc/rc2.d/S*nmbd /mnt/antiX/etc/rc2.d/K01nmbd >/dev/null 2>&1", false);
    }

    char rbuf[4];
    if (readlink("/mnt/antiX/sbin/init", rbuf, sizeof(rbuf)) >= 0) { // systemd check
        if (!sambaCheckBox->isChecked()) {
            execute("chroot /mnt/antiX systemctl disable smbd");
            execute("chroot /mnt/antiX systemctl disable nmbd");
            execute("chroot /mnt/antiX systemctl disable samba-ad-dc");
            execute("chroot /mnt/antiX systemctl mask smbd");
            execute("chroot /mnt/antiX systemctl mask nmbd");
            execute("chroot /mnt/antiX systemctl mask samba-ad-dc");
        }
    }

    //replaceStringInFile(PROJECTSHORTNAME + "1", computerNameEdit->text(), "/mnt/antiX/etc/hosts");
    QString cmd;
    cmd = QString("sed -i 's/'\"$(grep 127.0.0.1 /etc/hosts | grep -v localhost | head -1 | awk '{print $2}')\"'/" + computerNameEdit->text() + "/' /mnt/antiX/etc/hosts");
    execute(cmd, false);
    cmd = QString("echo \"%1\" | cat > /mnt/antiX/etc/hostname").arg(computerNameEdit->text());
    execute(cmd, false);
    cmd = QString("echo \"%1\" | cat > /mnt/antiX/etc/mailname").arg(computerNameEdit->text());
    execute(cmd, false);
    cmd = QString("sed -i 's/.*send host-name.*/send host-name \"%1\";/g' /mnt/antiX/etc/dhcp/dhclient.conf").arg(computerNameEdit->text());
    execute(cmd, false);
    cmd = QString("echo \"%1\" | cat > /mnt/antiX/etc/defaultdomain").arg(computerDomainEdit->text());
    execute(cmd, false);
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
    execute(cmd);
    cmd = QString("Language=%1").arg(localeCombo->currentData().toString());

    // /etc/localtime is either a file or a symlink to a file in /usr/share/zoneinfo. Use the one selected by the user.
    //replace with link
    cmd = QString("ln -nfs /usr/share/zoneinfo/%1 /mnt/antiX/etc/localtime").arg(timezoneCombo->currentText());
    execute(cmd, false);
    cmd = QString("ln -nfs /usr/share/zoneinfo/%1 /etc/localtime").arg(timezoneCombo->currentText());
    execute(cmd, false);
    // /etc/timezone is text file with the timezone written in it. Write the user-selected timezone in it now.
    cmd = QString("echo %1 > /mnt/antiX/etc/timezone").arg(timezoneCombo->currentText());
    execute(cmd, false);
    cmd = QString("echo %1 > /etc/timezone").arg(timezoneCombo->currentText());
    execute(cmd, false);

    // timezone
    execute("cp -f /etc/default/rcS /mnt/antiX/etc/default");
    // Set clock to use LOCAL
    if (localClockCheckBox->isChecked()) {
        execute("echo '0.0 0 0.0\n0\nLOCAL' > /etc/adjtime", false);
    } else {
        execute("echo '0.0 0 0.0\n0\nUTC' > /etc/adjtime", false);
    }
    execute("hwclock --hctosys");
    execute("cp -f /etc/adjtime /mnt/antiX/etc/");

    // Set clock format
    if (radio12h->isChecked()) {
        //mx systems
        execute("sed -i '/data0=/c\\data0=%l:%M' /home/demo/.config/xfce4/panel/xfce4-orageclock-plugin-1.rc", false);
        execute("sed -i '/data0=/c\\data0=%l:%M' /mnt/antiX/etc/skel/.config/xfce4/panel/xfce4-orageclock-plugin-1.rc", false);
        //antix systems
        execute("sed -i 's/%H:%M/%l:%M/g' /mnt/antiX/etc/skel/.icewm/preferences", false);
        execute("sed -i 's/%k:%M/%l:%M/g' /mnt/antiX/etc/skel/.fluxbox/init", false);
        execute("sed -i 's/%k:%M/%l:%M/g' /mnt/antiX/etc/skel/.jwm/tray", false);
    } else {
        //mx systems
        execute("sed -i '/data0=/c\\data0=%H:%M' /home/demo/.config/xfce4/panel/xfce4-orageclock-plugin-1.rc", false);
        execute("sed -i '/data0=/c\\data0=%H:%M' /mnt/antiX/etc/skel/.config/xfce4/panel/xfce4-orageclock-plugin-1.rc", false);
        //antix systems
        execute("sed -i 's/%H:%M/%H:%M/g' /mnt/antiX/etc/skel/.icewm/preferences", false);
        execute("sed -i 's/%k:%M/%k:%M/g' /mnt/antiX/etc/skel/.fluxbox/init", false);
        execute("sed -i 's/%k:%M/%k:%M/g' /mnt/antiX/etc/skel/.jwm/tray", false);
    }

    // localize repo
    qDebug() << "Localize repo";
    execute("chroot /mnt/antiX localize-repo default");
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
                    execute("chroot /mnt/antiX update-rc.d " + service + " enable");
                } else {
                    execute("chroot /mnt/antiX systemctl enable " + service);
                }
            } else {
                if (!systemd) {
                    execute("chroot /mnt/antiX update-rc.d " + service + " disable");
                } else {
                    execute("chroot /mnt/antiX systemctl disable " + service);
                    execute("chroot /mnt/antiX systemctl mask " + service);
                }
            }
        }
        ++it;
    }

    if (!isInsideVB()) {
        execute("mv -f /mnt/antiX/etc/rc5.d/S*virtualbox-guest-utils /mnt/antiX/etc/rc5.d/K01virtualbox-guest-utils >/dev/null 2>&1", false);
        execute("mv -f /mnt/antiX/etc/rc4.d/S*virtualbox-guest-utils /mnt/antiX/etc/rc4.d/K01virtualbox-guest-utils >/dev/null 2>&1", false);
        execute("mv -f /mnt/antiX/etc/rc3.d/S*virtualbox-guest-utils /mnt/antiX/etc/rc3.d/K01virtualbox-guest-utils >/dev/null 2>&1", false);
        execute("mv -f /mnt/antiX/etc/rc2.d/S*virtualbox-guest-utils /mnt/antiX/etc/rc2.d/K01virtualbox-guest-utils >/dev/null 2>&1", false);
        execute("mv -f /mnt/antiX/etc/rcS.d/S*virtualbox-guest-x11 /mnt/antiX/etc/rcS.d/K21virtualbox-guest-x11 >/dev/null 2>&1", false);
    }
}

void MInstall::failUI(const QString &msg)
{
    if (phase >= 0) {
        this->setEnabled(false);
        QMessageBox::critical(this, windowTitle(), msg);
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
            QString drv = "/dev/" + diskCombo->currentData().toString();
            QString msg = tr("OK to format and use the entire disk (%1) for %2?").arg(drv).arg(PROJECTNAME);
            int ans = QMessageBox::warning(this, windowTitle(), msg,
                                           QMessageBox::Yes, QMessageBox::No);
            if (ans != QMessageBox::Yes) { // don't format - stop install
                return curr;
            }
            calculateDefaultPartitions();
            return 4; // Go to Step_Progress
        }
    } else if (next == 3 && curr == 2) { // at Step_Partition (fwd)
        if (!validateChosenPartitions()) {
            return curr;
        }
        if (!pretend && !saveHomeBasic()) {
            const QString &msg = tr("The data in /home cannot be preserved because the required information could not be obtained.") + "\n"
                    + tr("If the partition containing /home is encrypted, please ensure the correct \"Encrypt\" boxes are selected, and that the entered password is correct.") + "\n"
                    + tr("The installer cannot encrypt an existing /home directory or partition.");
            QMessageBox::critical(this, windowTitle(), msg);
            return curr;
        }
        calculateChosenPartitions();
        return 4; // Go to Step_Progress
    } else if (curr == 3) { // at Step_FDE
        stashAdvancedFDE(next == 4);
        next = ixPageRefAdvancedFDE;
        ixPageRefAdvancedFDE = 0;
        return next;
    } else if (next == 3 && curr == 4) { // at Step_Progress (backward)
        if (!pretend && haveSnapshotUserAccounts) {
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
        if (!pretend && haveSnapshotUserAccounts) {
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
        mainHelp->setText(tr("<p><b>General Instructions</b><br/>BEFORE PROCEEDING, CLOSE ALL OTHER APPLICATIONS.</p>"
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
        if (phase < 0) {
            updateCursor(Qt::WaitCursor);
            phase = 0;
            updatePartitionWidgets();
            updateCursor();
        }
        break;

    case 2:  // choose partition
        mainHelp->setText(tr("<p><b>Limitations</b><br/>Remember, this software is provided AS-IS with no warranty what-so-ever. "
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
        break;

    case 3: // advanced encryption settings
        mainHelp->setText("<p><b>"
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
        mainHelp->setText("<p><b>" + tr("Installation in Progress") + "</b><br/>"
                          + tr("%1 is installing. For a fresh install, this will probably take 3-20 minutes, depending on the speed of your system and the size of any partitions you are reformatting.").arg(PROJECTNAME)
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
        mainHelp->setText(tr("<p><b>Select Boot Method</b><br/> %1 uses the GRUB bootloader to boot %1 and MS-Windows. "
                             "<p>By default GRUB2 is installed in the Master Boot Record (MBR) or ESP (EFI System Partition for 64-bit UEFI boot systems) of your boot drive and replaces the boot loader you were using before. This is normal.</p>"
                             "<p>If you choose to install GRUB2 to Partition Boot Record (PBR) instead, then GRUB2 will be installed at the beginning of the specified partition. This option is for experts only.</p>"
                             "<p>If you uncheck the Install GRUB box, GRUB will not be installed at this time. This option is for experts only.</p>").arg(PROJECTNAME));

        updateCursor(); // restore wait cursor set in install screen
        break;

    case 6: // set services
        mainHelp->setText(tr("<p><b>Common Services to Enable</b><br/>Select any of these common services that you might need with your system configuration and the services will be started automatically when you start %1.</p>").arg(PROJECTNAME));
        break;

    case 7: // set computer name
        mainHelp->setText(tr("<p><b>Computer Identity</b><br/>The computer name is a common unique name which will identify your computer if it is on a network. "
                             "The computer domain is unlikely to be used unless your ISP or local network requires it.</p>"
                             "<p>The computer and domain names can contain only alphanumeric characters, dots, hyphens. They cannot contain blank spaces, start or end with hyphens</p>"
                             "<p>The SaMBa Server needs to be activated if you want to use it to share some of your directories or printer "
                             "with a local computer that is running MS-Windows or Mac OSX.</p>"));
        break;

    case 8: // set localization, clock, services button
        mainHelp->setText(tr("<p><b>Localization Defaults</b><br/>Set the default keyboard and locale. These will apply unless they are overridden later by the user.</p>"
                             "<p><b>Configure Clock</b><br/>If you have an Apple or a pure Unix computer, by default the system clock is set to GMT or Universal Time. To change, check the box for 'System clock uses LOCAL.'</p>"
                             "<p><b>Timezone Settings</b><br/>The system boots with the timezone preset to GMT/UTC. To change the timezone, after you reboot into the new installation, right click on the clock in the Panel and select Properties.</p>"
                             "<p><b>Service Settings</b><br/>Most users should not change the defaults. Users with low-resource computers sometimes want to disable unneeded services in order to keep the RAM usage as low as possible. Make sure you know what you are doing! "));
        break;

    case 9: // set username and passwords
        mainHelp->setText(tr("<p><b>Default User Login</b><br/>The root user is similar to the Administrator user in some other operating systems. "
                             "You should not use the root user as your daily user account. "
                             "Please enter the name for a new (default) user account that you will use on a daily basis. "
                             "If needed, you can add other user accounts later with %1 User Manager. </p>"
                             "<p><b>Passwords</b><br/>Enter a new password for your default user account and for the root account. "
                             "Each password must be entered twice.</p>").arg(PROJECTNAME));
        if (!nextFocus) nextFocus = userNameEdit;
        break;

    case 10: // done
        closeButton->setEnabled(false);
        mainHelp->setText(tr("<p><b>Congratulations!</b><br/>You have completed the installation of %1</p>"
                             "<p><b>Finding Applications</b><br/>There are hundreds of excellent applications installed with %1 "
                             "The best way to learn about them is to browse through the Menu and try them. "
                             "Many of the apps were developed specifically for the %1 project. "
                             "These are shown in the main menus. "
                             "<p>In addition %1 includes many standard Linux applications that are run only from the command line and therefore do not show up in the Menu.</p>").arg(PROJECTNAME));
        break;

    default:
        // case 0 or any other
        mainHelp->setText("<p><b>" + tr("Enjoy using %1</b></p>").arg(PROJECTNAME) + "\n\n " + tr("<p><b>Support %1</b><br/>"
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
        if (!pretend && checkBoxExitReboot->isChecked()) {
            execute("/usr/local/bin/persist-config --shutdown --command reboot &", false);
        }
        qApp->exit(EXIT_SUCCESS);
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

void MInstall::updatePartitionWidgets()
{
    qDebug() << "+++" << __PRETTY_FUNCTION__ << "+++";

    diskCombo->setEnabled(false);
    diskCombo->clear();
    diskCombo->addItem(tr("Loading..."));
    buildBlockDevList();

    // disk combo box
    diskCombo->clear();
    for (const BlockDeviceInfo &bdinfo : listBlkDevs) {
        if (bdinfo.isDisk && bdinfo.size >= MIN_ROOT_DEVICE_SIZE
                && (!bdinfo.isBoot || INSTALL_FROM_ROOT_DEVICE)) {
            bdinfo.addToCombo(diskCombo);
        }
    }
    diskCombo->setCurrentIndex(0);
    diskCombo->setEnabled(true);

    // whole-disk vs custom-partition radio buttons
    existing_partitionsButton->hide();
    entireDiskButton->setChecked(true);
    for (const BlockDeviceInfo &bdinfo : listBlkDevs) {
        if (!bdinfo.isDisk) {
            // found at least one partition
            existing_partitionsButton->show();
            existing_partitionsButton->setChecked(true);
            break;
        }
    }

    // partition combo boxes
    updatePartitionCombos(NULL);
    on_rootCombo_currentIndexChanged(rootCombo->currentText());
}

void MInstall::updatePartitionCombos(QComboBox *changed)
{
    // rebuild the other combo boxes and leave the changed one alone
    for (QComboBox *combo : {rootCombo, swapCombo, homeCombo, bootCombo}) {
        if (combo != changed) {
            // block events for now and save the combo box state
            combo->blockSignals(true);
            QString curItem = combo->currentText();
            combo->clear();
            if (combo == homeCombo || combo == bootCombo) combo->addItem("root", "root");
            else if (combo == swapCombo) combo->addItem("none");

            // add each eligible partition that is not already selected elsewhere
            for (const BlockDeviceInfo &bdinfo : listBlkDevs) {
                if (!bdinfo.isDisk && (!bdinfo.isBoot || INSTALL_FROM_ROOT_DEVICE)
                        && !(rootCombo->currentText().startsWith(bdinfo.name))
                        && !(swapCombo->currentText().startsWith(bdinfo.name))
                        && !(homeCombo->currentText().startsWith(bdinfo.name))
                        && !(bootCombo->currentText().startsWith(bdinfo.name))) {
                    bool add = true;
                    if (combo == rootCombo) {
                        add = (!bdinfo.isSwap && bdinfo.size >= MIN_ROOT_DEVICE_SIZE);
                    } else if (combo == homeCombo) {
                        add = (!bdinfo.isSwap);
                    } else if (combo == bootCombo) {
                        add = (!bdinfo.isESP && bdinfo.size >= MIN_BOOT_DEVICE_SIZE);
                    }
                    if (add) bdinfo.addToCombo(combo);
                }
            }

            // restore the combo box state (if possible) and allow events again
            const int icur = combo->findText(curItem);
            if (icur >= 0) combo->setCurrentIndex(icur);
            combo->blockSignals(false);
        }
    }
    // if no valid root is found, the user should know
    if (rootCombo->count() == 0) rootCombo->addItem("none");
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

QStringList MInstall::splitDevice(const QString &devname) const
{
    static const QRegularExpression rxdev1("^(?:/dev/)+(mmcblk.*|nvme.*)p([0-9]*)$");
    static const QRegularExpression rxdev2("^(?:/dev/)+([a-z]*)([0-9]*)$");
    QRegularExpressionMatch rxmatch(rxdev1.match(devname));
    if (!rxmatch.hasMatch()) rxmatch = rxdev2.match(devname);
    QStringList list(rxmatch.capturedTexts());
    if (!list.isEmpty()) list.removeFirst();
    return list;
}

void MInstall::buildBlockDevList()
{
    execute("/sbin/partprobe", true);
    execute("blkid -c /dev/null", true);

    // expressions for matching various partition types
    static const QRegularExpression rxESP("^(c12a7328-f81f-11d2-ba4b-00a0c93ec93b|0xef)$");
    static const QRegularExpression rxSwap("^(0x82|0657fd6d-a4ab-43c4-84e5-0933c84b4f4f)$");
    static const QRegularExpression rxNative("^(0x83|0fc63daf-8483-4772-8e79-3d69d8477de4" // Linux data
                                             "|0x82|0657fd6d-a4ab-43c4-84e5-0933c84b4f4f" // Linux swap
                                             "|44479540-f297-41b2-9af7-d131d5f0458a" // Linux /root x86
                                             "|4f68bce3-e8cd-4db1-96e7-fbcaf984b709" // Linux /root x86-64
                                             "|933ac7e1-2eb4-4f13-b844-0e14e2aef915)$"); // Linux /home
    static const QRegularExpression rxWinLDM("^(0x42|5808c8aa-7e8f-42e0-85d2-e1e90434cfb3"
                                             "|e3c9e316-0b5c-4db8-817d-f92df00215ae)$"); // Windows LDM
    static const QRegularExpression rxNativeFS("^(btrfs|ext2|ext3|ext4|jfs|nilfs2|reiser4|reiserfs|ufs|xfs)$");

    QString bootUUID;
    if (QFile::exists("/live/config/initrd.out")) {
        QSettings livecfg("/live/config/initrd.out", QSettings::NativeFormat);
        bootUUID = livecfg.value("BOOT_UUID").toString();
    }

    // backup detection for drives that don't have UUID for ESP
    const QStringList backup_list = getCmdOuts("fdisk -l -o DEVICE,TYPE |grep 'EFI System' |cut -d\\  -f1 | cut -d/ -f3");

    // populate the block device list
    listBlkDevs.clear();
    bool gpt = false; // propagates to all partitions within the drive
    int driveIndex = 0; // for propagating the nasty flag to the drive
    const QStringList &blkdevs = getCmdOuts("lsblk -brno TYPE,NAME,UUID,SIZE,PARTTYPE,FSTYPE,LABEL,MODEL"
                                            " | grep -E '^(disk|part)'");
    for (const QString &blkdev : blkdevs) {
        const QStringList &bdsegs = blkdev.split(' ');
        const int segsize = bdsegs.size();
        if (segsize < 3) continue;

        BlockDeviceInfo bdinfo;
        bdinfo.isFuture = false;
        bdinfo.isDisk = (bdsegs[0] == "disk");
        if (bdinfo.isDisk) {
            driveIndex = listBlkDevs.count();
            gpt = isGpt("/dev/" + bdinfo.name);
        } else {
            // if it looks like an apple...
            if (execute("grub-probe -d /dev/sda2 2>/dev/null | grep hfsplus", false)) {
                bdinfo.isNasty = true;
                localClockCheckBox->setChecked(true);
            }
        }
        bdinfo.isGPT = gpt;

        bdinfo.name = bdsegs[1];
        const QString &uuid = bdsegs[2];

        bdinfo.size = bdsegs[3].toLongLong();
        bdinfo.isBoot = (!bootUUID.isEmpty() && uuid == bootUUID);
        if (segsize > 4) {
            const QString &seg4 = bdsegs[4];
            bdinfo.isESP = (seg4.count(rxESP) >= 1);
            bdinfo.isSwap = (seg4.count(rxSwap) >= 1);
            bdinfo.isNative = (seg4.count(rxNative) >= 1);
            if (!bdinfo.isNasty) bdinfo.isNasty = (seg4.count(rxWinLDM) >= 1);
        } else {
            bdinfo.isESP = bdinfo.isSwap = bdinfo.isNative = false;
        }

        if (!bdinfo.isDisk && !bdinfo.isESP) {
            // check the backup ESP detection list
            bdinfo.isESP = backup_list.contains(bdinfo.name);
        }
        if (segsize > 5) {
            bdinfo.fs = bdsegs[5];
            if(bdinfo.fs.count(rxNativeFS) >= 1) bdinfo.isNative = true;
        }
        if (segsize > 6) {
            const QByteArray seg(bdsegs[6].toUtf8().replace('%', "\\x25").replace("\\x", "%"));
            bdinfo.label = QUrl::fromPercentEncoding(seg).trimmed();
        }
        if (segsize > 7) {
            const QByteArray seg(bdsegs[7].toUtf8().replace('%', "\\x25").replace("\\x", "%"));
            bdinfo.model = QUrl::fromPercentEncoding(seg).trimmed();
        }
        listBlkDevs << bdinfo;
        // propagate the nasty flag up to the drive
        if (bdinfo.isNasty) listBlkDevs[driveIndex].isNasty = true;
    }
    // also refresh the backup block device list
    listBlkDevsBackup = listBlkDevs;

    // debug
    qDebug() << "Name Size Model FS | isDisk isGPT isBoot isESP isNative isSwap";
    for (const BlockDeviceInfo &bdi : listBlkDevs) {
        qDebug() << bdi.name << bdi.size << bdi.model << bdi.fs << "|"
                 << bdi.isDisk << bdi.isGPT << bdi.isBoot << bdi.isESP
                 << bdi.isNative << bdi.isSwap;
    }
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
        category = list[0];
        description = list[1];

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
// event handlers

void MInstall::changeEvent(QEvent *event)
{
    const QEvent::Type etype = event->type();
    if (etype == QEvent::ApplicationPaletteChange
        || etype == QEvent::PaletteChange || etype == QEvent::StyleChange)
    {
        QPalette pal = mainHelp->style()->standardPalette();
        QColor col = pal.color(QPalette::Base);
        col.setAlpha(150);
        pal.setColor(QPalette::Base, col);
        mainHelp->setPalette(pal);
        resizeEvent(NULL);
    }
}

void MInstall::resizeEvent(QResizeEvent *)
{
    mainHelp->resize(tab->size());
    helpbackdrop->resize(mainHelp->size());
}

void MInstall::closeEvent(QCloseEvent *event)
{
    if (abort(true)) {
        event->accept();
        cleanup();
        QWidget::closeEvent(event);
        if (widgetStack->currentWidget() != Step_End) {
            qApp->exit(EXIT_FAILURE);
        } else {
            proc->waitForFinished();
            qApp->exit(EXIT_SUCCESS);
        }
    } else {
        event->ignore();
    }
}

void MInstall::reject()
{
    // dummy (overrides QDialog::reject() so Escape won't close the window)
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
    execute("[ -f /usr/sbin/gparted ] && /usr/sbin/gparted || /usr/bin/partitionmanager", false);
    updatePartitionWidgets();
    qtpartedButton->setEnabled(true);
    nextButton->setEnabled(true);
    updateCursor();
}

void MInstall::on_buttonBenchmarkFDE_clicked()
{
    execute("x-terminal-emulator -e bash -c \"/sbin/cryptsetup benchmark"
            " && echo && read -n 1 -srp 'Press any key to close the benchmark window.'\"");
}

// root partition changed, rebuild home, swap, boot combo boxes
void MInstall::on_rootCombo_currentIndexChanged(const QString &text)
{
    updatePartitionCombos(rootCombo);
    rootLabelEdit->setEnabled(!text.isEmpty());
    rootTypeCombo->setEnabled(!text.isEmpty());
    checkBoxEncryptRoot->setEnabled(!text.isEmpty());
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
    // help the installer if it was stuck at the config pages
    if (onclose) {
        phase = -2;
    } else if (phase == 2 && !haveSysConfig) {
        phase = -1;
        gotoPage(1);
    } else {
        phase = -1;
    }
    if (!onclose) this->setEnabled(true);
    return true;
}

// run before closing the app, do some cleanup
void MInstall::cleanup(bool endclean)
{
    qDebug() << "+++" << __PRETTY_FUNCTION__ << "+++";
    if (pretend) return;

    if (endclean) {
        execute("command -v xfconf-query >/dev/null && su $(logname) -c 'xfconf-query --channel thunar-volman --property /automount-drives/enabled --set " + auto_mount.toUtf8() + "'", false);
        execute("cp /var/log/minstall.log /mnt/antiX/var/log >/dev/null 2>&1", false);
        execute("rm -rf /mnt/antiX/mnt/antiX >/dev/null 2>&1", false);
    }
    execute("umount -l /mnt/antiX/boot/efi", true);
    execute("(umount -l /mnt/antiX/proc; umount -l /mnt/antiX/sys; umount -l /mnt/antiX/dev/shm; umount -l /mnt/antiX/dev) >/dev/null 2>&1", false);
    execute("umount -l /mnt/antiX/home", true);
    execute("umount -lR /mnt/antiX", true);

    if (isRootEncrypted) {
        execute("cryptsetup luksClose rootfs", true);
    }
    if (isHomeEncrypted) {
        execute("cryptsetup luksClose homefs", true);
    }
    if (isSwapEncrypted) {
        execute("cryptsetup luksClose swapfs", true);
    }
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
    close();
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
    hide();
    execute("fskbsetting", false);
    show();
    setupkeyboardbutton();
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
        QMessageBox::warning(this, windowTitle(),
                             tr("This option also encrypts swap partition if selected, which will render the swap partition unable to be shared with other installed operating systems."),
                             QMessageBox::Ok);
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

void MInstall::on_homeCombo_currentIndexChanged(const QString &text)
{
    updatePartitionCombos(homeCombo);
    if (!homeCombo->isEnabled() || text.isEmpty()) return;
    homeLabelEdit->setEnabled(text != "root");
    homeTypeCombo->setEnabled(text != "root");
    checkBoxEncryptHome->setEnabled(text != "root");
    checkBoxEncryptHome->setChecked(checkBoxEncryptRoot->isChecked() && text == "root");
    if (text == "root") {
        homeTypeCombo->setCurrentIndex(rootTypeCombo->currentIndex());
    }
}

void MInstall::on_swapCombo_currentIndexChanged(const QString &text)
{
    updatePartitionCombos(swapCombo);
    swapLabelEdit->setEnabled(text != "none");
}

void MInstall::on_bootCombo_currentIndexChanged(int)
{
    updatePartitionCombos(bootCombo);
}

void MInstall::on_grubCheckBox_toggled(bool checked)
{
    grubEspButton->setEnabled(checked && canESP);
    grubMbrButton->setEnabled(checked && canMBR);
    grubPbrButton->setEnabled(checked && canPBR);
    grubInsLabel->setEnabled(checked);
    grubBootLabel->setEnabled(checked);
    grubBootCombo->setEnabled(checked);
}

void MInstall::on_grubMbrButton_toggled()
{
    grubBootCombo->clear();
    for (const BlockDeviceInfo &bdinfo : listBlkDevs) {
        if (bdinfo.isDisk) {
            if (!bdinfo.isNasty || brave) bdinfo.addToCombo(grubBootCombo, true);
        }
    }
    grubBootLabel->setText(tr("System boot disk:"));
}

void MInstall::on_grubPbrButton_toggled()
{
    grubBootCombo->clear();
    for (const BlockDeviceInfo &bdinfo : listBlkDevs) {
        if (!(bdinfo.isDisk || bdinfo.isSwap || bdinfo.isBoot || bdinfo.isESP)
                 && bdinfo.isNative && bdinfo.fs != "crypto_LUKS") {
            // list only Linux partitions excluding crypto_LUKS partitions
            if (!bdinfo.isNasty || brave) bdinfo.addToCombo(grubBootCombo, true);
        }
    }
    grubBootLabel->setText(tr("Partition to use:"));
}

void MInstall::on_grubEspButton_toggled()
{
    grubBootCombo->clear();
    for (const BlockDeviceInfo &bdinfo : listBlkDevs) {
        if (bdinfo.isESP) bdinfo.addToCombo(grubBootCombo);
    }
    grubBootLabel->setText(tr("Partition to use:"));
}

// build ESP list available to install GRUB
void MInstall::buildBootLists()
{
    // refresh lists and enable or disable options according to device presence
    on_grubMbrButton_toggled();
    canMBR = (grubBootCombo->count() > 0);
    grubMbrButton->setEnabled(canMBR);
    on_grubPbrButton_toggled();
    canPBR = (grubBootCombo->count() > 0);
    grubPbrButton->setEnabled(canPBR);
    on_grubEspButton_toggled();
    canESP = (uefi && grubBootCombo->count() > 0);
    grubEspButton->setEnabled(canESP);

    if (canESP) grubEspButton->click();
    else {
        // load MBR list as a default if no ESP exists
        on_grubMbrButton_toggled();
        grubMbrButton->click();
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
