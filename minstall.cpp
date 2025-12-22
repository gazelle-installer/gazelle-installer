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

#include <cstdlib>
#include <algorithm>
#include <utility>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/sysinfo.h>

#include <QCommandLineParser>
#include <QProcess>
#include <QProcessEnvironment>
#include <QFile>
#include <QFileInfo>
#include <QTimer>
#include <QToolTip>
#include <QPainter>
#include <QMessageBox>
#include <QDebug>

#include "msettings.h"
#include "checkmd5.h"
#include "partman.h"
#include "autopart.h"
#include "replacer.h"
#include "crypto.h"
#include "base.h"
#include "oobe.h"
#include "bootman.h"
#include "swapman.h"
#include "throbber.h"

#include "minstall.h"

// CODEBASE_VERSION should come from compiler flags.
#ifndef CODEBASE_VERSION
    #define CODEBASE_VERSION "0.0"
#endif

using namespace Qt::Literals::StringLiterals;

enum Step {
    SPLASH,
    TERMS,
    INSTALLATION,
    REPLACE,
    PARTITIONS,
    ENCRYPTION,
    CONFIRM,
    BOOT,
    SWAP,
    SERVICES,
    NETWORK,
    LOCALIZATION,
    USER_ACCOUNTS,
    OLD_HOME,
    TIPS,
    END
};

MInstall::MInstall(MIni &acfg, const QCommandLineParser &args, const QString &cfgfile, QWidget *parent) noexcept
    : QDialog(parent, Qt::Window), proc(this), core(proc), appConf(acfg), appArgs(args), configFile(cfgfile)
{
    appConf.setSection(u"GUI"_s);
    setWindowIcon(QIcon(appConf.getString(u"Logo"_s, u"/usr/share/gazelle-installer-data/logo.png"_s)));
    helpBackdrop = appConf.getString(u"HelpBackdrop"_s, u"/usr/share/gazelle-installer-data/backdrop-textbox.png"_s);
    appConf.setSection();
    gui.setupUi(this);
    gui.listLog->addItem(u"Version "_s + qApp->applicationVersion());
    proc.setupUI(gui.listLog, gui.progInstall);
    gui.textHelp->installEventFilter(this);
    gui.boxInstall->hide();

    advanced = args.isSet(u"advanced"_s);
    modeOOBE = args.isSet(u"oobe"_s);
    pretend = args.isSet(u"pretend"_s);
    if (!modeOOBE) {
        automatic = args.isSet(u"auto"_s);
        oem = args.isSet(u"oem"_s);
        mountkeep = args.isSet(u"mount-keep"_s);
    } else {
        automatic = oem = false;
        gui.pushClose->setText(tr("Shutdown"));
    }

    // setup system variables
    PROJECTNAME = appConf.getString(u"Name"_s);
    PROJECTSHORTNAME = appConf.getString(u"ShortName"_s);
    PROJECTVERSION = appConf.getString(u"Version"_s);
    appConf.setSection(u"Links"_s);
    PROJECTURL = appConf.getString(u"Website"_s);
    PROJECTFORUM = appConf.getString(u"Forum"_s);
    appConf.setSection();

    gotoPage(Step::SPLASH);

    // ensure the help widgets are displayed correctly when started
    // Qt will delete the heap-allocated event object when posted
    qApp->postEvent(this, new QEvent(QEvent::PaletteChange));
    QTimer::singleShot(0, this, [this]() noexcept {
        try {
            startup();
            phase = PH_READY;
        } catch (const char *msg) {
            if (checkmd5) {
                delete checkmd5;
                checkmd5 = nullptr;
            }
            proc.unhalt();
            const bool closenow = (!msg || !*msg);
            if(!closenow) {
                proc.log("FAILED START - "_L1 + msg, MProcess::LOG_FAIL);
                gui.labelSplash->setText(tr(msg));
            }
            abortEndUI(closenow);
        }
    });
}

void MInstall::runShutdown(const QString &action) noexcept
{
    if (QFile::exists(u"/usr/local/bin/persist-config"_s)) {
        proc.shell(QStringLiteral("/usr/local/bin/persist-config --shutdown --command %1 &").arg(action));
        return;
    }
    if (QFile::exists(u"/usr/bin/persist-config"_s)) {
        proc.shell(QStringLiteral("/usr/bin/persist-config --shutdown --command %1 &").arg(action));
        return;
    }
    if (QFileInfo(u"/usr/bin/systemctl"_s).isExecutable()) {
        proc.exec(u"systemctl"_s, {action});
        return;
    }

    const QString sbinPath = "/usr/sbin/"_L1 + action;
    const QString binPath = "/usr/bin/"_L1 + action;
    const QString &fallback = QFileInfo(sbinPath).isExecutable() ? sbinPath : binPath;
    proc.exec(fallback);
}

MInstall::~MInstall() {
    if (oobe) delete oobe;
    if (base) delete base;
    if (replacer) delete replacer;
    if (checkmd5) delete checkmd5;
    if (bootman) delete bootman;
    if (swapman) delete swapman;
    if (partman) delete partman;
    if (crypto) delete crypto;
    if (autopart) delete autopart;
    if (throbber) delete throbber;
}

