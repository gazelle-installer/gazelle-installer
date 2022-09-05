/****************************************************************************
 *  Copyright (C) 2003-2010 by Warren Woodford
 *  Heavily edited, with permision, by anticapitalista for antiX 2011-2014.
 *  Heavily revised by dolphin oracle, adrian, and anticaptialista 2018.
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
#include <QTimer>
#include <QToolTip>
#include <QPainter>
#include <QDir>

#include <cstdlib>
#include <fcntl.h>
#include <sys/stat.h>

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

MInstall::MInstall(QSettings &acfg, const QCommandLineParser &args, const QString &cfgfile)
    : proc(this), appConf(acfg), appArgs(args), helpBackdrop("/usr/share/gazelle-installer-data/backdrop-textbox.png")
{
    setupUi(this);
    listLog->addItem("Version " VERSION);
    proc.setupUI(listLog, progInstall);
    updateCursor(Qt::WaitCursor);
    setWindowFlags(Qt::Window); // for the close, min and max buttons
    textHelp->installEventFilter(this);
    boxInstall->hide();

    modeOOBE = args.isSet("oobe");
    pretend = args.isSet("pretend");
    if (!modeOOBE) {
        automatic = args.isSet("auto");
        oem = args.isSet("oem");
        mountkeep = args.isSet("mount-keep");
    } else {
        automatic = oem = false;
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

    // setup system variables
    PROJECTNAME = appConf.value("PROJECT_NAME").toString();
    PROJECTSHORTNAME = appConf.value("PROJECT_SHORTNAME").toString();
    PROJECTVERSION = appConf.value("VERSION").toString();
    PROJECTURL = appConf.value("PROJECT_URL").toString();
    PROJECTFORUM = appConf.value("FORUM_URL").toString();

    gotoPage(Step::Splash);

    // config file
    config = new MSettings(cfgfile, this);

    // ensure the help widgets are displayed correctly when started
    // Qt will delete the heap-allocated event object when posted
    qApp->postEvent(this, new QEvent(QEvent::PaletteChange));
    QTimer::singleShot(0, this, [this]() {
        try {
            startup();
        } catch (const char *msg) {
            proc.log(QStringLiteral("FAILED START - ") + msg, MProcess::Fail);
            QMessageBox::critical(this, windowTitle(), tr(msg));
            exit(EXIT_FAILURE);
        }
    });
}

MInstall::~MInstall() {
    if (oobe) delete oobe;
    if (base) delete base;
    if (bootman) delete bootman;
    if (partman) delete partman;
    if (throbber) delete throbber;
}

// meant to be run after the installer becomes visible
void MInstall::startup()
{
    proc.log(__PRETTY_FUNCTION__);

    if (!modeOOBE) {
        // Check for a bad combination, like 32-bit ISO and 64-bit UEFI.
        if (proc.detectEFI(true)==64 && proc.detectArch()=="i686") {
            const int ans = QMessageBox::question(this, windowTitle(),
                tr("You are running 32bit OS started in 64 bit UEFI mode, the system will not"
                    " be able to boot unless you select Legacy Boot or similar at restart.\n"
                    "We recommend you quit now and restart in Legacy Boot\n\n"
                    "Do you want to continue the installation?"), QMessageBox::Yes, QMessageBox::No);
            if (ans != QMessageBox::Yes) exit(EXIT_FAILURE);
        }

        partman = new PartMan(proc, *this, appConf, appArgs);
        base = new Base(proc, *partman, *this, appConf, appArgs);
        base->scanMedia();
        bootman = new BootMan(proc, *partman, *this, appConf, appArgs);

        ROOT_BUFFER = appConf.value("ROOT_BUFFER", 5000).toInt();
        HOME_BUFFER = appConf.value("HOME_BUFFER", 2000).toInt();
        INSTALL_FROM_ROOT_DEVICE = appConf.value("INSTALL_FROM_ROOT_DEVICE").toBool();

        // Unable to get space requirements since there's no media.
        if (!pretend && partman->bootSpaceNeeded==0) {
            throw QT_TR_NOOP("Cannot access installation media.");
        }

        autoMountEnabled = true; // disable auto mount by force
        if (!pretend) setupAutoMount(false);

        // Link block
        QString link_block;
        appConf.beginGroup("LINKS");
        QStringList links = appConf.childKeys();
        for (const QString &link : links) {
            link_block += "\n\n" + tr(link.toUtf8().constData()) + ": " + appConf.value(link).toString();
        }
        appConf.endGroup();

        // set some distro-centric text
        textReminders->setPlainText(tr("Support %1\n\n%1 is supported by people like you. Some help others at the support forum - %2, or translate help files into different languages, or make suggestions, write documentation, or help test new software.").arg(PROJECTNAME, PROJECTFORUM)
                            + "\n" + link_block);
    }

    // Password box setup
    textCryptoPass->setup(textCryptoPass2, progCryptoPassMeter, 1, 32, 9);
    textCryptoPassCust->setup(textCryptoPassCust2, progCryptoPassMeterCust, 1, 32, 9);
    connect(textCryptoPass, &MPassEdit::validationChanged, this, &MInstall::diskPassValidationChanged);
    connect(textCryptoPassCust, &MPassEdit::validationChanged, this, &MInstall::diskPassValidationChanged);

    setupkeyboardbutton();

    if (proc.detectMac()) checkLocalClock->setChecked(true);

    oobe = new Oobe(proc, *this, this, appConf, oem, modeOOBE);

    if (modeOOBE) manageConfig(ConfigLoadB);
    else {
        // Build disk widgets
        partman->scan();
        comboDisk->clear();
        for (DeviceItemIterator it(*partman); DeviceItem *item = *it; it.next()) {
            if (item->type == DeviceItem::Drive && item->size >= partman->rootSpaceNeeded
                    && (!item->flags.bootRoot || INSTALL_FROM_ROOT_DEVICE)) {
                item->addToCombo(comboDisk);
            }
        }
        if (comboDisk->count() >= 1) {
            comboDisk->setCurrentIndex(0);
            radioEntireDisk->setChecked(true);
            for (DeviceItemIterator it(*partman); *it; it.next()) {
                if ((*it)->isVolume()) {
                    // found at least one partition
                    radioCustomPart->setChecked(true);
                    break;
                }
            }
        } else {
            radioEntireDisk->setEnabled(false);
            boxAutoPart->setEnabled(false);
            radioCustomPart->setChecked(true);
        }
        setupPartitionSlider();
        // Override with whatever is in the config.
        manageConfig(ConfigLoadA);
    }
    oobe->stashServices(true);

    if (modeOOBE) gotoPage(Step::Network);
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

void MInstall::splashSetThrobber(bool active)
{
    if (active) {
        if (throbber) return;
        labelSplash->installEventFilter(this);
        throbber = new QTimer(this);
        connect(throbber, &QTimer::timeout, this, &MInstall::splashThrob);
        throbber->start(100);
    } else {
        if (!throbber) return;
        delete throbber;
        throbber = nullptr;
        labelSplash->removeEventFilter(this);
    }
    labelSplash->update();
}
void MInstall::splashThrob()
{
    ++throbPos;
    labelSplash->update();
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
    if (proc.shell("ps -e | grep 'udisksd'")) udisksd_running = true;
    // create a list of rules files that are being temporarily overridden
    QStringList udev_temp_mdadm_rules;
    finfo.setFile("/run/udev");
    if (finfo.isDir()) {
        proc.shell("egrep -l '^[^#].*mdadm (-I|--incremental)' /lib/udev/rules.d", nullptr, true);
        udev_temp_mdadm_rules = proc.readOutLines();
        for (QString &rule : udev_temp_mdadm_rules) {
            rule.replace("/lib/udev", "/run/udev");
        }
    }

    // auto-mount setup
    if (!enabled) {
        // disable auto-mount
        if (have_sysctl) {
            // Use systemctl to prevent automount by masking currently unmasked mount points
            proc.shell("systemctl list-units --full --all -t mount --no-legend 2>/dev/null"
                " | grep -v masked | cut -f1 -d' ' | egrep -v '^(dev-hugepages|dev-mqueue|proc-sys-fs-binfmt_misc"
                    "|run-user-.*-gvfs|sys-fs-fuse-connections|sys-kernel-config|sys-kernel-debug)'", nullptr, true);
            const QStringList &maskedMounts = proc.readOutLines();
            if (!maskedMounts.isEmpty()) {
                proc.exec("systemctl", QStringList({"--runtime", "mask", "--quiet", "--"}) + maskedMounts);
            }
        }
        // create temporary blank overrides for all udev rules which
        // automatically start Linux Software RAID array members
        proc.mkpath("/run/udev/rules.d");
        for (const QString &rule : qAsConst(udev_temp_mdadm_rules)) {
            proc.exec("touch", {rule});
        }

        if (udisksd_running) {
            proc.shell("echo 'SUBSYSTEM==\"block\", ENV{UDISKS_IGNORE}=\"1\"' > /run/udev/rules.d/91-mx-udisks-inhibit.rules");
            proc.exec("udevadm", {"control", "--reload"});
            proc.exec("udevadm", {"trigger", "--subsystem-match=block"});
        }
    } else {
        // enable auto-mount
        if (udisksd_running) {
            proc.exec("rm", {"-f", "/run/udev/rules.d/91-mx-udisks-inhibit.rules"});
            proc.exec("udevadm", {"control", "--reload"});
            proc.exec("partprobe", {"-s"});
            proc.sleep(1000);
        }
        // clear the rules that were temporarily overridden
        for (const QString &rule : qAsConst(udev_temp_mdadm_rules)) {
            proc.shell("rm -f " + rule); // TODO: check if each rule is a single file name.
        }

        // Use systemctl to restore that status of any mount points changed above
        if (have_sysctl && !listMaskedMounts.isEmpty()) {
            proc.shell("systemctl --runtime unmask --quiet -- $MOUNTLIST");
        }
    }
    autoMountEnabled = enabled;
}

/////////////////////////////////////////////////////////////////////////
// util functions

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
        while (qApp->overrideCursor() != nullptr) {
            qApp->restoreOverrideCursor();
        }
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
        if (proc.halted()) return false;
    }
    return true;
}

// process the next phase of installation if possible
bool MInstall::processNextPhase()
{
    try {
        widgetStack->setEnabled(true);
        // Phase < 0 = install has been aborted (Phase -2 on close)
        if (proc.halted()) return false;
        // Phase 0 = install not started yet, Phase 1 = install in progress
        // Phase 2 = waiting for operator input, Phase 3 = post-install steps
        if (phase == 0) { // no install started yet
            proc.advance(-1, -1);
            proc.status(tr("Preparing to install %1").arg(PROJECTNAME));
            if (!partman->checkTargetDrivesOK()) return false;
            phase = 1; // installation.

            // cleanup previous mounts
            cleanup(false);

            // the core of the installation
            if (!pretend) {
                proc.advance(12, partman->countPrepSteps());
                partman->preparePartitions();
                partman->formatPartitions();
                partman->mountPartitions();
                //run blkid -c /dev/null to freshen UUID cache
                proc.exec("blkid", {"-c", "/dev/null"});
                base->install();
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
                if (oem) oobe->enable();
                oobe->process();
                manageConfig(ConfigSave);
                config->dumpDebug();
                proc.exec("/bin/sync"); // the sync(2) system call will block the GUI
                bootman->install();
                if (oem) proc.shell("sed -i 's/splash\\b/nosplash/g' /mnt/antiX/boot/grub/grub.cfg");
            } else if (!pretendToInstall(5, 100)) {
                return false;
            }
            phase = 4;
            proc.advance(1, 1);
            proc.status(tr("Cleaning up"));
            cleanup();
            proc.status(tr("Finished"));
            if (!pretend && appArgs.isSet("reboot")) {
                proc.shell("/usr/local/bin/persist-config --shutdown --command reboot &");
            }
            if (!pretend && appArgs.isSet("poweroff")) {
                proc.shell("/usr/local/bin/persist-config --shutdown --command poweroff &");
            }
            gotoPage(Step::End);
        }
    } catch (const char *msg) {
        proc.log("FAILED Phase " + QString::number(phase) + " - " + msg, MProcess::Fail);
        if (!proc.halted()) {
            boxMain->setEnabled(false);
            QMessageBox::critical(this, windowTitle(), tr(msg));
            updateCursor(Qt::WaitCursor);
        }
        return false;
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
    if ((mode == ConfigSave || mode == ConfigLoadA) && !modeOOBE) {
        config->startGroup("Storage", pageDisk);
        const char *diskChoices[] = {"Drive", "Partitions"};
        QRadioButton *diskRadios[] = {radioEntireDisk, radioCustomPart};
        config->manageRadios("Target", 2, diskChoices, diskRadios);
        const bool targetDrive = radioEntireDisk->isChecked();
        if (targetDrive || mode!=ConfigSave) {
            config->manageComboBox("Drive", comboDisk, true);
            config->manageGroupCheckBox("DriveEncrypt", boxEncryptAuto);
            if (mode == ConfigSave) {
                config->setValue("RootPortion", sliderPart->value());
            } else if (config->contains("RootPortion")) {
                 const int sliderVal = config->value("RootPortion").toInt();
                 sliderPart->setValue(sliderVal);
                 on_sliderPart_valueChanged(sliderVal);
                 if (sliderPart->value() != sliderVal) {
                     config->markBadWidget(sliderPart);
                 }
            }
        }
        config->endGroup();
        // Custom partitions page. PartMan handles its config groups automatically.
        if (!targetDrive || mode!=ConfigSave) {
            config->setGroupWidget(pagePartitions);
            partman->manageConfig(*config, mode==ConfigSave);
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
            partman->loadKeyMaterial(config->value("KeyMaterial").toString());
        }
        config->endGroup();
    }

    if (mode == ConfigSave || mode == ConfigLoadB) {
        // GRUB page
        if (!modeOOBE && radioCustomPart->isChecked()) bootman->manageConfig(*config);
        // Manage the rest of the OOBE pages.
        oobe->manageConfig(*config, mode==ConfigSave);
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
    QString homedev = partman->getMountDev("/home", true);
    if (homedev.isEmpty() || partman->willFormat("/home")) {
        if (partman->willFormat("/")) return true;
        homedev = partman->getMountDev("/", true);
        homedir = "/home";
    }

    // Just in case the device or mount point is in use elsewhere.
    proc.exec("/usr/bin/umount", {"-q", homedev});
    proc.exec("/usr/bin/umount", {"-q", "/mnt/antiX"});

    // Store a listing of /home to compare with the user name given later.
    mkdir("/mnt/antiX", 0755);
    bool ok = proc.exec("/bin/mount", {"-o", "ro", homedev, "/mnt/antiX"});
    if (ok) {
        QDir hd("/mnt/antiX" + homedir);
        ok = hd.exists() && hd.isReadable();
        listHomes = hd.entryList(QDir::Dirs);
        proc.exec("/usr/bin/umount", {"-l", "/mnt/antiX"});
    }
    return ok;
}

// logic displaying pages
int MInstall::showPage(int curr, int next)
{
    if (next == Step::Splash) { // Enter splash screen
        splashSetThrobber(appConf.value("SPLASH_THROBBER", true).toBool());
    } else if (curr == Step::Splash) { // Leave splash screen
        splashSetThrobber(false);
    } else if (curr == Step::Disk && next > curr) {
        if (radioEntireDisk->isChecked()) {
            if (!automatic) {
                QString msg = tr("OK to format and use the entire disk (%1) for %2?");
                if (!proc.detectEFI()) {
                    DeviceItem *devit = partman->findByPath("/dev/" + comboDisk->currentData().toString());
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
            partman->clearAllUses();
            partman->selectedDriveAuto()->layoutDefault(-1, boxEncryptAuto->isChecked());
            if (!partman->composeValidate(true, PROJECTNAME)) {
                nextFocus = treePartitions;
                return curr;
            }
            bootman->buildBootLists(); // Load default boot options
            manageConfig(ConfigLoadB);
            return Step::Network;
        }
    } else if (curr == Step::Partitions && next > curr) {
        if (!partman->composeValidate(automatic, PROJECTNAME)) {
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
        nextFocus = oobe->validateUserInfo(automatic);
        if (nextFocus) return curr;
        // Check for pre-existing /home directory, see if user directory already exists.
        haveOldHome = listHomes.contains(textUserName->text());
        if (!haveOldHome) return Step::Progress; // Skip pageOldHome
        else {
            const QString &str = tr("The home directory for %1 already exists.");
            labelOldHome->setText(str.arg(textUserName->text()));
        }
    } else if (curr == Step::Network && next > curr) {
        nextFocus = oobe->validateComputerName();
        if (nextFocus) return curr;
    } else if (curr == Step::Network && next < curr) { // Backward
        return Step::Boot; // Skip pageServices
    } else if (curr == Step::Localization && next > curr) {
        if (!pretend && oobe->haveSnapshotUserAccounts) {
            return Step::Progress; // Skip pageUserAccounts and pageOldHome
        }
    } else if (curr == Step::OldHome && next < curr) { // Backward
        if (!pretend && oobe->haveSnapshotUserAccounts) {
            return Step::Localization; // Skip pageUserAccounts and pageOldHome
        }
    } else if (curr == Step::Progress && next < curr) { // Backward
        if (oem) return Step::Boot;
        if (!haveOldHome) {
            // skip pageOldHome
            if (!pretend && oobe->haveSnapshotUserAccounts) {
                return Step::Localization;
            }
            return Step::UserAccounts;
        }
    } else if (curr == Step::Services) { // Backward or forward
        oobe->stashServices(next > curr);
        return Step::Localization; // The page that called pageServices
    }
    return next;
}

void MInstall::pageDisplayed(int next)
{
    bool enableBack = true, enableNext = true;
    if (!modeOOBE) {
        const int ixProgress = widgetStack->indexOf(pageProgress);
        // progress bar shown only for install and configuration pages.
        boxInstall->setVisible(next >= widgetStack->indexOf(pageBoot) && next <= ixProgress);
        // save the last tip and stop it updating when the progress page is hidden.
        if (next != ixProgress) ixTipStart = ixTip;
    }

    QString tminroot, trecroot; // Only used for Step::Disk

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
        if (partman) {
            // These calculations are only for display text, and do not affect the installation.
            long long rootMin = partman->rootSpaceNeeded + 1048575;
            tminroot = QLocale::system().formattedDataSize(rootMin, 0, QLocale::DataSizeTraditionalFormat);
            rootMin = (4 * rootMin) + ROOT_BUFFER * 1048576; // (Root + snapshot [squashfs + ISO] + backup) + buffer.
            trecroot = QLocale::system().formattedDataSize(rootMin, 0, QLocale::DataSizeTraditionalFormat);
        }

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
        if (proc.halted()) {
            updateCursor(Qt::WaitCursor);
            phase = 0;
            proc.unhalt();
            updateCursor();
        }
        enableNext = radioCustomPart->isChecked()
            || !boxEncryptAuto->isChecked() || textCryptoPass->isValid();
        break;

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
                " When working with an existing layout, you may be able to preserve the format of the partition by selecting <b>Preserve</b>.") + "<br/>"
            + tr("Selecting <b>Preserve /home</b> for the root partition preserves the contents of the /home directory, deleting everything else."
                " This option can only be used when /home is on the same partition as the root.") + "</p>"
            "<p>" + tr("The ext2, ext3, ext4, jfs, xfs and btrfs Linux filesystems are supported and ext4 is recommended.") + "</p>"
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
            + tr("For %1, you may choose to format the partitions as ext2, ext3, ext4, f2fs, jfs, xfs or btrfs.").arg(PROJECTNAME) + "</p>"
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
        enableNext = !(boxCryptoPass->isEnabledTo(boxCryptoPass->parentWidget())) || textCryptoPassCust->isValid();
        break;

    case Step::Boot: // Start of installation.
        textHelp->setText(tr("<p><b>Select Boot Method</b><br/> %1 uses the GRUB bootloader to boot %1 and MS-Windows. "
                             "<p>By default GRUB2 is installed in the Master Boot Record (MBR) or ESP (EFI System Partition for 64-bit UEFI boot systems) of your boot drive and replaces the boot loader you were using before. This is normal.</p>"
                             "<p>If you choose to install GRUB2 to Partition Boot Record (PBR) instead, then GRUB2 will be installed at the beginning of the specified partition. This option is for experts only.</p>"
                             "<p>If you uncheck the Install GRUB box, GRUB will not be installed at this time. This option is for experts only.</p>").arg(PROJECTNAME));

        enableBack = false;
        if (phase <= 0) {
            bootman->buildBootLists();
            manageConfig(ConfigLoadB);
        }
        break;

    case Step::Services:
        textHelp->setText(tr("<p><b>Common Services to Enable</b><br/>Select any of these common services that you might need with your system configuration and the services will be started automatically when you start %1.</p>").arg(PROJECTNAME));
        break;

    case Step::Network:
        textHelp->setText(tr("<p><b>Computer Identity</b><br/>The computer name is a common unique name which will identify your computer if it is on a network. "
                             "The computer domain is unlikely to be used unless your ISP or local network requires it.</p>"
                             "<p>The computer and domain names can contain only alphanumeric characters, dots, hyphens. They cannot contain blank spaces, start or end with hyphens</p>"
                             "<p>The SaMBa Server needs to be activated if you want to use it to share some of your directories or printer "
                             "with a local computer that is running MS-Windows or Mac OSX.</p>"));
        if (modeOOBE) enableBack = false;
        else enableBack = radioCustomPart->isChecked();
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
        oobe->userPassValidationChanged();
        enableNext = false;
        break;

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
        oobe->oldHomeToggled();
        // if the Next button is disabled, avoid enabling both Back and Next at the end
        if (!pushNext->isEnabled()) {
            enableBack = true;
            enableNext = false;
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
        enableNext = false;
        break;

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

    pushBack->setEnabled(enableBack);
    pushNext->setEnabled(enableNext);
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
            proc.shell("/usr/local/bin/persist-config --shutdown --command reboot &");
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
        if (!MSettings::isBadWidget(widgetStack->currentWidget()) && next > curr) {
            pushNext->click();
        } else if (curr!=0) { // failed validation
            automatic = false;
        }
    }

    // process next installation phase
    if (next == Step::Boot || next == Step::Progress
        || (radioEntireDisk->isChecked() && next == Step::Network)) {
        if (modeOOBE) {
            updateCursor(Qt::BusyCursor);
            labelSplash->setText(tr("Configuring sytem. Please wait."));
            gotoPage(Step::Splash);
            try {
                oobe->process();
                labelSplash->setText(tr("Configuration complete. Restarting system."));
                proc.exec("/usr/sbin/reboot");
                qApp->exit(EXIT_SUCCESS);
            } catch (const char *msg) {
                splashSetThrobber(false);
                proc.log(QString("FAILED OOBE - ") + msg, MProcess::Fail);
                labelSplash->setText(tr(msg));
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

/////////////////////////////////////////////////////////////////////////
// event handlers

bool MInstall::eventFilter(QObject *watched, QEvent *event)
{
    if (event->type() != QEvent::Paint) return false;
    else if (watched == labelSplash) {
        // Setup needed to draw the load indicator.
        QPainter painter(labelSplash);
        painter.setRenderHints(QPainter::Antialiasing);
        const int lW = labelSplash->width(), lH = labelSplash->height();
        painter.translate(lW / 2, lH / 2);
        painter.scale(lW / 200.0, lH / 200.0);
        QColor color = labelSplash->palette().text().color();
        color.setRed(255 - color.red());
        color.setAlpha(70);
        QPen pen(color.darker());
        pen.setWidth(3);
        pen.setJoinStyle(Qt::MiterJoin);
        painter.setPen(pen);
        // Draw the load indicator on the splash screen.
        const int count = 16;
        const int alphaMin = 0, alphaMax = 70;
        const QPoint blade[] = {
            QPoint(0, -6), QPoint(9, -75),
            QPoint(0, -93), QPoint(-9, -75)
        };
        const qreal angle = 360.0 / count;
        painter.rotate(angle * throbPos);
        const int astep = (alphaMax - alphaMin) / count;
        for (int ixi = alphaMin; ixi <= alphaMax; ixi += astep) {
            color.setAlpha(ixi);
            painter.setBrush(color);
            painter.drawConvexPolygon(blade, 4);
            painter.rotate(angle);
        }
    } else if (watched == textHelp) {
        // Draw the help pane backdrop image, which goes through the alpha-channel background.
        QPainter painter(textHelp);
        painter.setRenderHints(QPainter::Antialiasing);
        painter.drawPixmap(textHelp->viewport()->rect(), helpBackdrop, helpBackdrop.rect());
    }
    return false;
}

void MInstall::changeEvent(QEvent *event)
{
    const QEvent::Type etype = event->type();
    if (etype == QEvent::ApplicationPaletteChange || etype == QEvent::PaletteChange || etype == QEvent::StyleChange) {
        QPalette pal = qApp->palette(textHelp);
        QColor col = pal.color(QPalette::Base);
        col.setAlpha(200);
        pal.setColor(QPalette::Base, col);
        textHelp->setPalette(pal);
        setupPartitionSlider();
    }
}

void MInstall::closeEvent(QCloseEvent *event)
{
    if (abortUI()) {
        event->accept();
        phase = -2;
        if (base) base->haltCheck(true);
        if (!modeOOBE) cleanup();
        else if (!pretend) {
            proc.unhalt();
            proc.exec("/usr/sbin/shutdown", {"-hP", "now"});
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

// Override QDialog::reject() so Escape won't close the window.
void MInstall::reject()
{
    if (base) base->haltCheck(false);
}

/////////////////////////////////////////////////////////////////////////
// slots

void MInstall::diskPassValidationChanged(bool valid)
{
    pushNext->setEnabled(valid);
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
    if(abortUI()) {
        if (!modeOOBE) cleanup();
        phase = -1;
        gotoPage(Step::Terms);
    }
    QApplication::beep();
    boxMain->setEnabled(true);
    updateCursor();
}

// clicking advanced button to go to Services page
void MInstall::on_pushServices_clicked()
{
    gotoPage(Step::Services);
}

bool MInstall::abortUI()
{
    proc.log(__PRETTY_FUNCTION__);
    boxMain->setEnabled(false);
    // ask for confirmation when installing (except for some steps that don't need confirmation)
    if (phase > 0 && phase < 4) {
        const QMessageBox::StandardButton rc = QMessageBox::warning(this, QString(),
            tr("The installation and configuration is incomplete.\nDo you really want to stop now?"),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (rc == QMessageBox::No) {
            boxMain->setEnabled(true);
            return false;
        }
    }
    updateCursor(Qt::WaitCursor);
    proc.log("Manual abort", MProcess::Fail);
    proc.halt();
    return true;
}

// run before closing the app, do some cleanup
void MInstall::cleanup(bool endclean)
{
    proc.log(__PRETTY_FUNCTION__);
    if (pretend) return;

    if (endclean) {
        proc.unhalt();
        setupAutoMount(true);
        proc.exec("/usr/bin/cp", {"/var/log/minstall.log", "/mnt/antiX/var/log"});
        proc.exec("/usr/bin/rm", {"-rf", "/mnt/antiX/mnt/antiX"});
    }
    proc.exec("/usr/bin/umount", {"-lq", "/mnt/antiX/boot/efi"});
    proc.exec("/usr/bin/umount", {"-lq", "/mnt/antiX/proc"});
    proc.exec("/usr/bin/umount", {"-lq", "/mnt/antiX/sys"});
    proc.exec("/usr/bin/umount", {"-lq", "/mnt/antiX/dev/shm"});
    proc.exec("/usr/bin/umount", {"-lq", "/mnt/antiX/dev"});
    if (endclean && !mountkeep) partman->unmount();
}

void MInstall::on_progInstall_valueChanged(int value)
{
    if (ixTipStart < 0 || widgetStack->currentWidget() != pageProgress) {
        return; // no point displaying a new hint if it will be invisible
    }

    const int tipcount = 6;
    ixTip = tipcount;
    if (ixTipStart < tipcount) {
        int imax = (progInstall->maximum() - iLastProgress) / (tipcount - ixTipStart);
        if (imax != 0) {
            ixTip = ixTipStart + (value - iLastProgress) / imax;
        }
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
    QFile file("/etc/default/keyboard");
    if (!file.open(QFile::ReadOnly | QFile::Text)) return;
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

void MInstall::on_pushSetKeyboard_clicked()
{
    hide();
    if (proc.shell("command -v  system-keyboard-qt >/dev/null 2>&1")) {
        proc.exec("system-keyboard-qt");
    } else {
        proc.shell("env GTK_THEME='Adwaita' fskbsetting");
    }
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
    if (sliderPart->isSliderDown()) {
        QToolTip::showText(QCursor::pos(), tipText, sliderPart);
    }
}
void MInstall::on_sliderPart_valueChanged(int value)
{
    if (!partman) return;
    const bool crypto = boxEncryptAuto->isChecked();
    DeviceItem *drvitem = partman->selectedDriveAuto();
    if (!drvitem) return;
    long long available = drvitem->layoutDefault(100, crypto, false);
    if (!available) return;
    const long long roundUp = available - 1;
    const long long rootMinMB = (partman->rootSpaceNeeded + 1048575) / 1048576;
    const long long homeMinMB = 16; // 16MB for basic profile and setup files
    const long long rootRecMB = rootMinMB + ROOT_BUFFER;
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
