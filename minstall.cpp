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
#include <QProcessEnvironment>
#include <QTimeZone>
#include <fcntl.h>
#include <sys/statvfs.h>
#include <sys/stat.h>

#include "version.h"
#include "minstall.h"

MInstall::MInstall(const QCommandLineParser &args, const QString &cfgfile)
    : proc(this), partman(proc, listBlkDevs, *this, this)
{
    setupUi(this);
    listLog->addItem("Version " VERSION);
    proc.setupUI(listLog, progressBar);
    updateCursor(Qt::WaitCursor);
    setWindowFlags(Qt::Window); // for the close, min and max buttons
    installBox->hide();

    oobe = args.isSet("oobe");
    pretend = args.isSet("pretend");
    nocopy = args.isSet("nocopy");
    sync = args.isSet("sync");
    if(!oobe) {
        brave = args.isSet("brave");
        automatic = args.isSet("auto");
        oem = args.isSet("oem");
        gptoverride = args.isSet("gpt-override");
    } else {
        brave = automatic = oem = gptoverride = false;
        closeButton->setText(tr("Shutdown"));
        phase = 2;
        // dark palette for the OOBE screen
        QColor charcoal(56, 56, 56);
        QPalette pal;
        pal.setColor(QPalette::Window, charcoal);
        pal.setColor(QPalette::WindowText, Qt::white);
        pal.setColor(QPalette::Base, charcoal.darker());
        pal.setColor(QPalette::AlternateBase, charcoal);
        pal.setColor(QPalette::Text, Qt::white);
        pal.setColor(QPalette::Button, charcoal);
        pal.setColor(QPalette::ButtonText, Qt::white);
        pal.setColor(QPalette::Active, QPalette::Button, charcoal);
        pal.setColor(QPalette::Disabled, QPalette::Light, charcoal.darker());
        pal.setColor(QPalette::Disabled, QPalette::Text, Qt::darkGray);
        pal.setColor(QPalette::Disabled, QPalette::WindowText, Qt::darkGray);
        pal.setColor(QPalette::Disabled, QPalette::ButtonText, Qt::darkGray);
        pal.setColor(QPalette::Highlight, Qt::lightGray);
        pal.setColor(QPalette::HighlightedText, Qt::black);
        pal.setColor(QPalette::ToolTipBase, Qt::black);
        pal.setColor(QPalette::ToolTipText, Qt::white);
        pal.setColor(QPalette::Link, Qt::cyan);
        qApp->setPalette(pal);
    }
    //if (pretend) listHomes = qApp->arguments(); // dummy existing homes

    // setup system variables
    QSettings settings("/usr/share/gazelle-installer-data/installer.conf", QSettings::NativeFormat);
    PROJECTNAME = settings.value("PROJECT_NAME").toString();
    PROJECTSHORTNAME = settings.value("PROJECT_SHORTNAME").toString();
    PROJECTVERSION = settings.value("VERSION").toString();
    PROJECTURL = settings.value("PROJECT_URL").toString();
    PROJECTFORUM = settings.value("FORUM_URL").toString();
    INSTALL_FROM_ROOT_DEVICE = settings.value("INSTALL_FROM_ROOT_DEVICE").toBool();
    DEFAULT_HOSTNAME = settings.value("DEFAULT_HOSTNAME").toString();
    ENABLE_SERVICES = settings.value("ENABLE_SERVICES").toStringList();
    POPULATE_MEDIA_MOUNTPOINTS = settings.value("POPULATE_MEDIA_MOUNTPOINTS").toBool();
    MIN_INSTALL_SIZE = settings.value("MIN_INSTALL_SIZE").toString();
    PREFERRED_MIN_INSTALL_SIZE = settings.value("PREFERRED_MIN_INSTALL_SIZE").toString();
    REMOVE_NOSPLASH = settings.value("REMOVE_NOSPLASH", "false").toBool();
    setWindowTitle(tr("%1 Installer").arg(PROJECTNAME));

    gotoPage(0);

    // config file
    config = new MSettings(cfgfile, this);

    // Link block
    QString link_block;
    settings.beginGroup("LINKS");
    QStringList links = settings.childKeys();
    for (const QString &link : links) {
        link_block += "\n\n" + tr(link.toUtf8().constData()) + ": " + settings.value(link).toString();
    }
    settings.endGroup();

    // set some distro-centric text
    remindersBrowser->setPlainText(tr("Support %1\n\n%1 is supported by people like you. Some help others at the support forum - %2, or translate help files into different languages, or make suggestions, write documentation, or help test new software.").arg(PROJECTNAME, PROJECTFORUM)
                        + "\n" + link_block);

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
    proc.log(__PRETTY_FUNCTION__);
    if (oobe) {
        containsSystemD = QFileInfo("/usr/bin/systemctl").isExecutable();
        saveDesktopCheckBox->hide();
    } else {
        containsSystemD = QFileInfo("/live/aufs/bin/systemctl").isExecutable();

        rootSources = "/live/aufs/bin /live/aufs/dev"
                      " /live/aufs/etc /live/aufs/lib /live/aufs/lib64 /live/aufs/media /live/aufs/mnt"
                      " /live/aufs/opt /live/aufs/root /live/aufs/sbin /live/aufs/selinux /live/aufs/usr"
                      " /live/aufs/var /live/aufs/home";

        //load some live variables
        QSettings livesettings("/live/config/initrd.out",QSettings::NativeFormat);
        SQFILE_FULL = livesettings.value("SQFILE_FULL", "/live/boot-dev/antiX/linuxfs").toString();
        isRemasteredDemoPresent = checkForRemaster();

        // calculate required disk space
        bootSource = "/live/aufs/boot";
        bootSpaceNeeded = proc.execOut("du -sb " + bootSource).section('\t', 0, 0).toLongLong();

        //rootspaceneeded is the size of the linuxfs file * a compression factor + contents of the rootfs.  conservative but fast
        //factors are same as used in live-remaster

        //get compression factor by reading the linuxfs squasfs file, if available
        qDebug() << "linuxfs file is at : " << SQFILE_FULL;
        long long compression_factor;
        QString linuxfs_compression_type = "xz"; //default conservative
        if (QFileInfo::exists(SQFILE_FULL)) {
            linuxfs_compression_type = proc.execOut("dd if=" + SQFILE_FULL + " bs=1 skip=20 count=2 status=none 2>/dev/null| od -An -tdI");
        }
        //gzip, xz, or lz4
        if ( linuxfs_compression_type == "1") {
            compression_factor = 37; // gzip
        } else if (linuxfs_compression_type == "2") {
            compression_factor = 52; //lzo, not used by antiX
        } else if (linuxfs_compression_type == "3") {
            compression_factor = 52;  //lzma, not used by antiX
        } else if (linuxfs_compression_type == "4") {
            compression_factor = 31; //xz
        } else if (linuxfs_compression_type == "5") {
            compression_factor = 52; // lz4
        } else {
            compression_factor = 30; //anythng else or linuxfs not reachable (toram), should be pretty conservative
        }

        qDebug() << "linuxfs compression type is " << linuxfs_compression_type << "compression factor is " << compression_factor;

        long long rootfs_file_size = 0;
        long long linuxfs_file_size = proc.execOut("df /live/linux --output=used --total |tail -n1").toLongLong() * 1024 * 100 / compression_factor;
        if (QFileInfo::exists("/live/perist-root")) {
            rootfs_file_size = proc.execOut("df /live/persist-root --output=used --total |tail -n1").toLongLong() * 1024;
        }

        qDebug() << "linuxfs file size is " << linuxfs_file_size << " rootfs file size is " << rootfs_file_size;

        //add rootfs file size to the calculated linuxfs file size.  probaby conservative, as rootfs will likely have some overlap with linuxfs
        long long safety_factor = 128 * 1024 * 1024; // 128 MB safety factor
        rootSpaceNeeded = linuxfs_file_size + rootfs_file_size + safety_factor;

        if (!pretend && bootSpaceNeeded==0) {
            QMessageBox::warning(this, windowTitle(),
                 tr("Cannot access source medium.\nActivating pretend installation."));
            pretend = true;
        }
        const long long spaceBlock = 134217728; // 128MB
        bootSpaceNeeded += 2*spaceBlock - (bootSpaceNeeded % spaceBlock);

        qDebug() << "Minimum space:" << bootSpaceNeeded << "(boot)," << rootSpaceNeeded << "(root)";

        // uefi = false if not uefi, or if a bad combination, like 32 bit iso and 64 bit uefi)
        if (proc.exec("uname -m | grep -q i686", false) && proc.exec("grep -q 64 /sys/firmware/efi/fw_platform_size")) {
            uefi = false;
        } else {
            uefi = proc.exec("test -d /sys/firmware/efi", true);
        }
        qDebug() << "uefi =" << uefi;

        autoMountEnabled = true; // disable auto mount by force
        if (!pretend) setupAutoMount(false);

        // advanced encryption settings page defaults
        on_comboFDEcipher_currentIndexChanged(comboFDEcipher->currentText());
        on_comboFDEchain_currentIndexChanged(comboFDEchain->currentText());
        on_comboFDEivgen_currentIndexChanged(comboFDEivgen->currentText());

        partman.defaultLabels["/boot"] = "boot";
        partman.defaultLabels["/"] = "root" + PROJECTSHORTNAME + PROJECTVERSION;
        partman.defaultLabels["/home"] = "home" + PROJECTSHORTNAME;
        partman.defaultLabels["swap"] = "swap" + PROJECTSHORTNAME;

        FDEpassword->hide();
        FDEpassword2->hide();
        labelFDEpass->hide();
        labelFDEpass2->hide();
        pbFDEpassMeter->hide();
        buttonAdvancedFDE->hide();
        gbEncrPass->hide();

        //disable encryption in gui if cryptsetup not present
        QFileInfo cryptsetup("/sbin/cryptsetup");
        QFileInfo crypsetupinitramfs("/usr/share/initramfs-tools/conf-hooks.d/cryptsetup");
        if ( !cryptsetup.exists() && !cryptsetup.isExecutable() && !crypsetupinitramfs.exists()) {
            checkBoxEncryptAuto->hide();
            buttonAdvancedFDE->hide();
            buttonAdvancedFDECust->hide();
            treePartitions->setColumnHidden(4, true);
        }

        // Detect snapshot-backup account(s)
        haveSnapshotUserAccounts = checkForSnapshot();
    }

    // Password box setup
    FDEpassword->setup(FDEpassword2, pbFDEpassMeter, 1, 32, 9);
    FDEpassCust->setup(FDEpassCust2, pbFDEpassMeterCust, 1, 32, 9);
    userPasswordEdit->setup(userPasswordEdit2, pbUserPassMeter);
    rootPasswordEdit->setup(rootPasswordEdit2, pbRootPassMeter);
    connect(FDEpassword, &MLineEdit::validationChanged, this, &MInstall::diskPassValidationChanged);
    connect(FDEpassCust, &MLineEdit::validationChanged, this, &MInstall::diskPassValidationChanged);
    connect(userPasswordEdit, &MLineEdit::validationChanged, this, &MInstall::userPassValidationChanged);
    connect(rootPasswordEdit, &MLineEdit::validationChanged, this, &MInstall::userPassValidationChanged);
    // User name is required
    connect(userNameEdit, &QLineEdit::textChanged, this, &MInstall::userPassValidationChanged);

    // set default host name
    computerNameEdit->setText(DEFAULT_HOSTNAME);

    setupkeyboardbutton();

    // timezone lists
    listTimeZones = proc.execOutLines("find -L /usr/share/zoneinfo/posix -mindepth 2 -type f -printf %P\\n", true);
    cmbTimeArea->clear();
    for (const QString &zone : listTimeZones) {
        const QString &area = zone.section('/', 0, 0);
        if (cmbTimeArea->findData(QVariant(area)) < 0) {
            QString text(area);
            if (area == "Indian" || area == "Pacific"
                || area == "Atlantic" || area == "Arctic") text.append(" Ocean");
            cmbTimeArea->addItem(text, area);
        }
    }
    cmbTimeArea->model()->sort(0);

    // locale list
    localeCombo->clear();
    QStringList loclist = proc.execOutLines("locale -a | grep -Ev '^(C|POSIX)\\.?' | grep -E 'utf8|UTF-8'");
    for (QString &strloc : loclist) {
        strloc.replace("utf8", "UTF-8", Qt::CaseInsensitive);
        QLocale loc(strloc);
        localeCombo->addItem(loc.nativeCountryName() + " - " + loc.nativeLanguageName(), QVariant(strloc));
    }
    localeCombo->model()->sort(0);
    // default locale selection
    int ixLocale = localeCombo->findData(QVariant(QLocale::system().name() + ".UTF-8"));
    if (localeCombo->currentIndex() != ixLocale) localeCombo->setCurrentIndex(ixLocale);
    else on_localeCombo_currentIndexChanged(ixLocale);

    // if it looks like an apple...
    if (proc.exec("grub-probe -d /dev/sda2 2>/dev/null | grep hfsplus", false)) {
        mactest = true;
        localClockCheckBox->setChecked(true);
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
    QString val = proc.execOut("dpkg -s samba | grep '^Status.*ok.*' | sed -e 's/.*ok //'");
    haveSamba = (val.compare("installed") == 0);

    buildServiceList();
    if (oobe) manageConfig(ConfigLoadB);
    else {
        updatePartitionWidgets();
        manageConfig(ConfigLoadA);
        stashAdvancedFDE(true);
    }
    stashServices(true);

    if (oobe) gotoPage(7); // go to Network page
    else {
        copyrightBrowser->setPlainText(tr("%1 is an independent Linux distribution based on Debian Stable.\n\n"
            "%1 uses some components from MEPIS Linux which are released under an Apache free license."
            " Some MEPIS components have been modified for %1.\n\nEnjoy using %1").arg(PROJECTNAME));
        gotoPage(1);
    }
    updateCursor();

    // automatic installation
    if (automatic) nextButton->click();
}

// turn auto-mount off and on
void MInstall::setupAutoMount(bool enabled)
{
    proc.log(__PRETTY_FUNCTION__);

    if (autoMountEnabled == enabled) return;
    QFileInfo finfo;
    // check if the systemctl program is present
    bool have_sysctl = false;
    const QStringList &envpath = QProcessEnvironment::systemEnvironment().value("PATH").split(':');
    for(const QString &path : envpath) {
        finfo.setFile(path + "/systemctl");
        if (finfo.isExecutable()) {
            have_sysctl = true;
            break;
        }
    }
    // check if udisksd is running.
    bool udisksd_running = false;
    if (proc.exec("ps -e | grep 'udisksd'")) udisksd_running = true;
    // create a list of rules files that are being temporarily overridden
    QStringList udev_temp_mdadm_rules;
    finfo.setFile("/run/udev");
    if (finfo.isDir()) {
        udev_temp_mdadm_rules = proc.execOutLines("egrep -l '^[^#].*mdadm (-I|--incremental)' /lib/udev/rules.d");
        for (QString &rule : udev_temp_mdadm_rules) {
            rule.replace("/lib/udev", "/run/udev");
        }
    }

    // auto-mount setup
    if (!enabled) {
        // disable auto-mount
        if (have_sysctl) {
            // Use systemctl to prevent automount by masking currently unmasked mount points
            listMaskedMounts = proc.execOutLines("systemctl list-units --full --all -t mount --no-legend 2>/dev/null | grep -v masked | cut -f1 -d' '"
                " | egrep -v '^(dev-hugepages|dev-mqueue|proc-sys-fs-binfmt_misc|run-user-.*-gvfs|sys-fs-fuse-connections|sys-kernel-config|sys-kernel-debug)'").join(' ');
            if (!listMaskedMounts.isEmpty()) {
                proc.exec("systemctl --runtime mask --quiet -- " + listMaskedMounts);
            }
        }
        // create temporary blank overrides for all udev rules which
        // automatically start Linux Software RAID array members
        proc.exec("mkdir -p /run/udev/rules.d");
        for (const QString &rule : udev_temp_mdadm_rules) {
            proc.exec("touch " + rule);
        }
        if (udisksd_running) {
            proc.exec("echo 'SUBSYSTEM==\"block\", ENV{UDISKS_IGNORE}=\"1\"' > /run/udev/rules.d/91-mx-udisks-inhibit.rules");
            proc.exec("udevadm control --reload");
            proc.exec("udevadm trigger --subsystem-match=block");
        }
    } else {
        // enable auto-mount
        if (udisksd_running) {
            proc.exec("rm -f /run/udev/rules.d/91-mx-udisks-inhibit.rules");
            proc.exec("udevadm control --reload");
            proc.exec("partprobe -s");
            proc.sleep(1000);
        }
        // clear the rules that were temporarily overridden
        for (const QString &rule : udev_temp_mdadm_rules) {
            proc.exec("rm -f " + rule);
        }
        // Use systemctl to restore that status of any mount points changed above
        if (have_sysctl && !listMaskedMounts.isEmpty()) {
            proc.exec("systemctl --runtime unmask --quiet -- $MOUNTLIST");
        }
    }
    autoMountEnabled = enabled;
}

/////////////////////////////////////////////////////////////////////////
// util functions

// Check if running inside VirtualBox
bool MInstall::isInsideVB()
{
    return proc.exec("lspci -d 80ee:beef  | grep -q .", false);
}

bool MInstall::replaceStringInFile(const QString &oldtext, const QString &newtext, const QString &filepath)
{
    QString cmd = QString("sed -i 's/%1/%2/g' %3").arg(oldtext, newtext, filepath);
    return proc.exec(cmd);
}

void MInstall::updateCursor(const Qt::CursorShape shape)
{
    if (shape != Qt::ArrowCursor) {
        qApp->setOverrideCursor(QCursor(shape));
    } else {
        while (qApp->overrideCursor() != nullptr) {
            qApp->restoreOverrideCursor();
        }
    }
    qApp->processEvents();
}

bool MInstall::pretendToInstall(int start, int stop)
{
    for (int ixi = start; ixi <= stop; ++ixi) {
        proc.status(tr("Pretending to install %1").arg(PROJECTNAME), ixi);
        proc.sleep(phase == 1 ? 100 : 1000, true);
        if (phase < 0) return false;
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
    const QLineEdit *passedit = checkBoxEncryptAuto->isChecked() ? FDEpassword : FDEpassCust;
    const QByteArray password(passedit->text().toUtf8());
    if (isRootEncrypted) { // if encrypting root
        bool newkey = (key.length() == 0);

        //create keyfile
        if (newkey) key.load(rngfile.toUtf8(), keylength);
        key.save("/mnt/antiX/root/keyfile", 0400);

        //add keyfile to container
        QString swapUUID;
        if (!swapDevice.isEmpty()) {
            swapUUID = proc.execOut("blkid -s UUID -o value " + swapDevice);

            proc.exec("cryptsetup luksAddKey " + swapDevice + " /mnt/antiX/root/keyfile",
                    true, &password);
        }

        if (isHomeEncrypted && newkey) { // if encrypting separate /home
            proc.exec("cryptsetup luksAddKey " + homeDevice + " /mnt/antiX/root/keyfile",
                    true, &password);
        }
        QString rootUUID = proc.execOut("blkid -s UUID -o value " + rootDevice);
        //write crypttab keyfile entry
        QFile file("/mnt/antiX/etc/crypttab");
        if (file.open(QIODevice::WriteOnly)) {
            QTextStream out(&file);
            out << "rootfs /dev/disk/by-uuid/" + rootUUID +" none luks \n";
            if (isHomeEncrypted) {
                QString homeUUID =  proc.execOut("blkid -s UUID -o value " + homeDevice);
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
            swapUUID = proc.execOut("blkid -s UUID -o value " + swapDevice);

            proc.exec("cryptsetup luksAddKey " + swapDevice + " /mnt/antiX/home/.keyfileDONOTdelete",
                    true, &password);
        }
        QString homeUUID = proc.execOut("blkid -s UUID -o value " + homeDevice);
        //write crypttab keyfile entry
        QFile file("/mnt/antiX/etc/crypttab");
        if (file.open(QIODevice::WriteOnly)) {
            QTextStream out(&file);
            out << "homefs /dev/disk/by-uuid/" + homeUUID +" none luks \n";
            if (!swapDevice.isEmpty()) {
                out << "swapfs /dev/disk/by-uuid/" + swapUUID +" /home/.keyfileDONOTdelete luks,nofail \n";
                proc.exec("sed -i 's/^CRYPTDISKS_MOUNT.*$/CRYPTDISKS_MOUNT=\"\\/home\"/' /mnt/antiX/etc/default/cryptdisks", false);
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
        proc.exec("touch /mnt/antiX/initramfs-tools/conf.d/resume");
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
    proc.log(__PRETTY_FUNCTION__);
    if (phase < 0) return -1;
    mkdir(point.toUtf8(), 0755);
    QString cmd = QString("/bin/mount %1 %2 -o %3").arg(dev).arg(point).arg(mntops);
    return proc.exec(cmd);
}

// process the next phase of installation if possible
bool MInstall::processNextPhase()
{
    widgetStack->setEnabled(true);
    const int progPhase23 = 94; // start of Phase 2/3 progress bar space
    // Phase < 0 = install has been aborted (Phase -2 on close)
    if (phase < 0) return false;
    // Phase 0 = install not started yet, Phase 1 = install in progress
    // Phase 2 = waiting for operator input, Phase 3 = post-install steps
    if (phase == 0) { // no install started yet
        proc.status(tr("Preparing to install %1").arg(PROJECTNAME), 0);
        if (!partman.checkTargetDrivesOK()) return false;
        phase = 1; // installation.

        // cleanup previous mounts
        cleanup(false);

        // the core of the installation
        if (!pretend) {
            bool ok = makePartitions();
            if (ok) ok = formatPartitions();
            if (!ok) {
                failUI(tr("Failed to format required partitions."));
                return false;
            }
            //run blkid -c /dev/null to freshen UUID cache
            proc.exec("blkid -c /dev/null", true);
            if (!installLinux(progPhase23 - 1)) return false;
        } else if (!pretendToInstall(1, progPhase23 - 1)) {
            return false;
        }
        if (widgetStack->currentWidget() != Step_Progress) {
            progressBar->setEnabled(false);
            proc.status(tr("Paused for required operator input"), progPhase23);
            QApplication::beep();
        }
        phase = 2;
    }
    if (phase == 2 && widgetStack->currentWidget() == Step_Progress) {
        phase = 3;
        progressBar->setEnabled(true);
        backButton->setEnabled(false);
        if (!pretend) {
            proc.status(tr("Setting system configuration"), progPhase23);
            if (!isInsideVB() && !oobe) {
                proc.exec("/bin/mv -f /mnt/antiX/etc/rc5.d/S*virtualbox-guest-utils /mnt/antiX/etc/rc5.d/K01virtualbox-guest-utils >/dev/null 2>&1", false);
                proc.exec("/bin/mv -f /mnt/antiX/etc/rc4.d/S*virtualbox-guest-utils /mnt/antiX/etc/rc4.d/K01virtualbox-guest-utils >/dev/null 2>&1", false);
                proc.exec("/bin/mv -f /mnt/antiX/etc/rc3.d/S*virtualbox-guest-utils /mnt/antiX/etc/rc3.d/K01virtualbox-guest-utils >/dev/null 2>&1", false);
                proc.exec("/bin/mv -f /mnt/antiX/etc/rc2.d/S*virtualbox-guest-utils /mnt/antiX/etc/rc2.d/K01virtualbox-guest-utils >/dev/null 2>&1", false);
                proc.exec("/bin/mv -f /mnt/antiX/etc/rcS.d/S*virtualbox-guest-x11 /mnt/antiX/etc/rcS.d/K21virtualbox-guest-x11 >/dev/null 2>&1", false);
            }
            if (oem) enableOOBE();
            else if(!processOOBE()) return false;
            manageConfig(ConfigSave);
            config->dumpDebug();
            proc.exec("/bin/sync", true); // the sync(2) system call will block the GUI
            if (!installLoader()) return false;
        } else if (!pretendToInstall(progPhase23, 99)){
            return false;
        }
        phase = 4;
        proc.status(tr("Cleaning up"), 100);
        cleanup();
        gotoPage(widgetStack->indexOf(Step_End));
    }
    return true;
}

void MInstall::manageConfig(enum ConfigAction mode)
{
    if (mode == ConfigSave) {
        delete config;
        config = new MSettings("/mnt/antiX/etc/minstall.conf", this);
    }
    if (!config) return;
    config->bad = false;

    if (mode == ConfigSave) {
        config->setSave(true);
        config->clear();
        config->setValue("Version", VERSION);
        config->setValue("Product", PROJECTNAME + " " + PROJECTVERSION);
    }
    if ((mode == ConfigSave || mode == ConfigLoadA) && !oobe) {
        config->startGroup("Storage", Step_Disk);
        const char *diskChoices[] = {"Drive", "Partitions"};
        QRadioButton *diskRadios[] = {entireDiskButton, customPartButton};
        config->manageRadios("Target", 2, diskChoices, diskRadios);
        const bool targetDrive = entireDiskButton->isChecked();

        if (targetDrive) {
            // Disk drive setup
            config->manageComboBox("Drive", diskCombo, true);
            config->manageCheckBox("DriveEncrypt", checkBoxEncryptAuto);
        } else {
            // Partition step
            config->setGroupWidget(Step_Partitions);
            config->manageCheckBox("SaveHome", saveHomeCheck);
            config->manageCheckBox("BadBlocksCheck", badblocksCheck);
            // Swap space
            //config->manageComboBox("Swap/Device", swapCombo, true);
            //config->manageCheckBox("Swap/Encrypt", checkBoxEncryptSwap);
            //config->manageLineEdit("Swap/Label", swapLabelEdit);
            // Tree starting with root (/)
            config->beginGroup("Tree");
            //config->manageComboBox("Device", rootCombo, true);
            //config->manageCheckBox("Encrypt", checkBoxEncryptRoot);
            //config->manageComboBox("Type", rootTypeCombo, false);
            //config->manageLineEdit("Label", rootLabelEdit);
            // Boot (/boot)
            //config->manageComboBox("boot/Device", bootCombo, true);
            // Home (/home)
            //config->manageComboBox("home/Device", homeCombo, true);
            //config->manageComboBox("home/Type", homeTypeCombo, false);
            //config->manageCheckBox("home/Encrypt", checkBoxEncryptHome);
            //config->manageLineEdit("home/Label", homeLabelEdit);
            config->endGroup();
        }
        config->endGroup();

        // AES step
        config->startGroup("Encryption", Step_FDE);
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
        config->manageComboBox("Cipher", comboFDEcipher, false);
        config->manageComboBox("ChainMode", comboFDEchain, false);
        config->manageComboBox("IVgenerator", comboFDEivgen, false);
        config->manageComboBox("IVhash", comboFDEivhash, false);
        config->manageSpinBox("KeySize", spinFDEkeysize);
        config->manageComboBox("LUKSkeyHash", comboFDEhash, false);
        config->manageComboBox("KernelRNG", comboFDErandom, false);
        config->manageSpinBox("KDFroundTime", spinFDEroundtime);
        config->endGroup();
        if (config->isBadWidget(Step_FDE)) {
            config->setGroupWidget(targetDrive ? Step_Disk : Step_Partitions);
            config->markBadWidget(buttonAdvancedFDE);
            config->markBadWidget(buttonAdvancedFDECust);
        }
    }

    if (mode == ConfigSave || mode == ConfigLoadB) {
        if (!oobe) {
            // GRUB step
            config->startGroup("GRUB", Step_Boot);
            if(grubCheckBox->isChecked()) {
                const char *grubChoices[] = {"MBR", "PBR", "ESP"};
                QRadioButton *grubRadios[] = {grubMbrButton, grubPbrButton, grubEspButton};
                config->manageRadios("Install", 3, grubChoices, grubRadios);
            }
            config->manageComboBox("Location", grubBootCombo, true);
            config->endGroup();
        }

        // Services step
        config->startGroup("Services", Step_Services);
        QTreeWidgetItemIterator it(csView);
        while (*it) {
            if ((*it)->parent() != nullptr) {
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

        // Network step
        config->startGroup("Network", Step_Network);
        config->manageLineEdit("ComputerName", computerNameEdit);
        config->manageLineEdit("Domain", computerDomainEdit);
        config->manageLineEdit("Workgroup", computerGroupEdit);
        config->manageCheckBox("Samba", sambaCheckBox);
        config->endGroup();

        // Localization step
        config->startGroup("Localization", Step_Localization);
        config->manageComboBox("Locale", localeCombo, true);
        config->manageCheckBox("LocalClock", localClockCheckBox);
        const char *clockChoices[] = {"24", "12"};
        QRadioButton *clockRadios[] = {radio24h, radio12h};
        config->manageRadios("ClockHours", 2, clockChoices, clockRadios);
        if (mode == ConfigSave) {
            config->setValue("Timezone", cmbTimeZone->currentData().toString());
        } else {
            QVariant def(QString(QTimeZone::systemTimeZoneId()));
            const int rc = selectTimeZone(config->value("Timezone", def).toString());
            if (rc == 1) config->markBadWidget(cmbTimeArea);
            else if (rc == 2) config->markBadWidget(cmbTimeZone);
        }
        config->manageComboBox("Timezone", cmbTimeZone, true);
        config->endGroup();

        // User Accounts step
        config->startGroup("User", Step_User_Accounts);
        config->manageLineEdit("Username", userNameEdit);
        config->manageCheckBox("Autologin", autologinCheckBox);
        config->manageCheckBox("SaveDesktop", saveDesktopCheckBox);
        if(oobe) saveDesktopCheckBox->setCheckState(Qt::Unchecked);
        const char *oldHomeActions[] = {"Use", "Save", "Delete"};
        QRadioButton *oldHomeRadios[] = {radioOldHomeUse, radioOldHomeSave, radioOldHomeDelete};
        config->manageRadios("OldHomeAction", 3, oldHomeActions, oldHomeRadios);
        if (mode != ConfigSave) {
            const QString &upass = config->value("UserPass").toString();
            userPasswordEdit->setText(upass);
            userPasswordEdit2->setText(upass);
            const QString &rpass = config->value("RootPass").toString();
            rootPasswordEdit->setText(rpass);
            rootPasswordEdit2->setText(rpass);
        }
        config->endGroup();
    }

    if (mode == ConfigSave) {
        config->sync();
        QFile::copy(config->fileName(), "/etc/minstalled.conf");
    }

    if (config->bad) {
        QMessageBox::critical(this, windowTitle(),
            tr("Invalid settings found in configuration file (%1)."
               " Please review marked fields as you encounter them.").arg(config->fileName()));
    }
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
    proc.log(__PRETTY_FUNCTION__);
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
            proc.status(statup);
            if (!partman.luksMake(swapdev, encPass)) return false;
        }
        proc.status(statup);
        if (!partman.luksOpen(swapdev, "swapfs", encPass)) return false;
        swapdev = "/dev/mapper/swapfs";
    }
    if (isRootEncrypted) {
        if (rootFormatSize) {
            proc.status(statup);
            if (!partman.luksMake(rootdev, encPass)) return false;
        }
        proc.status(statup);
        if (!partman.luksOpen(rootdev, "rootfs", encPass)) return false;
        rootdev = "/dev/mapper/rootfs";
    }
    if (isHomeEncrypted) {
        if (homeFormatSize) {
            proc.status(statup);
            if (!partman.luksMake(homedev, encPass)) return false;
        }
        proc.status(statup);
        if (!partman.luksOpen(homedev, "homefs", encPass)) return false;
        homedev = "/dev/mapper/homefs";
    }

    //if no swap is chosen do nothing
    if (swapFormatSize) {
        proc.status(tr("Formatting swap partition"));
        QString cmd("/sbin/mkswap " + swapdev);
        //const QString &mkswaplabel = swapLabelEdit->text();
        //if (!mkswaplabel.isEmpty()) cmd.append(" -L \"" + mkswaplabel + "\"");
        if (!proc.exec(cmd, true)) return false;
    }

    // maybe format root (if not saving /home on root), or if using --sync option
    if (rootFormatSize) {
        proc.status(tr("Formatting the / (root) partition"));
        if (!makeLinuxPartition(rootdev, "ext4", //rootTypeCombo->currentText(),
                                badblocksCheck->isChecked(), "")) { // rootLabelEdit->text())) {
            return false;
        }
    }
    if (!mountPartition(rootdev, "/mnt/antiX", root_mntops)) return false;

    // format and mount /boot if different than root
    if (bootFormatSize) {
        proc.status(tr("Formatting boot partition"));
        if (!makeLinuxPartition(bootDevice, "ext4", false, "boot")) return false;
    }

    // format ESP if necessary
    if (espFormatSize) {
        proc.status(tr("Formatting EFI System Partition"));
        if (!proc.exec("mkfs.msdos -F 32 " + espDevice)) return false;
        proc.exec("parted -s /dev/" + BlockDeviceInfo::split(espDevice).at(0) + " set 1 esp on"); // sets boot flag and esp flag
        proc.sleep(1000);
    }
    // maybe format home
    if (homeFormatSize) {
        proc.status(tr("Formatting the /home partition"));
        if (!makeLinuxPartition(homedev, "ext4", //homeTypeCombo->currentText(),
                                badblocksCheck->isChecked(), "")) { //homeLabelEdit->text())) {
            return false;
        }
        proc.exec("/bin/rm -r /mnt/antiX/home", true);
    }
    mkdir("/mnt/antiX/home", 0755);
    if (homedev != rootDevice) {
        // not on root
        proc.status(tr("Mounting the /home partition"));
        if (!mountPartition(homedev, "/mnt/antiX/home", home_mntops)) return false;
    }

    return true;
}

bool MInstall::makeLinuxPartition(const QString &dev, const QString &type, bool chkBadBlocks, const QString &label)
{
    proc.log(__PRETTY_FUNCTION__);
    if (phase < 0) return false;
    QString homedev = homeDevice;
    boot_mntops = "defaults,noatime";
    if (homedev == dev || dev == "/dev/mapper/homefs") {  // if formatting /home partition
        home_mntops = "defaults,noatime";
    } else {
        root_mntops = "defaults,noatime";
    }

    QString cmd;
    if (type == "reiserfs") {
        cmd = "mkfs.reiserfs -q";
    } else if (type == "reiser4") {
        cmd = "mkfs.reiser4 -f -y";
    } else if (type.startsWith("btrfs")) {
        // btrfs and set up fsck
        proc.exec("/bin/cp -fp /bin/true /sbin/fsck.auto");
        // set creation options for small drives using btrfs
        QString size_str = proc.execOut("/sbin/sfdisk -s " + dev);
        long long size = size_str.toLongLong();
        size = size / 1024; // in MiB
        // if drive is smaller than 6GB, create in mixed mode
        if (size < 6000) {
            cmd = "mkfs.btrfs -f -M -O skinny-metadata";
        } else {
            cmd = "mkfs.btrfs -f";
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
    } else if (type == "xfs" || type == "f2fs") {
        cmd = "mkfs." + type + " -f";
    } else { // jfs, ext2, ext3, ext4
        cmd = "mkfs." + type;
        if (type == "jfs") cmd.append(" -q");
        else cmd.append(" -F");
        if (chkBadBlocks) cmd.append(" -c");
    }

    cmd.append(" " + dev);
    if (!label.isEmpty()) {
        if (type == "reiserfs" || type == "f2fs") cmd.append(" -l \"");
        else cmd.append(" -L \"");
        cmd.append(label + "\"");
    }
    if (!proc.exec(cmd)) return false;

    if (type.startsWith("ext")) {
        // ext4 tuning
        proc.exec("/sbin/tune2fs -c0 -C0 -i1m " + dev);
    }
    proc.sleep(1000);
    return true;
}

///////////////////////////////////////////////////////////////////////////
// Make the chosen partitions and mount them

bool MInstall::makePartitions()
{
    proc.log(__PRETTY_FUNCTION__);
    if (phase < 0) return false;

    // detach all existing partitions on the selected drive
    for (const QString &strdev : listToUnmount) {
        proc.exec("swapoff /dev/" + strdev, true);
        proc.exec("/bin/umount /dev/" + strdev, true);
    }
    listToUnmount.clear();

    long long start = 1; // start with 1 MB to aid alignment

    auto lambdaPreparePart = [this, &start]
            (const QString &strdev, long long size, const QString &type) -> bool {
        const QStringList devsplit = BlockDeviceInfo::split(strdev);
        bool rc = true;
        // size=0 = nothing, size>0 = creation, size<0 = allocation.
        if (size > 0) {
            const long long end = start + size;
            rc = proc.exec("parted -s --align optimal /dev/" + devsplit.at(0) + " mkpart " + type
                         + " " + QString::number(start) + "MiB " + QString::number(end) + "MiB");
            start = end;
        } else if (size < 0){
            // command to set the partition type
            QString cmd;
            const int ixdev = listBlkDevs.findDevice(devsplit.at(0));
            if (ixdev >= 0 && listBlkDevs.at(ixdev).isGPT) {
                cmd = "/sbin/sgdisk /dev/%1 --typecode=%2:8303";
            } else {
                cmd = "/sbin/sfdisk /dev/%1 --part-type %2 83";
            }
            rc = proc.exec(cmd.arg(devsplit.at(0), devsplit.at(1)));
        }
        proc.sleep(1000);
        return rc;
    };

    qDebug() << " ---- PARTITION FORMAT SCHEDULE ----";
    qDebug() << "Root:" << rootDevice << rootFormatSize;
    qDebug() << "Home:" << homeDevice << homeFormatSize;
    qDebug() << "Swap:" << swapDevice << swapFormatSize;
    qDebug() << "Boot:" << bootDevice << bootFormatSize;
    qDebug() << "ESP:" << espDevice << espFormatSize;
    qDebug() << "BIOS-GRUB:" << biosGrubDevice;

    if (entireDiskButton->isChecked()) {
        const QString &drv = diskCombo->currentData().toString();
        proc.status(tr("Creating required partitions"));
        //proc.exec(QStringLiteral("/bin/dd if=/dev/zero of=/dev/%1 bs=512 count=100").arg(drv));
        clearpartitiontables(drv);
        const bool useGPT = listBlkDevs.at(listBlkDevs.findDevice(drv)).isGPT;
        if (!proc.exec("parted -s /dev/" + drv + " mklabel " + (useGPT ? "gpt" : "msdos"))) return false;
    } else {
        proc.status(tr("Preparing required partitions"));
    }
    proc.sleep(1000);

    // any new partitions they will appear in this order on the disk
    if (!biosGrubDevice.isEmpty()) {
        if (!lambdaPreparePart(biosGrubDevice, 1, "primary")) return false;
    }
    if (!lambdaPreparePart(espDevice, espFormatSize, "ESP")) return false;
    if (!lambdaPreparePart(bootDevice, bootFormatSize, "primary")) return false;
    if (!lambdaPreparePart(rootDevice, rootFormatSize, "primary ext4")) return false;
    if (!lambdaPreparePart(homeDevice, homeFormatSize, "primary")) return false;
    if (!lambdaPreparePart(swapDevice, swapFormatSize, "primary")) return false;
    proc.exec("partprobe -s", true);
    proc.sleep(1000);
    return true;
}

bool MInstall::saveHomeBasic()
{
    proc.log(__PRETTY_FUNCTION__);
    if (!(saveHomeCheck->isChecked())) return true;
    cleanup(false); // cleanup previous mounts
    // if preserving /home, obtain some basic information
    bool ok = false;
    const QByteArray &pass = FDEpassCust->text().toUtf8();
    QString rootdev = rootDevice;
    QString homedev = homeDevice;
    // mount the root partition
    if (isRootEncrypted) {
        if (!partman.luksOpen(rootdev, "rootfs", pass, "--readonly")) return false;
        rootdev = "/dev/mapper/rootfs";
    }
    if (!mountPartition(rootdev, "/mnt/antiX", "ro")) goto ending2;
    // mount the home partition
    if (homedev != rootDevice) {
        if (isHomeEncrypted) {
            if (!partman.luksOpen(homedev, "homefs", pass, "--readonly")) goto ending2;
            homedev = "/dev/mapper/homefs";
        }
        if (!mountPartition(homedev, "/mnt/home-tmp", "ro")) goto ending1;
    }

    // store a listing of /home to compare with the user name given later
    listHomes = proc.execOutLines("/bin/ls -1 /mnt/home-tmp/");
    // recycle the old key for /home if possible
    key.load("/mnt/antiX/root/keyfile", -1);

    ok = true;
 ending1:
    // unmount partitions
    if (homedev != rootDevice) {
        proc.exec("/bin/umount -l /mnt/home-tmp", false);
        if (isHomeEncrypted) proc.exec("cryptsetup close homefs", true);
        proc.exec("rmdir /mnt/home-tmp");
    }
 ending2:
    proc.exec("/bin/umount -l /mnt/antiX", false);
    if (isRootEncrypted) proc.exec("cryptsetup close rootfs", true);
    return ok;
}

bool MInstall::installLinux(const int progend)
{
    proc.log(__PRETTY_FUNCTION__);
    QString rootdev = (isRootEncrypted ? "/dev/mapper/rootfs" : rootDevice);
    if (phase < 0) return false;

    if (!(rootFormatSize || sync)) {
        // if root was not formatted and not using --sync option then re-use it
        proc.status(tr("Mounting the / (root) partition"));
        mountPartition(rootdev, "/mnt/antiX", root_mntops);
        // remove all folders in root except for /home
        proc.status(tr("Deleting old system"));
        proc.exec("find /mnt/antiX -mindepth 1 -maxdepth 1 ! -name home -exec rm -r {} \\;", false);

        if (proc.exitStatus() != QProcess::NormalExit) {
            failUI(tr("Failed to delete old %1 on destination.\nReturning to Step 1.").arg(PROJECTNAME));
            return false;
        }
    }

    // make empty dirs for opt, dev, proc, sys, run,
    // home already done
    proc.status(tr("Creating system directories"));
    mkdir("/mnt/antiX/opt", 0755);
    mkdir("/mnt/antiX/dev", 0755);
    mkdir("/mnt/antiX/proc", 0755);
    mkdir("/mnt/antiX/sys", 0755);
    mkdir("/mnt/antiX/run", 0755);

    //if separate /boot in use, mount that to /mnt/antiX/boot
    if (!bootDevice.isEmpty() && bootDevice != rootDevice) {
        mkdir("/mnt/antiX/boot", 0755);
        proc.exec("fsck.ext4 -y " + bootDevice); // needed to run fsck because sfdisk --part-type can mess up the partition
        if (!mountPartition(bootDevice, "/mnt/antiX/boot", boot_mntops)) {
            qDebug() << "Could not mount /boot on " + bootDevice;
            return false;
        }
    }

    setupAutoMount(true);
    if(!copyLinux(progend - 1)) return false;

    proc.status(tr("Fixing configuration"), progend);
    mkdir("/mnt/antiX/tmp", 01777);
    chmod("/mnt/antiX/tmp", 01777);

    // Copy live set up to install and clean up.
    proc.exec("/usr/sbin/live-to-installed /mnt/antiX", false);
    qDebug() << "Desktop menu";
    proc.exec("chroot /mnt/antiX desktop-menu --write-out-global", false);

    makeFstab();
    writeKeyFile();
    disablehiberanteinitramfs();

    //remove home unless a demo home is found in remastered linuxfs
    if (!isRemasteredDemoPresent) {
        proc.exec("/bin/rm -rf /mnt/antiX/home/demo");
    }

    // if POPULATE_MEDIA_MOUNTPOINTS is true in gazelle-installer-data, don't clean /media folder
    // not sure if this is still needed with the live-to-installed change but OK
    if (!POPULATE_MEDIA_MOUNTPOINTS) {
        proc.exec("/bin/rm -rf /mnt/antiX/media/sd*", false);
        proc.exec("/bin/rm -rf /mnt/antiX/media/hd*", false);
    }

    // guess localtime vs UTC
    if (proc.execOut("guess-hwclock") == "localtime") {
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
    QString rootdevUUID = "UUID=" + proc.execOut(cmdBlkID + rootDevice);
    QString homedevUUID = "UUID=" + proc.execOut(cmdBlkID + homeDevice);
    QString swapdevUUID = "UUID=" + proc.execOut(cmdBlkID + swapDevice);
    const QString bootdevUUID = "UUID=" + proc.execOut(cmdBlkID + bootDevice);

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

    QString fstype = proc.execOut("blkid " + rootdev + " -o value -s TYPE");
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
            out << bootdevUUID + " /boot ext4 " + boot_mntops + " 1 1\n";
        }
        if (grubEspButton->isChecked()) {
            const QString espdev = "/dev/" + grubBootCombo->currentData().toString();
            const QString espdevUUID = "UUID=" + proc.execOut(cmdBlkID + espdev);
            qDebug() << "espdev" << espdev << espdevUUID;
            out << espdevUUID + " /boot/efi vfat defaults,noatime,dmask=0002,fmask=0113 0 0\n";
        }
        if (!homedev.isEmpty() && homedev != rootDevice) {
            fstype = proc.execOut("blkid " + homedev + " -o value -s TYPE");
            if (homeFormatSize) {
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
        proc.exec("/sbin/make-fstab -O --install=/mnt/antiX --mntpnt=/media");
    }
}

bool MInstall::copyLinux(const int progend)
{
    proc.log(__PRETTY_FUNCTION__);
    if (phase < 0) return false;

    // copy most except usr, mnt and home
    // must copy boot even if saving, the new files are required
    // media is already ok, usr will be done next, home will be done later
    proc.status(tr("Copying new system"));
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
    cmd.append(" " + bootSource + " " + rootSources + " /mnt/antiX");
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
        qDebug() << "Exec COPY:" << cmd;
        QListWidgetItem *logEntry = proc.log(cmd, MProcess::Exec);

        QEventLoop eloop;
        connect(&proc, static_cast<void(QProcess::*)(int, QProcess::ExitStatus)>(&QProcess::finished), &eloop, &QEventLoop::quit);
        connect(&proc, static_cast<void(QProcess::*)()>(&QProcess::readyRead), &eloop, &QEventLoop::quit);
        proc.start(cmd);
        const int progspace = progend - progstart;
        const int progdiv = (progspace != 0) ? (sourceInodes / progspace) : 0;
        while (proc.state() != QProcess::NotRunning) {
            eloop.exec();
            proc.readAllStandardOutput();
            if(statvfs("/mnt/antiX", &svfs) == 0 && progdiv != 0) {
                int i = (svfs.f_files - svfs.f_ffree - targetInodes) / progdiv;
                if (i > progspace) i = progspace;
                progressBar->setValue(i + progstart);
            } else {
                proc.status(tr("Copy progress unknown. No file system statistics."));
            }
        }
        disconnect(&proc, static_cast<void(QProcess::*)()>(&QProcess::readyRead), nullptr, nullptr);
        disconnect(&proc, static_cast<void(QProcess::*)(int, QProcess::ExitStatus)>(&QProcess::finished), nullptr, nullptr);

        qDebug() << "Exit COPY:" << proc.exitCode() << proc.exitStatus();
        if (proc.exitStatus() != QProcess::NormalExit) {
            proc.log(logEntry, -1);
            failUI(tr("Failed to write %1 to destination.\nReturning to Step 1.").arg(PROJECTNAME));
            return false;
        }
        proc.log(logEntry, proc.exitCode() ? 0 : 1);
    }

    return true;
}

///////////////////////////////////////////////////////////////////////////
// install loader

// build a grub configuration and install grub
bool MInstall::installLoader()
{
    proc.log(__PRETTY_FUNCTION__);
    if (phase < 0) return false;

    const QString &statup = tr("Installing GRUB");
    proc.status(statup);
    QString cmd;
    QString val = proc.execOut("/bin/ls /mnt/antiX/boot | grep 'initrd.img-3.6'");

    // the old initrd is not valid for this hardware
    if (!val.isEmpty()) {
        proc.exec("/bin/rm -f /mnt/antiX/boot/" + val);
    }

    if (!grubCheckBox->isChecked()) {
        // skip it
        //if useing f2fs, then add modules to /etc/initramfs-tools/modules
        qDebug() << "Update initramfs";
        //if (rootTypeCombo->currentText() == "f2fs" || homeTypeCombo->currentText() == "f2fs") {
            //proc.exec("grep -q f2fs /mnt/antiX/etc/initramfs-tools/modules || echo f2fs >> /mnt/antiX/etc/initramfs-tools/modules");
            //proc.exec("grep -q crypto-crc32 /mnt/antiX/etc/initramfs-tools/modules || echo crypto-crc32 >> /mnt/antiX/etc/initramfs-tools/modules");
        //}
        proc.exec("chroot /mnt/antiX update-initramfs -u -t -k all");
        return true;
    }

    //add switch to change root partition info
    QString boot = grubBootCombo->currentData().toString();
    const int ixboot = listBlkDevs.findDevice(boot);
    boot.insert(0, "/dev/");

    if (grubMbrButton->isChecked() && ixboot >= 0) {
        QString part_num;
        if (bootDevice.startsWith(boot)) part_num = bootDevice;
        else if (rootDevice.startsWith(boot)) part_num = rootDevice;
        else if (homeDevice.startsWith(boot)) part_num = homeDevice;
        if (!part_num.isEmpty()) {
            // remove the non-digit part to get the number of the root partition
            part_num.remove(QRegularExpression("\\D+\\d*\\D+"));
        }
        const char *bootflag = nullptr;
        if (listBlkDevs.at(ixboot).isGPT) {
            if (!part_num.isEmpty()) bootflag = " legacy_boot on";
            if (!biosGrubDevice.isEmpty()) {
                QStringList devsplit(BlockDeviceInfo::split(biosGrubDevice));
                proc.exec("parted -s /dev/" + devsplit.at(0) + " set " + devsplit.at(1) + " bios_grub on", true);
            }
        } else if (entireDiskButton->isChecked()) {
            if (!part_num.isEmpty()) bootflag = " boot on";
        }
        if (bootflag) proc.exec("parted -s " + boot + " set " + part_num + bootflag, true);
    }

    proc.sleep(1000);

    // set mounts for chroot
    proc.exec("/bin/mount -o bind /dev /mnt/antiX/dev", true);
    proc.exec("/bin/mount -o bind /sys /mnt/antiX/sys", true);
    proc.exec("/bin/mount -o bind /proc /mnt/antiX/proc", true);
    proc.exec("/bin/mount -o bind /run /mnt/antiX/run", true);

    QString arch;

    // install new Grub now
    if (!grubEspButton->isChecked()) {
        cmd = QString("grub-install --target=i386-pc --recheck --no-floppy --force --boot-directory=/mnt/antiX/boot %1").arg(boot);
    } else {
        mkdir("/mnt/antiX/boot/efi", 0755);
        QString mount = QString("/bin/mount %1 /mnt/antiX/boot/efi").arg(boot);
        proc.exec(mount);
        // rename arch to match grub-install target
        arch = proc.execOut("cat /sys/firmware/efi/fw_platform_size");
        if (arch == "32") {
            arch = "i386";
        } else if (arch == "64") {
            arch = "x86_64";
        }

        cmd = QString("chroot /mnt/antiX grub-install --no-nvram --force-extra-removable --target=%1-efi --efi-directory=/boot/efi --bootloader-id=%2%3 --recheck").arg(arch, PROJECTSHORTNAME, PROJECTVERSION);
    }

    qDebug() << "Installing Grub";
    if (!proc.exec(cmd)) {
        // error
        QMessageBox::critical(this, windowTitle(),
                              tr("GRUB installation failed. You can reboot to the live medium and use the GRUB Rescue menu to repair the installation."));
        proc.exec("/bin/umount /mnt/antiX/proc", true);
        proc.exec("/bin/umount /mnt/antiX/sys", true);
        proc.exec("/bin/umount /mnt/antiX/dev", true);
        proc.exec("/bin/umount /mnt/antiX/run", true);
        if (proc.exec("mountpoint -q /mnt/antiX/boot/efi", true)) {
            proc.exec("/bin/umount /mnt/antiX/boot/efi", true);
        }
        return false;
    }

    // update NVRAM boot entries (only if installing on ESP)
    proc.status(statup);
    if (grubEspButton->isChecked()) {
        cmd = QString("chroot /mnt/antiX grub-install --force-extra-removable --target=%1-efi --efi-directory=/boot/efi --bootloader-id=%2%3 --recheck").arg(arch, PROJECTSHORTNAME, PROJECTVERSION);
        if (!proc.exec(cmd)) {
            QMessageBox::warning(this, windowTitle(), tr("NVRAM boot variable update failure. The system may not boot, but it can be repaired with the GRUB Rescue boot menu."));
        }
    }

    //added non-live boot codes to those in /etc/default/grub, remove duplicates
    //get non-live boot codes
    QString cmdline = proc.execOut("/live/bin/non-live-cmdline");

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
    proc.exec(cmd, false);

    //copy memtest efi files if needed

    if (uefi) {
        mkdir("/mnt/antiX/boot/uefi-mt", 0755);
        if (arch == "i386") {
            proc.exec("/bin/cp /live/boot-dev/boot/uefi-mt/mtest-32.efi /mnt/antiX/boot/uefi-mt", true);
        } else {
            proc.exec("/bin/cp /live/boot-dev/boot/uefi-mt/mtest-64.efi /mnt/antiX/boot/uefi-mt", true);
        }
    }
    proc.status(statup);

    //update grub with new config

    qDebug() << "Update Grub";
    proc.exec("chroot /mnt/antiX update-grub");
    proc.status(statup);

    qDebug() << "Update initramfs";
    //if useing f2fs, then add modules to /etc/initramfs-tools/modules
    //if (rootTypeCombo->currentText() == "f2fs" || homeTypeCombo->currentText() == "f2fs") {
        //proc.exec("grep -q f2fs /mnt/antiX/etc/initramfs-tools/modules || echo f2fs >> /mnt/antiX/etc/initramfs-tools/modules");
        //proc.exec("grep -q crypto-crc32 /mnt/antiX/etc/initramfs-tools/modules || echo crypto-crc32 >> /mnt/antiX/etc/initramfs-tools/modules");
    //}
    proc.exec("chroot /mnt/antiX update-initramfs -u -t -k all");
    proc.status(statup);
    qDebug() << "clear chroot env";
    proc.exec("/bin/umount /mnt/antiX/proc", true);
    proc.exec("/bin/umount /mnt/antiX/sys", true);
    proc.exec("/bin/umount /mnt/antiX/dev", true);
    proc.exec("/bin/umount /mnt/antiX/run", true);
    if (proc.exec("mountpoint -q /mnt/antiX/boot/efi", true)) {
        proc.exec("/bin/umount /mnt/antiX/boot/efi", true);
    }

    return true;
}

// out-of-box experience
void MInstall::enableOOBE()
{
    setServices(); // Disable services to speed up the OOBE boot.
    proc.exec("chroot /mnt/antiX/ update-rc.d oobe defaults", true);
}
bool MInstall::processOOBE()
{
    setServices();
    if (!setComputerName()) return false;
    setLocale();
    if (haveSnapshotUserAccounts) { // skip user account creation
        QString cmd = "rsync -a /home/ /mnt/antiX/home/ --exclude '.cache' --exclude '.gvfs' --exclude '.dbus' --exclude '.Xauthority' --exclude '.ICEauthority'";
        proc.exec(cmd);
    } else {
        if (!setUserInfo()) return false;
    }
    if(oobe) proc.exec("update-rc.d oobe disable", false);
    return true;
}

/////////////////////////////////////////////////////////////////////////
// user account functions

bool MInstall::validateUserInfo()
{
    const QString &userName = userNameEdit->text();
    nextFocus = userNameEdit;
    // see if username is reasonable length
    if (!userName.contains(QRegExp("^[a-zA-Z_][a-zA-Z0-9_-]*[$]?$"))) {
        QMessageBox::critical(this, windowTitle(),
                              tr("The user name cannot contain special characters or spaces.\n"
                                 "Please choose another name before proceeding."));
        return false;
    }
    // check that user name is not already used
    QFile file("/etc/passwd");
    if (file.open(QFile::ReadOnly | QFile::Text)) {
        const QByteArray &match = QString("%1:").arg(userName).toUtf8();
        while (!file.atEnd()) {
            if (file.readLine().startsWith(match)) {
                QMessageBox::critical(this, windowTitle(),
                                      tr("Sorry, that name is in use.\n"
                                         "Please select a different name."));
                return false;
            }
        }
    }

    if (!automatic && rootPasswordEdit->text().isEmpty()) {
        // Confirm that an empty root password is not accidental.
        const QMessageBox::StandardButton ans = QMessageBox::warning(this,
            windowTitle(), tr("You did not provide a password for the root account."
                " Do you want to continue?"), QMessageBox::Yes|QMessageBox::No, QMessageBox::No);
        if (ans!=QMessageBox::Yes) return false;
    }

    // Check for pre-existing /home directory
    // see if user directory already exists
    haveOldHome = listHomes.contains(userName);
    if (haveOldHome) {
        const QString &str = tr("The home directory for %1 already exists.");
        labelOldHome->setText(str.arg(userName));
    }
    nextFocus = nullptr;
    return true;
}

// setup the user, cannot be rerun
bool MInstall::setUserInfo()
{
    if (nocopy) return true;
    if (phase < 0) return false;

    // set the user passwords first
    bool ok = true;
    QString cmdChRoot;
    if(!oobe) cmdChRoot = "chroot /mnt/antiX ";
    const QString &userPass = userPasswordEdit->text();
    const QString &rootPass = rootPasswordEdit->text();
    QByteArray userinfo;
    if(rootPass.isEmpty()) ok = proc.exec(cmdChRoot + "passwd -d root", true);
    else userinfo.append(QString("root:" + rootPass).toUtf8());
    if(ok && userPass.isEmpty()) ok = proc.exec(cmdChRoot + "passwd -d demo", true);
    else {
        if(!userinfo.isEmpty()) userinfo.append('\n');
        userinfo.append(QString("demo:" + userPass).toUtf8());
    }
    if(ok && !userinfo.isEmpty()) ok = proc.exec(cmdChRoot + "chpasswd", true, &userinfo);
    if(!ok) {
        failUI(tr("Failed to set user account passwords."));
        return false;
    }

    QString rootpath;
    if (!oobe) rootpath = "/mnt/antiX";
    QString skelpath = rootpath + "/etc/skel";
    QString cmd;

    QString dpath = rootpath + "/home/" + userNameEdit->text();

    if (QFileInfo::exists(dpath.toUtf8())) {
        if (radioOldHomeSave->isChecked()) {
            // save the old directory
            bool ok = false;
            cmd = QString("/bin/mv -f %1 %1.00%2").arg(dpath);
            for (int ixi = 1; ixi < 10 && !ok; ++ixi) {
                ok = proc.exec(cmd.arg(ixi));
            }
            if (!ok) {
                failUI(tr("Failed to save old home directory."));
                return false;
            }
        } else if (radioOldHomeDelete->isChecked()) {
            // delete the directory
            cmd = QString("/bin/rm -rf %1").arg(dpath);
            if (!proc.exec(cmd)) {
                failUI(tr("Failed to delete old home directory."));
                return false;
            }
        }
        //now that directory is moved or deleted, make new one
        if (!QFileInfo::exists(dpath.toUtf8())) {
            proc.exec("/usr/bin/mkdir -p " + dpath, true);
        }
        // clean up directory
        proc.exec("/bin/cp -n " + skelpath + "/.bash_profile " + dpath, true);
        proc.exec("/bin/cp -n " + skelpath + "/.bashrc " + dpath, true);
        proc.exec("/bin/cp -n " + skelpath + "/.gtkrc " + dpath, true);
        proc.exec("/bin/cp -n " + skelpath + "/.gtkrc-2.0 " + dpath, true);
        proc.exec("/bin/cp -Rn " + skelpath + "/.config " + dpath, true);
        proc.exec("/bin/cp -Rn " + skelpath + "/.local " + dpath, true);
    } else { // dir does not exist, must create it
        // copy skel to demo
        // don't copy skel to demo if found demo folder in remastered linuxfs
        if (!isRemasteredDemoPresent) {
            if (!proc.exec("/bin/cp -a " + skelpath + " " + rootpath + "/home")) {
                failUI(tr("Sorry, failed to create user directory."));
                return false;
            }
            cmd = QString("/bin/mv -f " + rootpath + "/home/skel %1").arg(dpath);
        } else { // still rename the demo directory even if remastered demo home folder is detected
            cmd = QString("/bin/mv -f " + rootpath + "/home/demo %1").arg(dpath);
        }
        if (!proc.exec(cmd)) {
            failUI(tr("Sorry, failed to name user directory."));
            return false;
        }
    }

    // saving Desktop changes
    if (saveDesktopCheckBox->isChecked()) {
        resetBlueman();
        rsynchomefolder(dpath);
    }

    // check if remaster exists and saveDesktopCheckbox not checked, modify the remastered demo folder
    if (isRemasteredDemoPresent && ! saveDesktopCheckBox->isChecked())
    {
        resetBlueman();
        changeRemasterdemoToNewUser(dpath);
    }

    // fix the ownership, demo=newuser
    cmd = QString("chown -R demo:demo %1").arg(dpath);
    if (!proc.exec(cmd)) {
        failUI(tr("Sorry, failed to set ownership of user directory."));
        return false;
    }

    // change in files
    replaceStringInFile("demo", userNameEdit->text(), rootpath + "/etc/group");
    replaceStringInFile("demo", userNameEdit->text(), rootpath + "/etc/gshadow");
    replaceStringInFile("demo", userNameEdit->text(), rootpath + "/etc/passwd");
    replaceStringInFile("demo", userNameEdit->text(), rootpath + "/etc/shadow");
    replaceStringInFile("demo", userNameEdit->text(), rootpath + "/etc/slim.conf");
    replaceStringInFile("demo", userNameEdit->text(), rootpath + "/etc/lightdm/lightdm.conf");
    replaceStringInFile("demo", userNameEdit->text(), rootpath + "/home/*/.gtkrc-2.0");
    replaceStringInFile("demo", userNameEdit->text(), rootpath + "/root/.gtkrc-2.0");
    if (autologinCheckBox->isChecked()) {
        replaceStringInFile("#auto_login", "auto_login", rootpath + "/etc/slim.conf");
        replaceStringInFile("#default_user ", "default_user ", rootpath + "/etc/slim.conf");
        replaceStringInFile("User=", "User=" + userNameEdit->text(), rootpath + "/etc/sddm.conf");
    }
    else {
        replaceStringInFile("auto_login", "#auto_login", rootpath + "/etc/slim.conf");
        replaceStringInFile("default_user ", "#default_user ", rootpath + "/etc/slim.conf");
        replaceStringInFile("autologin-user=", "#autologin-user=", rootpath + "/etc/lightdm/lightdm.conf");
        replaceStringInFile("User=.*", "User=", rootpath + "/etc/sddm.conf");
    }
    cmd = QString("touch " + rootpath + "/var/mail/%1").arg(userNameEdit->text());
    proc.exec(cmd);

    return true;
}

/////////////////////////////////////////////////////////////////////////
// computer name functions

bool MInstall::validateComputerName()
{
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

    nextFocus = nullptr;
    return true;
}

// set the computer name, can not be rerun
bool MInstall::setComputerName()
{
    if (phase < 0) return false;
    QString etcpath = oobe ? "/etc" : "/mnt/antiX/etc";
    if (haveSamba) {
        //replaceStringInFile(PROJECTSHORTNAME + "1", computerNameEdit->text(), "/mnt/antiX/etc/samba/smb.conf");
        replaceStringInFile("WORKGROUP", computerGroupEdit->text(), etcpath + "/samba/smb.conf");
    }
    if (sambaCheckBox->isChecked()) {
        proc.exec("/bin/mv -f " + etcpath + "/rc5.d/K*smbd " + etcpath + "/rc5.d/S06smbd >/dev/null 2>&1", false);
        proc.exec("/bin/mv -f " + etcpath + "/rc4.d/K*smbd " + etcpath + "/rc4.d/S06smbd >/dev/null 2>&1", false);
        proc.exec("/bin/mv -f " + etcpath + "/rc3.d/K*smbd " + etcpath + "/rc3.d/S06smbd >/dev/null 2>&1", false);
        proc.exec("/bin/mv -f " + etcpath + "/rc2.d/K*smbd " + etcpath + "/rc2.d/S06smbd >/dev/null 2>&1", false);
        proc.exec("/bin/mv -f " + etcpath + "/rc5.d/K*samba-ad-dc " + etcpath + "/rc5.d/S01samba-ad-dc >/dev/null 2>&1", false);
        proc.exec("/bin/mv -f " + etcpath + "/rc4.d/K*samba-ad-dc " + etcpath + "/rc4.d/S01samba-ad-dc >/dev/null 2>&1", false);
        proc.exec("/bin/mv -f " + etcpath + "/rc3.d/K*samba-ad-dc " + etcpath + "/rc3.d/S01samba-ad-dc >/dev/null 2>&1", false);
        proc.exec("/bin/mv -f " + etcpath + "/rc2.d/K*samba-ad-dc " + etcpath + "/rc2.d/S01samba-ad-dc >/dev/null 2>&1", false);
        proc.exec("/bin/mv -f " + etcpath + "/rc5.d/K*nmbd " + etcpath + "/rc5.d/S01nmbd >/dev/null 2>&1", false);
        proc.exec("/bin/mv -f " + etcpath + "/rc4.d/K*nmbd " + etcpath + "/rc4.d/S01nmbd >/dev/null 2>&1", false);
        proc.exec("/bin/mv -f " + etcpath + "/rc3.d/K*nmbd " + etcpath + "/rc3.d/S01nmbd >/dev/null 2>&1", false);
        proc.exec("/bin/mv -f " + etcpath + "/rc2.d/K*nmbd " + etcpath + "/rc2.d/S01nmbd >/dev/null 2>&1", false);
    } else {
        proc.exec("/bin/mv -f " + etcpath + "/rc5.d/S*smbd " + etcpath + "/rc5.d/K01smbd >/dev/null 2>&1", false);
        proc.exec("/bin/mv -f " + etcpath + "/rc4.d/S*smbd " + etcpath + "/rc4.d/K01smbd >/dev/null 2>&1", false);
        proc.exec("/bin/mv -f " + etcpath + "/rc3.d/S*smbd " + etcpath + "/rc3.d/K01smbd >/dev/null 2>&1", false);
        proc.exec("/bin/mv -f " + etcpath + "/rc2.d/S*smbd " + etcpath + "/rc2.d/K01smbd >/dev/null 2>&1", false);
        proc.exec("/bin/mv -f " + etcpath + "/rc5.d/S*samba-ad-dc " + etcpath + "/rc5.d/K01samba-ad-dc >/dev/null 2>&1", false);
        proc.exec("/bin/mv -f " + etcpath + "/rc4.d/S*samba-ad-dc " + etcpath + "/rc4.d/K01samba-ad-dc >/dev/null 2>&1", false);
        proc.exec("/bin/mv -f " + etcpath + "/rc3.d/S*samba-ad-dc " + etcpath + "/rc3.d/K01samba-ad-dc >/dev/null 2>&1", false);
        proc.exec("/bin/mv -f " + etcpath + "/rc2.d/S*samba-ad-dc " + etcpath + "/rc2.d/K01samba-ad-dc >/dev/null 2>&1", false);
        proc.exec("/bin/mv -f " + etcpath + "/rc5.d/S*nmbd " + etcpath + "/rc5.d/K01nmbd >/dev/null 2>&1", false);
        proc.exec("/bin/mv -f " + etcpath + "/rc4.d/S*nmbd " + etcpath + "/rc4.d/K01nmbd >/dev/null 2>&1", false);
        proc.exec("/bin/mv -f " + etcpath + "/rc3.d/S*nmbd " + etcpath + "/rc3.d/K01nmbd >/dev/null 2>&1", false);
        proc.exec("/bin/mv -f " + etcpath + "/rc2.d/S*nmbd " + etcpath + "/rc2.d/K01nmbd >/dev/null 2>&1", false);
    }

    if (containsSystemD && !sambaCheckBox->isChecked()) {
        proc.exec("chroot /mnt/antiX systemctl disable smbd");
        proc.exec("chroot /mnt/antiX systemctl disable nmbd");
        proc.exec("chroot /mnt/antiX systemctl disable samba-ad-dc");
        proc.exec("chroot /mnt/antiX systemctl mask smbd");
        proc.exec("chroot /mnt/antiX systemctl mask nmbd");
        proc.exec("chroot /mnt/antiX systemctl mask samba-ad-dc");
    }

    //replaceStringInFile(PROJECTSHORTNAME + "1", computerNameEdit->text(), "/mnt/antiX/etc/hosts");
    const QString &compname = computerNameEdit->text();
    QString cmd("sed -i 's/'\"$(grep 127.0.0.1 /etc/hosts | grep -v localhost"
        " | head -1 | awk '{print $2}')\"'/" + compname + "/' ");
    if (!oobe) proc.exec(cmd + "/mnt/antiX/etc/hosts", false);
    else {
        proc.exec(cmd + "/tmp/hosts", false);
        proc.exec("/bin/mv -f /tmp/hosts " + etcpath, true);
    }
    proc.exec("echo \"" + compname + "\" | cat > " + etcpath + "/hostname", false);
    proc.exec("echo \"" + compname + "\" | cat > " + etcpath + "/mailname", false);
    proc.exec("sed -i 's/.*send host-name.*/send host-name \""
        + compname + "\";/g' " + etcpath + "/dhcp/dhclient.conf", false);
    proc.exec("echo \"" + compname + "\" | cat > " + etcpath + "/defaultdomain", false);
    return true;
}

void MInstall::setLocale()
{
    proc.log(__PRETTY_FUNCTION__);
    if (phase < 0) return;
    QString cmd2;
    QString cmd;

    //locale
    if (!oobe) cmd = "chroot /mnt/antiX ";
    cmd += QString("/usr/sbin/update-locale \"LANG=%1\"").arg(localeCombo->currentData().toString());
    qDebug() << "Update locale";
    proc.exec(cmd);
    cmd = QString("Language=%1").arg(localeCombo->currentData().toString());

    // /etc/localtime is either a file or a symlink to a file in /usr/share/zoneinfo. Use the one selected by the user.
    //replace with link
    if (!oobe) {
        cmd = QString("/bin/ln -nfs /usr/share/zoneinfo/%1 /mnt/antiX/etc/localtime").arg(cmbTimeZone->currentData().toString());
        proc.exec(cmd, false);
    }
    cmd = QString("/bin/ln -nfs /usr/share/zoneinfo/%1 /etc/localtime").arg(cmbTimeZone->currentData().toString());
    proc.exec(cmd, false);
    // /etc/timezone is text file with the timezone written in it. Write the user-selected timezone in it now.
    if (!oobe) {
        cmd = QString("echo %1 > /mnt/antiX/etc/timezone").arg(cmbTimeZone->currentData().toString());
        proc.exec(cmd, false);
    }
    cmd = QString("echo %1 > /etc/timezone").arg(cmbTimeZone->currentData().toString());
    proc.exec(cmd, false);

    // Set clock to use LOCAL
    if (localClockCheckBox->isChecked()) {
        proc.exec("echo '0.0 0 0.0\n0\nLOCAL' > /etc/adjtime", false);
    } else {
        proc.exec("echo '0.0 0 0.0\n0\nUTC' > /etc/adjtime", false);
    }
    proc.exec("hwclock --hctosys");
    if (!oobe) {
        proc.exec("/bin/cp -f /etc/adjtime /mnt/antiX/etc/");
        proc.exec("/bin/cp -f /etc/default/rcS /mnt/antiX/etc/default");
    }

    // Set clock format
    QString skelpath = oobe ? "/etc/skel" : "/mnt/antiX/etc/skel";
    if (radio12h->isChecked()) {
        //mx systems
        proc.exec("sed -i '/data0=/c\\data0=%l:%M' /home/demo/.config/xfce4/panel/xfce4-orageclock-plugin-1.rc", false);
        proc.exec("sed -i '/data0=/c\\data0=%l:%M' " + skelpath + "/.config/xfce4/panel/xfce4-orageclock-plugin-1.rc", false);
        proc.exec("sed -i '/time_format=/c\\time_format=%l:%M' /home/demo/.config/xfce4/panel/datetime-1.rc", false);
        proc.exec("sed -i '/time_format=/c\\time_format=%l:%M' " + skelpath + "/.config/xfce4/panel/datetime-1.rc", false);

        //mx kde
        proc.exec("sed -i '/use24hFormat=/c\\use24hFormat=0' /home/demo/.config/plasma-org.kde.plasma.desktop-appletsrc", false);
        proc.exec("sed -i '/use24hFormat=/c\\use24hFormat=0' " + skelpath + "/.config/plasma-org.kde.plasma.desktop-appletsrc", false);

        //antix systems
        proc.exec("sed -i 's/%H:%M/%l:%M/g' " + skelpath + "/.icewm/preferences", false);
        proc.exec("sed -i 's/%k:%M/%l:%M/g' " + skelpath + "/.fluxbox/init", false);
        proc.exec("sed -i 's/%k:%M/%l:%M/g' " + skelpath + "/.jwm/tray", false);
    } else {
        //mx systems
        proc.exec("sed -i '/data0=/c\\data0=%H:%M' /home/demo/.config/xfce4/panel/xfce4-orageclock-plugin-1.rc", false);
        proc.exec("sed -i '/data0=/c\\data0=%H:%M' " + skelpath + "/.config/xfce4/panel/xfce4-orageclock-plugin-1.rc", false);
        proc.exec("sed -i '/time_format=/c\\time_format=%H:%M' /home/demo/.config/xfce4/panel/datetime-1.rc", false);
        proc.exec("sed -i '/time_format=/c\\time_format=%H:%M' " + skelpath + "/.config/xfce4/panel/datetime-1.rc", false);

        //mx kde
        proc.exec("sed -i '/use24hFormat=/c\\use24hFormat=2' /home/demo/.config/plasma-org.kde.plasma.desktop-appletsrc", false);
        proc.exec("sed -i '/use24hFormat=/c\\use24hFormat=2' " + skelpath + "/.config/plasma-org.kde.plasma.desktop-appletsrc", false);

        //antix systems
        proc.exec("sed -i 's/%H:%M/%H:%M/g' " + skelpath + "/.icewm/preferences", false);
        proc.exec("sed -i 's/%k:%M/%k:%M/g' " + skelpath + "/.fluxbox/init", false);
        proc.exec("sed -i 's/%k:%M/%k:%M/g' " + skelpath + "/.jwm/tray", false);
    }

    // localize repo
    qDebug() << "Localize repo";
    if (oobe) proc.exec("localize-repo default");
    else proc.exec("chroot /mnt/antiX localize-repo default");
}

void MInstall::stashServices(bool save)
{
    QTreeWidgetItemIterator it(csView);
    while (*it) {
        if ((*it)->parent() != nullptr) {
            (*it)->setCheckState(save?2:0, (*it)->checkState(save?0:2));
        }
        ++it;
    }
}

void MInstall::setServices()
{
    proc.log(__PRETTY_FUNCTION__);
    if (phase < 0) return;

    QString chroot;
    if (!oobe) chroot = "chroot /mnt/antiX ";
    QTreeWidgetItemIterator it(csView);
    while (*it) {
        if ((*it)->parent() != nullptr) {
            QString service = (*it)->text(0);
            qDebug() << "Service: " << service;
            if (!oem && (*it)->checkState(0) == Qt::Checked) {
                proc.exec(chroot + "update-rc.d " + service + " enable");
                if (containsSystemD) {
                    proc.exec(chroot + "systemctl enable " + service);
                }
            } else { // In OEM mode, disable the services for the OOBE.
                proc.exec(chroot + "update-rc.d " + service + " disable");
                if (containsSystemD) {
                    proc.exec(chroot + "systemctl disable " + service);
                    proc.exec(chroot + "systemctl mask " + service);
                }
            }
        }
        ++it;
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
    if (next == 4 && ixPageRefAdvancedFDE != 0) { // at Step_FDE
        return next;
    }

    if (next == 3 && curr == 2) { // at Step_Disk (forward)
        if (entireDiskButton->isChecked()) {
            if (!automatic) {
                QString msg = tr("OK to format and use the entire disk (%1) for %2?");
                if (!uefi) {
                    const int bdindex = listBlkDevs.findDevice(diskCombo->currentData().toString());
                    if (bdindex >= 0 && listBlkDevs.at(bdindex).size >= (2048LL*1073741824LL)) {
                        msg += "\n\n" + tr("WARNING: The selected drive has a capacity of at least 2TB and must be formatted using GPT."
                                           " On some systems, a GPT-formatted disk will not boot.");
                        return curr;
                    }
                }
                int ans = QMessageBox::warning(this, windowTitle(),
                                               msg.arg(diskCombo->currentData().toString(), PROJECTNAME),
                                               QMessageBox::Yes, QMessageBox::No);
                if (ans != QMessageBox::Yes) {
                    return curr; // don't format - stop install
                }
            }
            partman.layoutDefault();
            return 5; // Go to Step_Boot
        }
    } else if (next == 4 && curr == 3) { // at Step_Partition (fwd)
        QWidget *nf = partman.composeValidate(MIN_INSTALL_SIZE, PROJECTNAME);
        if (nf) {
            nextFocus = nf;
            return curr;
        }
        if (!pretend && !saveHomeBasic()) {
            const QString &msg = tr("The data in /home cannot be preserved because the required information could not be obtained.") + "\n"
                    + tr("If the partition containing /home is encrypted, please ensure the correct \"Encrypt\" boxes are selected, and that the entered password is correct.") + "\n"
                    + tr("The installer cannot encrypt an existing /home directory or partition.");
            QMessageBox::critical(this, windowTitle(), msg);
            return curr;
        }
        return 5; // Go to Step_Boot
    } else if (curr == 4) { // at Step_FDE
        stashAdvancedFDE(next >= 5);
        next = ixPageRefAdvancedFDE;
        ixPageRefAdvancedFDE = 0;
        return next;
    } else if (next == 6 && curr == 5) { // at Step_Boot (forward)
        if (oem) return 11; // straight to Step_Progress
        return next + 1; // skip Services screen
    } else if (next == 10 && curr == 9) { // at Step_User_Accounts (forward)
        if (!validateUserInfo()) return curr;
        if (!haveOldHome) return next + 1; // skip Step_Old_Home
    } else if (next == 8 && curr == 7) { // at Step_Network (forward)
        if (!validateComputerName()) return curr;
    } else if (next == 6 && curr == 7) { // at Step_Network (backward)
        return next - 1; // skip Services screen
    } else if (next == 9 && curr == 8) { // at Step_Localization (forward)
        if (!pretend && haveSnapshotUserAccounts) {
            return 11; // skip Step_User_Accounts and go to Step_Progress
        }
    } else if (next == 9 && curr == 10) { // at Step_Old_Home (backward)
        if (!pretend && haveSnapshotUserAccounts) {
            return 8; // skip Step_User_Accounts and go to Step_Localization
        }
    } else if (next == 10 && curr == 11) { // at Step_Progress (backward)
        if (oem) return 5; // go back to Step_Boot
        if (!haveOldHome) {
            // skip Step_Old_Home
            if (!pretend && haveSnapshotUserAccounts) {
                return 8; // go to Step_Localization
            }
            return 9; // go to Step_User_Accounts
        }
    } else if (curr == 6) { // at Step_Services
        stashServices(next >= 7);
        return 8; // goes back to the screen that called Services screen
    }
    return next;
}

void MInstall::pageDisplayed(int next)
{
    if (!oobe) {
        const int ixProgress = widgetStack->indexOf(Step_Progress);
        // progress bar shown only for install and configuration pages.
        installBox->setVisible(next >= widgetStack->indexOf(Step_Boot) && next <= ixProgress);
        // save the last tip and stop it updating when the progress page is hidden.
        if(next != ixProgress) ixTipStart = ixTip;
    }

    switch (next) {
    case 2: // choose disk
        mainHelp->setText("<p><b>" + tr("General Instructions") + "</b><br/>"
                          + tr("BEFORE PROCEEDING, CLOSE ALL OTHER APPLICATIONS.") + "</p>"
                          "<p>" + tr("On each page, please read the instructions, make your selections, and then click on Next when you are ready to proceed."
                                     " You will be prompted for confirmation before any destructive actions are performed.") + "</p>"
                          "<p>" + tr("Installation requires about %1 of space. %2 or more is preferred."
                                     " You can use the entire disk or you can put the installation on existing partitions.").arg(MIN_INSTALL_SIZE, PREFERRED_MIN_INSTALL_SIZE) + "</p>"
                          "<p>" + tr("If you are running Mac OS or Windows OS (from Vista onwards), you may have to use that system's software to set up partitions and boot manager before installing.") + "</p>"
                          "<p>" + tr("The ext2, ext3, ext4, jfs, xfs, btrfs and reiserfs Linux filesystems are supported and ext4 is recommended.") + "</p>"
                          "<p>" + tr("Autoinstall will place home on the root partition.") + "</p>"
                          "<p><b>" + tr("Encryption") + "</b><br/>"
                          + tr("Encryption is possible via LUKS. A password is required.") + "</p>"
                          "<p>" + tr("A separate unencrypted boot partition is required."
                                     " For additional settings including cipher selection, use the <b>Advanced encryption settings</b> button.") + "</p>"
                          "<p>" + tr("When encryption is used with autoinstall, the separate boot partition will be automatically created.") + "</p>");
        if (phase < 0) {
            updateCursor(Qt::WaitCursor);
            phase = 0;
            proc.unhalt();
            updatePartitionWidgets();
            listToUnmount.clear();
            updateCursor();
        }
        break;

    case 3:  // choose partition
        mainHelp->setText("<p><b>" + tr("Limitations") + "</b><br/>"
                          + tr("Remember, this software is provided AS-IS with no warranty what-so-ever."
                               " It is solely your responsibility to backup your data before proceeding.") + "</p>"
                          "<p><b>" + tr("Choose Partitions") + "</b><br/>"
                          + tr("%1 requires a root partition. The swap partition is optional but highly recommended."
                               " If you want to use the Suspend-to-Disk feature of %1, you will need a swap partition that is larger than your physical memory size.").arg(PROJECTNAME) + "</p>"
                          "<p>" + tr("If you choose a separate /home partition it will be easier for you to upgrade in the future, but this will not be possible if you are upgrading from an installation that does not have a separate home partition.") + "</p>"
                          "<p><b>" + tr("Upgrading") + "</b><br/>"
                          + tr("To upgrade from an existing Linux installation, select the same home partition as before and check the preference to preserve data in /home.") + "</p>"
                          "<p>" + tr("If you are preserving an existing /home directory tree located on your root partition, the installer will not reformat the root partition."
                                     " As a result, the installation will take much longer than usual.") + "</p>"
                          "<p><b>" + tr("Preferred Filesystem Type") + "</b><br/>"
                          + tr("For %1, you may choose to format the partitions as ext2, ext3, ext4, f2fs, jfs, xfs, btrfs or reiser.").arg(PROJECTNAME) + "</p>"
                          "<p>" + tr("Additional compression options are available for drives using btrfs."
                                     " Lzo is fast, but the compression is lower. Zlib is slower, with higher compression.") + "</p>"
                          "<p><b>" + tr("Bad Blocks") + "</b><br/>"
                          + tr("If you choose ext2, ext3 or ext4 as the format type, you have the option of checking and correcting for bad blocks on the drive."
                               " The badblock check is very time consuming, so you may want to skip this step unless you suspect that your drive has bad blocks.") + "</p>"
                          "<p><b>" + tr("Encryption") + "</b><br/>"
                          + tr("Encryption is possible via LUKS. A password is required.") + "</p>"
                          "<p>" + tr("A separate unencrypted boot partition is required. For additional settings including cipher selection, use the <b>Advanced encryption settings</b> button.") + "</p>");
        break;

    case 4: // advanced encryption settings
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
                          + "<b>XTS</b> " + tr("XEX-based Tweaked codebook with ciphertext Stealing) is a modern chain mode, which supersedes CBC and EBC. It is the default (and recommended) chain mode. Using ESSIV over Plain64 will incur a performance penalty, with negligible known security gain.") + "<br />"
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

    case 5: // set bootloader (start of installation)
        mainHelp->setText(tr("<p><b>Select Boot Method</b><br/> %1 uses the GRUB bootloader to boot %1 and MS-Windows. "
                             "<p>By default GRUB2 is installed in the Master Boot Record (MBR) or ESP (EFI System Partition for 64-bit UEFI boot systems) of your boot drive and replaces the boot loader you were using before. This is normal.</p>"
                             "<p>If you choose to install GRUB2 to Partition Boot Record (PBR) instead, then GRUB2 will be installed at the beginning of the specified partition. This option is for experts only.</p>"
                             "<p>If you uncheck the Install GRUB box, GRUB will not be installed at this time. This option is for experts only.</p>").arg(PROJECTNAME));

        backButton->setEnabled(false);
        nextButton->setEnabled(true);
        if (phase <= 0) {
            buildBootLists();
            manageConfig(ConfigLoadB);
        }
        return; // avoid the end that enables both Back and Next buttons
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
        if(oobe) {
            backButton->setEnabled(false);
            nextButton->setEnabled(true);
            return; // avoid the end that enables both Back and Next buttons
        }
        break;

    case 8: // set localization, clock, services button
        mainHelp->setText("<p><b>" + tr("Localization Defaults") + "</b><br/>"
                          + tr("Set the default locale. This will apply unless they are overridden later by the user.") + "</p>"
                          "<p><b>" + tr("Configure Clock") + "</b><br/>"
                          + tr("If you have an Apple or a pure Unix computer, by default the system clock is set to Greenwich Meridian Time (GMT) or Coordinated Universal Time (UTC)."
                               " To change this, check the \"<b>System clock uses local time</b>\" box.") + "</p>"
                          "<p>" + tr("The system boots with the timezone preset to GMT/UTC."
                               " To change the timezone, after you reboot into the new installation, right click on the clock in the Panel and select Properties.") + "</p>"
                          "<p><b>" + tr("Service Settings") + "</b><br/>"
                          + tr("Most users should not change the defaults."
                               " Users with low-resource computers sometimes want to disable unneeded services in order to keep the RAM usage as low as possible."
                               " Make sure you know what you are doing!"));
        break;

    case 9: // set username and passwords
        mainHelp->setText("<p><b>" + tr("Default User Login") + "</b><br/>"
                          + tr("The root user is similar to the Administrator user in some other operating systems."
                               " You should not use the root user as your daily user account."
                               " Please enter the name for a new (default) user account that you will use on a daily basis."
                               " If needed, you can add other user accounts later with %1 User Manager.").arg(PROJECTNAME) + "</p>"
                          "<p><b>" + tr("Passwords") + "</b><br/>"
                          + tr("Enter a new password for your default user account and for the root account."
                               " Each password must be entered twice.") + "</p>"
                          "<p><b>" + tr("No passwords") + "</b><br/>"
                          + tr("If you want the default user account to have no password, leave its password fields empty."
                               " This allows you to log in without requiring a password.") + "<br/>"
                          + tr("Obviously, this should only be done in situations where the user account"
                               " does not need to be secure, such as a public terminal.") + "</p>");
        if (!nextFocus) nextFocus = userNameEdit;
        backButton->setEnabled(true);
        userPassValidationChanged();
        return; // avoid the end that enables both Back and Next buttons
        break;

    case 10: // deal with an old home directory
        mainHelp->setText("<p><b>" + tr("Old Home Directory") + "</b><br/>"
                          + tr("A home directory already exists for the user name you have chosen."
                               " This screen allows you to choose what happens to this directory.") + "</p>"
                          "<p><b>" + tr("Re-use it for this installation") + "</b><br/>"
                          + tr("The old home directory will be used for this user account."
                               " This is a good choice when upgrading, and your files and settings will be readily available.") + "</p>"
                          "<p><b>" + tr("Rename it and create a new directory") + "</b><br/>"
                          + tr("A new home directory will be created for the user, but the old home directory will be renamed."
                               " Your files and settings will not be immediately visible in the new installation, but can be accessed using the renamed directory.") + "</p>"
                          "<p>" + tr("The old directory will have a number at the end of it, depending on how many times the directory has been renamed before.") + "</p>"
                          "<p><b>" + tr("Delete it and create a new directory") + +"</b><br/>"
                          + tr("The old home directory will be deleted, and a new one will be created from scratch.") + "<br/>"
                          "<b>" + tr("Warning") + "</b>: "
                          + tr("All files and settings will be deleted permanently if this option is selected."
                               " Your chances of recovering them are low.") + "</p>");
        // disable the Next button if none of the old home options are selected
        on_radioOldHomeUse_toggled(false);
        // if the Next button is disabled, avoid enabling both Back and Next at the end
        if(nextButton->isEnabled() == false) {
            backButton->setEnabled(true);
            return;
        }
        break;

    case 11: // installation step
        if(ixTipStart >= 0) {
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
        backButton->setEnabled(true);
        nextButton->setEnabled(false);
        return; // avoid enabling both Back and Next buttons at the end
        break;

    case 12: // done
        closeButton->setEnabled(false);
        mainHelp->setText(tr("<p><b>Congratulations!</b><br/>You have completed the installation of %1</p>"
                             "<p><b>Finding Applications</b><br/>There are hundreds of excellent applications installed with %1 "
                             "The best way to learn about them is to browse through the Menu and try them. "
                             "Many of the apps were developed specifically for the %1 project. "
                             "These are shown in the main menus. "
                             "<p>In addition %1 includes many standard Linux applications that are run only from the command line and therefore do not show up in the Menu.</p>").arg(PROJECTNAME));
        break;

    default:
        // case 1 or any other
        mainHelp->setText("<p><b>" + tr("Enjoy using %1</b></p>").arg(PROJECTNAME) + "\n\n "
                          + tr("<p><b>Support %1</b><br/>"
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
    closeButton->setHidden(next == 0);
    backButton->setHidden(next <= 1);
    nextButton->setHidden(next == 0);

    int c = widgetStack->count();
    QSize isize = nextButton->iconSize();
    isize.setWidth(isize.height());
    if (next >= c-1) {
        // entering the last page
        backButton->hide();
        nextButton->setText(tr("Finish"));
    } else if (next == 4 || next == 6){
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
        if (!pretend && checkBoxExitReboot->isChecked()) {
            proc.exec("/usr/local/bin/persist-config --shutdown --command reboot &", false);
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
        nextFocus = nullptr;
    }

    // automatic installation
    if (automatic) {
        if (!MSettings::isBadWidget(widgetStack->currentWidget())
            && next > curr) nextButton->click();
        else automatic = false; // failed validation
    }

    // process next installation phase
    if (next == widgetStack->indexOf(Step_Boot) || next == widgetStack->indexOf(Step_Progress)) {
        if (oobe) {
            updateCursor(Qt::BusyCursor);
            labelSplash->setText(tr("Configuring sytem. Please wait."));
            gotoPage(0);
            if(processOOBE()) {
                labelSplash->setText(tr("Configuration complete. Restarting system."));
                proc.exec("/usr/sbin/reboot", true);
                qApp->exit(EXIT_SUCCESS);
            } else {
                labelSplash->setText(tr("Could not complete configuration."));
                closeButton->show();
            }
            updateCursor();
        } else if (!processNextPhase() && phase > -2) {
            cleanup(false);
            gotoPage(2);
        }
    }
}

void MInstall::updatePartitionWidgets()
{
    proc.log(__PRETTY_FUNCTION__);

    diskCombo->setEnabled(false);
    diskCombo->clear();
    diskCombo->addItem(tr("Loading..."));
    listBlkDevs.build(proc);
    if (mactest) {
        for (BlockDeviceInfo &bdinfo : listBlkDevs) {
            if (bdinfo.name.startsWith("sda")) bdinfo.isNasty = true;
        }
    }

    // disk combo box
    diskCombo->clear();
    for (const BlockDeviceInfo &bdinfo : listBlkDevs) {
        if (bdinfo.isDrive && bdinfo.size >= rootSpaceNeeded
                && (!bdinfo.isBoot || INSTALL_FROM_ROOT_DEVICE)) {
            bdinfo.addToCombo(diskCombo);
        }
    }
    diskCombo->setCurrentIndex(0);
    diskCombo->setEnabled(true);

    // whole-disk vs custom-partition radio buttons
    entireDiskButton->setChecked(true);
    for (const BlockDeviceInfo &bdinfo : listBlkDevs) {
        if (!bdinfo.isDrive) {
            // found at least one partition
            customPartButton->setChecked(true);
            break;
        }
    }

    // partition tree
    partman.populate();
}

void MInstall::buildServiceList()
{
    proc.log(__PRETTY_FUNCTION__);

    //setup csView
    csView->header()->setMinimumSectionSize(150);
    csView->header()->resizeSection(0,150);

    QSettings services_desc("/usr/share/gazelle-installer-data/services.list", QSettings::NativeFormat);

    for (const QString &service : qAsConst(ENABLE_SERVICES)) {
        const QString &lang = QLocale::system().bcp47Name().toLower();
        QString lang_str = (lang == "en")? "" : "_" + lang;
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
}

/////////////////////////////////////////////////////////////////////////
// event handlers

void MInstall::changeEvent(QEvent *event)
{
    const QEvent::Type etype = event->type();
    if (etype == QEvent::ApplicationPaletteChange
        || etype == QEvent::PaletteChange || etype == QEvent::StyleChange)
    {
        QPalette pal = qApp->palette(mainHelp);
        QColor col = pal.color(QPalette::Base);
        col.setAlpha(200);
        pal.setColor(QPalette::Base, col);
        mainHelp->setPalette(pal);
        resizeEvent(nullptr);
    }
}

void MInstall::resizeEvent(QResizeEvent *)
{
    mainHelp->resize(tabHelp->size());
    helpbackdrop->resize(mainHelp->size());
}

void MInstall::closeEvent(QCloseEvent *event)
{
    if (abort(true)) {
        event->accept();
        if (!oobe) cleanup();
        else if (!pretend) {
            proc.unhalt();
            proc.exec("/usr/sbin/shutdown -hP now");
        }
        QWidget::closeEvent(event);
        if (widgetStack->currentWidget() != Step_End) {
            qApp->exit(EXIT_FAILURE);
        } else {
            proc.waitForFinished();
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

void MInstall::on_mainTabs_currentChanged(int index)
{
    // Make the help widgets the correct size.
    if(index == 0) resizeEvent(nullptr);
}

void MInstall::diskPassValidationChanged(bool valid)
{
    nextButton->setEnabled(valid);
}
void MInstall::userPassValidationChanged()
{
    nextButton->setEnabled(!(userNameEdit->text().isEmpty())
        && userPasswordEdit->isValid() && rootPasswordEdit->isValid());
}

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
    proc.exec("[ -f /usr/sbin/gparted ] && /usr/sbin/gparted || /usr/bin/partitionmanager", false);
    updatePartitionWidgets();
    qtpartedButton->setEnabled(true);
    nextButton->setEnabled(true);
    updateCursor();
}

void MInstall::on_buttonBenchmarkFDE_clicked()
{
    proc.exec("x-terminal-emulator -e bash -c \"/sbin/cryptsetup benchmark"
            " && echo && read -n 1 -srp 'Press any key to close the benchmark window.'\"");
}

bool MInstall::abort(bool onclose)
{
    proc.log(__PRETTY_FUNCTION__);
    this->setEnabled(false);
    // ask for confirmation when installing (except for some steps that don't need confirmation)
    if (phase > 0 && phase < 4) {
        const QMessageBox::StandardButton rc = QMessageBox::warning(this,
            tr("Confirmation"), tr("The installation and configuration"
                " is incomplete.\nDo you really want to stop now?"),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if(rc == QMessageBox::No) {
            this->setEnabled(true);
            return false;
        }
    }
    updateCursor(Qt::WaitCursor);
    proc.halt();
    // help the installer if it was stuck at the config pages
    if (onclose) {
        phase = -2;
    } else if (phase == 2 && widgetStack->currentWidget() != Step_Progress) {
        phase = -1;
        gotoPage(2);
    } else {
        phase = -1;
    }
    if (!onclose) this->setEnabled(true);
    return true;
}

// run before closing the app, do some cleanup
void MInstall::cleanup(bool endclean)
{
    proc.log(__PRETTY_FUNCTION__);
    if (pretend) return;

    proc.unhalt();
    if (endclean) {
        setupAutoMount(true);
        proc.exec("/bin/cp /var/log/minstall.log /mnt/antiX/var/log >/dev/null 2>&1", false);
        proc.exec("/bin/rm -rf /mnt/antiX/mnt/antiX >/dev/null 2>&1", false);
    }
    proc.exec("/bin/umount -l /mnt/antiX/boot/efi", true);
    proc.exec("/bin/umount -l /mnt/antiX/proc", true);
    proc.exec("/bin/umount -l /mnt/antiX/sys", true);
    proc.exec("/bin/umount -l /mnt/antiX/dev/shm", true);
    proc.exec("/bin/umount -l /mnt/antiX/dev", true);
    partman.unmount(true);
}

void MInstall::on_progressBar_valueChanged(int value)
{
    if (ixTipStart < 0 || widgetStack->currentWidget() != Step_Progress) {
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
    proc.log(__PRETTY_FUNCTION__);
    QFile file("/etc/default/keyboard");
    if (file.open(QFile::ReadOnly | QFile::Text)) {
        while (!file.atEnd()) {
            QString line(file.readLine().trimmed());
            QLabel *plabel = nullptr;
            if (line.startsWith("XKBMODEL")) plabel = labelModel;
            else if (line.startsWith("XKBLAYOUT")) plabel = labelLayout;
            else if (line.startsWith("XKBVARIANT")) plabel = labelVariant;
            if (plabel != nullptr) {
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
    if (proc.exec("command -v  system-keyboard-qt >/dev/null 2>&1", false))
        proc.exec("system-keyboard-qt", false);
    else
        proc.exec("env GTK_THEME='Adwaita' fskbsetting", false);
    show();
    setupkeyboardbutton();
}

void MInstall::on_checkBoxEncryptAuto_toggled(bool checked)
{
    FDEpassword->clear();
    nextButton->setDisabled(checked);
    FDEpassword->setVisible(checked);
    FDEpassword2->setVisible(checked);
    labelFDEpass->setVisible(checked);
    labelFDEpass2->setVisible(checked);
    pbFDEpassMeter->setVisible(checked);
    buttonAdvancedFDE->setVisible(checked);
    grubPbrButton->setDisabled(checked);
    if (checked) FDEpassword->setFocus();
}

void MInstall::on_customPartButton_clicked(bool checked)
{
    checkBoxEncryptAuto->setChecked(!checked);
}

void MInstall::on_buttonAdvancedFDE_clicked()
{
    ixPageRefAdvancedFDE = widgetStack->currentIndex();
    gotoPage(4);
}

void MInstall::on_buttonAdvancedFDECust_clicked()
{
    ixPageRefAdvancedFDE = widgetStack->currentIndex();
    gotoPage(4);
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
        if (bdinfo.isDrive && (!bdinfo.isBoot || INSTALL_FROM_ROOT_DEVICE)) {
            if (!bdinfo.isNasty || brave) bdinfo.addToCombo(grubBootCombo, true);
        }
    }
    grubBootLabel->setText(tr("System boot disk:"));
}

void MInstall::on_grubPbrButton_toggled()
{
    grubBootCombo->clear();
    for (const BlockDeviceInfo &bdinfo : listBlkDevs) {
        if (!(bdinfo.isDrive || bdinfo.fs=="swap" || bdinfo.isESP)
            && (!bdinfo.isBoot || INSTALL_FROM_ROOT_DEVICE)
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
        if (bdinfo.isESP && (!bdinfo.isBoot || INSTALL_FROM_ROOT_DEVICE)) bdinfo.addToCombo(grubBootCombo);
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

    // load one as the default in preferential order: ESP, MBR, PBR
    if (canESP) grubEspButton->setChecked(true);
    else if (canMBR) {
        on_grubMbrButton_toggled();
        grubMbrButton->setChecked(true);
    } else if (canPBR) {
        on_grubPbrButton_toggled();
        grubPbrButton->setChecked(true);
    }
}

void MInstall::on_localeCombo_currentIndexChanged(int index)
{
    // riot control
    QLocale locale(localeCombo->itemData(index).toString());
    if (locale.timeFormat().startsWith('h')) radio12h->setChecked(true);
    else radio24h->setChecked(true);
}

// return 0 = success, 1 = bad area, 2 = bad zone
int MInstall::selectTimeZone(const QString &zone)
{
    int index = cmbTimeArea->findData(QVariant(zone.section('/', 0, 0)));
    if (index < 0) return 1;
    cmbTimeArea->setCurrentIndex(index);
    qApp->processEvents();
    index = cmbTimeZone->findData(QVariant(zone));
    if (index < 0) return 2;
    cmbTimeZone->setCurrentIndex(index);
    return 0;
}
void MInstall::on_cmbTimeArea_currentIndexChanged(int index)
{
    if (index < 0 || index >= cmbTimeArea->count()) return;
    const QString &area = cmbTimeArea->itemData(index).toString();
    cmbTimeZone->clear();
    for (const QString &zone : listTimeZones) {
        if (zone.startsWith(area)) {
            QString text(QString(zone).section('/', 1));
            text.replace('_', ' ');
            cmbTimeZone->addItem(text, QVariant(zone));
        }
    }
    cmbTimeZone->model()->sort(0);
}

void MInstall::on_radioOldHomeUse_toggled(bool)
{
    nextButton->setEnabled(radioOldHomeUse->isChecked()
                           || radioOldHomeSave->isChecked()
                           || radioOldHomeDelete->isChecked());
}
void MInstall::on_radioOldHomeSave_toggled(bool)
{
    on_radioOldHomeUse_toggled(false);
}
void MInstall::on_radioOldHomeDelete_toggled(bool)
{
    on_radioOldHomeUse_toggled(false);
}

void MInstall::clearpartitiontables(const QString &dev)
{
    //setup block size and offsets info
    QString bytes = proc.execOut("parted --script /dev/" + dev + " unit B print 2>/dev/null | sed -rn 's/^Disk.*: ([0-9]+)B$/\\1/ip\'");
    qDebug() << "bytes is " << bytes;
    int block_size = 512;
    int pt_size = 17 * 1024;
    int pt_count = pt_size / block_size;
    int total_blocks = bytes.toLongLong() / block_size;
    qDebug() << "total blocks is " << total_blocks;

    //clear primary partition table
    proc.exec("dd if=/dev/zero of=/dev/" + dev + " bs=" + QString::number(block_size) + " count=" + QString::number(pt_count));

    // Clear out sneaky iso-hybrid partition table
    proc.exec("dd if=/dev/zero of=/dev/" + dev +" bs=" + QString::number(block_size) + " count=" + QString::number(pt_count) + " seek=64");

    // clear secondary partition table

    if ( ! bytes.isEmpty()) {
        int offset = total_blocks - pt_count;
        proc.exec("dd conv=notrunc if=/dev/zero of=/dev/" + dev + " bs=" + QString::number(block_size) + " count=" + QString::number(pt_count) + " seek=" + QString::number(offset));
    }

}

bool MInstall::checkForSnapshot()
{
    // test if there's another user than demo in /home, indicating a possible snapshot or complicated live-usb
    qDebug() << "check for possible snapshot";
    return proc.exec("/bin/ls -1 /home | grep -Ev '(lost\\+found|demo|snapshot)' | grep -q [a-zA-Z0-9]", false);
}

bool MInstall::checkForRemaster()
{
    // check the linuxfs squashfs for a home/demo folder, which indicates a remaster perserving /home.
    qDebug() << "check for remastered home demo folder";
    return proc.exec("test -d /live/linux/home/demo", true);
}

void MInstall::rsynchomefolder(QString dpath)
{
    QString cmd = ("rsync -a --info=name1 /home/demo/ %1"
                  " --exclude '.cache' --exclude '.gvfs' --exclude '.dbus' --exclude '.Xauthority' --exclude '.ICEauthority'"
                  " --exclude '.mozilla' --exclude 'Installer.desktop' --exclude 'minstall.desktop' --exclude 'Desktop/antixsources.desktop'"
                  " --exclude '.jwm/menu' --exclude '.icewm/menu' --exclude '.fluxbox/menu'"
                  " --exclude '.config/rox.sourceforge.net/ROX-Filer/pb_antiX-fluxbox' --exclude '.config/rox.sourceforge.net/ROX-Filer/pb_antiX-icewm'"
                  " --exclude '.config/rox.sourceforge.net/ROX-Filer/pb_antiX-jwm' | xargs -I '$' sed -i 's|home/demo|home/" + userNameEdit->text() + "|g' %1/$").arg(dpath);
    proc.exec(cmd);
}

void MInstall::changeRemasterdemoToNewUser(QString dpath)
{
    QString cmd = ("find " + dpath + " -maxdepth 1 -type f -name '.*' -print0 | xargs -0 sed -i 's|home/demo|home/" + userNameEdit->text() + "|g'").arg(dpath);
    proc.exec(cmd);
    cmd = ("find " + dpath + "/.config -type f -print0 | xargs -0 sed -i 's|home/demo|home/" + userNameEdit->text() + "|g'").arg(dpath);
    proc.exec(cmd);
    cmd = ("find " + dpath + "/.local -type f  -print0 | xargs -0 sed -i 's|home/demo|home/" + userNameEdit->text() + "|g'").arg(dpath);
    proc.exec(cmd);
}

void MInstall::resetBlueman()
{
    proc.exec("runuser -l demo -c 'dconf reset /org/blueman/transfer/shared-path'"); //reset blueman path
}