// meant to be run after the installer becomes visible
void MInstall::startup()
{
    proc.log(__PRETTY_FUNCTION__, MProcess::LOG_MARKER);
    connect(gui.pushClose, &QPushButton::clicked, this, &MInstall::close);
    connect(gui.progInstall, &QProgressBar::valueChanged, this, &MInstall::progressUpdate);
    // Lambda slots
    connect(gui.pushBack, &QPushButton::clicked, this, [this]() noexcept {
        gotoPage(gui.widgetStack->currentIndex() - 1);
    });
    connect(gui.pushNext, &QPushButton::clicked, this, [this]() noexcept {
        gotoPage(gui.widgetStack->currentIndex() + 1);
    });
    connect(gui.pushAbort, &QPushButton::clicked, this, [this]() noexcept {
        abortUI(true, false);
    });
    connect(gui.pushServices, &QPushButton::clicked, this, [this]() noexcept {
        gotoPage(Step::SERVICES);
    });

    if (!modeOOBE) {
        // Check for a bad combination, like 32-bit ISO and 64-bit UEFI.
        if (core.detectEFI(true)==64 && core.detectArch()=="i686"_L1) {
            QMessageBox msgbox(this);
            msgbox.setText(tr("You are running 32-bit OS started in 64-bit UEFI mode."));
            msgbox.setInformativeText(tr("The system will not be able to boot unless you"
                " restart the system in Legacy Boot (or similar mode) before proceeding.")
                + "\n\n"_L1 + tr("Do you want to continue the installation?"));
            msgbox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
            msgbox.setDefaultButton(QMessageBox::No);
            if (msgbox.exec() != QMessageBox::Yes) {
                exit(EXIT_FAILURE);
            }
        }

        // Log live boot command line, looking for the "checkmd5" option.
        bool nocheck = pretend;
        QFile fileCLine(u"/live/config/proc-cmdline"_s);
        if (fileCLine.open(QFile::ReadOnly | QFile::Text)) {
            QByteArray data = fileCLine.readAll();
            nocheck = data.contains("checkmd5\n");
            proc.log("Live boot: " + data.simplified());
            fileCLine.close();
        }
        // Check the installation media for errors (skip if not required).
        if (appArgs.isSet(u"media-check"_s)) nocheck = false;
        else if (appArgs.isSet(u"no-media-check"_s)) nocheck = true;
        if(nocheck) {
            proc.log(u"No media check"_s);
        } else {
            checkmd5 = new CheckMD5(proc, gui.labelSplash);
        }

        crypto = new Crypto(proc, gui, this);
        partman = new PartMan(proc, core, gui, *crypto, appConf, appArgs, this);
        base = new Base(proc, core, *partman, appConf, appArgs);
        bootman = new BootMan(proc, core, *partman, gui, appConf, appArgs, this);
        swapman = new SwapMan(proc, core, *partman, gui, this);
        autopart = new AutoPart(proc, core, partman, gui, appConf, this);
        partman->autopart = autopart;
        replacer = new Replacer(proc, partman, gui, *crypto, appConf);
        //experimental tag
        gui.radioReplace->setText(gui.radioReplace->text() + " (" + tr("Experimental","As In feature is not polished and may not work properly") + ")");
        connect(gui.radioEntireDrive, &QRadioButton::toggled, gui.boxAutoPart, &QGroupBox::setEnabled);
        gui.labelConfirm->setText(tr("The %1 installer will now perform the requested actions.").arg(PROJECTNAME)
            + "<br/><img src=':/dialog-warning'/>"_L1 + tr("These actions cannot be undone. Do you want to continue?")
            + "<img src=':/dialog-warning'/>"_L1);

        // set some distro-centric text
        QString link_block;
        appConf.setSection(u"Links"_s);
        const QStringList &links = appConf.getKeys();
        for (const QString &link : links) {
            link_block += "\n\n"_L1 + tr(link.toUtf8().constData()) + ": "_L1 + appConf.getString(link);
        }
        appConf.setSection();
        gui.textReminders->setPlainText(tr("Support %1").arg(PROJECTNAME) + "\n\n"_L1
            + tr("%1 is supported by people like you. Some help others at the support forum,"
                " or translate help files into different languages, or make suggestions,"
                " write documentation, or help test new software.").arg(PROJECTNAME)
            + '\n' + link_block);
    }

    setupkeyboardbutton();
    connect(gui.pushSetKeyboard, &QPushButton::clicked, this, &MInstall::runKeyboardSetup);

    oobe = new Oobe(proc, core, gui, appConf, oem, modeOOBE, this);

    if (modeOOBE) loadConfig(2);
    else {
        // Build disk widgets
        partman->scan();
        autopart->scan();
        replacer->scan(false);
        if (gui.comboDriveSystem->count() > 0) {
            gui.comboDriveSystem->setCurrentIndex(0);
            gui.radioEntireDrive->setChecked(true);
            for (PartMan::Iterator it(*partman); *it; it.next()) {
                if ((*it)->isVolume()) {
                    // found at least one partition
                    gui.radioCustomPart->setChecked(true);
                    break;
                }
            }
        } else {
            gui.radioEntireDrive->setEnabled(false);
            gui.boxAutoPart->setEnabled(false);
            gui.radioCustomPart->setChecked(true);
        }
        // Override with whatever is in the config.
        loadConfig(1);
    }
    oobe->stashServices(true);

    gui.textCopyright->setPlainText(tr("%1 is an independent Linux distribution based on Debian Stable.\n\n"
        "%1 uses some components from MEPIS Linux which are released under an Apache free license."
        " Some MEPIS components have been modified for %1.\n\nEnjoy using %1").arg(PROJECTNAME));

    // Wait for any outstanding MD5 checks to finish.
    if (checkmd5) {
        checkmd5->wait(); // On error this throws an exception.
        delete checkmd5;
        checkmd5 = nullptr;
    }

    gotoPage(Step::TERMS);
}

// turn auto-mount off and on
void MInstall::setupAutoMount(bool enabled)
{
    proc.log(__PRETTY_FUNCTION__, MProcess::LOG_MARKER);

    if (autoMountEnabled == enabled) return;
    // check if the systemctl program is present
    bool have_sysctl = false;
    const QStringList &envpath = QProcessEnvironment::systemEnvironment().value(u"PATH"_s).split(':');
    for (const QString &path : envpath) {
        if (QFileInfo(path + "/systemctl"_L1).isExecutable()) {
            have_sysctl = true;
            break;
        }
    }
    // check if udisksd is running.
    bool udisksd_running = false;
    if (proc.shell(u"ps -e | grep 'udisksd'"_s)) udisksd_running = true;
    // create a list of rules files that are being temporarily overridden
    QStringList udev_temp_mdadm_rules;
    if (QFileInfo(u"/run/udev"_s).isDir()) {
        proc.shell(u"grep -El '^[^#].*mdadm (-I|--incremental)' /lib/udev/rules.d/*"_s, nullptr, true);
        udev_temp_mdadm_rules = proc.readOutLines();
        for (QString &rule : udev_temp_mdadm_rules) {
            rule.replace("/lib/udev"_L1, "/run/udev"_L1);
        }
    }

    // auto-mount setup
    if (!enabled) {
        // disable auto-mount
        if (have_sysctl) {
            // Use systemctl to prevent automount by masking currently unmasked mount points
            proc.shell(u"systemctl list-units --full --all -t mount --no-legend --plain 2>/dev/null"
                " | grep -v masked | cut -f1 -d' ' | grep -Ev '^(dev-hugepages|dev-mqueue|proc-sys-fs-binfmt_misc"
                    "|run-user-.*-gvfs|sys-fs-fuse-connections|sys-kernel-config|sys-kernel-debug)'"_s, nullptr, true);
            const QStringList &maskedMounts = proc.readOutLines();
            if (!maskedMounts.isEmpty()) {
                proc.exec(u"systemctl"_s, QStringList({u"--runtime"_s, u"mask"_s, u"--quiet"_s, u"--"_s}) + maskedMounts);
            }
        }
        // create temporary blank overrides for all udev rules which
        // automatically start Linux Software RAID array members
        core.mkpath(u"/run/udev/rules.d"_s);
        for (const QString &rule : std::as_const(udev_temp_mdadm_rules)) {
            proc.exec(u"touch"_s, {rule});
        }

        if (udisksd_running) {
            proc.shell(u"echo 'SUBSYSTEM==\"block\", ENV{UDISKS_IGNORE}=\"1\"' > /run/udev/rules.d/91-mx-udisks-inhibit.rules"_s);
            proc.exec(u"udevadm"_s, {u"control"_s, u"--reload"_s});
            proc.exec(u"udevadm"_s, {u"trigger"_s, u"--subsystem-match=block"_s});
        }
    } else {
        // enable auto-mount
        if (udisksd_running) {
            proc.exec(u"rm"_s, {u"-f"_s, u"/run/udev/rules.d/91-mx-udisks-inhibit.rules"_s});
            proc.exec(u"udevadm"_s, {u"control"_s, u"--reload"_s});
            // For partitions to appear in the file manager.
            proc.exec(u"udevadm"_s, {u"trigger"_s});
        }
        // clear the rules that were temporarily overridden
        for (const QString &rule : std::as_const(udev_temp_mdadm_rules)) {
            proc.shell("rm -f "_L1 + rule); // TODO: check if each rule is a single file name.
        }

        // Use systemctl to restore that status of any mount points changed above
        if (have_sysctl && !listMaskedMounts.isEmpty()) {
            proc.shell(u"systemctl --runtime unmask --quiet -- $MOUNTLIST"_s);
        }
    }
    autoMountEnabled = enabled;
}

