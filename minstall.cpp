/****************************************************************************
 *  Copyright (C) 2003-2010 by Warren Woodford
 *  Heavily edited, with permision, by anticapitalista for antiX 2011-2014.
 *  Heavily revised by dolphin oracle, adrian, and anticaptialista 2018.
 *  additional mount and compression oftions for btrfs by rob 2018
 *  Major GUI update and user experience improvements by AK-47 2019.
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
 ****************************************************************************/

#include <QDebug>
#include <QFileInfo>
#include <QProcess>
#include <QProcessEnvironment>
#include <QTimeZone>
#include <QTimer>
#include <QToolTip>

#include <cstdlib>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/statvfs.h>

#include "version.h"
#include "minstall.h"

enum Step {
    Splash,
    Terms,
    Disk,
    Partitions,
    Boot,
    Services,
    Network,
    Localization,
    UserAccounts,
    OldHome,
    Progress,
    End
};

MInstall::MInstall(const QCommandLineParser &args, const QString &cfgfile)
    : proc(this), partman(proc, *this, this)
{
    setupUi(this);
    listLog->addItem("Version " VERSION);
    proc.setupUI(listLog, progInstall);
    updateCursor(Qt::WaitCursor);
    setWindowFlags(Qt::Window); // for the close, min and max buttons
    boxInstall->hide();

    oobe = args.isSet("oobe");
    pretend = args.isSet("pretend");
    nocopy = args.isSet("nocopy");
    sync = args.isSet("sync");
    if (!oobe) {
        partman.brave = brave = args.isSet("brave");
        automatic = args.isSet("auto");
        oem = args.isSet("oem");
        partman.gptoverride = args.isSet("gpt-override");
        mountkeep = args.isSet("mount-keep");
    } else {
        brave = automatic = oem = false;
        pushClose->setText(tr("Shutdown"));
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
    REMOVE_NOSPLASH = settings.value("REMOVE_NOSPLASH", "false").toBool();
    ROOT_BUFFER = settings.value("ROOT_BUFFER", 5000).toInt();
    HOME_BUFFER = settings.value("HOME_BUFFER", 2000).toInt();
    setWindowTitle(tr("%1 Installer").arg(PROJECTNAME));

    gotoPage(Step::Splash);

    // config file
    config = new MSettings(cfgfile, this);

    // Link block
    QString link_block;
    settings.beginGroup("LINKS");
    QStringList links = settings.childKeys();
    for (const QString &link : links)
        link_block += "\n\n" + tr(link.toUtf8().constData()) + ": " + settings.value(link).toString();

    settings.endGroup();

    // set some distro-centric text
    textReminders->setPlainText(tr("Support %1\n\n%1 is supported by people like you. Some help others at the support forum - %2, or translate help files into different languages, or make suggestions, write documentation, or help test new software.").arg(PROJECTNAME, PROJECTFORUM)
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
    resizeEvent(nullptr);

    if (oobe) {
        containsSystemD = QFileInfo("/usr/bin/systemctl").isExecutable();
        if (QFile::exists("/etc/service") && QFile::exists("/lib/runit/runit-init"))
            containsRunit = true;
        checkSaveDesktop->hide();
    } else {
        containsSystemD = QFileInfo("/live/aufs/bin/systemctl").isExecutable();
        if (QFile::exists("/live/aufs/etc/service") && QFile::exists("/live/aufs/sbin/runit"))
            containsRunit = true;

        rootSources << "/live/aufs/bin" << "/live/aufs/dev"
            << "/live/aufs/etc" << "/live/aufs/lib" << "/live/aufs/libx32" << "/live/aufs/lib64"
            << "/live/aufs/media" << "/live/aufs/mnt" << "/live/aufs/opt" << "/live/aufs/root"
            << "/live/aufs/sbin" << "/live/aufs/usr" << "/live/aufs/var" << "/live/aufs/home";

        //load some live variables
        QSettings livesettings("/live/config/initrd.out",QSettings::NativeFormat);
        SQFILE_FULL = livesettings.value("SQFILE_FULL", "/live/boot-dev/antiX/linuxfs").toString();
        // check the linuxfs squashfs for a home/demo folder, which indicates a remaster perserving /home.
        isRemasteredDemoPresent = QFileInfo("/live/linux/home/demo").isDir();
        qDebug() << "check for remastered home demo folder:" << isRemasteredDemoPresent;

        // calculate required disk space
        bootSource = "/live/aufs/boot";
        partman.bootSpaceNeeded = proc.execOut("du -sb " + bootSource).section('\t', 0, 0).toLongLong();
        if (!pretend && partman.bootSpaceNeeded==0) {
            QMessageBox::critical(this, windowTitle(), tr("Cannot access installation source."));
            exit(EXIT_FAILURE);
        }

        //rootspaceneeded is the size of the linuxfs file * a compression factor + contents of the rootfs.  conservative but fast
        //factors are same as used in live-remaster

        //get compression factor by reading the linuxfs squasfs file, if available
        qDebug() << "linuxfs file is at : " << SQFILE_FULL;
        long long compression_factor;
        QString linuxfs_compression_type = "xz"; //default conservative
        if (QFileInfo::exists(SQFILE_FULL))
            linuxfs_compression_type = proc.execOut("dd if=" + SQFILE_FULL + " bs=1 skip=20 count=2 status=none 2>/dev/null | od -An -tdI");

        // gzip, xz, or lz4
        switch (linuxfs_compression_type.toInt()) {
            case 1: // gzip
                compression_factor = 30;
                break;
            case 2: // lzo, not used by antiX
            case 3: // lzma, not used by antiX
            case 5: // lz4
                compression_factor = 42;
                break;
            case 4: // xz
            default: // anythng else or linuxfs not reachable (toram), should be pretty conservative
                compression_factor = 25;
        }

        qDebug() << "linuxfs compression type is " << linuxfs_compression_type << "compression factor is " << compression_factor;

        long long rootfs_file_size = 0;
        long long linuxfs_file_size = (proc.execOut("df /live/linux --output=used --total |tail -n1").toLongLong() * 1024 * 100) / compression_factor;
        if (QFileInfo::exists("/live/perist-root")) {
            rootfs_file_size = proc.execOut("df /live/persist-root --output=used --total |tail -n1").toLongLong() * 1024;
        }

        qDebug() << "linuxfs file size is " << linuxfs_file_size << " rootfs file size is " << rootfs_file_size;

        //add rootfs file size to the calculated linuxfs file size.  probaby conservative, as rootfs will likely have some overlap with linuxfs
        long long safety_factor = 1024 * 1024 * 1024; // 1GB safety factor
        partman.rootSpaceNeeded = linuxfs_file_size + rootfs_file_size + safety_factor;

        const long long spaceBlock = 134217728; // 128MB
        partman.bootSpaceNeeded += 2*spaceBlock - (partman.bootSpaceNeeded % spaceBlock);

        qDebug() << "Minimum space:" << partman.bootSpaceNeeded << "(boot)," << partman.rootSpaceNeeded << "(root)";

        // uefi = false if not uefi, or if a bad combination, like 32 bit iso and 64 bit uefi)
        if (proc.exec("uname -m | grep -q i686", false) && proc.exec("grep -q 64 /sys/firmware/efi/fw_platform_size")) {
            const int ans = QMessageBox::question(this, windowTitle(),
                tr("You are running 32bit OS started in 64 bit UEFI mode, the system will not"
                    " be able to boot unless you select Legacy Boot or similar at restart.\n"
                    "We recommend you quit now and restart in Legacy Boot\n\n"
                    "Do you want to continue the installation?"), QMessageBox::Yes, QMessageBox::No);
            if (ans != QMessageBox::Yes) exit(EXIT_FAILURE);
            uefi = false;
        } else {
            uefi = QFileInfo("/sys/firmware/efi").isDir();
        }
        partman.uefi = uefi;
        qDebug() << "uefi =" << uefi;

        autoMountEnabled = true; // disable auto mount by force
        if (!pretend) setupAutoMount(false);

        partman.defaultLabels["/boot"] = "boot";
        partman.defaultLabels["/"] = "root" + PROJECTSHORTNAME + PROJECTVERSION;
        partman.defaultLabels["/home"] = "home" + PROJECTSHORTNAME;
        partman.defaultLabels["SWAP"] = "swap" + PROJECTSHORTNAME;

        // Detect snapshot-backup account(s)
        // test if there's another user other than demo in /home,
        // indicating a possible snapshot or complicated live-usb
        haveSnapshotUserAccounts = proc.exec("/bin/ls -1 /home"
            " | grep -Ev '(lost\\+found|demo|snapshot)' | grep -q [a-zA-Z0-9]", false);
        qDebug() << "check for possible snapshot:" << haveSnapshotUserAccounts;
    }

    // Password box setup
    textCryptoPass->setup(textCryptoPass2, progCryptoPassMeter, 1, 32, 9);
    textCryptoPassCust->setup(textCryptoPassCust2, progCryptoPassMeterCust, 1, 32, 9);
    textUserPass->setup(textUserPass2, progUserPassMeter);
    textRootPass->setup(textRootPass2, progRootPassMeter);
    connect(textCryptoPass, &MPassEdit::validationChanged, this, &MInstall::diskPassValidationChanged);
    connect(textCryptoPassCust, &MPassEdit::validationChanged, this, &MInstall::diskPassValidationChanged);
    connect(textUserPass, &MPassEdit::validationChanged, this, &MInstall::userPassValidationChanged);
    connect(textRootPass, &MPassEdit::validationChanged, this, &MInstall::userPassValidationChanged);
    // User name is required
    connect(textUserName, &QLineEdit::textChanged, this, &MInstall::userPassValidationChanged);
    // Root account
    connect(boxRootAccount, &QGroupBox::toggled, this, &MInstall::userPassValidationChanged);

    // set default host name
    textComputerName->setText(DEFAULT_HOSTNAME);

    setupkeyboardbutton();

    // timezone lists
    listTimeZones = proc.execOutLines("find -L /usr/share/zoneinfo/posix -mindepth 2 -type f -printf %P\\n", true);
    comboTimeArea->clear();
    for (const QString &zone : listTimeZones) {
        const QString &area = zone.section('/', 0, 0);
        if (comboTimeArea->findData(QVariant(area)) < 0) {
            QString text(area);
            if (area == "Indian" || area == "Pacific"
                || area == "Atlantic" || area == "Arctic") text.append(" Ocean");
            comboTimeArea->addItem(text, area);
        }
    }
    comboTimeArea->model()->sort(0);

    // locale list
    comboLocale->clear();
    QStringList loclist = proc.execOutLines("locale -a | grep -Ev '^(C|POSIX)\\.?' | grep -E 'utf8|UTF-8'");
    for (QString &strloc : loclist) {
        strloc.replace("utf8", "UTF-8", Qt::CaseInsensitive);
        QLocale loc(strloc);
        comboLocale->addItem(loc.nativeCountryName() + " - " + loc.nativeLanguageName(), QVariant(strloc));
    }
    comboLocale->model()->sort(0);
    // default locale selection
    int ixLocale = comboLocale->findData(QVariant(QLocale::system().name() + ".UTF-8"));
    if (comboLocale->currentIndex() != ixLocale) comboLocale->setCurrentIndex(ixLocale);
    else on_comboLocale_currentIndexChanged(ixLocale);

    // if it looks like an apple...
    if (proc.exec("grub-probe -d /dev/sda2 2>/dev/null | grep hfsplus", false)) {
        mactest = true;
        checkLocalClock->setChecked(true);
    }

    //check for samba
    QFileInfo info("/etc/init.d/smbd");
    if (!info.exists()) {
        labelComputerGroup->setEnabled(false);
        textComputerGroup->setEnabled(false);
        textComputerGroup->setText("");
        checkSamba->setChecked(false);
        checkSamba->setEnabled(false);
    }

    // check for the Samba server
    QString val = proc.execOut("dpkg -s samba | grep '^Status.*ok.*' | sed -e 's/.*ok //'");
    haveSamba = (val.compare("installed") == 0);

    buildServiceList();
    if (oobe) manageConfig(ConfigLoadB);
    else {
        updatePartitionWidgets(true);
        manageConfig(ConfigLoadA);
    }
    stashServices(true);

    if (oobe) gotoPage(Step::Network);
    else {
        textCopyright->setPlainText(tr("%1 is an independent Linux distribution based on Debian Stable.\n\n"
            "%1 uses some components from MEPIS Linux which are released under an Apache free license."
            " Some MEPIS components have been modified for %1.\n\nEnjoy using %1").arg(PROJECTNAME));
        gotoPage(Step::Terms);
    }
    updateCursor();

    // automatic installation
    if (automatic) pushNext->click();
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
    for (const QString &path : envpath) {
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
            if (!listMaskedMounts.isEmpty())
                proc.exec("systemctl --runtime mask --quiet -- " + listMaskedMounts);
        }
        // create temporary blank overrides for all udev rules which
        // automatically start Linux Software RAID array members
        proc.mkpath("/run/udev/rules.d");
        for (const QString &rule : udev_temp_mdadm_rules)
            proc.exec("touch " + rule);

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
        for (const QString &rule : udev_temp_mdadm_rules)
            proc.exec("rm -f " + rule);

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
    return proc.exec("lspci -n | grep -qE '80ee:beef|80ee:cafe'", false);
}

bool MInstall::replaceStringInFile(const QString &oldtext, const QString &newtext, const QString &filepath)
{
    QString cmd = QString("sed -i 's/%1/%2/g' %3").arg(oldtext, newtext, filepath);
    return proc.exec(cmd);
}

QString MInstall::sliderSizeString(long long size)
{
    QString strout(QLocale::system().formattedDataSize(size, 1, QLocale::DataSizeTraditionalFormat));
    if (strout.length() > 6)
        return QLocale::system().formattedDataSize(size, 0, QLocale::DataSizeTraditionalFormat);
    return strout;
}

void MInstall::updateCursor(const Qt::CursorShape shape)
{
    if (shape != Qt::ArrowCursor) {
        qApp->setOverrideCursor(QCursor(shape));
    } else {
        while (qApp->overrideCursor() != nullptr)
            qApp->restoreOverrideCursor();
    }
    qApp->processEvents();
}

bool MInstall::pretendToInstall(int space, long steps)
{
    proc.advance(space, steps);
    proc.status(tr("Pretending to install %1").arg(PROJECTNAME));
    for (long ixi = 0; ixi < steps; ++ixi) {
        proc.sleep(100, true);
        proc.status();
        if (phase < 0) return false;
    }
    return true;
}

bool MInstall::writeKeyFile()
{
    if (phase < 0) return false;
    static const char *const rngfile = "/dev/urandom";
    const unsigned int keylength = 4096;
    const QLineEdit *passedit = radioEntireDisk->isChecked() ? textCryptoPass : textCryptoPassCust;
    const QByteArray password(passedit->text().toUtf8());
    const char *keyfile = nullptr;
    bool newkey = true;
    if (partman.isEncrypt("/")) { // if encrypting root
        newkey = (key.length() == 0);
        keyfile = "/mnt/antiX/root/keyfile";
        if (newkey) key.load(rngfile, keylength);
        key.save(keyfile, 0400);
    } else if (partman.isEncrypt("/home") && partman.isEncrypt(QString())>1) {
        // if encrypting /home without encrypting root
        keyfile = "/mnt/antiX/home/.keyfileDONOTdelete";
        key.load(rngfile, keylength);
        key.save(keyfile, 0400);
        key.erase();
    }
    if (!partman.fixCryptoSetup(QString(keyfile).remove(0,10), newkey)) {
        failUI(tr("Failed to finalize encryption setup."));
        return false;
    }
    return true;
}

// disable hibernate when using encrypted swap
void MInstall::disablehiberanteinitramfs()
{
    if (phase < 0) return;
    if (partman.isEncrypt("SWAP")) {
        proc.exec("touch /mnt/antiX/initramfs-tools/conf.d/resume");
        QFile file("/mnt/antiX/etc/initramfs-tools/conf.d/resume");
        if (file.open(QIODevice::WriteOnly)) {
            QTextStream out(&file);
            out << "RESUME=none";
        }
        file.close();
    }
}

// process the next phase of installation if possible
bool MInstall::processNextPhase()
{
    widgetStack->setEnabled(true);
    // Phase < 0 = install has been aborted (Phase -2 on close)
    if (phase < 0) return false;
    // Phase 0 = install not started yet, Phase 1 = install in progress
    // Phase 2 = waiting for operator input, Phase 3 = post-install steps
    if (phase == 0) { // no install started yet
        proc.advance(-1, -1);
        proc.status(tr("Preparing to install %1").arg(PROJECTNAME));
        if (!partman.checkTargetDrivesOK()) return false;
        phase = 1; // installation.

        // cleanup previous mounts
        cleanup(false);

        // the core of the installation
        if (!pretend) {
            proc.advance(12, partman.countPrepSteps());
            bool ok = partman.preparePartitions();
            if (ok) ok = partman.formatPartitions();
            if (ok) ok = partman.mountPartitions();
            if (!ok) {
                failUI(tr("Failed to prepare required partitions."));
                return false;
            }
            //run blkid -c /dev/null to freshen UUID cache
            proc.exec("blkid -c /dev/null", true);
            if (!installLinux()) return false;
        } else {
            if (!pretendToInstall(14, 200)) return false;
            if (!pretendToInstall(80, 500)) return false;
        }
        if (widgetStack->currentWidget() != pageProgress) {
            progInstall->setEnabled(false);
            proc.status(tr("Paused for required operator input"));
            QApplication::beep();
        }
        phase = 2;
    }
    if (phase == 2 && widgetStack->currentWidget() == pageProgress) {
        phase = 3;
        progInstall->setEnabled(true);
        pushBack->setEnabled(false);
        if (!pretend) {
            proc.advance(1, 1);
            proc.status(tr("Setting system configuration"));
            if (!isInsideVB() && !oobe) {
                proc.exec("/bin/mv -f /mnt/antiX/etc/rc5.d/S*virtualbox-guest-utils /mnt/antiX/etc/rc5.d/K01virtualbox-guest-utils >/dev/null 2>&1", false);
                proc.exec("/bin/mv -f /mnt/antiX/etc/rc4.d/S*virtualbox-guest-utils /mnt/antiX/etc/rc4.d/K01virtualbox-guest-utils >/dev/null 2>&1", false);
                proc.exec("/bin/mv -f /mnt/antiX/etc/rc3.d/S*virtualbox-guest-utils /mnt/antiX/etc/rc3.d/K01virtualbox-guest-utils >/dev/null 2>&1", false);
                proc.exec("/bin/mv -f /mnt/antiX/etc/rc2.d/S*virtualbox-guest-utils /mnt/antiX/etc/rc2.d/K01virtualbox-guest-utils >/dev/null 2>&1", false);
                proc.exec("/bin/mv -f /mnt/antiX/etc/rcS.d/S*virtualbox-guest-x11 /mnt/antiX/etc/rcS.d/K21virtualbox-guest-x11 >/dev/null 2>&1", false);
            }
            if (oem) enableOOBE();
            else if (!processOOBE()) return false;
            manageConfig(ConfigSave);
            config->dumpDebug();
            proc.exec("/bin/sync", true); // the sync(2) system call will block the GUI
            if (!installLoader()) return false;
        } else if (!pretendToInstall(5, 100)) {
            return false;
        }
        phase = 4;
        proc.advance(1, 1);
        proc.status(tr("Cleaning up"));
        cleanup();
        proc.status(tr("Finished"));
        gotoPage(Step::End);
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
        config->startGroup("Storage", pageDisk);
        const char *diskChoices[] = {"Drive", "Partitions"};
        QRadioButton *diskRadios[] = {radioEntireDisk, radioCustomPart};
        config->manageRadios("Target", 2, diskChoices, diskRadios);
        const bool targetDrive = radioEntireDisk->isChecked();
        if (targetDrive || mode!=ConfigSave) {
            config->manageComboBox("Drive", comboDisk, true);
            config->manageGroupCheckBox("DriveEncrypt", boxEncryptAuto);
            if (mode==ConfigSave) config->setValue("RootPortion", sliderPart->value());
            else if (config->contains("RootPortion")) {
                 const int sliderVal = config->value("RootPortion").toInt();
                 sliderPart->setValue(sliderVal);
                 on_sliderPart_valueChanged(sliderVal);
                 if (sliderPart->value() != sliderVal) config->markBadWidget(sliderPart);
            }
        }
        config->endGroup();
        // Custom partitions page. PartMan handles its config groups automatically.
        if (!targetDrive || mode!=ConfigSave) {
            config->setGroupWidget(pagePartitions);
            partman.manageConfig(*config, mode==ConfigSave);
        }

        // Encryption
        config->startGroup("Encryption", targetDrive ? pageDisk : pagePartitions);
        if (mode != ConfigSave) {
            const QString &epass = config->value("Pass").toString();
            if (targetDrive) {
                textCryptoPass->setText(epass);
                textCryptoPass2->setText(epass);
            } else {
                textCryptoPassCust->setText(epass);
                textCryptoPassCust2->setText(epass);
            }
            const QString &keyfile = config->value("KeyMaterial").toString();
            if (!keyfile.isEmpty()) {
                key.load(keyfile.toUtf8().constData(), -1);
                const int keylen = key.length();
                if (keylen>0) {
                    proc.log(QStringLiteral("Loaded %1-byte key material: ").arg(keylen)
                        + keyfile, MProcess::Standard);
                }
            }
        }
        config->endGroup();
    }

    if (mode == ConfigSave || mode == ConfigLoadB) {
        if (!oobe) {
            // GRUB page
            config->startGroup("GRUB", pageBoot);
            config->manageGroupCheckBox("Install", boxBoot);
            const char *grubChoices[] = {"MBR", "PBR", "ESP"};
            QRadioButton *grubRadios[] = {radioBootMBR, radioBootPBR, radioBootESP};
            config->manageRadios("TargetType", 3, grubChoices, grubRadios);
            config->manageComboBox("Location", comboBoot, true);
            config->endGroup();
        }

        // Services page
        config->startGroup("Services", pageServices);
        QTreeWidgetItemIterator it(treeServices);
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

        // Network page
        config->startGroup("Network", pageNetwork);
        config->manageLineEdit("ComputerName", textComputerName);
        config->manageLineEdit("Domain", textComputerDomain);
        config->manageLineEdit("Workgroup", textComputerGroup);
        config->manageCheckBox("Samba", checkSamba);
        config->endGroup();

        // Localization page
        config->startGroup("Localization", pageLocalization);
        config->manageComboBox("Locale", comboLocale, true);
        config->manageCheckBox("LocalClock", checkLocalClock);
        const char *clockChoices[] = {"24", "12"};
        QRadioButton *clockRadios[] = {radioClock24, radioClock12};
        config->manageRadios("ClockHours", 2, clockChoices, clockRadios);
        if (mode == ConfigSave) {
            config->setValue("Timezone", comboTimeZone->currentData().toString());
        } else {
            QVariant def = QString(QTimeZone::systemTimeZoneId());
            const int rc = selectTimeZone(config->value("Timezone", def).toString());
            if (rc == 1) config->markBadWidget(comboTimeArea);
            else if (rc == 2) config->markBadWidget(comboTimeZone);
        }
        config->manageComboBox("Timezone", comboTimeZone, true);
        config->endGroup();

        // User Accounts page
        config->startGroup("User", pageUserAccounts);
        config->manageLineEdit("Username", textUserName);
        config->manageCheckBox("Autologin", checkAutoLogin);
        config->manageCheckBox("SaveDesktop", checkSaveDesktop);
        if (oobe) checkSaveDesktop->setCheckState(Qt::Unchecked);
        const char *oldHomeActions[] = {"Use", "Save", "Delete"};
        QRadioButton *oldHomeRadios[] = {radioOldHomeUse, radioOldHomeSave, radioOldHomeDelete};
        config->manageRadios("OldHomeAction", 3, oldHomeActions, oldHomeRadios);
        config->manageGroupCheckBox("EnableRoot", boxRootAccount);
        if (mode != ConfigSave) {
            const QString &upass = config->value("UserPass").toString();
            textUserPass->setText(upass);
            textUserPass2->setText(upass);
            const QString &rpass = config->value("RootPass").toString();
            textRootPass->setText(rpass);
            textRootPass2->setText(rpass);
        }
        config->endGroup();
    }

    if (mode == ConfigSave) {
        config->sync();
        QFile::remove("/etc/minstalled.conf");
        QFile::copy(config->fileName(), "/etc/minstalled.conf");
    }

    if (config->bad) {
        QMessageBox::critical(this, windowTitle(),
            tr("Invalid settings found in configuration file (%1)."
               " Please review marked fields as you encounter them.").arg(config->fileName()));
    }
}

bool MInstall::saveHomeBasic()
{
    proc.log(__PRETTY_FUNCTION__);
    QString homedir("/");
    QString homedev = partman.getMountDev("/home", true);
    if (homedev.isEmpty() || partman.willFormat("/home")) {
        if (partman.willFormat("/")) return true;
        homedev = partman.getMountDev("/", true);
        homedir = "/home";
    }

    mkdir("/mnt/antiX", 0755);
    const bool ok = proc.exec("/bin/mount -o ro " + homedev + " /mnt/antiX");
    // Store a listing of /home to compare with the user name given later.
    if (ok) listHomes = proc.execOutLines("/bin/ls -1 /mnt/antiX" + homedir);
    proc.exec("/bin/umount -l /mnt/antiX", false);
    return ok;
}

bool MInstall::installLinux()
{
    proc.log(__PRETTY_FUNCTION__);
    if (phase < 0) return false;
    proc.advance(1, 2);

    if (!partman.willFormat("/")) {
        // if root was not formatted and not using --sync option then re-use it
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

    setupAutoMount(true);
    if (!copyLinux()) return false;

    proc.advance(1, 1);
    proc.status(tr("Fixing configuration"));
    mkdir("/mnt/antiX/tmp", 01777);
    chmod("/mnt/antiX/tmp", 01777);

    // Copy live set up to install and clean up.
    proc.exec("/usr/sbin/live-to-installed /mnt/antiX", false);
    qDebug() << "Desktop menu";
    proc.exec("chroot /mnt/antiX desktop-menu --write-out-global", false);

    // if POPULATE_MEDIA_MOUNTPOINTS is true in gazelle-installer-data, then use the --mntpnt switch
    partman.makeFstab(POPULATE_MEDIA_MOUNTPOINTS);
    if(!writeKeyFile()) return false;
    disablehiberanteinitramfs();

    //remove home unless a demo home is found in remastered linuxfs
    if (!isRemasteredDemoPresent)
        proc.exec("/bin/rm -rf /mnt/antiX/home/demo");

    // if POPULATE_MEDIA_MOUNTPOINTS is true in gazelle-installer-data, don't clean /media folder
    // not sure if this is still needed with the live-to-installed change but OK
    if (!POPULATE_MEDIA_MOUNTPOINTS) {
        proc.exec("/bin/rm -rf /mnt/antiX/media/sd*", false);
        proc.exec("/bin/rm -rf /mnt/antiX/media/hd*", false);
    }

    // guess localtime vs UTC
    if (proc.execOut("guess-hwclock") == "localtime")
        checkLocalClock->setChecked(true);

    // create a /etc/machine-id file and /var/lib/dbus/machine-id file
    proc.exec("/bin/mount --rbind --make-rslave /dev /mnt/antiX/dev", true);
    proc.exec("chroot /mnt/antiX rm  /var/lib/dbus/machine-id /etc/machine-id", false);
    proc.exec("chroot /mnt/antiX dbus-uuidgen --ensure=/etc/machine-id", false);
    proc.exec("chroot /mnt/antiX dbus-uuidgen --ensure", false);
    proc.exec("/bin/umount -R /mnt/antiX/dev", true);

    return true;
}

bool MInstall::copyLinux()
{
    proc.log(__PRETTY_FUNCTION__);
    if (phase < 0) return false;

    // copy most except usr, mnt and home
    // must copy boot even if saving, the new files are required
    // media is already ok, usr will be done next, home will be done later
    // setup and start the process
    QString prog = "/bin/cp";
    QStringList args("-av");
    if (sync) {
        prog = "rsync";
        args << "--delete";
        if (!partman.willFormat("/")) args << "--filter" << "protect home/*";
    }
    args << bootSource << rootSources << "/mnt/antiX";
    struct statvfs svfs;
    fsfilcnt_t sourceInodes = 1;
    if (statvfs("/live/linux", &svfs) == 0) {
        sourceInodes = svfs.f_files - svfs.f_ffree;
    }
    proc.advance(80, sourceInodes);
    proc.status(tr("Copying new system"));

    if (!nocopy) {
        if (phase < 0) return false;
        const QString &joined = MProcess::joinCommand(prog, args);
        qDebug().noquote() << "Exec COPY:" << joined;
        QListWidgetItem *logEntry = proc.log(joined, MProcess::Exec);

        QEventLoop eloop;
        connect(&proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), &eloop, &QEventLoop::quit);
        connect(&proc, &QProcess::readyRead, &eloop, &QEventLoop::quit);
        proc.start(prog, args);
        long ncopy = 0;
        while (proc.state() != QProcess::NotRunning) {
            eloop.exec();
            ncopy += proc.readAllStandardOutput().count('\n');
            proc.status(ncopy);
        }
        disconnect(&proc, &QProcess::readyRead, nullptr, nullptr);
        disconnect(&proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), nullptr, nullptr);

        const QByteArray &StdErr = proc.readAllStandardError();
        if (!StdErr.isEmpty()) {
            qDebug() << "SErr COPY:" << StdErr;
            QFont logFont = logEntry->font();
            logFont.setItalic(true);
            logEntry->setFont(logFont);
        }
        qDebug() << "Exit COPY:" << proc.exitCode() << proc.exitStatus();
        if (proc.exitStatus() != QProcess::NormalExit) {
            proc.log(logEntry, -1);
            failUI(tr("Failed to write %1 to destination.\nReturning to Step 1.").arg(PROJECTNAME));
            return false;
        }
        proc.log(logEntry, proc.exitCode() ? 0 : 1);
    }

    return (phase >= 0); // Reduces domino effect if the copy is aborted.
}

///////////////////////////////////////////////////////////////////////////
// install loader

// build a grub configuration and install grub
bool MInstall::installLoader()
{
    proc.log(__PRETTY_FUNCTION__);
    if (phase < 0) return false;
    proc.advance(4, 4);

    QString cmd;
    QString val = proc.execOut("/bin/ls /mnt/antiX/boot | grep 'initrd.img-3.6'");

    // the old initrd is not valid for this hardware
    if (!val.isEmpty())
        proc.exec("/bin/rm -f /mnt/antiX/boot/" + val);

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
    if (efivarfs && !efivarfs_mounted)
        proc.exec("/bin/mount -t efivarfs efivarfs /sys/firmware/efi/efivars", true);

    if (!boxBoot->isChecked()) {
        // skip it
        proc.status(tr("Updating initramfs"));
        //if useing f2fs, then add modules to /etc/initramfs-tools/modules
        qDebug() << "Update initramfs";
        //if (rootTypeCombo->currentText() == "f2fs" || homeTypeCombo->currentText() == "f2fs") {
            //proc.exec("grep -q f2fs /mnt/antiX/etc/initramfs-tools/modules || echo f2fs >> /mnt/antiX/etc/initramfs-tools/modules");
            //proc.exec("grep -q crypto-crc32 /mnt/antiX/etc/initramfs-tools/modules || echo crypto-crc32 >> /mnt/antiX/etc/initramfs-tools/modules");
        //}
        return proc.exec("chroot /mnt/antiX update-initramfs -u -t -k all");
    }

    proc.status(tr("Installing GRUB"));

    // set mounts for chroot
    proc.exec("/bin/mount --rbind --make-rslave /dev /mnt/antiX/dev", true);
    proc.exec("/bin/mount --rbind --make-rslave /sys /mnt/antiX/sys", true);
    proc.exec("/bin/mount --rbind /proc /mnt/antiX/proc", true);
    proc.exec("/bin/mount -t tmpfs -o size=100m,nodev,mode=755 tmpfs /mnt/antiX/run", true);
    proc.exec("/bin/mkdir /mnt/antiX/run/udev", true);
    proc.exec("/bin/mount --rbind /run/udev /mnt/antiX/run/udev", true);

    QString arch;

    // install new Grub now
    const QString &boot = "/dev/" + comboBoot->currentData().toString();
    if (!radioBootESP->isChecked()) {
        cmd = QString("grub-install --target=i386-pc --recheck --no-floppy --force --boot-directory=/mnt/antiX/boot %1").arg(boot);
    } else {
        mkdir("/mnt/antiX/boot/efi", 0755);
        QString mount = QString("/bin/mount %1 /mnt/antiX/boot/efi").arg(boot);
        proc.exec(mount);
        // rename arch to match grub-install target
        arch = proc.execOut("cat /sys/firmware/efi/fw_platform_size");
        arch = (arch == "32") ? "i386" : "x86_64";  // fix arch name for 32bit
        cmd = QString("chroot /mnt/antiX grub-install --force-extra-removable --target=%1-efi --efi-directory=/boot/efi --bootloader-id=%2%3 --recheck").arg(arch, PROJECTSHORTNAME, PROJECTVERSION);
    }

    qDebug() << "Installing Grub";
    if (!proc.exec(cmd)) {
        // error
        QMessageBox::critical(this, windowTitle(),
                              tr("GRUB installation failed. You can reboot to the live medium and use the GRUB Rescue menu to repair the installation."));
        proc.exec("/bin/umount -R /mnt/antiX/run", true);
        proc.exec("/bin/umount -R /mnt/antiX/proc", true);
        proc.exec("/bin/umount -R /mnt/antiX/sys", true);
        proc.exec("/bin/umount -R /mnt/antiX/dev", true);
        if (proc.exec("mountpoint -q /mnt/antiX/boot/efi", true))
            proc.exec("/bin/umount /mnt/antiX/boot/efi", true);
        return false;
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
    if (REMOVE_NOSPLASH) finalcmdline.removeAll("nosplash");
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
        if (arch == "i386")
            proc.exec("/bin/cp /live/boot-dev/boot/uefi-mt/mtest-32.efi /mnt/antiX/boot/uefi-mt", true);
        else
            proc.exec("/bin/cp /live/boot-dev/boot/uefi-mt/mtest-64.efi /mnt/antiX/boot/uefi-mt", true);
    }
    proc.status();

    //update grub with new config

    qDebug() << "Update Grub";
    proc.exec("chroot /mnt/antiX update-grub");

    proc.status(tr("Updating initramfs"));
    //if useing f2fs, then add modules to /etc/initramfs-tools/modules
    //if (rootTypeCombo->currentText() == "f2fs" || homeTypeCombo->currentText() == "f2fs") {
        //proc.exec("grep -q f2fs /mnt/antiX/etc/initramfs-tools/modules || echo f2fs >> /mnt/antiX/etc/initramfs-tools/modules");
        //proc.exec("grep -q crypto-crc32 /mnt/antiX/etc/initramfs-tools/modules || echo crypto-crc32 >> /mnt/antiX/etc/initramfs-tools/modules");
    //}
    proc.exec("chroot /mnt/antiX update-initramfs -u -t -k all");
    proc.status();
    qDebug() << "clear chroot env";
    proc.exec("/bin/umount -R /mnt/antiX/run", true);
    proc.exec("/bin/umount -R /mnt/antiX/proc", true);
    proc.exec("/bin/umount -R /mnt/antiX/sys", true);
    proc.exec("/bin/umount -R /mnt/antiX/dev", true);

    if (proc.exec("mountpoint -q /mnt/antiX/boot/efi", true))
        proc.exec("/bin/umount /mnt/antiX/boot/efi", true);

    return true;
}

// out-of-box experience
void MInstall::enableOOBE()
{
    QTreeWidgetItemIterator it(treeServices);
    for (; *it; ++it) {
        if ((*it)->parent()) setService((*it)->text(0), false); // Speed up the OOBE boot.
    }
    proc.exec("chroot /mnt/antiX/ update-rc.d oobe defaults", true);
}
bool MInstall::processOOBE()
{
    QTreeWidgetItemIterator it(treeServices);
    for (; *it; ++it) {
        if ((*it)->parent()) setService((*it)->text(0), (*it)->checkState(0) == Qt::Checked);
    }

    if (!setComputerName()) return false;
    setLocale();
    if (haveSnapshotUserAccounts) { // skip user account creation
        QString cmd = "rsync -a /home/ /mnt/antiX/home/"
                      " --exclude '.cache' --exclude '.gvfs' --exclude '.dbus' --exclude '.Xauthority'"
                      " --exclude '.ICEauthority' --exclude '.config/session'";
        proc.exec(cmd);
    } else {
        if (!setUserInfo()) return false;
    }
    if (oobe) proc.exec("update-rc.d oobe disable", false);
    return true;
}

/////////////////////////////////////////////////////////////////////////
// user account functions

bool MInstall::validateUserInfo()
{
    const QString &userName = textUserName->text();
    nextFocus = textUserName;
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

    if (!automatic && boxRootAccount->isChecked() && textRootPass->text().isEmpty()) {
        // Confirm that an empty root password is not accidental.
        const QMessageBox::StandardButton ans = QMessageBox::warning(this,
            windowTitle(), tr("You did not provide a password for the root account."
                " Do you want to continue?"), QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
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
    if (!oobe) cmdChRoot = "chroot /mnt/antiX ";
    const QString &userPass = textUserPass->text();
    QByteArray userinfo;
    if (!(boxRootAccount->isChecked())) ok = proc.exec(cmdChRoot + "passwd -l root", true);
    else {
        const QString &rootPass = textRootPass->text();
        if (rootPass.isEmpty()) ok = proc.exec(cmdChRoot + "passwd -d root", true);
        else userinfo.append(QString("root:" + rootPass).toUtf8());
    }
    if (ok && userPass.isEmpty()) ok = proc.exec(cmdChRoot + "passwd -d demo", true);
    else {
        if (!userinfo.isEmpty()) userinfo.append('\n');
        userinfo.append(QString("demo:" + userPass).toUtf8());
    }
    if (ok && !userinfo.isEmpty()) ok = proc.exec(cmdChRoot + "chpasswd", true, &userinfo);
    if (!ok) {
        failUI(tr("Failed to set user account passwords."));
        return false;
    }

    QString rootpath;
    if (!oobe) rootpath = "/mnt/antiX";
    QString skelpath = rootpath + "/etc/skel";
    QString dpath = rootpath + "/home/" + textUserName->text();

    if (QFileInfo::exists(dpath)) {
        if (radioOldHomeSave->isChecked()) {
            bool ok = false;
            QString cmd = QString("/bin/mv -f %1 %1.00%2").arg(dpath);
            for (int ixi = 1; ixi < 10 && !ok; ++ixi)
                ok = proc.exec(cmd.arg(ixi));
            if (!ok) {
                failUI(tr("Failed to save old home directory."));
                return false;
            }
        } else if (radioOldHomeDelete->isChecked()) {
            if (!proc.exec("/bin/rm -rf " + dpath)) {
                failUI(tr("Failed to delete old home directory."));
                return false;
            }
        }
        proc.exec("/bin/sync", true); // The sync(2) system call will block the GUI.
    }

    if (QFileInfo::exists(dpath.toUtf8())) { // Still exists.
        proc.exec("/bin/cp -n " + skelpath + "/.bash_profile " + dpath, true);
        proc.exec("/bin/cp -n " + skelpath + "/.bashrc " + dpath, true);
        proc.exec("/bin/cp -n " + skelpath + "/.gtkrc " + dpath, true);
        proc.exec("/bin/cp -n " + skelpath + "/.gtkrc-2.0 " + dpath, true);
        proc.exec("/bin/cp -Rn " + skelpath + "/.config " + dpath, true);
        proc.exec("/bin/cp -Rn " + skelpath + "/.local " + dpath, true);
    } else { // dir does not exist, must create it
        // Copy skel to demo, unless demo folder exists in remastered linuxfs.
        if (!isRemasteredDemoPresent) {
            if (!proc.exec("/bin/cp -a " + skelpath + ' ' + dpath)) {
                failUI(tr("Sorry, failed to create user directory."));
                return false;
            }
        } else { // still rename the demo directory even if remastered demo home folder is detected
            if (!proc.exec("/bin/mv -f " + rootpath + "/home/demo " + dpath)) {
                failUI(tr("Sorry, failed to name user directory."));
                return false;
            }
        }
    }

    // saving Desktop changes
    if (checkSaveDesktop->isChecked()) {
        resetBlueman();
        rsynchomefolder(dpath);
    }

    // check if remaster exists and checkSaveDesktop not checked, modify the remastered demo folder
    if (isRemasteredDemoPresent && ! checkSaveDesktop->isChecked())
    {
        resetBlueman();
        changeRemasterdemoToNewUser(dpath);
    }

    // fix the ownership, demo=newuser
    if (!proc.exec("chown -R demo:demo " + dpath)) {
        failUI(tr("Sorry, failed to set ownership of user directory."));
        return false;
    }

    // change in files
    replaceStringInFile("demo", textUserName->text(), rootpath + "/etc/group");
    replaceStringInFile("demo", textUserName->text(), rootpath + "/etc/gshadow");
    replaceStringInFile("demo", textUserName->text(), rootpath + "/etc/passwd");
    replaceStringInFile("demo", textUserName->text(), rootpath + "/etc/shadow");
    replaceStringInFile("demo", textUserName->text(), rootpath + "/etc/slim.conf");
    replaceStringInFile("demo", textUserName->text(), rootpath + "/etc/lightdm/lightdm.conf");
    replaceStringInFile("demo", textUserName->text(), rootpath + "/home/*/.gtkrc-2.0");
    replaceStringInFile("demo", textUserName->text(), rootpath + "/root/.gtkrc-2.0");
    if (checkAutoLogin->isChecked()) {
        replaceStringInFile("#auto_login", "auto_login", rootpath + "/etc/slim.conf");
        replaceStringInFile("#default_user ", "default_user ", rootpath + "/etc/slim.conf");
        replaceStringInFile("User=", "User=" + textUserName->text(), rootpath + "/etc/sddm.conf");
    }
    else {
        replaceStringInFile("auto_login", "#auto_login", rootpath + "/etc/slim.conf");
        replaceStringInFile("default_user ", "#default_user ", rootpath + "/etc/slim.conf");
        replaceStringInFile("autologin-user=", "#autologin-user=", rootpath + "/etc/lightdm/lightdm.conf");
        replaceStringInFile("User=.*", "User=", rootpath + "/etc/sddm.conf");
    }
    proc.exec("touch " + rootpath + "/var/mail/" + textUserName->text());

    return true;
}

/////////////////////////////////////////////////////////////////////////
// computer name functions

bool MInstall::validateComputerName()
{
    // see if name is reasonable
    nextFocus = textComputerName;
    if (textComputerName->text().isEmpty()) {
        QMessageBox::critical(this, windowTitle(), tr("Please enter a computer name."));
        return false;
    } else if (textComputerName->text().contains(QRegExp("[^0-9a-zA-Z-.]|^[.-]|[.-]$|\\.\\."))) {
        QMessageBox::critical(this, windowTitle(),
                              tr("Sorry, your computer name contains invalid characters.\nYou'll have to select a different\nname before proceeding."));
        return false;
    }
    // see if name is reasonable
    nextFocus = textComputerDomain;
    if (textComputerDomain->text().isEmpty()) {
        QMessageBox::critical(this, windowTitle(), tr("Please enter a domain name."));
        return false;
    } else if (textComputerDomain->text().contains(QRegExp("[^0-9a-zA-Z-.]|^[.-]|[.-]$|\\.\\."))) {
        QMessageBox::critical(this, windowTitle(),
                              tr("Sorry, your computer domain contains invalid characters.\nYou'll have to select a different\nname before proceeding."));
        return false;
    }

    if (haveSamba) {
        // see if name is reasonable
        if (textComputerGroup->text().isEmpty()) {
            QMessageBox::critical(this, windowTitle(), tr("Please enter a workgroup."));
            nextFocus = textComputerGroup;
            return false;
        }
    } else {
        textComputerGroup->clear();
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
        //replaceStringInFile(PROJECTSHORTNAME + "1", textComputerName->text(), "/mnt/antiX/etc/samba/smb.conf");
        replaceStringInFile("WORKGROUP", textComputerGroup->text(), etcpath + "/samba/smb.conf");
        const bool enable = checkSamba->isChecked();
        setService("smbd", enable);
        setService("nmbd", enable);
        setService("samba-ad-dc", enable);
    }
    //replaceStringInFile(PROJECTSHORTNAME + "1", textComputerName->text(), "/mnt/antiX/etc/hosts");
    const QString &compname = textComputerName->text();
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
    cmd += QString("/usr/sbin/update-locale \"LANG=%1\"").arg(comboLocale->currentData().toString());
    qDebug() << "Update locale";
    proc.exec(cmd);
    cmd = QString("Language=%1").arg(comboLocale->currentData().toString());

    // /etc/localtime is either a file or a symlink to a file in /usr/share/zoneinfo. Use the one selected by the user.
    //replace with link
    if (!oobe) {
        cmd = QString("/bin/ln -nfs /usr/share/zoneinfo/%1 /mnt/antiX/etc/localtime").arg(comboTimeZone->currentData().toString());
        proc.exec(cmd, false);
    }
    cmd = QString("/bin/ln -nfs /usr/share/zoneinfo/%1 /etc/localtime").arg(comboTimeZone->currentData().toString());
    proc.exec(cmd, false);
    // /etc/timezone is text file with the timezone written in it. Write the user-selected timezone in it now.
    if (!oobe) {
        cmd = QString("echo %1 > /mnt/antiX/etc/timezone").arg(comboTimeZone->currentData().toString());
        proc.exec(cmd, false);
    }
    cmd = QString("echo %1 > /etc/timezone").arg(comboTimeZone->currentData().toString());
    proc.exec(cmd, false);

    // Set clock to use LOCAL
    if (checkLocalClock->isChecked())
        proc.exec("echo '0.0 0 0.0\n0\nLOCAL' > /etc/adjtime", false);
    else
        proc.exec("echo '0.0 0 0.0\n0\nUTC' > /etc/adjtime", false);
    proc.exec("hwclock --hctosys");
    if (!oobe) {
        proc.exec("/bin/cp -f /etc/adjtime /mnt/antiX/etc/");
        proc.exec("/bin/cp -f /etc/default/rcS /mnt/antiX/etc/default");
    }

    // Set clock format
    QString skelpath = oobe ? "/etc/skel" : "/mnt/antiX/etc/skel";
    if (radioClock12->isChecked()) {
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
    QTreeWidgetItemIterator it(treeServices);
    while (*it) {
        if ((*it)->parent() != nullptr) {
            (*it)->setCheckState(save?2:0, (*it)->checkState(save?0:2));
        }
        ++it;
    }
}

void MInstall::setService(const QString &service, bool enabled)
{
    qDebug() << "Set service:" << service << enabled;
    QString chroot, rootpath;
    if (!oobe) {
        chroot = "chroot /mnt/antiX ";
        rootpath = "/mnt/antiX";
    }
    if (enabled) {
        proc.exec(chroot + "update-rc.d " + service + " defaults");
        if (containsSystemD)
            proc.exec(chroot + "systemctl enable " + service);
        if (containsRunit) {
            QFile::remove(rootpath+"/etc/sv/" + service + "/down");
            if (!QFile::exists(rootpath+"/etc/sv/" + service)) {
                proc.mkpath(rootpath+"/etc/sv/" + service);
                proc.exec(chroot + "ln -fs /etc/sv/" + service + " /etc/service/");
            }
        }
    } else {
        proc.exec(chroot + "update-rc.d " + service + " remove");
        if (containsSystemD) {
            proc.exec(chroot + "systemctl disable " + service);
            proc.exec(chroot + "systemctl mask " + service);
        }
        if (containsRunit) {
            if (!QFile::exists(rootpath+"/etc/sv/" + service)) {
                proc.mkpath(rootpath+"/etc/sv/" + service);
                proc.exec(chroot + "ln -fs /etc/sv/" + service + " /etc/service/");
            }
            proc.exec(chroot + "touch /etc/sv/" + service + "/down");
        }
    }
}
void MInstall::failUI(const QString &msg)
{
    proc.log(__PRETTY_FUNCTION__);
    qDebug() << "Phase" << phase << '-' << msg;
    if (phase >= 0) {
        boxMain->setEnabled(false);
        QMessageBox::critical(this, windowTitle(), msg);
        updateCursor(Qt::WaitCursor);
    }
}


// logic displaying pages
int MInstall::showPage(int curr, int next)
{
    if (curr == Step::Disk && next > curr) {
        if (radioEntireDisk->isChecked()) {
            if (!automatic) {
                QString msg = tr("OK to format and use the entire disk (%1) for %2?");
                if (!uefi) {
                    DeviceItem *devit = partman.findDevice(comboDisk->currentData().toString());
                    if (devit && devit->size >= (2048LL*1073741824LL)) {
                        msg += "\n\n" + tr("WARNING: The selected drive has a capacity of at least 2TB and must be formatted using GPT."
                                           " On some systems, a GPT-formatted disk will not boot.");
                        return curr;
                    }
                }
                int ans = QMessageBox::warning(this, windowTitle(),
                    msg.arg(comboDisk->currentData().toString(), PROJECTNAME),
                    QMessageBox::Yes, QMessageBox::No);
                if (ans != QMessageBox::Yes) return curr; // don't format - stop install
            }
            partman.clearAllUses();
            partman.selectedDriveAuto()->layoutDefault(-1, boxEncryptAuto->isChecked());
            if (!partman.composeValidate(true, PROJECTNAME)) {
                nextFocus = treePartitions;
                return curr;
            }
            return Step::Boot;
        }
    } else if (curr == Step::Partitions && next > curr) {
        if (!partman.composeValidate(automatic, PROJECTNAME)) {
            nextFocus = treePartitions;
            return curr;
        }
        if (!pretend && !saveHomeBasic()) {
            QMessageBox::critical(this, windowTitle(),
                tr("The data in /home cannot be preserved because"
                    " the required information could not be obtained."));
            return curr;
        }
        return Step::Boot;
    } else if (curr == Step::Boot && next > curr) {
        if (oem) return Step::Progress;
        return Step::Services + 1;
    } else if (curr == Step::UserAccounts && next > curr) {
        if (!validateUserInfo()) return curr;
        if (!haveOldHome) return Step::Progress; // Skip pageOldHome
    } else if (curr == Step::Network && next > curr) {
        if (!validateComputerName()) return curr;
    } else if (curr == Step::Network && next < curr) { // Backward
        return Step::Boot; // Skip pageServices
    } else if (curr == Step::Localization && next > curr) {
        if (!pretend && haveSnapshotUserAccounts) {
            return Step::Progress; // Skip pageUserAccounts and pageOldHome
        }
    } else if (curr == Step::OldHome && next < curr) { // Backward
        if (!pretend && haveSnapshotUserAccounts) {
            return Step::Localization; // Skip pageUserAccounts and pageOldHome
        }
    } else if (curr == Step::Progress && next < curr) { // Backward
        if (oem) return Step::Boot;
        if (!haveOldHome) {
            // skip pageOldHome
            if (!pretend && haveSnapshotUserAccounts) return Step::Localization;
            return Step::UserAccounts;
        }
    } else if (curr == Step::Services) { // Backward or forward
        stashServices(next > curr);
        return Step::Localization; // The page that called pageServices
    }
    return next;
}

void MInstall::pageDisplayed(int next)
{
    if (!oobe) {
        const int ixProgress = widgetStack->indexOf(pageProgress);
        // progress bar shown only for install and configuration pages.
        boxInstall->setVisible(next >= widgetStack->indexOf(pageBoot) && next <= ixProgress);
        // save the last tip and stop it updating when the progress page is hidden.
        if (next != ixProgress) ixTipStart = ixTip;
    }

    // These calculations are only for display text, and do not affect the installation.
    long long rootMin = partman.rootSpaceNeeded + 1048575;
    const QString &tminroot = QLocale::system().formattedDataSize(rootMin, 0, QLocale::DataSizeTraditionalFormat);
    rootMin = (4 * rootMin) + ROOT_BUFFER * 1048576; // (Root + snapshot [squashfs + ISO] + backup) + buffer.
    const QString &trecroot = QLocale::system().formattedDataSize(rootMin, 0, QLocale::DataSizeTraditionalFormat);

    switch (next) {
    case Step::Terms:
        textHelp->setText("<p><b>" + tr("General Instructions") + "</b><br/>"
            + tr("BEFORE PROCEEDING, CLOSE ALL OTHER APPLICATIONS.") + "</p>"
            "<p>" + tr("On each page, please read the instructions, make your selections, and then click on Next when you are ready to proceed."
                " You will be prompted for confirmation before any destructive actions are performed.") + "</p>"
            + "<p><b>" + tr("Limitations") + "</b><br/>"
            + tr("Remember, this software is provided AS-IS with no warranty what-so-ever."
                " It is solely your responsibility to backup your data before proceeding.") + "</p>");
        pushNext->setDefault(true);
        break;
    case Step::Disk:
        textHelp->setText("<p><b>" + tr("Installation Options") + "</b><br/>"
            + tr("Installation requires about %1 of space. %2 or more is preferred.").arg(tminroot, trecroot) + "</p>"
            "<p>" + tr("If you are running Mac OS or Windows OS (from Vista onwards), you may have to use that system's software to set up partitions and boot manager before installing.") + "</p>"
            "<p><b>" + tr("Using the root-home space slider") + "</b><br/>"
            + tr("The drive can be divided into separate system (root) and user data (home) partitions using the slider.") + "</p>"
            "<p>" + tr("The <b>root</b> partition will contain the operating system and applications.") + "<br/>"
            + tr("The <b>home</b> partition will contain the data of all users, such as their settings, files, documents, pictures, music, videos, etc.") + "</p>"
            "<p>" + tr("Move the slider to the right to increase the space for <b>root</b>. Move it to the left to increase the space for <b>home</b>.") + "<br/>"
            + tr("Move the slider all the way to the right if you want both root and home on the same partition.") + "</p>"
            "<p>" + tr("Keeping the home directory in a separate partition improves the reliability of operating system upgrades. It also makes backing up and recovery easier."
                " This can also improve overall performance by constraining the system files to a defined portion of the drive.") + "</p>"
            "<p><b>" + tr("Encryption") + "</b><br/>"
            + tr("Encryption is possible via LUKS. A password is required.") + "</p>"
            "<p>" + tr("A separate unencrypted boot partition is required.") + "</p>"
            "<p>" + tr("When encryption is used with autoinstall, the separate boot partition will be automatically created.") + "</p>"
            "<p><b>" + tr("Using a custom disk layout") + "</b><br/>"
            + tr("If you need more control over where %1 is installed to, select \"<b>%2</b>\" and click <b>Next</b>."
                " On the next page, you will then be able to select and configure the storage devices and"
                " partitions you need.").arg(PROJECTNAME, radioCustomPart->text().remove('&')) + "</p>");
        if (phase < 0) {
            updateCursor(Qt::WaitCursor);
            phase = 0;
            proc.unhalt();
            updatePartitionWidgets(true);
            updateCursor();
        }
        pushBack->setEnabled(true);
        pushNext->setEnabled(radioCustomPart->isChecked()
            || !boxEncryptAuto->isChecked() || textCryptoPass->isValid());
        return; // avoid the end that enables both Back and Next buttons

    case Step::Partitions:
        textHelp->setText("<p><b>" + tr("Choose Partitions") + "</b><br/>"
            + tr("The partition list allows you to choose what partitions are used for this installation.") + "</p>"
            "<p>" + tr("<i>Device</i> - This is the block device name that is, or will be, assigned to the created partition.") + "</p>"
            "<p>" + tr("<i>Size</i> - The size of the partition. This can only be changed on a new layout.") + "</p>"
            "<p>" + tr("<i>Use For</i> - To use this partition in an installation, you must select something here.") + "<br/>"
            " - " + tr("Format - Format without mounting.") + "<br/>"
            " - " + tr("BIOS-GRUB - BIOS Boot GPT partition for GRUB.") + "<br/>"
            " - " + tr("EFI - EFI System Partition.") + "<br/>"
            " - " + tr("boot - Boot manager (/boot).") + "<br/>"
            " - " + tr("root - System root (/).") + "<br/>"
            " - " + tr("swap - Swap space.") + "<br/>"
            " - " + tr("home - User data (/home).") + "<br/>"
            + tr("In addition to the above, you can also type your own mount point. Custom mount points must start with a slash (\"/\").") + "<br/>"
            + tr("The installer treats \"/boot\", \"/\", and \"/home\" exactly the same as \"boot\", \"root\", and \"home\", respectively.") + "</p>"
            "<p>" + tr("<i>Label</i> - The label that is assigned to the partition once it has been formatted.") + "</p>"
            "<p>" + tr("<i>Encrypt</i> - Use LUKS encryption for this partition. The password applies to all partitions selected for encryption.") + "</p>"
            "<p>" + tr("<i>Format</i> - This is the partition's format. Available formats depend on what the partition is used for."
                " When working with an existing layout, you may be able to preserve the format of the partition by selecting <b>Preserve</b>.") + "</p>"
            "<p>" + tr("The ext2, ext3, ext4, jfs, xfs, btrfs and reiserfs Linux filesystems are supported and ext4 is recommended.") + "</p>"
            "<p>" + tr("<i>Check</i> - Check and correct for bad blocks on the drive (not supported for all formats)."
                " This is very time consuming, so you may want to skip this step unless you suspect that your drive has bad blocks.") + "</p>"
            "<p>" + tr("<i>Mount Options</i> - This specifies mounting options that will be used for this partition.") + "</p>"
            "<p>" + tr("<i>Dump</i> - Instructs the dump utility to include this partition in the backup.") + "</p>"
            "<p>" + tr("<i>Pass</i> - The sequence in which this file system is to be checked at boot. If zero, the file system is not checked.") + "</p>"
            "<p><b>" + tr("Menus and actions") + "</b><br/>"
            + tr("A variety of actions are available by right-clicking any drive or partition item in the list.") + "<br/>"
            + tr("The buttons to the right of the list can also be used to manipulate the entries.") + "</p>"
            "<p>" + tr("The installer cannot modify the layout already on the drive."
                " To create a custom layout, mark the drive for a new layout with the <b>New layout</b> menu action"
                " or button (%1). This clears the existing layout.").arg("<img src=':/edit-clear-all'/>") + "</p>"
            "<p><b>" + tr("Basic layout requirements") + "</b><br/>"
            + tr("%1 requires a root partition. The swap partition is optional but highly recommended."
                " If you want to use the Suspend-to-Disk feature of %1, you will need a swap partition that is larger than your physical memory size.").arg(PROJECTNAME) + "</p>"
            "<p>" + tr("If you choose a separate /home partition it will be easier for you to upgrade in the future,"
                " but this will not be possible if you are upgrading from an installation that does not have a separate home partition.") + "</p>"
            "<p><b>" + tr("Active partition") + "</b><br/>"
            + tr("For the installed operating system to boot, the appropriate partition (usually the boot or root partition) must be the marked as active.") + "</p>"
            "<p>" + tr("The active partition of a drive can be chosen using the <b>Active partition</b> menu action.") + "<br/>"
            + tr("A partition with an asterisk (*) next to its device name is, or will become, the active partition.") + "</p>"
            "<p><b>" + tr("Boot partition") + "</b><br/>"
            + tr("This partition is generally only required for root partitions on virtual devices such as encrypted, LVM or software RAID volumes.") + "<br/>"
            + tr("It contains a basic kernel and drivers used to access the encrypted disk or virtual devices.") + "</p>"
            "<p><b>" + tr("BIOS-GRUB partition") + "</b><br/>"
            + tr("When using a GPT-formatted drive on a non-EFI system, a 1MB BIOS boot partition is required when using GRUB.") + "<br/>"
            + tr("New drives are formatted in GPT if more than 4 partitions are to be created, or the drive has a capacity greater than 2TB."
                " If the installer is about to format the disk in GPT, and there is no BIOS-GRUB partition, a warning will be displayed before the installation starts.") + "</p>"
            "<p><b>" + tr("Need help creating a layout?") + "</b><br/>"
            + tr("Just right-click on a drive to bring up a menu, and select a layout template. These layouts are similar to that of the regular install.") + "</p>"
            "<p>" + tr("<i>Standard install</i> - Suited to most setups. This template does not add a separate boot partition, and so it is unsuitable for use with an encrypted operating system.") + "</p>"
            "<p>" + tr("<i>Encrypted system</i> - Contains the boot partition required to load an encrypted operating system. This template can also be used as the basis for a multi-boot system.") + "</p>"
            "<p><b>" + tr("Upgrading") + "</b><br/>"
            + tr("To upgrade from an existing Linux installation, select the same home partition as before and select <b>Preserve</b> as the format.") + "</p>"
            "<p>" + tr("If you do not use a separate home partition, select <b>Preserve /home</b> on the root file system entry to preserve the existing /home directory located on your root partition."
                " The installer will only preserve /home, and will delete everything else. As a result, the installation will take much longer than usual.") + "</p>"
            "<p><b>" + tr("Preferred Filesystem Type") + "</b><br/>"
            + tr("For %1, you may choose to format the partitions as ext2, ext3, ext4, f2fs, jfs, xfs, btrfs or reiser.").arg(PROJECTNAME) + "</p>"
            "<p>" + tr("Additional compression options are available for drives using btrfs."
                " Lzo is fast, but the compression is lower. Zlib is slower, with higher compression.") + "</p>"
            "<p><b>" + tr("System partition management tool") + "</b><br/>"
            + tr("For more control over the drive layouts (such as modifying the existing layout on a disk), click the"
                " partition management button (%1). This will run the operating system's partition management tool,"
                " which will allow you to create the exact layout you need.").arg("<img src=':/partitionmanager'/>") + "</p>"
            "<p><b>" + tr("Encryption") + "</b><br/>"
            + tr("Encryption is possible via LUKS. A password is required.") + "</p>"
            "<p>" + tr("A separate unencrypted boot partition is required.") + "</p>"
            "<p>" + tr("To preserve an encrypted partition, right-click on it and select <b>Unlock</b>. In the dialog that appears, enter a name for the virtual device and the password."
                " When the device is unlocked, the name you chose will appear under <i>Virtual Devices</i>, with similar options to that of a regular partition.") + "</p><p>"
            + tr("For the encrypted partition to be unlocked at boot, it needs to be added to the crypttab file. Use the <b>Add to crypttab</b> menu action to do this.") + "</p>"
            "<p><b>" + tr("Other partitions") + "</b><br/>"
            + tr("The installer allows other partitions to be created or used for other purposes, however be mindful that older systems cannot handle drives with more than 4 partitions.") + "</p>"
            "<p><b>" + tr("Subvolumes") + "</b><br/>"
            + tr("Some file systems, such as Btrfs, support multiple subvolumes in a single partition."
                " These are not physical subdivisions, and so their order does not matter.") + "<br/>"
            + tr("Use the <b>Scan subvolumes</b> menu action to search an existing Btrfs partition for subvolumes."
                " To create a new subvolume, use the <b>New subvolume</b> menu action.") + "</p><p>"
            + tr("Existing subvolumes can be preserved, however the name must remain the same.") + "</p>"
            "<p><b>" + tr("Virtual Devices") + "</b><br/>"
            + tr("If the intaller detects any virtual devices such as opened LUKS partitions, LVM logical volumes or software-based RAID volumes, they may be used for the installation.") + "</p>"
            "<p>" + tr("The use of virtual devices (beyond preserving encrypted file systems) is an advanced feature. You may have to edit some files (eg. initramfs, crypttab, fstab) to ensure the virtual devices used are created upon boot.") + "</p>");
        pushBack->setEnabled(true);
        pushNext->setEnabled(!(boxCryptoPass->isEnabledTo(boxCryptoPass->parentWidget())) || textCryptoPassCust->isValid());
        return; // avoid the end that enables both Back and Next buttons

    case Step::Boot: // Start of installation.
        textHelp->setText(tr("<p><b>Select Boot Method</b><br/> %1 uses the GRUB bootloader to boot %1 and MS-Windows. "
                             "<p>By default GRUB2 is installed in the Master Boot Record (MBR) or ESP (EFI System Partition for 64-bit UEFI boot systems) of your boot drive and replaces the boot loader you were using before. This is normal.</p>"
                             "<p>If you choose to install GRUB2 to Partition Boot Record (PBR) instead, then GRUB2 will be installed at the beginning of the specified partition. This option is for experts only.</p>"
                             "<p>If you uncheck the Install GRUB box, GRUB will not be installed at this time. This option is for experts only.</p>").arg(PROJECTNAME));

        pushBack->setEnabled(false);
        pushNext->setEnabled(true);
        if (phase <= 0) {
            buildBootLists();
            manageConfig(ConfigLoadB);
        }
        return; // avoid the end that enables both Back and Next buttons

    case Step::Services:
        textHelp->setText(tr("<p><b>Common Services to Enable</b><br/>Select any of these common services that you might need with your system configuration and the services will be started automatically when you start %1.</p>").arg(PROJECTNAME));
        break;

    case Step::Network:
        textHelp->setText(tr("<p><b>Computer Identity</b><br/>The computer name is a common unique name which will identify your computer if it is on a network. "
                             "The computer domain is unlikely to be used unless your ISP or local network requires it.</p>"
                             "<p>The computer and domain names can contain only alphanumeric characters, dots, hyphens. They cannot contain blank spaces, start or end with hyphens</p>"
                             "<p>The SaMBa Server needs to be activated if you want to use it to share some of your directories or printer "
                             "with a local computer that is running MS-Windows or Mac OSX.</p>"));
        if (oobe) {
            pushBack->setEnabled(false);
            pushNext->setEnabled(true);
            return; // avoid the end that enables both Back and Next buttons
        }
        break;

    case Step::Localization:
        textHelp->setText("<p><b>" + tr("Localization Defaults") + "</b><br/>"
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

    case Step::UserAccounts:
        textHelp->setText("<p><b>" + tr("Default User Login") + "</b><br/>"
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
        if (!nextFocus) nextFocus = textUserName;
        pushBack->setEnabled(true);
        userPassValidationChanged();
        return; // avoid the end that enables both Back and Next buttons

    case Step::OldHome:
        textHelp->setText("<p><b>" + tr("Old Home Directory") + "</b><br/>"
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
        if (pushNext->isEnabled() == false) {
            pushBack->setEnabled(true);
            return;
        }
        break;

    case Step::Progress:
        if (ixTipStart >= 0) {
            iLastProgress = progInstall->value();
            on_progInstall_valueChanged(iLastProgress);
        }
        textHelp->setText("<p><b>" + tr("Installation in Progress") + "</b><br/>"
            + tr("%1 is installing. For a fresh install, this will probably take 3-20 minutes, depending on the speed of your system and the size of any partitions you are reformatting.").arg(PROJECTNAME)
            + "</p><p>"
            + tr("If you click the Abort button, the installation will be stopped as soon as possible.")
            + "</p><p>"
            + "<b>" + tr("Change settings while you wait") + "</b><br/>"
            + tr("While %1 is being installed, you can click on the <b>Next</b> or <b>Back</b> buttons to enter other information required for the installation.").arg(PROJECTNAME)
            + "</p><p>"
            + tr("Complete these steps at your own pace. The installer will wait for your input if necessary.")
            + "</p>");
        pushBack->setEnabled(true);
        pushNext->setEnabled(false);
        return; // avoid enabling both Back and Next buttons at the end

    case Step::End:
        pushClose->setEnabled(false);
        textHelp->setText(tr("<p><b>Congratulations!</b><br/>You have completed the installation of %1</p>"
                             "<p><b>Finding Applications</b><br/>There are hundreds of excellent applications installed with %1 "
                             "The best way to learn about them is to browse through the Menu and try them. "
                             "Many of the apps were developed specifically for the %1 project. "
                             "These are shown in the main menus. "
                             "<p>In addition %1 includes many standard Linux applications that are run only from the command line and therefore do not show up in the Menu.</p>").arg(PROJECTNAME));
        break;

    default: // other
        textHelp->setText("<p><b>" + tr("Enjoy using %1</b></p>").arg(PROJECTNAME) + "\n\n "
        + tr("<p><b>Support %1</b><br/>"
            "%1 is supported by people like you. Some help others at the "
            "support forum - %2 - or translate help files into different "
            "languages, or make suggestions, write documentation, or help test new software.</p>").arg(PROJECTNAME).arg(PROJECTFORUM));
        pushNext->setDefault(true);
        break;
    }

    pushBack->setEnabled(true);
    pushNext->setEnabled(true);
}

void MInstall::gotoPage(int next)
{
    pushBack->setEnabled(false);
    pushNext->setEnabled(false);
    widgetStack->setEnabled(false);
    int curr = widgetStack->currentIndex();
    next = showPage(curr, next);

    // modify ui for standard cases
    pushClose->setHidden(next == 0);
    pushBack->setHidden(next <= 1);
    pushNext->setHidden(next == 0);

    QSize isize = pushNext->iconSize();
    isize.setWidth(isize.height());
    if (next >= Step::End) {
        // entering the last page
        pushBack->hide();
        pushNext->setText(tr("Finish"));
    } else if (next == Step::Services){
        isize.setWidth(0);
        pushNext->setText(tr("OK"));
    } else {
        pushNext->setText(tr("Next"));
    }
    pushNext->setIconSize(isize);
    if (next > Step::End) {
        // finished
        updateCursor(Qt::WaitCursor);
        if (!pretend && checkExitReboot->isChecked()) {
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
            && next > curr) pushNext->click();
        else if (curr!=0) automatic = false; // failed validation
    }

    // process next installation phase
    if (next == widgetStack->indexOf(pageBoot) || next == widgetStack->indexOf(pageProgress)) {
        if (oobe) {
            updateCursor(Qt::BusyCursor);
            labelSplash->setText(tr("Configuring sytem. Please wait."));
            gotoPage(Step::Splash);
            if (processOOBE()) {
                labelSplash->setText(tr("Configuration complete. Restarting system."));
                proc.exec("/usr/sbin/reboot", true);
                qApp->exit(EXIT_SUCCESS);
            } else {
                labelSplash->setText(tr("Could not complete configuration."));
                pushClose->show();
            }
        } else if (!processNextPhase() && phase > -2) {
            cleanup(false);
            gotoPage(Step::Disk);
            boxMain->setEnabled(true);
        }
        updateCursor();
    }
}

void MInstall::updatePartitionWidgets(bool all)
{
    proc.log(__PRETTY_FUNCTION__);

    comboDisk->setEnabled(false);
    comboDisk->clear();
    comboDisk->addItem(tr("Loading..."));
    partman.scan();
    if (mactest) {
        for (DeviceItemIterator it(partman); DeviceItem *item = *it; it.next()) {
            if (item->device.startsWith("sda")) {
                item->flags.nasty = true;
                partman.notifyChange(item);
            }
        }
    }

    // disk combo box
    comboDisk->clear();
    for (DeviceItemIterator it(partman); DeviceItem *item = *it; it.next()) {
        if (item->type == DeviceItem::Drive && item->size >= partman.rootSpaceNeeded
                && (!item->flags.bootRoot || INSTALL_FROM_ROOT_DEVICE)) {
            item->addToCombo(comboDisk);
        }
    }
    comboDisk->setCurrentIndex(0);
    comboDisk->setEnabled(true);

    if (all) {
        // whole-disk vs custom-partition radio buttons
        radioEntireDisk->setChecked(true);
        for (DeviceItemIterator it(partman); *it; it.next()) {
            if ((*it)->isVolume()) {
                // found at least one partition
                radioCustomPart->setChecked(true);
                break;
            }
        }
    }

    // Partition slider.
    setupPartitionSlider();
}
void MInstall::setupPartitionSlider()
{
    // Allow the slider labels to fit all possible formatted sizes.
    const QString &strMB = sliderSizeString(1072693248) + '\n'; // "1,023 GB"
    const QFontMetrics &fmetrics = labelSliderRoot->fontMetrics();
    int mwidth = fmetrics.boundingRect(QRect(), Qt::AlignCenter, strMB + tr("Root")).width();
    labelSliderRoot->setMinimumWidth(mwidth);
    mwidth = fmetrics.boundingRect(QRect(), Qt::AlignCenter, strMB + tr("Home")).width();
    labelSliderHome->setMinimumWidth(mwidth);
    labelSliderRoot->setText("----");
    labelSliderHome->setText("----");
    // Snap the slider to the legal range.
    on_sliderPart_valueChanged(sliderPart->value());
}

void MInstall::buildServiceList()
{
    proc.log(__PRETTY_FUNCTION__);

    //setup treeServices
    treeServices->header()->setMinimumSectionSize(150);
    treeServices->header()->resizeSection(0,150);

    QSettings services_desc("/usr/share/gazelle-installer-data/services.list", QSettings::NativeFormat);

    for (const QString &service : qAsConst(ENABLE_SERVICES)) {
        const QString &lang = QLocale::system().bcp47Name().toLower();
        QString lang_str = (lang == "en")? "" : "_" + lang;
        QStringList list = services_desc.value(service + lang_str).toStringList();
        if (list.size() != 2) {
            list = services_desc.value(service).toStringList(); // Use English definition
            if (list.size() != 2)
                continue;
        }
        QString category, description;
        category = list.at(0);
        description = list.at(1);

        if (QFile::exists("/etc/init.d/"+service) || QFile::exists("/etc/sv/"+service)) {
            QList<QTreeWidgetItem *> found_items = treeServices->findItems(category, Qt::MatchExactly, 0);
            QTreeWidgetItem *top_item;
            QTreeWidgetItem *item;
            QTreeWidgetItem *parent;
            if (found_items.size() == 0) { // add top item if no top items found
                top_item = new QTreeWidgetItem(treeServices);
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
    treeServices->expandAll();
    treeServices->resizeColumnToContents(0);
    treeServices->resizeColumnToContents(1);
}

/////////////////////////////////////////////////////////////////////////
// event handlers

void MInstall::changeEvent(QEvent *event)
{
    const QEvent::Type etype = event->type();
    if (etype == QEvent::ApplicationPaletteChange
        || etype == QEvent::PaletteChange || etype == QEvent::StyleChange)
    {
        QPalette pal = qApp->palette(textHelp);
        QColor col = pal.color(QPalette::Base);
        col.setAlpha(200);
        pal.setColor(QPalette::Base, col);
        textHelp->setPalette(pal);
        setupPartitionSlider();
        resizeEvent(nullptr);
    }
}

void MInstall::resizeEvent(QResizeEvent *)
{
    textHelp->resize(tabHelp->size());
    labelHelpBackdrop->resize(textHelp->size());
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
        if (widgetStack->currentWidget() != pageEnd) {
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

void MInstall::on_splitter_splitterMoved(int, int)
{
    resizeEvent(nullptr);
}
void MInstall::on_tabsMain_currentChanged(int index)
{
    // Make the help widgets the correct size.
    if (index == 0) resizeEvent(nullptr);
}

void MInstall::diskPassValidationChanged(bool valid)
{
    pushNext->setEnabled(valid);
}
void MInstall::userPassValidationChanged()
{
    bool ok = !textUserName->text().isEmpty();
    if (ok) ok = textUserPass->isValid() || textUserName->text().isEmpty();
    if (ok && boxRootAccount->isChecked()) {
        ok = textRootPass->isValid() || textRootPass->text().isEmpty();
    }
    pushNext->setEnabled(ok);
}

void MInstall::on_pushNext_clicked()
{
    gotoPage(widgetStack->currentIndex() + 1);
}
void MInstall::on_pushBack_clicked()
{
    gotoPage(widgetStack->currentIndex() - 1);
}

void MInstall::on_pushAbort_clicked()
{
    abort(false);
    QApplication::beep();
}

// clicking advanced button to go to Services page
void MInstall::on_pushServices_clicked()
{
    gotoPage(Step::Services);
}

void MInstall::on_pushPartReload_clicked()
{
    updateCursor(Qt::WaitCursor);
    boxMain->setEnabled(false);
    updatePartitionWidgets(false);
    boxMain->setEnabled(true);
    updateCursor();
}
void MInstall::on_pushRunPartMan_clicked()
{
    updateCursor(Qt::WaitCursor);
    boxMain->setEnabled(false);
    if (QFile::exists("/usr/sbin/gparted")) proc.exec("/usr/sbin/gparted", true);
    else proc.exec("/usr/bin/partitionmanager", true);
    updatePartitionWidgets(false);
    boxMain->setEnabled(true);
    updateCursor();
}

bool MInstall::abort(bool onclose)
{
    proc.log(__PRETTY_FUNCTION__);
    boxMain->setEnabled(false);
    // ask for confirmation when installing (except for some steps that don't need confirmation)
    if (phase > 0 && phase < 4) {
        const QMessageBox::StandardButton rc = QMessageBox::warning(this,
            tr("Confirmation"), tr("The installation and configuration"
                " is incomplete.\nDo you really want to stop now?"),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (rc == QMessageBox::No) {
            boxMain->setEnabled(true);
            return false;
        }
    }
    updateCursor(Qt::WaitCursor);
    proc.halt();
    // help the installer if it was stuck at the config pages
    if (onclose) {
        phase = -2;
    } else if (phase == 2 && widgetStack->currentWidget() != pageProgress) {
        phase = -1;
        gotoPage(Step::Disk);
    } else {
        phase = -1;
    }
    if (!onclose) boxMain->setEnabled(true);
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
    if (!mountkeep) partman.unmount();
}

void MInstall::on_progInstall_valueChanged(int value)
{
    if (ixTipStart < 0 || widgetStack->currentWidget() != pageProgress)
        return; // no point displaying a new hint if it will be invisible

    const int tipcount = 6;
    ixTip = tipcount;
    if (ixTipStart < tipcount) {
        int imax = (progInstall->maximum() - iLastProgress) / (tipcount - ixTipStart);
        if (imax != 0)
            ixTip = ixTipStart + (value - iLastProgress) / imax;
    }

    switch(ixTip)
    {
    case 0:
        textTips->setText(tr("<p><b>Getting Help</b><br/>"
                             "Basic information about %1 is at %2.</p><p>"
                             "There are volunteers to help you at the %3 forum, %4</p>"
                             "<p>If you ask for help, please remember to describe your problem and your computer "
                             "in some detail. Usually statements like 'it didn't work' are not helpful.</p>").arg(PROJECTNAME).arg(PROJECTURL).arg(PROJECTSHORTNAME).arg(PROJECTFORUM));
        break;

    case 1:
        textTips->setText(tr("<p><b>Repairing Your Installation</b><br/>"
                             "If %1 stops working from the hard drive, sometimes it's possible to fix the problem by booting from LiveDVD or LiveUSB and running one of the included utilities in %1 or by using one of the regular Linux tools to repair the system.</p>"
                             "<p>You can also use your %1 LiveDVD or LiveUSB to recover data from MS-Windows systems!</p>").arg(PROJECTNAME));
        break;

    case 2:
        textTips->setText(tr("<p><b>Support %1</b><br/>"
                             "%1 is supported by people like you. Some help others at the "
                             "support forum - %2 - or translate help files into different "
                             "languages, or make suggestions, write documentation, or help test new software.</p>").arg(PROJECTNAME).arg(PROJECTFORUM));

        break;

    case 3:
        textTips->setText(tr("<p><b>Adjusting Your Sound Mixer</b><br/>"
                             " %1 attempts to configure the sound mixer for you but sometimes it will be "
                             "necessary for you to turn up volumes and unmute channels in the mixer "
                             "in order to hear sound.</p> "
                             "<p>The mixer shortcut is located in the menu. Click on it to open the mixer. </p>").arg(PROJECTNAME));
        break;

    case 4:
        textTips->setText(tr("<p><b>Keep Your Copy of %1 up-to-date</b><br/>"
                             "For more information and updates please visit</p><p> %2</p>").arg(PROJECTNAME).arg(PROJECTFORUM));
        break;

    default:
        textTips->setText(tr("<p><b>Special Thanks</b><br/>Thanks to everyone who has chosen to support %1 with their time, money, suggestions, work, praise, ideas, promotion, and/or encouragement.</p>"
                             "<p>Without you there would be no %1.</p>"
                             "<p>%2 Dev Team</p>").arg(PROJECTNAME).arg(PROJECTSHORTNAME));
        break;
    }
}

void MInstall::on_pushClose_clicked()
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
            if (line.startsWith("XKBMODEL")) plabel = labelKeyboardModel;
            else if (line.startsWith("XKBLAYOUT")) plabel = labelKeyboardLayout;
            else if (line.startsWith("XKBVARIANT")) plabel = labelKeyboardVariant;
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

void MInstall::on_pushSetKeyboard_clicked()
{
    hide();
    if (proc.exec("command -v  system-keyboard-qt >/dev/null 2>&1", false))
        proc.exec("system-keyboard-qt", false);
    else
        proc.exec("env GTK_THEME='Adwaita' fskbsetting", false);
    show();
    setupkeyboardbutton();
}

void MInstall::on_comboDisk_currentIndexChanged(int)
{
    on_sliderPart_valueChanged(sliderPart->value());
}

void MInstall::on_sliderPart_sliderPressed()
{
    QString tipText(tr("%1% root\n%2% home"));
    const int val = sliderPart->value();
    if (val==100) tipText = tr("Combined root and home");
    else if (val<1) tipText = tipText.arg(">0", "<100");
    else tipText = tipText.arg(val).arg(100-val);
    sliderPart->setToolTip(tipText);
    if (sliderPart->isSliderDown()) QToolTip::showText(QCursor::pos(), tipText, sliderPart);
}
void MInstall::on_sliderPart_valueChanged(int value)
{
    const bool crypto = boxEncryptAuto->isChecked();
    DeviceItem *drvitem = partman.selectedDriveAuto();
    if (!drvitem) return;
    long long available = drvitem->layoutDefault(100, crypto, false);
    if (!available) return;
    const long long roundUp = available - 1;
    const long long rootMinMB = (partman.rootSpaceNeeded + 1048575) / 1048576;
    const long long homeMinMB = 16; // 16MB for basic profile and setup files
    const long long rootRecMB = (4 * rootMinMB) + ROOT_BUFFER; // (Root + snapshot [squashfs + ISO] + backup) + buffer
    const long long homeRecMB = homeMinMB + HOME_BUFFER;
    const int minPercent = ((rootMinMB * 100) + roundUp) / available;
    const int maxPercent = 100 - (((homeMinMB * 100) + roundUp) / available);
    const int recPercentMin = ((rootRecMB * 100) + roundUp) / available; // Recommended root size.
    const int recPercentMax = 100 - (((homeRecMB * 100) + roundUp) / available); // Recommended minimum home.

    const int origValue = value;
    if (value < minPercent) value = minPercent;
    else if (value > maxPercent) value = 100;
    if (value != origValue) {
        qApp->beep();
        sliderPart->blockSignals(true);
        sliderPart->setValue(value);
        sliderPart->blockSignals(false);
    }

    const long long availRoot = drvitem->layoutDefault(value, crypto, false);
    QString valstr = sliderSizeString(availRoot*1048576);
    available -= availRoot;
    labelSliderRoot->setText(valstr + "\n" + tr("Root"));

    QPalette palRoot = QApplication::palette();
    QPalette palHome = QApplication::palette();
    if (value < recPercentMin) palRoot.setColor(QPalette::WindowText, Qt::red);
    if (value==100) valstr = tr("----");
    else {
        valstr = sliderSizeString(available*1048576);
        valstr += "\n" + tr("Home");
        if (value > recPercentMax) palHome.setColor(QPalette::WindowText, Qt::red);
    }
    labelSliderHome->setText(valstr);
    labelSliderRoot->setPalette(palRoot);
    labelSliderHome->setPalette(palHome);
    on_sliderPart_sliderPressed(); // For the tool tip.
}

void MInstall::on_boxEncryptAuto_toggled(bool checked)
{
    pushNext->setEnabled(!checked || textCryptoPass->isValid());
    radioBootPBR->setDisabled(checked);
    // Account for addition/removal of the boot partition.
    on_sliderPart_valueChanged(sliderPart->value());
}

void MInstall::on_radioCustomPart_toggled(bool checked)
{
    if (checked) pushNext->setEnabled(true);
    else on_boxEncryptAuto_toggled(boxEncryptAuto->isChecked());
}

void MInstall::on_radioBootMBR_toggled()
{
    comboBoot->clear();
    for (DeviceItemIterator it(partman); DeviceItem *item = *it; it.next()) {
        if (item->type == DeviceItem::Drive && (!item->flags.bootRoot || INSTALL_FROM_ROOT_DEVICE)) {
            if (!item->flags.nasty || brave) item->addToCombo(comboBoot, true);
        }
    }
    labelBoot->setText(tr("System boot disk:"));
}

void MInstall::on_radioBootPBR_toggled()
{
    comboBoot->clear();
    for (DeviceItemIterator it(partman); DeviceItem *item = *it; it.next()) {
        if (item->type == DeviceItem::Partition && (!item->flags.bootRoot || INSTALL_FROM_ROOT_DEVICE)) {
            if (item->flags.curESP || item->realUseFor() == "ESP") continue;
            else if (!item->format.compare("SWAP", Qt::CaseInsensitive)) continue;
            else if (item->format == "crypto_LUKS") continue;
            else if (item->format.isEmpty() || item->format == "PRESERVE") {
                if (item->curFormat == "crypto_LUKS") continue;
                if (!item->curFormat.compare("SWAP", Qt::CaseInsensitive)) continue;
            }
            if (!item->flags.nasty || brave) item->addToCombo(comboBoot, true);
        }
    }
    labelBoot->setText(tr("Partition to use:"));
}

void MInstall::on_radioBootESP_toggled()
{
    comboBoot->clear();
    for (DeviceItemIterator it(partman); DeviceItem *item = *it; it.next()) {
        if ((item->flags.curESP || item->realUseFor() == "ESP")
            && (!item->flags.bootRoot || INSTALL_FROM_ROOT_DEVICE)) item->addToCombo(comboBoot);
    }
    labelBoot->setText(tr("Partition to use:"));
}

// build ESP list available to install GRUB
void MInstall::buildBootLists()
{
    // refresh lists and enable or disable options according to device presence
    on_radioBootMBR_toggled();
    const bool canMBR = (comboBoot->count() > 0);
    radioBootMBR->setEnabled(canMBR);
    on_radioBootPBR_toggled();
    const bool canPBR = (comboBoot->count() > 0);
    radioBootPBR->setEnabled(canPBR);
    on_radioBootESP_toggled();
    const bool canESP = (uefi && comboBoot->count() > 0);
    radioBootESP->setEnabled(canESP);

    // load one as the default in preferential order: ESP, MBR, PBR
    if (canESP) radioBootESP->setChecked(true);
    else if (canMBR) {
        on_radioBootMBR_toggled();
        radioBootMBR->setChecked(true);
    } else if (canPBR) {
        on_radioBootPBR_toggled();
        radioBootPBR->setChecked(true);
    }
}

void MInstall::on_comboLocale_currentIndexChanged(int index)
{
    // riot control
    QLocale locale(comboLocale->itemData(index).toString());
    if (locale.timeFormat().startsWith('h')) radioClock12->setChecked(true);
    else radioClock24->setChecked(true);
}

// return 0 = success, 1 = bad area, 2 = bad zone
int MInstall::selectTimeZone(const QString &zone)
{
    int index = comboTimeArea->findData(QVariant(zone.section('/', 0, 0)));
    if (index < 0) return 1;
    comboTimeArea->setCurrentIndex(index);
    qApp->processEvents();
    index = comboTimeZone->findData(QVariant(zone));
    if (index < 0) return 2;
    comboTimeZone->setCurrentIndex(index);
    return 0;
}
void MInstall::on_comboTimeArea_currentIndexChanged(int index)
{
    if (index < 0 || index >= comboTimeArea->count()) return;
    const QString &area = comboTimeArea->itemData(index).toString();
    comboTimeZone->clear();
    for (const QString &zone : listTimeZones) {
        if (zone.startsWith(area)) {
            QString text(QString(zone).section('/', 1));
            text.replace('_', ' ');
            comboTimeZone->addItem(text, QVariant(zone));
        }
    }
    comboTimeZone->model()->sort(0);
}

void MInstall::on_radioOldHomeUse_toggled(bool)
{
    pushNext->setEnabled(radioOldHomeUse->isChecked()
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

void MInstall::rsynchomefolder(QString dpath)
{
    QString cmd = ("rsync -a --info=name1 /home/demo/ %1"
                  " --exclude '.cache' --exclude '.gvfs' --exclude '.dbus' --exclude '.Xauthority' --exclude '.ICEauthority'"
                  " --exclude '.mozilla' --exclude 'Installer.desktop' --exclude 'minstall.desktop' --exclude 'Desktop/antixsources.desktop'"
                  " --exclude '.idesktop/gazelle.lnk' --exclude '.jwm/menu' --exclude '.icewm/menu' --exclude '.fluxbox/menu'"
                  " --exclude '.config/rox.sourceforge.net/ROX-Filer/pb_antiX-fluxbox'"
                  " --exclude '.config/rox.sourceforge.net/ROX-Filer/pb_antiX-icewm'"
                  " --exclude '.config/rox.sourceforge.net/ROX-Filer/pb_antiX-jwm'"
                  " --exclude '.config/session' | xargs -I '$' sed -i 's|home/demo|home/" + textUserName->text() + "|g' %1/$").arg(dpath);
    proc.exec(cmd);
}

void MInstall::changeRemasterdemoToNewUser(QString dpath)
{
    QString cmd = ("find " + dpath + " -maxdepth 1 -type f -name '.*' -print0 | xargs -0 sed -i 's|home/demo|home/" + textUserName->text() + "|g'").arg(dpath);
    proc.exec(cmd);
    cmd = ("find " + dpath + "/.config -type f -print0 | xargs -0 sed -i 's|home/demo|home/" + textUserName->text() + "|g'").arg(dpath);
    proc.exec(cmd);
    cmd = ("find " + dpath + "/.local -type f  -print0 | xargs -0 sed -i 's|home/demo|home/" + textUserName->text() + "|g'").arg(dpath);
    proc.exec(cmd);
}

void MInstall::resetBlueman()
{
    proc.exec("runuser -l demo -c 'dconf reset /org/blueman/transfer/shared-path'"); //reset blueman path
}