// process the next phase of installation if possible
void MInstall::processNextPhase() noexcept
{
    try {
        gui.widgetStack->setEnabled(true);
        if (proc.halted()) throw ""; // Abortion
        if (pretend) {
            pretendNextPhase();
            return;
        }
        if (!modeOOBE && phase == PH_READY) { // no install started yet
            phase = PH_PREPARING;
            proc.advance(-1, -1);
            proc.status(tr("Preparing to install %1").arg(PROJECTNAME));

            // Start zram swap. In particular, the Argon2id KDF for LUKS uses a lot of memory.
            swapman->setupZRam();

            if (!partman->checkTargetDrivesOK()) throw "";
            autoMountEnabled = true; // disable auto mount by force
            setupAutoMount(false);

            // the core of the installation
            phase = PH_INSTALLING;
            proc.advance(11, partman->countPrepSteps());
            partman->prepStorage();
            base->install();
            if (gui.widgetStack->currentIndex() != Step::TIPS) {
                gui.progInstall->setEnabled(false);
                // Using proc.status() prepends the percentage to the text.
                gui.progInstall->setFormat(tr("Paused for required operator input"));
                proc.log(gui.progInstall->format(), MProcess::LOG_STATUS);
                QApplication::beep();
            }
            phase = PH_WAITING_FOR_INFO;
        }
        if (phase == PH_WAITING_FOR_INFO && gui.widgetStack->currentIndex() == Step::TIPS) {
            phase = PH_CONFIGURING;
            gui.progInstall->setEnabled(true);
            gui.pushBack->setEnabled(false);

            proc.advance(1, 1);
            proc.status(tr("Setting system configuration"));
            if (oem) oobe->enable();
            oobe->process();
            if (!modeOOBE) {
                saveConfig();
            }
            proc.exec(u"sync"_s); // the sync(2) system call will block the GUI
            QStringList grubextra;
            swapman->install(grubextra);
            bootman->install(grubextra);

            proc.advance(1, 1);
            proc.status(tr("Cleaning up"));
            cleanup();

            phase = PH_FINISHED;
            proc.status(tr("Finished"));
            if (appArgs.isSet(u"reboot"_s)) {
                runShutdown(u"reboot"_s);
            }
            if (appArgs.isSet(u"poweroff"_s)) {
                runShutdown(u"poweroff"_s);
            }
            gotoPage(Step::END);
        }
        // This OOBE phase is only run under --oobe mode.
        if (modeOOBE && phase == PH_READY) {
            phase = PH_OUT_OF_BOX;
            gui.labelSplash->setText(tr("Configuring system. Please wait."));
            gotoPage(Step::SPLASH);
            oobe->process();
            phase = PH_FINISHED;
            gui.labelSplash->setText(tr("Configuration complete. Restarting system."));
            const QString rebootCmd = QFileInfo(u"/usr/sbin/reboot"_s).isExecutable()
                ? u"/usr/sbin/reboot"_s : u"/usr/bin/reboot"_s;
            proc.exec(rebootCmd);
        }
    } catch (const char *msg) {
        if (!msg || !msg[0] || abortion) {
            msg = QT_TR_NOOP("The installation was aborted.");
        }
        proc.log("FAILED Phase "_L1 + QString::number(phase) + " - "_L1 + msg, MProcess::LOG_FAIL);

        const bool closing = (abortion == AB_CLOSING);
        gui.labelSplash->setText(tr(msg));
        abortUI(false, closing);
        proc.unhalt();
        if (!modeOOBE) {
            saveConfig();
            cleanup();
        }

        abortion = AB_ABORTED;
        abortEndUI(closing);
    }
}

void MInstall::pretendNextPhase() noexcept
{
    if (phase == PH_READY) {
        if (modeOOBE) phase = PH_FINISHED;
        else {
            phase = PH_INSTALLING;
            proc.status(tr("Pretending to install %1").arg(PROJECTNAME));
            phase = PH_WAITING_FOR_INFO;
        }
    }
    if (phase == PH_WAITING_FOR_INFO && gui.widgetStack->currentIndex() == Step::TIPS) {
        saveConfig();
        phase = PH_FINISHED;
        gotoPage(Step::END);
    }
}

void MInstall::loadConfig(int stage) noexcept
{
    MSettings config(configFile, false);

    if (stage == 1) {
        // Automatic or Manual partitioning
        config.setSection(u"Storage"_s, gui.pageInstallation);
        static constexpr const char *diskChoices[] = {"Drives", "Partitions"};
        QRadioButton *const diskRadios[] = {gui.radioEntireDrive, gui.radioCustomPart};
        config.manageRadios(u"Target"_s, 2, diskChoices, diskRadios);

        // Storage and partition management
        autopart->manageConfig(config);
        partman->loadConfig(config);
        crypto->manageConfig(config);
    } else if (stage == 2) {
        if (!modeOOBE) {
            bootman->manageConfig(config);
            swapman->manageConfig(config);
        }
        if (!oem) {
            oobe->manageConfig(config);
        }
    }

    if (!config.good) {
        QMessageBox msgbox(this);
        msgbox.setIcon(QMessageBox::Critical);
        msgbox.setText(tr("Invalid settings found in configuration file (%1).").arg(configFile));
        msgbox.setInformativeText(tr("Please review marked fields as you encounter them."));
        msgbox.exec();
    }
}
void MInstall::saveConfig() noexcept
{
    configFile = pretend ? "./minstall.conf"_L1 : "/mnt/antiX/etc/minstall.conf"_L1;
    QFile::rename(configFile, configFile+".bak"_L1);
    MSettings config(configFile, true);

    config.setString(u"Version"_s, u"" CODEBASE_VERSION ""_s);
    config.setString(u"Product"_s, PROJECTNAME + ' ' + PROJECTVERSION);

    // Automatic or Manual partitioning
    config.setSection(u"Storage"_s, gui.pageInstallation);
    static constexpr const char *diskChoices[] = {"Drives", "Partitions"};
    QRadioButton *const diskRadios[] = {gui.radioEntireDrive, gui.radioCustomPart};
    config.manageRadios(u"Target"_s, 2, diskChoices, diskRadios);

    // Storage and partition management
    if(gui.radioEntireDrive->isChecked()) {
        autopart->manageConfig(config);
    } else {
        partman->saveConfig(config);
    }
    crypto->manageConfig(config);

    bootman->manageConfig(config);
    swapman->manageConfig(config);
    if (!oem) {
        oobe->manageConfig(config);
    }

    config.dumpDebug();
    if (!pretend) {
        config.closeAndCopyTo(u"/etc/minstalled.conf"_s);
        chmod(configFile.toUtf8().constData(), 0600);
    }
}

// logic displaying pages
int MInstall::showPage(int curr, int next) noexcept
{
    if (next == Step::SPLASH) { // Enter splash screen
        gui.boxMain->setCursor(Qt::WaitCursor);
        appConf.setSection(u"GUI"_s);
        if (appConf.getBoolean(u"SplashThrobber"_s, true)) {
            throbber = new Throbber(gui.labelSplash);
        }
        appConf.setSection();
    } else if (curr == Step::SPLASH) { // Leave splash screen
        gui.labelSplash->clear();
        if (throbber) {
            delete throbber;
            throbber = nullptr;
        }
        gui.boxMain->unsetCursor();
    } else if (curr == Step::TERMS && next > curr) {
        if (modeOOBE) return Step::NETWORK;
    } else if (curr == Step::INSTALLATION && next > curr) {
        if (gui.radioEntireDrive->isChecked()) {
            if (!autopart->validate(automatic, PROJECTNAME)) {
                return curr;
            }
            if (gui.checkEncryptAuto->isChecked()) {
                return Step::ENCRYPTION;
            }
            return Step::CONFIRM;
        } else if (gui.radioCustomPart->isChecked()) {
            return Step::PARTITIONS;
        }
    } else if (curr == Step::REPLACE && next > curr) {
        gui.boxMain->setCursor(Qt::WaitCursor);
        if (!replacer->validate()) {
            nextFocus = gui.tableExistInst;
            gui.boxMain->unsetCursor();
            return curr;
        }
        if (!replacer->preparePartMan()) {
            gui.boxMain->unsetCursor();
            return curr;
        }
        if (!pretend && !(base && base->saveHomeBasic())) {
            gui.boxMain->unsetCursor();
            QMessageBox msgbox(this);
            msgbox.setIcon(QMessageBox::Critical);
            msgbox.setText(tr("The data in /home cannot be preserved because"
                " the required information could not be obtained."));
            msgbox.exec();
            return curr;
        }
        gui.boxMain->unsetCursor();
        return Step::CONFIRM;
    } else if (curr == Step::REPLACE && next < curr) {
        gui.boxMain->setCursor(Qt::WaitCursor);
        replacer->clean();
        gui.boxMain->unsetCursor();
    } else if (curr == Step::PARTITIONS) {
        if (next > curr) {
            if (!partman->validate(automatic)) {
                nextFocus = gui.treePartitions;
                return curr;
            }
            if (!pretend && !(base && base->saveHomeBasic())) {
                QMessageBox msgbox(this);
                msgbox.setIcon(QMessageBox::Critical);
                msgbox.setText(tr("The data in /home cannot be preserved because"
                    " the required information could not be obtained."));
                msgbox.exec();
                return curr;
            }
            for (PartMan::Iterator it(*partman); *it; it.next()) {
                if ((*it)->willEncrypt()) {
                    return Step::ENCRYPTION;
                }
            }
            return Step::CONFIRM;
        }
        return Step::INSTALLATION;
    } else if (curr == Step::ENCRYPTION && next < curr) {
        if (gui.radioEntireDrive->isChecked()) {
            return Step::INSTALLATION;
        }
        return Step::PARTITIONS;
    } else if (curr == Step::CONFIRM) {
        gui.treeConfirm->clear(); // Revisiting a step produces a fresh confirmation list.
        if (next > curr) {
            if (gui.radioEntireDrive->isChecked()) {
                if (!autopart->buildLayout()) {
                    gui.labelSplash->setText(tr("Cannot find selected drive."));
                    abortEndUI(false);
                    return Step::SPLASH;
                }
            } else if (!gui.radioReplace->isChecked()) {
                advanced = true;
            }
            next = (advanced ? Step::BOOT : Step::SWAP);
            bootman->buildBootLists();
            swapman->setupDefaults();
            loadConfig(2);
        } else {
            if (gui.radioEntireDrive->isChecked()) {
                return Step::INSTALLATION;
            } else if (gui.radioCustomPart->isChecked()) {
                return Step::PARTITIONS;
            } else if (gui.radioReplace->isChecked()) {
                return Step::REPLACE;
            }
        }
    } else if (curr == Step::BOOT) {
        return next;
    } else if (curr == Step::SWAP && next > curr) {
        return oem ? Step::TIPS : Step::NETWORK;
    } else if (curr == Step::NETWORK) {
        if(next > curr) {
            nextFocus = oobe->validateComputerName();
            if (nextFocus) return curr;
        } else { // Backward
            return modeOOBE ? Step::TERMS : Step::SWAP;
        }
    } else if (curr == Step::LOCALIZATION && next > curr) {
        if (!pretend && oobe->haveSnapshotUserAccounts) {
            return Step::TIPS; // Skip pageUserAccounts and pageOldHome
        }
    } else if (curr == Step::USER_ACCOUNTS && next > curr) {
        nextFocus = oobe->validateUserInfo(automatic);
        if (nextFocus) return curr;
        // Check for pre-existing /home directory, see if user directory already exists.
        haveOldHome = base && base->homes.contains(gui.textUserName->text());
        if (!haveOldHome) return Step::TIPS; // Skip pageOldHome
        else {
            const QString &str = tr("The home directory for %1 already exists.");
            gui.labelOldHome->setText(str.arg(gui.textUserName->text()));
        }
    } else if (curr == Step::OLD_HOME && next < curr) { // Backward
        if (!pretend && oobe->haveSnapshotUserAccounts) {
            return Step::LOCALIZATION; // Skip pageUserAccounts and pageOldHome
        }
    } else if (curr == Step::TIPS && next < curr) { // Backward
        if (oem) {
            return Step::SWAP;
        } else if (!haveOldHome) {
            // skip pageOldHome
            if (!pretend && oobe->haveSnapshotUserAccounts) {
                return Step::LOCALIZATION;
            }
            return Step::USER_ACCOUNTS;
        }
    } else if (curr == Step::SERVICES) { // Backward or forward
        oobe->stashServices(next > curr);
        return Step::LOCALIZATION; // The page that called pageServices
    }
    return next;
}

void MInstall::pageDisplayed(int next) noexcept
{
    bool enableBack = true, enableNext = true;
    if (!modeOOBE) {
        // progress bar shown only for install and configuration pages.
        gui.boxInstall->setVisible(next >= Step::BOOT && next <= Step::TIPS);
        // save the last tip and stop it updating when the progress page is hidden.
        if (next != Step::TIPS) ixTipStart = ixTip;
    }

    // This prevents the user accidentally skipping the confirmation.
    gui.pushNext->setDefault(next != Step::CONFIRM);

    switch (next) {
    case Step::SPLASH:
        if (phase > PH_READY) break;
        [[fallthrough]];
    case Step::TERMS:
        gui.textHelp->setText("<p><b>"_L1 + tr("General Instructions") + "</b><br/>"_L1
            + (modeOOBE ? QString() : (tr("BEFORE PROCEEDING, CLOSE ALL OTHER APPLICATIONS.") + "</p><p>"_L1))
            + tr("On each page, please read the instructions, make your selections, and then click on Next when you are ready to proceed."
                " You will be prompted for confirmation before any destructive actions are performed.") + "</p>"_L1
            + "<p><b>"_L1 + tr("Limitations") + "</b><br/>"_L1
            + tr("Remember, this software is provided AS-IS with no warranty what-so-ever."
                " It is solely your responsibility to backup your data before proceeding.") + "</p>"_L1);
        break;
    case Step::INSTALLATION:
        gui.textHelp->setText("<p><b>"_L1 + tr("Installation Options") + "</b><br/>"_L1
            + tr("If you are running Mac OS or Windows OS (from Vista onwards), you may have to use that system's software to set up partitions and boot manager before installing.") + "</p>"_L1
            "<p><b>"_L1 + tr("Dual drive") + "</b><br/>"_L1
            + tr("If your system has multiple storage drives, this option allows you to have the system files on one drive (the System drive),"
                " while keeping the data of all users on a separate drive (the Home drive).") + "</p>"_L1
            "<p><b>"_L1 + tr("Using the root-home space slider") + "</b><br/>"_L1
            + tr("The drive can be divided into separate system (root) and user data (home) partitions using the slider.") + "</p>"_L1
            "<p>"_L1 + tr("The <b>root</b> partition will contain the operating system and applications.") + "<br/>"_L1
            + tr("The <b>home</b> partition will contain the data of all users, such as their settings, files, documents, pictures, music, videos, etc.") + "</p>"_L1
            "<p>"_L1 + tr("Move the slider to the right to increase the space for <b>root</b>. Move it to the left to increase the space for <b>home</b>.") + "<br/>"_L1
            + tr("Move the slider all the way to the right if you want both root and home on the same partition.") + "</p>"_L1
            "<p>"_L1 + tr("Keeping the home directory in a separate partition improves the reliability of operating system upgrades. It also makes backing up and recovery easier."
                " This can also improve overall performance by constraining the system files to a defined portion of the drive.") + "</p>"_L1
            "<p><b>"_L1 + tr("Encryption") + "</b><br/>"_L1
            + tr("Encryption is possible via LUKS. A password is required.") + "</p>"_L1
            "<p>"_L1 + tr("A separate unencrypted boot partition is required.") + "</p>"_L1
            "<p>"_L1 + tr("When encryption is used with autoinstall, the separate boot partition will be automatically created.") + "</p>"_L1
            "<p><b>"_L1 + tr("Using a custom disk layout") + "</b><br/>"_L1
            + tr("If you need more control over where %1 is installed to, select \"<b>%2</b>\" and click <b>Next</b>."
                " On the next page, you will then be able to select and configure the storage devices and"
                " partitions you need.").arg(PROJECTNAME, gui.radioCustomPart->text().remove('&')) + "</p>"_L1
            "<p><b>"_L1 + tr("Replace existing installation") + "</b><br/>"_L1 + tr("Replace existing installation option will attempt to replace an existing installation with the same "
                "disk configuration as the existing installation.  Home directories are preserved."));
        break;

    case Step::REPLACE:
        gui.textHelp->setText("<p><b>"_L1 + tr("Replace existing installation") + "</b><br/>"_L1
            + tr("If you have an existing installation, you can use this function to replace it with a fresh installation.") + "</p>"_L1
            "<p>"_L1 + tr("This is particularly useful if you are upgrading from a previous version and want to preserve your data.") + "</p>"_L1
            "<p><b>"_L1 + tr("Warning") + "</b><br/>"_L1
            + tr("There is no guarantee of this working successfully. Ensure you have a good working backup of all important data before continuing.") + "</p>"_L1
            "<p>"_L1 + tr("This feature is designed to replace an installation performed using the regular install method,"
                " and may fail to replace an installation with a complex layout or storage scheme. Corruption or data loss may occur.") + "<br/>"_L1
            + tr("To replace an installation with a complex layout or storage scheme, it is recommended to use the custom layout option instead.") + "</p>"_L1);

        if(gui.tableExistInst->rowCount() <= 0) {
            QTimer::singleShot(0, gui.pushReplaceScan, &QPushButton::click);
        }
        break;

    case Step::PARTITIONS:
        gui.textHelp->setText("<p><b>"_L1 + tr("Choose Partitions") + "</b><br/>"_L1
            + tr("The partition list allows you to choose what partitions are used for this installation.") + "</p>"_L1
            "<p>"_L1 + tr("<i>Device</i> - This is the block device name that is, or will be, assigned to the created partition.") + "</p>"_L1
            "<p>"_L1 + tr("<i>Size</i> - The size of the partition. This can only be changed on a new layout.") + "</p>"_L1
            "<p>"_L1 + tr("<i>Use For</i> - To use this partition in an installation, you must select something here.")
            + "<table border='1'>"_L1
            "<tr><td>FORMAT</td><td>"_L1 + tr("Format without mounting") + "</td></tr>"_L1
            "<tr><td>BIOS-GRUB</td><td>"_L1 + tr("BIOS Boot GPT partition for GRUB") + "</td></tr>"_L1
            "<tr><td>ESP</td><td rowspan='2'>"_L1 + tr("EFI System Partition") + "</td></tr>"_L1
            "<tr><td>/boot/efi</td></tr>"_L1
            "<tr><td>/boot</td><td>"_L1 + tr("Boot manager") + "</td></tr>"_L1
            "<tr><td>/</td><td>"_L1 + tr("System root") + "</td></tr>"_L1
            "<tr><td>/home</td><td>"_L1 + tr("User data") + "</td></tr>"_L1
            "<tr><td>/usr</td><td>"_L1 + tr("Static data") + "</td></tr>"_L1
            "<tr><td>/var</td><td>"_L1 + tr("Variable data") + "</td></tr>"_L1
            "<tr><td>/tmp</td><td>"_L1 + tr("Temporary files") + "</td></tr>"_L1
            "<tr><td>/swap</td><td>"_L1 + tr("Swap files") + "</td></tr>"_L1
            "<tr><td>SWAP</td><td>"_L1 + tr("Swap partition") + "</td></tr>"_L1
            "</table>"_L1
            + tr("In addition to the above, you can also type your own mount point. Custom mount points must start with a slash (\"/\").") + "</p>"_L1
            "<p>"_L1 + tr("<i>Label</i> - The label that is assigned to the partition once it has been formatted.") + "</p>"_L1
            "<p>"_L1 + tr("<i>Encrypt</i> - Use LUKS encryption for this partition. The password applies to all partitions selected for encryption.") + "</p>"_L1
            "<p>"_L1 + tr("<i>Format</i> - This is the partition's format. Available formats depend on what the partition is used for."
                " When working with an existing layout, you may be able to preserve the format of the partition by selecting <b>Preserve</b>.") + "<br/>"_L1
            + tr("Selecting <b>Preserve /home</b> for the root partition preserves the contents of the /home directory, deleting everything else."
                " This option can only be used when /home is on the same partition as the root partition.") + "</p>"_L1
            "<p>"_L1 + tr("The ext2, ext3, ext4, jfs, xfs and btrfs Linux filesystems are supported and ext4 is recommended.") + "</p>"_L1
            "<p>"_L1 + tr("<i>Check</i> - Check and correct for bad blocks on the drive (not supported for all formats)."
                " This is very time consuming, so you may want to skip this step unless you suspect that your drive has bad blocks.") + "</p>"_L1
            "<p>"_L1 + tr("<i>Mount Options</i> - This specifies mounting options that will be used for this partition.") + "</p>"_L1
            "<p>"_L1 + tr("<i>Dump</i> - Instructs the dump utility to include this partition in the backup.") + "</p>"_L1
            "<p>"_L1 + tr("<i>Pass</i> - The sequence in which this file system is to be checked at boot. If zero, the file system is not checked.") + "</p>"_L1
            "<p><b>"_L1 + tr("Menus and actions") + "</b><br/>"_L1
            + tr("A variety of actions are available by right-clicking any drive or partition item in the list.") + "<br/>"_L1
            + tr("The buttons to the right of the list can also be used to manipulate the entries.") + "</p>"_L1
            "<p>"_L1 + tr("The installer cannot modify the layout already on the drive."
                " To create a custom layout, mark the drive for a new layout with the <b>New layout</b> menu action"
                " or button (%1). This clears the existing layout.").arg(u"<img src=':/edit-clear-all'/>"_s) + "</p>"_L1
            "<p><b>"_L1 + tr("Basic layout requirements") + "</b><br/>"_L1
            + tr("%1 requires a root partition. The swap partition is optional but highly recommended."
                " If you want to use the Suspend-to-Disk feature of %1, you will need a swap partition that is larger than your physical memory size.").arg(PROJECTNAME) + "</p>"_L1
            "<p>"_L1 + tr("If you choose a separate /home partition it will be easier for you to upgrade in the future,"
                " but this will not be possible if you are upgrading from an installation that does not have a separate home partition.") + "</p>"_L1
            "<p><b>"_L1 + tr("Active partition") + "</b><br/>"_L1
            + tr("For the installed operating system to boot, the appropriate partition (usually the boot or root partition) must be the marked as active.") + "</p>"_L1
            "<p>"_L1 + tr("The active partition of a drive can be chosen using the <b>Active partition</b> menu action.") + "<br/>"_L1
            + tr("A partition with an asterisk (*) next to its device name is, or will become, the active partition.") + "</p>"_L1
            "<p><b>"_L1 + tr("EFI System Partition") + "</b><br/>"_L1
            + tr("If your system uses the Extensible Firmware Interface (EFI), a partition known as the EFI System Partition (ESP) is required for the system to boot.") + "<br/>"_L1
            + tr("These systems do not require any partition marked as Active, but instead require a partition formatted with a FAT file system, marked as an ESP.") + "<br/>"_L1
            + tr("Most systems built within the last 10 years use EFI.") + "</p>"_L1
            "<p><b>"_L1 + tr("Boot partition") + "</b><br/>"_L1
            + tr("This partition is generally only required for root partitions on virtual devices such as encrypted, LVM or software RAID volumes.") + "<br/>"_L1
            + tr("It contains a basic kernel and drivers used to access the encrypted disk or virtual devices.") + "</p>"_L1
            "<p><b>"_L1 + tr("BIOS-GRUB partition") + "</b><br/>"_L1
            + tr("When using a GPT-formatted drive on a non-EFI system, a 1MB BIOS boot partition is required when using GRUB.") + "</p>"_L1
            "<p><b>"_L1 + tr("Need help creating a layout?") + "</b><br/>"_L1
            + tr("Just right-click on a drive and select <b>Layout Builder</b> from the menu. This can create a layout similar to that of the regular install.") + "</p>"_L1
            "<p><b>"_L1 + tr("Upgrading") + "</b><br/>"_L1
            + tr("To upgrade from an existing Linux installation, select the same home partition as before and select <b>Preserve</b> as the format.") + "</p>"_L1
            "<p>"_L1 + tr("If you do not use a separate home partition, select <b>Preserve /home</b> on the root file system entry to preserve the existing /home directory located on your root partition."
                " The installer will only preserve /home, and will delete everything else. As a result, the installation will take much longer than usual.") + "</p>"_L1
            "<p><b>"_L1 + tr("Preferred Filesystem Type") + "</b><br/>"_L1
            + tr("For %1, you may choose to format the partitions as ext2, ext3, ext4, f2fs, jfs, xfs or btrfs.").arg(PROJECTNAME) + "</p>"_L1
            "<p>"_L1 + tr("Additional compression options are available for drives using btrfs."
                " Lzo is fast, but the compression is lower. Zlib is slower, with higher compression.") + "</p>"_L1
            "<p><b>"_L1 + tr("System partition management tool") + "</b><br/>"_L1
            + tr("For more control over the drive layouts (such as modifying the existing layout on a disk), click the"
                " partition management button (%1). This will run the operating system's partition management tool,"
                " which will allow you to create the exact layout you need.").arg(u"<img src=':/partitionmanager'/>"_s) + "</p>"_L1
            "<p><b>"_L1 + tr("Encryption") + "</b><br/>"_L1
            + tr("Encryption is possible via LUKS. A password is required.") + "</p>"_L1
            "<p>"_L1 + tr("A separate unencrypted boot partition is required.") + "</p>"_L1
            "<p>"_L1 + tr("To preserve an encrypted partition, right-click on it and select <b>Unlock</b>. In the dialog that appears, enter a name for the virtual device and the password."
                " When the device is unlocked, the name you chose will appear under <i>Virtual Devices</i>, with similar options to that of a regular partition.") + "</p><p>"_L1
            + tr("For the encrypted partition to be unlocked at boot, it needs to be added to the crypttab file. Use the <b>Add to crypttab</b> menu action to do this.") + "</p>"_L1
            "<p><b>"_L1 + tr("Other partitions") + "</b><br/>"_L1
            + tr("The installer allows other partitions to be created or used for other purposes, however be mindful that older systems cannot handle drives with more than 4 partitions.") + "</p>"_L1
            "<p><b>"_L1 + tr("Subvolumes") + "</b><br/>"_L1
            + tr("Some file systems, such as Btrfs, support multiple subvolumes in a single partition."
                " These are not physical subdivisions, and so their order does not matter.") + "<br/>"_L1
            + tr("Use the <b>Scan subvolumes</b> menu action to search an existing Btrfs partition for subvolumes."
                " To create a new subvolume, use the <b>New subvolume</b> menu action.") + "</p><p>"_L1
            + tr("Existing subvolumes can be preserved, however the name must remain the same.") + "</p>"_L1
            "<p><b>"_L1 + tr("Virtual Devices") + "</b><br/>"_L1
            + tr("If the installer detects any virtual devices such as opened LUKS partitions, LVM logical volumes or software-based RAID volumes, they may be used for the installation.") + "</p>"_L1
            "<p>"_L1 + tr("The use of virtual devices (beyond preserving encrypted file systems) is an advanced feature. You may have to edit some files (eg. initramfs, crypttab, fstab) to ensure the virtual devices used are created upon boot.") + "</p>"_L1);
        break;

    case Step::ENCRYPTION: // Disk encryption.
        gui.textHelp->setText("<p><b>"_L1 + tr("Encryption") + "</b><br/>"_L1
            + tr("You have chosen to encrypt at least one volume, and more information is required before continuing.") + "</p>"_L1);
        enableNext = crypto->valid();
        break;

    case Step::CONFIRM: // Confirmation and review.
        gui.textHelp->setText("<p><b>"_L1 + tr("Final Review and Confirmation") + "</b><br/>"_L1
            + tr("Please review this list carefully. This is the last opportunity to check, review and confirm the actions of the installation process before proceeding.") + "</p>"_L1);

        gui.treeConfirm->expandAll();
        gui.treeConfirm->resizeColumnToContents(0);
        if (!automatic) {
            core.sleep(500, true); // Prevent accidentally skipping the confirmation.
        }
        break;

    case Step::BOOT: // Start of installation.
        gui.textHelp->setText("<p><b>"_L1 + tr("Install GRUB for Linux and Windows") + "</b><br/>"_L1
            + tr("%1 uses the GRUB bootloader to boot %1 and Microsoft Windows.").arg(PROJECTNAME) + "</p>"_L1
            "<p>"_L1 + tr("By default GRUB is installed in the Master Boot Record (MBR) or ESP (EFI System Partition for 64-bit UEFI boot systems) of your boot drive and replaces the boot loader you were using before. This is normal.") + "</p>"_L1
            "<p>"_L1 + tr("If you choose to install GRUB to Partition Boot Record (PBR) instead, then GRUB will be installed at the beginning of the specified partition. This option is for experts only.") + "</p>"_L1
            "<p>"_L1 + tr("If you uncheck the Install GRUB box, GRUB will not be installed at this time. This option is for experts only.") + "</p>"_L1
        "<p>"_L1 + tr("Generate host-specific initramfs will try to create an initramfs tailored for the particular device rather than a generic all-purpose initramfs. This option is for experts only.") + "</p>"_L1);
        enableBack = false;
        break;

    case Step::SWAP:
        gui.textHelp->setText("<p><b>"_L1 + tr("Create a swap file") + "</b><br/>"_L1
            + tr("A swap file is more flexible than a swap partition; it is considerably easier to resize a swap file to adapt to changes in system usage.") + "</p>"_L1
            "<p>"_L1 + tr("By default, this is checked if no swap partitions have been set, and unchecked if swap partitions are set. This option should be left untouched, and is for experts only.") + "</p>"_L1
            "<p>"_L1 + tr("Zram swap is a method of putting swap space in RAM.  A compressed swap device is placed in RAM.  It may be used in conjunction with other forms of swap, or on its own.") + "</p>"_L1);
        enableBack = advanced;
        break;

    case Step::SERVICES:
        gui.textHelp->setText("<p><b>"_L1 + tr("Common Services to Enable") + "</b><br/>"
            + tr("Select any of these common services that you might need with your system configuration"
                " and the services will be started automatically when you start %1.").arg(PROJECTNAME) + "</p>"_L1);
        break;

    case Step::NETWORK:
        gui.textHelp->setText("<p><b>"_L1 + tr("Computer Identity") + "</b><br/>"
            + tr("The computer name is a common unique name which will identify your computer if it is on a network.")
            + tr("The computer domain is unlikely to be used unless your ISP or local network requires it.") + "</p>"_L1
            "<p>"_L1 + tr("The computer and domain names can contain only alphanumeric characters, dots, hyphens. "
                "They cannot contain blank spaces, start or end with hyphens.") + "</p>"_L1
            "<p>"_L1 + tr("The SaMBa Server needs to be activated if you want to use it to share some of your directories or printer "
                "with a local computer that is running MS-Windows or Mac OSX.") + "</p>"_L1);
        break;

    case Step::LOCALIZATION:
        gui.textHelp->setText("<p><b>"_L1 + tr("Localization Defaults") + "</b><br/>"_L1
            + tr("Set the default locale. This will apply unless they are overridden later by the user.") + "</p>"_L1
            "<p><b>"_L1 + tr("Configure Clock") + "</b><br/>"_L1
            + tr("If you have an Apple or a pure Unix computer, by default the system clock is set to Greenwich Meridian Time (GMT) or Coordinated Universal Time (UTC)."
                " To change this, check the \"<b>System clock uses local time</b>\" box.") + "</p>"_L1
            "<p>"_L1 + tr("The system boots with the timezone preset to GMT/UTC."
                " To change the timezone, after you reboot into the new installation, right click on the clock in the Panel and select Properties.") + "</p>"_L1
            "<p><b>"_L1 + tr("Service Settings") + "</b><br/>"_L1
            + tr("Most users should not change the defaults."
                " Users with low-resource computers sometimes want to disable unneeded services in order to keep the RAM usage as low as possible."
                " Make sure you know what you are doing!"));
        break;

    case Step::USER_ACCOUNTS:
        gui.textHelp->setText("<p><b>"_L1 + tr("Default User Login") + "</b><br/>"_L1
        + tr("Please enter the name for a new (default) user account that you will use on a daily basis."
            " If needed, you can add other user accounts later with %1 User Manager.").arg(PROJECTNAME) + "</p>"_L1
        "<p><b>"_L1 + tr("Root (administrator) account") + "</b><br/>"_L1
        + tr("The root user is similar to the Administrator user in some other operating systems."
            " You should not use the root user as your daily user account.") + "<br/>"_L1
        + tr("The root account is disabled on MX Linux, as administrative tasks are performed with an elevation prompt for the default user.") + "<br/>"
        "<i>"_L1 + tr("Enabling the root account is strongly recommended for antiX Linux.") + "</i></p>"_L1
        "<p><b>"_L1 + tr("Passwords") + "</b><br/>"_L1
        + tr("Enter a new password for your default user account and for the root account."
            " Each password must be entered twice.") + "</p>"_L1
        "<p><b>"_L1 + tr("No passwords") + "</b><br/>"_L1
        + tr("If you want the default user account to have no password, leave its password fields empty."
            " This allows you to log in without requiring a password.") + "<br/>"_L1
        + tr("Obviously, this should only be done in situations where the user account"
            " does not need to be secure, such as a public terminal.") + "</p>"_L1);
        if (!nextFocus) nextFocus = gui.textUserName;
        oobe->userPassValidationChanged();
        enableNext = gui.pushNext->isEnabled();
        break;

    case Step::OLD_HOME:
        gui.textHelp->setText("<p><b>"_L1 + tr("Old Home Directory") + "</b><br/>"_L1
            + tr("A home directory already exists for the user name you have chosen."
                " This screen allows you to choose what happens to this directory.") + "</p>"_L1
            "<p><b>"_L1 + tr("Re-use it for this installation") + "</b><br/>"_L1
            + tr("The old home directory will be used for this user account."
                " This is a good choice when upgrading, and your files and settings will be readily available.") + "</p>"_L1
            "<p><b>"_L1 + tr("Rename it and create a new directory") + "</b><br/>"_L1
            + tr("A new home directory will be created for the user, but the old home directory will be renamed."
                " Your files and settings will not be immediately visible in the new installation, but can be accessed using the renamed directory.") + "</p>"_L1
            "<p>"_L1 + tr("The old directory will have a number at the end of it, depending on how many times the directory has been renamed before.") + "</p>"_L1
            "<p><b>"_L1 + tr("Delete it and create a new directory") + "</b><br/>"_L1
            + tr("The old home directory will be deleted, and a new one will be created from scratch.") + "<br/>"_L1
            "<b>"_L1 + tr("Warning") + "</b>: "_L1
            + tr("All files and settings will be deleted permanently if this option is selected."
                " Your chances of recovering them are low.") + "</p>"_L1);
        // disable the Next button if none of the old home options are selected
        oobe->oldHomeToggled();
        // if the Next button is disabled, avoid enabling both Back and Next at the end
        if (!gui.pushNext->isEnabled()) {
            enableBack = true;
            enableNext = false;
        }
        break;

    case Step::TIPS:
        gui.textHelp->setText("<p><b>"_L1 + tr("Installation in Progress") + "</b><br/>"_L1
            + tr("%1 is installing. For a fresh install, this will probably take 3-20 minutes, depending on the speed of your system and the size of any partitions you are reformatting.").arg(PROJECTNAME)
            + "</p><p>"_L1
            + tr("If you click the Abort button, the installation will be stopped as soon as possible.")
            + "</p><p>"_L1
            + "<b>"_L1 + tr("Change settings while you wait") + "</b><br/>"_L1
            + tr("While %1 is being installed, you can click on the <b>Next</b> or <b>Back</b> buttons to enter other information required for the installation.").arg(PROJECTNAME)
            + "</p><p>"_L1
            + tr("Complete these steps at your own pace. The installer will wait for your input if necessary.")
            + "</p>"_L1);
        enableNext = false;
        break;

    case Step::END:
        gui.pushClose->setEnabled(false);
        gui.textHelp->setText("<p><b>"_L1 + tr("Congratulations!") + "</b><br/>"_L1
            + tr("You have completed the installation of %1.").arg(PROJECTNAME) + "</p>"_L1
            "<p><b>"_L1 + tr("Finding Applications") + "</b><br/>"_L1
            + tr("There are hundreds of excellent applications installed with %1."
                " The best way to learn about them is to browse through the Menu and try them."
                " Many of the apps were developed specifically for the %1 project."
                " These are shown in the main menus.").arg(PROJECTNAME)
            + "<p>"_L1 + tr("In addition %1 includes many standard Linux applications"
                " that are run only from the command line and therefore do not show up in the Menu.").arg(PROJECTNAME)
            + "</p><p><b>"_L1 + tr("Enjoy using %1").arg(PROJECTNAME) + "</b></p>"_L1);
        break;
    }

    gui.pushBack->setEnabled(enableBack);
    gui.pushNext->setEnabled(enableNext);
}

void MInstall::gotoPage(int next) noexcept
{
    gui.pushBack->setEnabled(false);
    gui.pushNext->setEnabled(false);
    gui.widgetStack->setEnabled(false);
    int curr = gui.widgetStack->currentIndex();
    next = showPage(curr, next);

    // modify ui for standard cases
    gui.pushClose->setHidden(next == 0);
    gui.pushBack->setHidden(next <= 1);
    gui.pushNext->setHidden(next == 0);

    QSize isize = gui.pushNext->iconSize();
    isize.setWidth(isize.height());
    if (next >= Step::END) {
        // entering the last page
        gui.pushBack->hide();
        gui.pushNext->setText(tr("Finish"));
    } else if (next == Step::CONFIRM) {
        isize.setWidth(0);
        gui.pushNext->setText(tr("Start"));
    } else if (next == Step::SERVICES){
        isize.setWidth(0);
        gui.pushNext->setText(tr("OK"));
    } else {
        gui.pushNext->setText(tr("Next"));
    }
    gui.pushNext->setIconSize(isize);

    if (next > Step::END) {
        // finished
        qApp->setOverrideCursor(Qt::WaitCursor);
        if (!pretend && gui.checkExitReboot->isChecked()) {
            runShutdown(u"reboot"_s);
        }
        qApp->exit(EXIT_SUCCESS);
        return;
    }
    // display the next page
    gui.widgetStack->setCurrentIndex(next);
    qApp->processEvents();

    // anything to do after displaying the page
    pageDisplayed(next);
    gui.widgetStack->setEnabled(true);
    if (nextFocus) {
        nextFocus->setFocus();
        nextFocus = nullptr;
    }

    // automatic installation
    if (automatic) {
        if (!MSettings::isBadWidget(gui.widgetStack->currentWidget()) && next > curr) {
            QTimer::singleShot(0, gui.pushNext, &QPushButton::click);
        } else if (curr!=0) { // failed validation
            automatic = false;
        }
    }

    // process next installation phase
    if (next == Step::BOOT || next == Step::TIPS || (!advanced && next == Step::SWAP)) {
        processNextPhase();
    }
}

/////////////////////////////////////////////////////////////////////////
// event handlers

bool MInstall::eventFilter(QObject *watched, QEvent *event) noexcept
{
    if (watched == gui.textHelp && event->type() == QEvent::Paint) {
        // Draw the help pane backdrop image, which goes through the alpha-channel background.
        QPainter painter(gui.textHelp);
        painter.setRenderHints(QPainter::Antialiasing);
        painter.drawPixmap(gui.textHelp->viewport()->rect(), helpBackdrop, helpBackdrop.rect());
    }
    return QDialog::eventFilter(watched, event);
}

void MInstall::changeEvent(QEvent *event) noexcept
{
    const QEvent::Type etype = event->type();
    if (etype == QEvent::ApplicationPaletteChange || etype == QEvent::PaletteChange || etype == QEvent::StyleChange) {
        QPalette pal = qApp->palette(gui.textHelp);
        QColor col = pal.color(QPalette::Base);
        col.setAlpha(200);
        pal.setColor(QPalette::Base, col);
        gui.textHelp->setPalette(pal);
        if (autopart) autopart->refresh();
    }
}

void MInstall::closeEvent(QCloseEvent *event) noexcept
{
    if (phase > PH_READY && phase < PH_FINISHED && abortion != AB_ABORTED) {
        // Currently installing, could be pending abortion (but not finished aborting).
        event->ignore();
        abortUI(true, true);
    } else if (modeOOBE) {
        // Shutdown for pending or fully aborted OOBE
        event->ignore();
        gui.labelSplash->clear();
        gotoPage(Step::SPLASH);
        proc.unhalt();
        if (!pretend) {
            const QString shutdownCmd = QFileInfo(u"/usr/sbin/shutdown"_s).isExecutable()
                ? u"/usr/sbin/shutdown"_s : u"/usr/bin/shutdown"_s;
            proc.exec(shutdownCmd, {u"-hP"_s, u"now"_s});
        }
    } else {
        // Fully aborted installation (but not OOBE).
        event->accept();
        if (phase == PH_STARTUP) proc.halt(true);
        if (checkmd5) checkmd5->halt(true);
    }
}

// Override QDialog::reject() so Escape won't close the window.
void MInstall::reject() noexcept
{
    if (checkmd5) checkmd5->halt();
}

void MInstall::abortUI(bool manual, bool closing) noexcept
{
    // ask for confirmation when installing (except for some steps that don't need confirmation)
    if (abortion != AB_NO_ABORT) return; // Don't abort an abortion.
    else if (phase > PH_READY && phase < PH_FINISHED) {
        if (manual) {
            QMessageBox msgbox(this);
            msgbox.setIcon(QMessageBox::Warning);
            msgbox.setText(tr("The installation and configuration is incomplete."));
            msgbox.setInformativeText(tr("Do you really want to stop now?"));
            msgbox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
            msgbox.setDefaultButton(QMessageBox::No);
            if (msgbox.exec() == QMessageBox::No) return;
            proc.log(u"MANUALLY ABORTED"_s, MProcess::LOG_FAIL);
        }
    }
    // At this point the abortion has not been cancelled.
    abortion = closing ? AB_CLOSING : AB_ABORTING;
    gotoPage(Step::SPLASH);
    proc.halt(true);
    // Early phase bump if waiting on input, to trigger abortion cleanup.
    if (manual && phase == PH_WAITING_FOR_INFO) processNextPhase();
}
void MInstall::abortEndUI(bool closenow) noexcept
{
    if (closenow) qApp->exit(EXIT_FAILURE); // this->close() doesn't work.
    else {
        if (throbber) {
            delete throbber;
            throbber = nullptr;
        }
        gui.boxMain->unsetCursor();
        // Close should be the right button at this stage.
        disconnect(gui.pushNext);
        connect(gui.pushNext, &QPushButton::clicked, this, &MInstall::close);
        gui.pushNext->setText(gui.pushClose->text());
        gui.pushNext->show();
    }
}

// run before closing the app, do some cleanup
void MInstall::cleanup()
{
    proc.log(__PRETTY_FUNCTION__, MProcess::LOG_MARKER);
    if (pretend) return;

    const char *destlog = "/mnt/antiX/var/log/minstall.log";
    QFile::remove(destlog);
    bool ok = QFile::copy(u"/var/log/minstall.log"_s, destlog);
    if (ok) ok = (chmod(destlog, 0400) == 0);
    if (!ok) proc.log(u"Failed to copy the installation log to the target."_s, MProcess::LOG_FAIL);

    proc.exec(u"rm"_s, {u"-rf"_s, u"/mnt/antiX/mnt/antiX"_s});
    if (!mountkeep) partman->clearWorkArea();
    setupAutoMount(true);
}

void MInstall::progressUpdate(int value) noexcept
{
    int newtip = 0;
    if (ixTipStart < 0) {
        ixTipStart = 0; // First invocation of this function.
    } else {
        if (gui.widgetStack->currentIndex() != Step::TIPS) {
            iLastProgress = -1;
            return; // No point loading a new tip when will be invisible.
        } else if (iLastProgress < 0) {
            iLastProgress = value; // First invocation since progress page last shown.
        }
        static constexpr int tipcount = 5;
        newtip = tipcount;
        if (ixTipStart < tipcount) {
            int imax = (gui.progInstall->maximum() - iLastProgress) / (tipcount - ixTipStart);
            if (imax != 0) {
                newtip = ixTipStart + (value - iLastProgress) / imax;
            }
        }
        if (newtip == ixTip) {
            return; // No point loading a tip that is already visible.
        }
    }
    ixTip = newtip;

    switch(newtip)
    {
    case 0:
        gui.textTips->setText(tr("<p><b>Getting Help</b><br/>"
                             "Basic information about %1 is at %2.</p><p>"
                             "There are volunteers to help you at the %3 forum, %4</p>"
                             "<p>If you ask for help, please remember to describe your problem and your computer "
                             "in some detail. Usually statements like 'it didn't work' are not helpful.</p>").arg(PROJECTNAME, PROJECTURL, PROJECTSHORTNAME, PROJECTFORUM));
        break;

    case 1:
        gui.textTips->setText(tr("<p><b>Repairing Your Installation</b><br/>"
                             "If %1 stops working from the hard drive, sometimes it's possible to fix the problem by booting from LiveDVD or LiveUSB and running one of the included utilities in %1 or by using one of the regular Linux tools to repair the system.</p>"
                             "<p>You can also use your %1 LiveDVD or LiveUSB to recover data from MS-Windows systems!</p>").arg(PROJECTNAME));
        break;

    case 3:
        gui.textTips->setText(tr("<p><b>Adjusting Your Sound Mixer</b><br/>"
                             " %1 attempts to configure the sound mixer for you but sometimes it will be "
                             "necessary for you to turn up volumes and unmute channels in the mixer "
                             "in order to hear sound.</p> "
                             "<p>The mixer shortcut is located in the menu. Click on it to open the mixer. </p>").arg(PROJECTNAME));
        break;

    case 4:
        gui.textTips->setText(tr("<p><b>Keep Your Copy of %1 up-to-date</b><br/>"
                             "For more information and updates please visit</p><p> %2</p>").arg(PROJECTNAME, PROJECTFORUM));
        break;

    default:
        gui.textTips->setText(tr("<p><b>Special Thanks</b><br/>Thanks to everyone who has chosen to support %1 with their time, money, suggestions, work, praise, ideas, promotion, and/or encouragement.</p>"
                             "<p>Without you there would be no %1.</p>"
                             "<p>%2 Dev Team</p>").arg(PROJECTNAME, PROJECTSHORTNAME));
        break;
    }
}

void MInstall::setupkeyboardbutton() noexcept
{
    const MIni ini(u"/etc/default/keyboard"_s, MIni::ReadOnly);
    gui.labelKeyboardModel->setText(ini.getString(u"XKBMODEL"_s));
    gui.labelKeyboardLayout->setText(ini.getString(u"XKBLAYOUT"_s));
    gui.labelKeyboardVariant->setText(ini.getString(u"XKBVARIANT"_s));
}

void MInstall::runKeyboardSetup() noexcept
{
    this->setEnabled(false);
    if (proc.shell(u"command -v  system-keyboard-qt >/dev/null 2>&1"_s)) {
        proc.exec(u"system-keyboard-qt"_s);
    } else {
        proc.shell(u"env GTK_THEME='Adwaita' fskbsetting"_s);
    }
    setupkeyboardbutton();
    this->setEnabled(true);
}
