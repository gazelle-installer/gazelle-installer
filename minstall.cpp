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
#include <functional>
#include <cstring>
#include <cmath>
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
#include "ui/qmessagebox.h"
#include <QDebug>
#include <QTreeWidgetItem>
#include <QRegularExpression>

#include "msettings.h"
#include "ui/context.h"
#include "ui/qcheckbox.h"
#include "ui/qlabel.h"
#include "ui/qradiobutton.h"
#include "ui/qlineedit.h"
#include "ui/qcombobox.h"
#include "ui/qslider.h"
#include "qtui/tpushbutton.h"
#include "qtui/tlineedit.h"
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

// Include ncurses AFTER all Qt headers to avoid macro conflicts
#include <ncurses.h>

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
    if (ui::Context::isTUI()) {
        proc.setProgressCallback([this]() {
            if (currentPageIndex == Step::TIPS || (currentPageIndex == Step::CONFIRM && tui_installStarting)) {
                ++tui_spinnerTick;
                renderCurrentPage();
                refresh();

                // Poll for non-blocking input during installation
                // This allows the reboot checkbox to be toggled while installing
                timeout(0);  // Non-blocking getch
                int ch = getch();
                if (ch != ERR) {
                    if (ch == KEY_MOUSE) {
                        MEVENT event;
                        if (getmouse(&event) == OK) {
                            handleMouse(event.y, event.x, event.bstate);
                        }
                    } else {
                        handleInput(ch);
                    }
                    renderCurrentPage();
                    refresh();
                }
                timeout(-1);  // Restore blocking mode
            }
        });
    }
    gui.textHelp->installEventFilter(this);
    gui.boxInstall->hide();
    gui.checkExitReboot->setChecked(false);
    gui.checkExitReboot->hide();

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

    // Setup TUI widgets if in TUI mode
    if (ui::Context::isTUI()) {
        setupPageSplashTUI();
        setupPageTermsTUI();
        setupPageInstallationTUI();
        setupPageReplaceTUI();
        setupPagePartitionsTUI();
        setupPageEncryptionTUI();
        setupPageConfirmTUI();
        setupPageBootTUI();
        setupPageSwapTUI();
        setupPageNetworkTUI();
        setupPageLocalizationTUI();
        setupPageServicesTUI();
        setupPageUserAccountsTUI();
        setupPageOldHomeTUI();
        setupPageTipsTUI();
        setupPageEndTUI();
    }

    gotoPage(Step::SPLASH);

    // ensure the help widgets are displayed correctly when started
    // Qt will delete the heap-allocated event object when posted
    qApp->postEvent(this, new QEvent(QEvent::PaletteChange));

    //automount disalbe
    autoMountEnabled = true; // disable auto mount by force
    setupAutoMount(false);


    // Auto-start in both GUI and TUI mode
    if (ui::Context::isGUI()) {
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
    } else {
        // TUI mode: Call startup directly (blocking, silently)
        try {
            startup();
            phase = PH_READY;
        } catch (const char *msg) {
            if (checkmd5) {
                delete checkmd5;
                checkmd5 = nullptr;
            }
            proc.unhalt();
            // Error will be visible when TUI starts
        }
    }
}

void MInstall::gotoAfterPartitionsTui() noexcept
{
    if (!partman) {
        return;
    }
    if (!partman->validate(false)) {
        return;
    }
    for (PartMan::Iterator it(*partman); *it; it.next()) {
        PartMan::Device *device = *it;
        if (device && device->type == PartMan::Device::PARTITION
            && device->encrypt && device->canEncrypt()) {
            gotoPage(Step::ENCRYPTION);
            return;
        }
    }
    gotoPage(Step::CONFIRM);
}

bool MInstall::tuiWantsEsc() const noexcept
{
    if (!ui::Context::isTUI()) {
        return false;
    }
    if (tui_partitionEditing) {
        return true;
    }

    auto popupVisible = [](ui::QComboBox *combo) -> bool {
        if (!combo) return false;
        auto *tuiCombo = dynamic_cast<qtui::TComboBox*>(combo->tuiWidget());
        return tuiCombo && tuiCombo->isPopupVisible();
    };

    if (popupVisible(tui_comboDriveSystem)
        || popupVisible(tui_comboDriveHome)
        || popupVisible(tui_comboBoot)
        || popupVisible(tui_comboLocale)
        || popupVisible(tui_comboTimeArea)
        || popupVisible(tui_comboTimeZone)) {
        return true;
    }

    // Treat focused text fields as ESC targets so the app doesn't quit.
    if (currentPageIndex == Step::ENCRYPTION) {
        return true;
    }
    if (currentPageIndex == Step::SWAP) {
        return (tui_focusSwap == 1 || tui_focusSwap == 2 || tui_focusSwap == 6 || tui_focusSwap == 8);
    }
    if (currentPageIndex == Step::NETWORK) {
        return (tui_focusNetwork >= 0 && tui_focusNetwork <= 2);
    }
    if (currentPageIndex == Step::USER_ACCOUNTS) {
        return (tui_focusUserAccounts == 0 || tui_focusUserAccounts == 1 || tui_focusUserAccounts == 2
            || tui_focusUserAccounts == 4 || tui_focusUserAccounts == 5);
    }

    return false;
}

bool MInstall::canGoBack() const noexcept
{
    if (!ui::Context::isTUI()) {
        return false;
    }

    // Can't go back from SPLASH page
    if (currentPageIndex == Step::SPLASH) {
        return false;
    }

    // Can't go back once installation starts
    if (currentPageIndex == Step::BOOT) {
        return false;
    }

    // Can't go back from SWAP unless in advanced mode
    if (currentPageIndex == Step::SWAP && !advanced) {
        return false;
    }

    return true;
}

bool MInstall::canGoNext() const noexcept
{
    if (!ui::Context::isTUI()) {
        return false;
    }

    switch (currentPageIndex) {
    case Step::ENCRYPTION:
        // Can't proceed without valid encryption
        return crypto && crypto->valid();

    case Step::TIPS:
        // Can't proceed during installation
        return false;

    case Step::USER_ACCOUNTS:
        // Check password validation
        if (oobe) {
            // In TUI mode, check if passwords are valid
            const QString userName = gui.textUserName->text();
            const QString userPass1 = gui.textUserPass->text();
            const QString userPass2 = gui.textUserPass2->text();
            const QString rootPass1 = gui.textRootPass->text();
            const QString rootPass2 = gui.textRootPass2->text();

            // Username must not be empty
            if (userName.isEmpty()) {
                return false;
            }

            // User passwords must match
            if (userPass1 != userPass2) {
                return false;
            }

            // Root passwords must match
            if (rootPass1 != rootPass2) {
                return false;
            }
        }
        return true;

    case Step::OLD_HOME:
        // Check if an old home option is selected
        if (gui.radioOldHomeUse && gui.radioOldHomeUse->isChecked()) {
            return true;
        }
        if (gui.radioOldHomeSave && gui.radioOldHomeSave->isChecked()) {
            return true;
        }
        if (gui.radioOldHomeDelete && gui.radioOldHomeDelete->isChecked()) {
            return true;
        }
        return false;

    case Step::END:
        // Can't proceed past the end
        return false;

    default:
        return true;
    }
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

    const QString binPath = "/usr/bin/"_L1 + action;
    const QString sbinPath = "/usr/sbin/"_L1 + action;
    const QString &fallback = QFileInfo(binPath).isExecutable() ? binPath : sbinPath;
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
        // Skip media check when Arch ISO is started with copytoram
        if (QFile::exists(u"/run/archiso/copytoram"_s)) {
            nocheck = true;
            proc.log(u"Arch ISO copytoram detected, skipping media check"_s);
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

        QString reminderText = tr("Support %1").arg(PROJECTNAME) + "\n\n"_L1
            + tr("%1 is supported by people like you. Some help others at the support forum,"
                " or translate help files into different languages, or make suggestions,"
                " write documentation, or help test new software.").arg(PROJECTNAME)
            + '\n' + link_block;

        if (ui::Context::isGUI()) {
            gui.textReminders->setPlainText(reminderText);
        } else {
            if (tui_textReminders) {
                tui_textReminders->setText(reminderText);
            }
        }
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
            proc.shell(
                u"echo -e '#\n"
                u"# This file contains custom mount options for udisks 2.x\n"
                u"#\n"
                u"# Skip if not a block device or if requested by other rules\n"
                u"#\n"
                u"SUBSYSTEM!=\"block\", GOTO=\"udisks_no_automount_options_end\"\n"
                u"ENV{DM_MULTIPATH_DEVICE_PATH}==\"1\", "
                u"GOTO=\"udisks_no_automount_options_end\"\n"
                u"ENV{DM_UDEV_DISABLE_OTHER_RULES_FLAG}==\"?*\", "
                u"GOTO=\"udisks_no_automount_options_end\"\n"
                u"\n"
                u"# disable automounting during minstall-launcher run\n"
                u"# activate with\n"
                u"# sudo udevadm control --reload\n"
                u"# sudo udevadm trigger\n"
                u"\n"
                u"SUBSYSTEM==\"block\", ENV{UDISKS_IGNORE}=\"1\"\n"
                u"\n"
                u"LABEL=\"udisks_no_automount_options_end\"'"
                u" > /run/udev/rules.d/99-mx-automount-inhibit.rules"_s
                );
            proc.exec(u"udevadm"_s, {u"control"_s, u"--reload"_s});
            proc.exec(u"udevadm"_s, {u"trigger"_s, u"--subsystem-match=block"_s});
        }
    } else {
        // enable auto-mount
        if (udisksd_running) {
            proc.exec(u"rm"_s, {u"-f"_s, u"/run/udev/rules.d/99-mx-automount-inhibit.rules"_s});
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
            // TUI mode: check if automatic reboot checkbox is selected
            if (ui::Context::isTUI() && tui_checkTipsReboot && tui_checkTipsReboot->isChecked()) {
                runShutdown(u"reboot"_s);
            }
            // GUI mode: skip END page and reboot if checkbox was checked
            if (ui::Context::isGUI() && gui.checkExitReboot->isChecked()) {
                if (!pretend) runShutdown(u"reboot"_s);
                qApp->exit(EXIT_SUCCESS);
                return;
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
            const QString rebootCmd = QFileInfo(u"/usr/bin/reboot"_s).isExecutable()
                ? u"/usr/bin/reboot"_s : u"/usr/sbin/reboot"_s;
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
        // TUI mode: check if automatic reboot checkbox is selected (skip in pretend mode)
        if (!pretend && ui::Context::isTUI() && tui_checkTipsReboot && tui_checkTipsReboot->isChecked()) {
            runShutdown(u"reboot"_s);
        }
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
        if (ui::Context::isGUI()) {
            QMessageBox msgbox(this);
            msgbox.setIcon(QMessageBox::Critical);
            msgbox.setText(tr("Invalid settings found in configuration file (%1).").arg(configFile));
            msgbox.setInformativeText(tr("Please review marked fields as you encounter them."));
            msgbox.exec();
        } else {
            ui::QMessageBox::critical(nullptr,
                tr("Error"),
                tr("Invalid settings found in configuration file (%1).\nPlease review marked fields as you encounter them.").arg(configFile));
        }
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
            if (ui::Context::isTUI() && pretend) {
                if (gui.checkEncryptAuto->isChecked()) {
                    return Step::ENCRYPTION;
                }
                return Step::CONFIRM;
            }
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
        if (ui::Context::isGUI()) {
            gui.boxMain->setCursor(Qt::WaitCursor);
            if (!replacer || !replacer->validate()) {
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
        } else {
            if (!replacer || !replacer->validate()) {
                return curr;
            }
            if (!replacer->preparePartMan()) {
                return curr;
            }
            if (!pretend && !(base && base->saveHomeBasic())) {
                ui::QMessageBox::critical(nullptr,
                    tr("Error"),
                    tr("The data in /home cannot be preserved because"
                        " the required information could not be obtained."));
                return curr;
            }
        }
        // In TUI mode, validation is skipped for now (pretend mode)
        return Step::CONFIRM;
    } else if (curr == Step::REPLACE && next < curr) {
        if (ui::Context::isGUI()) {
            gui.boxMain->setCursor(Qt::WaitCursor);
        }
        if (replacer) {
            replacer->clean();
        }
        if (ui::Context::isGUI()) {
            gui.boxMain->unsetCursor();
        }
    } else if (curr == Step::PARTITIONS) {
        if (ui::Context::isTUI()) {
            return next > curr ? next : Step::INSTALLATION;
        }
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
        if (ui::Context::isGUI()) {
            gui.treeConfirm->clear(); // Revisiting a step produces a fresh confirmation list.
        }
        if (next > curr) {
            if (ui::Context::isTUI()) {
                if (gui.radioEntireDrive->isChecked()) {
                    if (!autopart->buildLayout()) {
                        ui::QMessageBox::critical(nullptr,
                            tr("Error"),
                            tr("Cannot find selected drive."));
                        return curr;
                    }
                }
                next = Step::BOOT;
                if (bootman) bootman->buildBootLists();
                if (swapman) swapman->setupDefaults();
                loadConfig(2);
            } else {
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
            }
        } else {
            // Going back from CONFIRM
            if (ui::Context::isTUI()) {
                return Step::INSTALLATION;
            } else if (gui.radioEntireDrive->isChecked()) {
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
            if (ui::Context::isGUI() && oobe) {
                nextFocus = oobe->validateComputerName();
                if (nextFocus) return curr;
            }
        } else { // Backward
            return modeOOBE ? Step::TERMS : Step::SWAP;
        }
    } else if (curr == Step::LOCALIZATION && next > curr) {
        if (!pretend && oobe->haveSnapshotUserAccounts) {
            return Step::TIPS; // Skip pageUserAccounts and pageOldHome
        }
    } else if (curr == Step::USER_ACCOUNTS && next > curr) {
        if (ui::Context::isGUI() && oobe) {
            nextFocus = oobe->validateUserInfo(automatic);
            if (nextFocus) return curr;
        }
        // Check for pre-existing /home directory, see if user directory already exists.
        haveOldHome = base && base->homes.contains(gui.textUserName->text());
        if (!haveOldHome) return Step::TIPS; // Skip pageOldHome
        const QString &str = tr("The home directory for %1 already exists.");
        gui.labelOldHome->setText(str.arg(gui.textUserName->text()));
    } else if (curr == Step::OLD_HOME && next < curr) { // Backward
        if (!pretend && oobe && oobe->haveSnapshotUserAccounts) {
            return Step::LOCALIZATION; // Skip pageUserAccounts and pageOldHome
        }
    } else if (curr == Step::TIPS && next < curr) { // Backward
        if (oem) {
            return Step::SWAP;
        } else if (!haveOldHome) {
            // skip pageOldHome
            if (!pretend && oobe && oobe->haveSnapshotUserAccounts) {
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
    if (ui::Context::isGUI() && !modeOOBE) {
        // progress bar shown only for install and configuration pages.
        gui.boxInstall->setVisible(next >= Step::BOOT && next <= Step::TIPS);
        // save the last tip and stop it updating when the progress page is hidden.
        if (next != Step::TIPS) ixTipStart = ixTip;
    }

    // This prevents the user accidentally skipping the confirmation.
    if (ui::Context::isGUI()) {
        gui.pushNext->setDefault(next != Step::CONFIRM);
    }

    // GUI-specific help text and widget updates
    if (ui::Context::isGUI()) {
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
        if (!ui::Context::isTUI()) {
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
        }
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
        if (ui::Context::isGUI() && gui.checkExitReboot->isHidden()) {
            gui.checkExitReboot->setText(tr("Automatically restart when the installation is done"));
            gui.boxTips->layout()->addWidget(gui.checkExitReboot);
            gui.checkExitReboot->show();
        }
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
    } // end if (ui::Context::isGUI())
}

void MInstall::gotoPage(int next) noexcept
{
    if (ui::Context::isGUI()) {
        gui.pushBack->setEnabled(false);
        gui.pushNext->setEnabled(false);
        gui.widgetStack->setEnabled(false);
    }

    int curr = ui::Context::isGUI() ? gui.widgetStack->currentIndex() : currentPageIndex;
    next = showPage(curr, next);

    // Track current page for TUI rendering
    currentPageIndex = next;

    if (next != Step::CONFIRM) {
        // Hide CONFIRM page widgets when leaving
        if (ui::Context::isTUI()) {
            if (tui_labelConfirmTitle) tui_labelConfirmTitle->hide();
            if (tui_labelConfirmInfo) tui_labelConfirmInfo->hide();
        }
    }

    if (ui::Context::isTUI()) {
        gui.widgetStack->setCurrentIndex(next);
        clear();  // Clear screen before rendering new page
        mvprintw(0, 0, "Gazelle Installer (TUI Mode) - Press ESC to quit (Alt+Left = Back)");
        mvprintw(1, 0, "========================================================");
        renderCurrentPage();
        
        // Show different footer based on page
        int maxY, maxX;
        getmaxyx(stdscr, maxY, maxX);
        (void)maxX;
        if (next == Step::TIPS) {
            mvprintw(maxY - 1, 0, "Installation in progress - please wait");
        } else {
            mvprintw(maxY - 1, 0, "SPACE: Toggle | ESC: Quit | Alt+Left: Back");
        }
        
        refresh();
    }

    if (ui::Context::isTUI() && next == Step::REPLACE) {
        if (replacer && replacer->installationCount() <= 0) {
            tui_replaceScanning = true;
            tui_replaceScanPending = true;
            tui_focusReplace = 0;
        }
    }
    if (ui::Context::isTUI() && next == Step::LOCALIZATION && !tui_localizationInitialized) {
        loadConfig(2);
        tui_localizationInitialized = true;
    }

    if (ui::Context::isGUI()) {
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
    }

    if (next > Step::END) {
        // finished
        if (ui::Context::isGUI()) {
            qApp->setOverrideCursor(Qt::WaitCursor);
        }

        bool shouldReboot = false;
        if (ui::Context::isGUI()) {
            shouldReboot = gui.checkExitReboot->isChecked();
        } else {
            shouldReboot = tui_checkExitReboot && tui_checkExitReboot->isChecked();
        }

        if (!pretend && shouldReboot) {
            runShutdown(u"reboot"_s);
        }
        qApp->exit(EXIT_SUCCESS);
        return;
    }

    // display the next page
    if (ui::Context::isGUI()) {
        gui.widgetStack->setCurrentIndex(next);
    }
    // TUI mode: page switching happens in event loop via renderCurrentPage()
    qApp->processEvents();

    // anything to do after displaying the page
    if (ui::Context::isGUI()) {
        pageDisplayed(next);
        gui.widgetStack->setEnabled(true);
        if (nextFocus) {
            nextFocus->setFocus();
            nextFocus = nullptr;
        }
    }

    // automatic installation (GUI only)
    if (ui::Context::isGUI() && automatic) {
        if (!MSettings::isBadWidget(gui.widgetStack->currentWidget()) && next > curr) {
            QTimer::singleShot(0, gui.pushNext, &QPushButton::click);
        } else if (curr!=0) { // failed validation
            automatic = false;
        }
    }
    // automatic installation (TUI)
    if (ui::Context::isTUI() && automatic) {
        if (!MSettings::isBadWidget(gui.widgetStack->currentWidget()) && next > curr) {
            if (next != Step::TIPS && next != Step::END && tui_deferredPage < 0) {
                tui_deferredPage = next + 1;
            }
        } else if (curr != 0) { // failed validation
            automatic = false;
        }
    }

    // process next installation phase (skip in TUI pretend mode)
    if (ui::Context::isGUI()) {
        if (next == Step::BOOT || next == Step::TIPS || (!advanced && next == Step::SWAP)) {
            processNextPhase();
        }
    } else {
        // In TUI mode, only start installation on TIPS page (after all config is gathered)
        if (next == Step::TIPS) {
            processNextPhase();
        }
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
            const QString shutdownCmd = QFileInfo(u"/usr/bin/shutdown"_s).isExecutable()
                ? u"/usr/bin/shutdown"_s : u"/usr/sbin/shutdown"_s;
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

// TUI Functions

void MInstall::setupPageSplashTUI() noexcept
{
    if (!ui::Context::isTUI()) {
        return;
    }
    
    qInfo() << "Setting up TUI widgets for pageSplash...";
    
    // Create TUI label for splash message
    tui_labelSplash = new ui::QLabel();
    tui_labelSplash->setPosition(8, 10);  // Centered-ish position
    tui_labelSplash->setText(tr("Welcome to %1 Installer").arg(PROJECTNAME));
    tui_labelSplash->show();
}

void MInstall::setupPageEndTUI() noexcept
{
    if (!ui::Context::isTUI()) {
        return;
    }

    qInfo() << "Setting up TUI widgets for pageEnd...";

    // Create TUI label for reminders text
    tui_textReminders = new ui::QLabel();
    tui_textReminders->setPosition(3, 5);
    tui_textReminders->setText("Support information will be displayed here.");
    tui_textReminders->show();
    
    // Note: Reboot checkbox is now on TIPS page, not here
}

void MInstall::setupPageTermsTUI() noexcept
{
    if (!ui::Context::isTUI()) {
        return;
    }
    
    qInfo() << "Setting up TUI widgets for pageTerms...";
    
    // Create TUI label for copyright/terms text
    tui_textCopyright = new ui::QLabel();
    tui_textCopyright->setPosition(3, 2);
    tui_textCopyright->setText(tr("%1 is an independent Linux distribution based on Debian Stable.\n\n"
        "%1 uses some components from MEPIS Linux which are released under an Apache free license."
        " Some MEPIS components have been modified for %1.\n\nEnjoy using %1").arg(PROJECTNAME));
    tui_textCopyright->show();
    
    // Keyboard settings not available in TUI mode
}

void MInstall::setupPageInstallationTUI() noexcept
{
    if (!ui::Context::isTUI()) {
        return;
    }

    qInfo() << "Setting up TUI widgets for pageInstallation...";

    // Create button group for radio buttons
    tui_installButtonGroup = new qtui::TButtonGroup(this);

    // Create TUI label for install type heading
    tui_labelInstallType = new ui::QLabel();
    tui_labelInstallType->setPosition(2, 2);
    tui_labelInstallType->setText(gui.boxInstallationType->title());
    tui_labelInstallType->show();

    // Create TUI radio buttons
    tui_radioEntireDrive = new ui::QRadioButton();
    tui_radioEntireDrive->setText(gui.radioEntireDrive->text());
    tui_radioEntireDrive->setPosition(4, 4);
    tui_radioEntireDrive->setChecked(true);
    tui_radioEntireDrive->setFocus(true);  // Initial focus on first option
    tui_radioEntireDrive->setButtonGroup(nullptr, tui_installButtonGroup);
    tui_radioEntireDrive->show();

    tui_radioCustomPart = new ui::QRadioButton();
    tui_radioCustomPart->setText(gui.radioCustomPart->text());
    tui_radioCustomPart->setPosition(6, 4);
    tui_radioCustomPart->setButtonGroup(nullptr, tui_installButtonGroup);
    tui_radioCustomPart->show();

    tui_radioReplace = new ui::QRadioButton();
    tui_radioReplace->setText(gui.radioReplace->text());
    tui_radioReplace->setPosition(8, 4);
    tui_radioReplace->setButtonGroup(nullptr, tui_installButtonGroup);
    tui_radioReplace->show();

    tui_checkDualDrive = new ui::QCheckBox();
    tui_checkDualDrive->setText(gui.checkDualDrive->text());
    tui_checkDualDrive->setPosition(4, 60);
    tui_checkDualDrive->show();

    tui_labelDriveSystem = new ui::QLabel();
    tui_labelDriveSystem->setPosition(10, 4);
    tui_labelDriveSystem->setText(gui.labelDriveSystem->text());
    tui_labelDriveSystem->show();

    tui_comboDriveSystem = new ui::QComboBox();
    tui_comboDriveSystem->setPosition(10, 20);
    tui_comboDriveSystem->setWidth(30);
    tui_comboDriveSystem->show();

    tui_labelDriveHome = new ui::QLabel();
    tui_labelDriveHome->setPosition(12, 4);
    tui_labelDriveHome->setText(gui.labelDriveHome->text());
    tui_labelDriveHome->show();

    tui_comboDriveHome = new ui::QComboBox();
    tui_comboDriveHome->setPosition(12, 20);
    tui_comboDriveHome->setWidth(30);
    tui_comboDriveHome->show();

    tui_sliderPart = new ui::QSlider();
    tui_sliderPart->setPosition(14, 4);
    tui_sliderPart->setWidth(50);
    tui_sliderPart->setRange(0, 100);
    tui_sliderPart->setSingleStep(1);
    tui_sliderPart->setPageStep(10);
    tui_sliderPart->setLeftLabel(gui.labelSliderRoot->text());
    tui_sliderPart->setRightLabel(gui.labelSliderHome->text());
    tui_sliderPart->show();

    tui_checkEncryptAuto = new ui::QCheckBox();
    tui_checkEncryptAuto->setPosition(16, 4);
    tui_checkEncryptAuto->setText(gui.checkEncryptAuto->text());
    tui_checkEncryptAuto->show();

    tui_focusInstallationField = 0;
    syncInstallationTuiFromGui();
}

void MInstall::syncInstallationTuiFromGui() noexcept
{
    if (!tui_comboDriveSystem || !tui_comboDriveHome || !tui_checkDualDrive) {
        return;
    }

    if (tui_comboDriveSystem->count() != gui.comboDriveSystem->count()) {
        tui_comboDriveSystem->clear();
        for (int i = 0; i < gui.comboDriveSystem->count(); ++i) {
            tui_comboDriveSystem->addItem(gui.comboDriveSystem->itemText(i), gui.comboDriveSystem->itemData(i));
        }
    }
    if (tui_comboDriveHome->count() != gui.comboDriveHome->count()) {
        tui_comboDriveHome->clear();
        for (int i = 0; i < gui.comboDriveHome->count(); ++i) {
            tui_comboDriveHome->addItem(gui.comboDriveHome->itemText(i), gui.comboDriveHome->itemData(i));
        }
    }

    tui_comboDriveSystem->setCurrentIndex(gui.comboDriveSystem->currentIndex());
    tui_comboDriveHome->setCurrentIndex(gui.comboDriveHome->currentIndex());

    tui_checkDualDrive->setChecked(gui.checkDualDrive->isChecked());
    if (tui_checkEncryptAuto) {
        tui_checkEncryptAuto->setChecked(gui.checkEncryptAuto->isChecked());
    }

    if (tui_radioEntireDrive) tui_radioEntireDrive->setChecked(gui.radioEntireDrive->isChecked());
    if (tui_radioCustomPart) tui_radioCustomPart->setChecked(gui.radioCustomPart->isChecked());
    if (tui_radioReplace) tui_radioReplace->setChecked(gui.radioReplace->isChecked());
    if (gui.radioEntireDrive->isChecked()) tui_focusInstallation = 0;
    if (gui.radioCustomPart->isChecked()) tui_focusInstallation = 1;
    if (gui.radioReplace->isChecked()) tui_focusInstallation = 2;

    if (tui_sliderPart) {
        // Sync slider range from GUI spinbox limits
        tui_sliderPart->setMinimum(gui.spinRoot->minimum());
        tui_sliderPart->setMaximum(100); // Allow 100% for combined root+home
        if (tui_focusInstallationField != 6) {
            tui_sliderPart->setValue(gui.spinRoot->value());
        }
    }

    const bool autoPartEnabled = gui.boxAutoPart->isEnabled();
    const bool dualDrive = gui.checkDualDrive->isChecked();
    if (tui_labelDriveSystem) tui_labelDriveSystem->setEnabled(autoPartEnabled);
    tui_comboDriveSystem->setEnabled(autoPartEnabled);
    if (tui_labelDriveHome) tui_labelDriveHome->setEnabled(autoPartEnabled && dualDrive);
    tui_comboDriveHome->setEnabled(autoPartEnabled && dualDrive);
    // Slider only visible for regular install without dual drives
    if (tui_sliderPart) {
        const bool showSlider = autoPartEnabled && !dualDrive && gui.radioEntireDrive->isChecked();
        if (showSlider) tui_sliderPart->show();
        else tui_sliderPart->hide();
    }
    if (tui_checkDualDrive) tui_checkDualDrive->setEnabled(autoPartEnabled);
    if (tui_checkEncryptAuto) tui_checkEncryptAuto->setEnabled(autoPartEnabled);
}

void MInstall::setupPageConfirmTUI() noexcept
{
    if (!ui::Context::isTUI()) {
        return;
    }

    qInfo() << "Setting up TUI widgets for pageConfirm...";

    // Create TUI label for confirm title
    tui_labelConfirmTitle = new ui::QLabel();
    tui_labelConfirmTitle->setPosition(2, 2);
    tui_labelConfirmTitle->setText(tr("Installation Confirmation"));
    tui_labelConfirmTitle->show();

    // Create TUI label for confirmation info
    tui_labelConfirmInfo = new ui::QLabel();
    tui_labelConfirmInfo->setPosition(4, 2);
    tui_labelConfirmInfo->setText(tr("Please review the installation settings.\n\n"
        "This is the last opportunity to check and confirm\n"
        "the actions of the installation process.\n\n"
        "Press ENTER to begin installation or Backspace to go back."));
    tui_labelConfirmInfo->show();
}

void MInstall::setupPageNetworkTUI() noexcept
{
    if (!ui::Context::isTUI()) {
        return;
    }

    qInfo() << "Setting up TUI widgets for pageNetwork...";

    // Label for Computer Name
    tui_labelComputerName = new ui::QLabel();
    tui_labelComputerName->setPosition(3, 2);
    tui_labelComputerName->setText(gui.labelComputerName->text());
    tui_labelComputerName->show();

    // LineEdit for Computer Name
    tui_textComputerName = new ui::QLineEdit();
    tui_textComputerName->setPosition(3, 20);
    tui_textComputerName->setWidth(30);
    tui_textComputerName->setPlaceholderText(tr("Enter computer name"));
    tui_textComputerName->show();
    tui_textComputerName->setFocus();
    tui_textComputerName->setText(gui.textComputerName->text());

    // Label for Domain
    tui_labelComputerDomain = new ui::QLabel();
    tui_labelComputerDomain->setPosition(5, 2);
    tui_labelComputerDomain->setText(gui.labelComputerDomain->text());
    tui_labelComputerDomain->show();

    // LineEdit for Domain
    tui_textComputerDomain = new ui::QLineEdit();
    tui_textComputerDomain->setPosition(5, 20);
    tui_textComputerDomain->setWidth(30);
    tui_textComputerDomain->setPlaceholderText(tr("Enter domain name"));
    tui_textComputerDomain->show();
    tui_textComputerDomain->setText(gui.textComputerDomain->text());

    // Label for Workgroup
    tui_labelComputerGroup = new ui::QLabel();
    tui_labelComputerGroup->setPosition(7, 2);
    tui_labelComputerGroup->setText(gui.labelComputerGroup->text());
    tui_labelComputerGroup->show();

    // LineEdit for Workgroup
    tui_textComputerGroup = new ui::QLineEdit();
    tui_textComputerGroup->setPosition(7, 20);
    tui_textComputerGroup->setWidth(30);
    tui_textComputerGroup->setPlaceholderText(tr("Workgroup"));
    tui_textComputerGroup->show();
    tui_textComputerGroup->setText(gui.textComputerGroup->text());

    // Samba checkbox
    tui_checkSamba = new ui::QCheckBox();
    tui_checkSamba->setPosition(9, 2);
    tui_checkSamba->setText(gui.checkSamba->text());
    tui_checkSamba->show();
    tui_checkSamba->setChecked(gui.checkSamba->isChecked());
    tui_checkSamba->setEnabled(gui.checkSamba->isEnabled());
    if (tui_labelComputerGroup) tui_labelComputerGroup->setEnabled(gui.textComputerGroup->isEnabled());
    if (tui_textComputerGroup) tui_textComputerGroup->setEnabled(gui.textComputerGroup->isEnabled());
    tui_networkError.clear();
}

void MInstall::setupPageBootTUI() noexcept
{
    if (!ui::Context::isTUI()) {
        return;
    }

    qInfo() << "Setting up TUI widgets for pageBoot...";

    // Create button group for radio buttons
    tui_bootButtonGroup = new qtui::TButtonGroup(this);

    tui_checkBootInstall = new ui::QCheckBox();
    tui_checkBootInstall->setPosition(4, 2);
    tui_checkBootInstall->setText(gui.boxBoot->title());
    tui_checkBootInstall->setChecked(gui.boxBoot->isChecked());
    tui_checkBootInstall->show();
    tui_checkBootInstall->setFocus(true);

    tui_labelBootLocation = new ui::QLabel();
    tui_labelBootLocation->setPosition(6, 4);
    tui_labelBootLocation->setText(gui.labelBoot->text());
    tui_labelBootLocation->show();

    tui_comboBoot = new ui::QComboBox();
    tui_comboBoot->setPosition(6, 24);
    tui_comboBoot->setWidth(30);
    tui_comboBoot->show();

    // Radio buttons for boot location
    tui_radioBootMBR = new ui::QRadioButton();
    tui_radioBootMBR->setText(gui.radioBootMBR->text());
    tui_radioBootMBR->setPosition(8, 6);
    tui_radioBootMBR->setChecked(true);
    tui_radioBootMBR->setButtonGroup(nullptr, tui_bootButtonGroup);
    tui_radioBootMBR->show();

    tui_radioBootPBR = new ui::QRadioButton();
    tui_radioBootPBR->setText(gui.radioBootPBR->text());
    tui_radioBootPBR->setPosition(10, 6);
    tui_radioBootPBR->setButtonGroup(nullptr, tui_bootButtonGroup);
    tui_radioBootPBR->show();

    tui_radioBootESP = new ui::QRadioButton();
    tui_radioBootESP->setText(gui.radioBootESP->text());
    tui_radioBootESP->setPosition(12, 6);
    tui_radioBootESP->setButtonGroup(nullptr, tui_bootButtonGroup);
    tui_radioBootESP->show();

    tui_checkBootHostSpecific = new ui::QCheckBox();
    tui_checkBootHostSpecific->setPosition(14, 4);
    tui_checkBootHostSpecific->setText(gui.checkBootHostSpecific->text());
    tui_checkBootHostSpecific->show();

    tui_focusBootField = 0;
}

void MInstall::setupPageLocalizationTUI() noexcept
{
    if (!ui::Context::isTUI()) {
        return;
    }

    qInfo() << "Setting up TUI widgets for pageLocalization...";

    // Title label
    tui_labelLocaleTitle = new ui::QLabel();
    tui_labelLocaleTitle->setPosition(2, 2);
    tui_labelLocaleTitle->setText(tr("Localization Settings"));
    tui_labelLocaleTitle->show();

    // Locale selection
    tui_labelLocale = new ui::QLabel();
    tui_labelLocale->setPosition(4, 2);
    tui_labelLocale->setText(tr("Locale:"));
    tui_labelLocale->show();

    tui_comboLocale = new ui::QComboBox();
    tui_comboLocale->setPosition(4, 20);
    tui_comboLocale->setWidth(50);
    tui_comboLocale->show();

    // Time Area selection
    tui_labelTimeArea = new ui::QLabel();
    tui_labelTimeArea->setPosition(6, 2);
    tui_labelTimeArea->setText(tr("Time Area:"));
    tui_labelTimeArea->show();

    tui_comboTimeArea = new ui::QComboBox();
    tui_comboTimeArea->setPosition(6, 20);
    tui_comboTimeArea->setWidth(50);
    tui_comboTimeArea->show();

    // Time Zone selection
    tui_labelTimeZone = new ui::QLabel();
    tui_labelTimeZone->setPosition(8, 2);
    tui_labelTimeZone->setText(tr("Time Zone:"));
    tui_labelTimeZone->show();

    tui_comboTimeZone = new ui::QComboBox();
    tui_comboTimeZone->setPosition(8, 20);
    tui_comboTimeZone->setWidth(50);
    tui_comboTimeZone->show();

    tui_checkLocalClock = new ui::QCheckBox();
    tui_checkLocalClock->setPosition(10, 2);
    tui_checkLocalClock->setText(gui.checkLocalClock->text());
    tui_checkLocalClock->setChecked(gui.checkLocalClock->isChecked());
    tui_checkLocalClock->show();

    tui_labelClockFormat = new ui::QLabel();
    tui_labelClockFormat->setPosition(12, 2);
    tui_labelClockFormat->setText(gui.labelClockFormat->text());
    tui_labelClockFormat->show();

    tui_clockButtonGroup = new qtui::TButtonGroup(this);
    tui_radioClock24 = new ui::QRadioButton();
    tui_radioClock24->setPosition(12, 20);
    tui_radioClock24->setText(gui.radioClock24->text());
    tui_radioClock24->setButtonGroup(nullptr, tui_clockButtonGroup);
    tui_radioClock24->setChecked(gui.radioClock24->isChecked());
    tui_radioClock24->show();

    tui_radioClock12 = new ui::QRadioButton();
    tui_radioClock12->setPosition(12, 32);
    tui_radioClock12->setText(gui.radioClock12->text());
    tui_radioClock12->setButtonGroup(nullptr, tui_clockButtonGroup);
    tui_radioClock12->setChecked(gui.radioClock12->isChecked());
    tui_radioClock12->show();

    // Copy items from GUI combo boxes to TUI combo boxes
    if (tui_comboLocale) {
        for (int i = 0; i < gui.comboLocale->count(); ++i) {
            tui_comboLocale->addItem(gui.comboLocale->itemText(i), gui.comboLocale->itemData(i));
        }
        tui_comboLocale->setCurrentIndex(gui.comboLocale->currentIndex());
    }

    if (tui_comboTimeArea) {
        for (int i = 0; i < gui.comboTimeArea->count(); ++i) {
            tui_comboTimeArea->addItem(gui.comboTimeArea->itemText(i), gui.comboTimeArea->itemData(i));
        }
        tui_comboTimeArea->setCurrentIndex(gui.comboTimeArea->currentIndex());
    }

    if (tui_comboTimeZone) {
        for (int i = 0; i < gui.comboTimeZone->count(); ++i) {
            tui_comboTimeZone->addItem(gui.comboTimeZone->itemText(i), gui.comboTimeZone->itemData(i));
        }
        tui_comboTimeZone->setCurrentIndex(gui.comboTimeZone->currentIndex());
    }

    tui_focusLocalization = 0;
}

void MInstall::setupPageServicesTUI() noexcept
{
    if (!ui::Context::isTUI()) {
        return;
    }

    qInfo() << "Setting up TUI widgets for pageServices...";

    // Title label
    tui_labelServicesTitle = new ui::QLabel();
    tui_labelServicesTitle->setPosition(2, 2);
    tui_labelServicesTitle->setText(gui.boxServices->title());
    tui_labelServicesTitle->show();

    // Info label
    tui_labelServicesInfo = new ui::QLabel();
    tui_labelServicesInfo->setPosition(4, 2);
    tui_labelServicesInfo->setText(tr("System services will be configured with default settings.\n"
        "You can modify service settings after installation\n"
        "using your system's service manager.\n\n"
        "Press ENTER to continue."));
    tui_labelServicesInfo->show();
}

void MInstall::setupPageReplaceTUI() noexcept
{
    if (!ui::Context::isTUI()) {
        return;
    }

    qInfo() << "Setting up TUI widgets for pageReplace...";

    // Title label
    tui_labelReplaceTitle = new ui::QLabel();
    tui_labelReplaceTitle->setPosition(2, 2);
    tui_labelReplaceTitle->setText(tr("Replace Existing Installation"));
    tui_labelReplaceTitle->show();

    tui_focusReplace = 0;
}

void MInstall::setupPagePartitionsTUI() noexcept
{
    if (!ui::Context::isTUI()) {
        return;
    }

    qInfo() << "Setting up TUI widgets for pagePartitions...";

    tui_labelPartitionsInfo = new ui::QLabel();
    tui_labelPartitionsInfo->setPosition(1, 2);
    tui_labelPartitionsInfo->setText(tr("Custom Partitioning"));
    tui_labelPartitionsInfo->show();

    tui_partitionRow = 0;
    tui_partitionCol = 0;
    tui_partitionScroll = 0;
    tui_partitionEditing = false;
    tui_partitionEditIsLabel = false;
    tui_partitionEditIsSize = false;
    tui_partitionUnlocking = false;
    tui_unlockDevice = nullptr;
    tui_unlockError.clear();

    if (!tui_buttonPartitionsApply) {
        tui_buttonPartitionsApply = new qtui::TPushButton(tr("Apply"));
        tui_buttonPartitionsApply->setPosition(19, 2);
        tui_buttonPartitionsApply->show();
        connect(tui_buttonPartitionsApply, &qtui::TPushButton::clicked, this, [this]() noexcept {
            gotoAfterPartitionsTui();
        });
    }

    if (!tui_partitionLabelEdit) {
        tui_partitionLabelEdit = new qtui::TLineEdit();
        tui_partitionLabelEdit->setWidth(10);
    }
    if (!tui_partitionSizeEdit) {
        tui_partitionSizeEdit = new qtui::TLineEdit();
        tui_partitionSizeEdit->setWidth(8);
    }
    if (!tui_unlockPassEdit) {
        tui_unlockPassEdit = new qtui::TLineEdit();
        tui_unlockPassEdit->setWidth(24);
        tui_unlockPassEdit->setEchoMode(qtui::TLineEdit::Password);
    }
}

void MInstall::setupPageSwapTUI() noexcept
{
    if (!ui::Context::isTUI()) {
        return;
    }

    qInfo() << "Setting up TUI widgets for pageSwap...";

    // Title label
    tui_labelSwapTitle = new ui::QLabel();
    tui_labelSwapTitle->setPosition(2, 2);
    tui_labelSwapTitle->setText(tr("Swap File Configuration"));
    tui_labelSwapTitle->show();

    // Swap file checkbox
    tui_checkSwapFile = new ui::QCheckBox();
    tui_checkSwapFile->setText(tr("Create a swap file"));
    tui_checkSwapFile->setPosition(4, 4);
    tui_checkSwapFile->setFocus(true);
    tui_checkSwapFile->show();

    // Swap file location
    tui_labelSwapLocation = new ui::QLabel();
    tui_labelSwapLocation->setPosition(6, 6);
    tui_labelSwapLocation->setText(tr("Location:"));
    tui_labelSwapLocation->show();

    tui_textSwapFile = new ui::QLineEdit();
    tui_textSwapFile->setPosition(6, 18);
    tui_textSwapFile->setWidth(28);
    tui_textSwapFile->show();

    // Swap size
    tui_labelSwapSize = new ui::QLabel();
    tui_labelSwapSize->setPosition(8, 6);
    tui_labelSwapSize->setText(tr("Size (MB):"));
    tui_labelSwapSize->show();

    tui_textSwapSize = new ui::QLineEdit();
    tui_textSwapSize->setPosition(8, 18);
    tui_textSwapSize->setWidth(8);
    tui_textSwapSize->show();

    tui_labelSwapMax = new ui::QLabel();
    tui_labelSwapMax->setPosition(8, 28);
    tui_labelSwapMax->show();

    tui_labelSwapReset = new ui::QLabel();
    tui_labelSwapReset->setPosition(8, 40);
    tui_labelSwapReset->setText(tr("R: reset size"));
    tui_labelSwapReset->show();

    // Hibernation support
    tui_checkHibernation = new ui::QCheckBox();
    tui_checkHibernation->setPosition(10, 6);
    tui_checkHibernation->setText(tr("Enable hibernation support"));
    tui_checkHibernation->show();

    // Zram section
    tui_labelZramTitle = new ui::QLabel();
    tui_labelZramTitle->setPosition(12, 4);
    tui_labelZramTitle->setText(tr("Zram swap"));
    tui_labelZramTitle->show();

    tui_checkZram = new ui::QCheckBox();
    tui_checkZram->setPosition(13, 6);
    tui_checkZram->setText(tr("Enable zram swap"));
    tui_checkZram->show();

    tui_zramButtonGroup = new qtui::TButtonGroup(this);

    tui_radioZramPercent = new ui::QRadioButton();
    tui_radioZramPercent->setPosition(15, 8);
    tui_radioZramPercent->setText(tr("Allocate based on RAM (%):"));
    tui_radioZramPercent->setButtonGroup(nullptr, tui_zramButtonGroup);
    tui_radioZramPercent->show();

    tui_textZramPercent = new ui::QLineEdit();
    tui_textZramPercent->setPosition(15, 38);
    tui_textZramPercent->setWidth(6);
    tui_textZramPercent->show();

    tui_labelZramRecPercent = new ui::QLabel();
    tui_labelZramRecPercent->setPosition(15, 46);
    tui_labelZramRecPercent->show();

    tui_radioZramSize = new ui::QRadioButton();
    tui_radioZramSize->setPosition(17, 8);
    tui_radioZramSize->setText(tr("Allocate fixed size (MB):"));
    tui_radioZramSize->setButtonGroup(nullptr, tui_zramButtonGroup);
    tui_radioZramSize->show();

    tui_textZramSize = new ui::QLineEdit();
    tui_textZramSize->setPosition(17, 38);
    tui_textZramSize->setWidth(8);
    tui_textZramSize->show();

    tui_labelZramRecSize = new ui::QLabel();
    tui_labelZramRecSize->setPosition(17, 48);
    tui_labelZramRecSize->show();

    tui_focusSwap = 0;
    syncSwapTuiFromGui();
}

void MInstall::syncSwapTuiFromGui() noexcept
{
    if (!tui_checkSwapFile || !tui_checkZram) {
        return;
    }

    const bool swapEnabled = gui.boxSwapFile->isChecked();
    const bool zramEnabled = gui.boxSwapZram->isChecked();
    const bool zramPercent = gui.radioZramPercent->isChecked();
    const bool zramSize = gui.radioZramSize->isChecked();

    tui_checkSwapFile->setChecked(swapEnabled);
    if (tui_textSwapFile && tui_focusSwap != 1) {
        tui_textSwapFile->setText(gui.textSwapFile->text());
    }
    if (tui_textSwapSize && tui_focusSwap != 2) {
        tui_textSwapSize->setText(QString::number(gui.spinSwapSize->value()));
    }
    if (tui_labelSwapMax) {
        tui_labelSwapMax->setText(gui.labelSwapMax->text());
    }
    if (tui_labelSwapReset) {
        tui_labelSwapReset->setEnabled(swapEnabled);
    }
    if (tui_checkHibernation) {
        tui_checkHibernation->setChecked(gui.checkHibernation->isChecked());
    }

    tui_checkZram->setChecked(zramEnabled);
    if (tui_radioZramPercent) {
        tui_radioZramPercent->setChecked(zramPercent);
    }
    if (tui_radioZramSize) {
        tui_radioZramSize->setChecked(zramSize);
    }
    if (tui_textZramPercent && tui_focusSwap != 6) {
        tui_textZramPercent->setText(QString::number(gui.spinZramPercent->value()));
    }
    if (tui_textZramSize && tui_focusSwap != 8) {
        tui_textZramSize->setText(QString::number(gui.spinZramSize->value()));
    }
    if (tui_labelZramRecPercent) {
        tui_labelZramRecPercent->setText(gui.labelZramRecPercent->text());
    }
    if (tui_labelZramRecSize) {
        tui_labelZramRecSize->setText(gui.labelZramRecSize->text());
    }

    if (tui_labelSwapLocation) tui_labelSwapLocation->setEnabled(swapEnabled);
    if (tui_textSwapFile) tui_textSwapFile->setEnabled(swapEnabled);
    if (tui_labelSwapSize) tui_labelSwapSize->setEnabled(swapEnabled);
    if (tui_textSwapSize) tui_textSwapSize->setEnabled(swapEnabled);
    if (tui_labelSwapMax) tui_labelSwapMax->setEnabled(swapEnabled);
    if (tui_checkHibernation) tui_checkHibernation->setEnabled(swapEnabled && gui.checkHibernation->isEnabled());

    if (tui_labelZramTitle) tui_labelZramTitle->setEnabled(true);
    if (tui_checkZram) tui_checkZram->setEnabled(true);
    if (tui_radioZramPercent) tui_radioZramPercent->setEnabled(zramEnabled);
    if (tui_radioZramSize) tui_radioZramSize->setEnabled(zramEnabled);
    if (tui_textZramPercent) tui_textZramPercent->setEnabled(zramEnabled && zramPercent);
    if (tui_labelZramRecPercent) tui_labelZramRecPercent->setEnabled(zramEnabled && zramPercent);
    if (tui_textZramSize) tui_textZramSize->setEnabled(zramEnabled && zramSize);
    if (tui_labelZramRecSize) tui_labelZramRecSize->setEnabled(zramEnabled && zramSize);
}

void MInstall::buildServicesTui() noexcept
{
    if (tui_servicesBuilt) {
        return;
    }

    if (gui.treeServices->topLevelItemCount() == 0) {
        return;
    }

    int row = 6;
    for (int i = 0; i < gui.treeServices->topLevelItemCount(); ++i) {
        QTreeWidgetItem *parent = gui.treeServices->topLevelItem(i);
        if (!parent) {
            continue;
        }
        auto *category = new ui::QLabel();
        category->setPosition(row, 2);
        category->setText(parent->text(0));
        category->show();
        tui_serviceCategoryLabels.append(category);
        row += 1;

        for (int j = 0; j < parent->childCount(); ++j) {
            QTreeWidgetItem *child = parent->child(j);
            if (!child) {
                continue;
            }
            auto *checkbox = new ui::QCheckBox();
            const QString label = child->text(0) + u" - "_s + child->text(1);
            checkbox->setText(label);
            checkbox->setPosition(row, 4);
            checkbox->setChecked(child->checkState(0) == Qt::Checked);
            checkbox->show();
            tui_serviceItems.append({checkbox, child, row});
            row += 1;
        }
    }

    tui_servicesBuilt = true;
    tui_focusServices = 0;
}

void MInstall::syncServicesTuiFromGui() noexcept
{
    if (!tui_servicesBuilt) {
        return;
    }
    for (auto &item : tui_serviceItems) {
        if (item.checkbox && item.item) {
            item.checkbox->setChecked(item.item->checkState(0) == Qt::Checked);
        }
    }
}

void MInstall::setupPageEncryptionTUI() noexcept
{
    if (!ui::Context::isTUI()) {
        return;
    }

    qInfo() << "Setting up TUI widgets for pageEncryption...";

    tui_labelEncryptionTitle = new ui::QLabel();
    tui_labelEncryptionTitle->setPosition(2, 2);
    tui_labelEncryptionTitle->setText(tr("Encryption options"));
    tui_labelEncryptionTitle->show();

    tui_labelCryptoPass = new ui::QLabel();
    tui_labelCryptoPass->setPosition(4, 4);
    tui_labelCryptoPass->setText(tr("Encryption password:"));
    tui_labelCryptoPass->show();

    tui_textCryptoPass = new ui::QLineEdit();
    tui_textCryptoPass->setPosition(4, 28);
    tui_textCryptoPass->setWidth(26);
    tui_textCryptoPass->setEchoMode(QLineEdit::Password);
    tui_textCryptoPass->show();

    tui_labelCryptoPass2 = new ui::QLabel();
    tui_labelCryptoPass2->setPosition(6, 4);
    tui_labelCryptoPass2->setText(tr("Confirm password:"));
    tui_labelCryptoPass2->show();

    tui_textCryptoPass2 = new ui::QLineEdit();
    tui_textCryptoPass2->setPosition(6, 28);
    tui_textCryptoPass2->setWidth(26);
    tui_textCryptoPass2->setEchoMode(QLineEdit::Password);
    tui_textCryptoPass2->show();

    if (tui_textCryptoPass) {
        tui_textCryptoPass->setText(gui.textCryptoPass->text());
        tui_textCryptoPass->setFocus();
    }
    if (tui_textCryptoPass2) {
        tui_textCryptoPass2->setText(gui.textCryptoPass2->text());
    }

    tui_focusEncryption = 0;
    tui_encryptionError.clear();
}

void MInstall::setupPageUserAccountsTUI() noexcept
{
    if (!ui::Context::isTUI()) {
        return;
    }

    qInfo() << "Setting up TUI widgets for pageUserAccounts...";

    const int labelCol = 2;
    const int fieldCol = 28;
    const int fieldWidth = 25;

    // Label and LineEdit for Username
    tui_labelUserName = new ui::QLabel();
    tui_labelUserName->setPosition(3, labelCol);
    tui_labelUserName->setText(gui.labelUserName->text());
    tui_labelUserName->show();

    tui_textUserName = new ui::QLineEdit();
    tui_textUserName->setPosition(3, fieldCol);
    tui_textUserName->setWidth(fieldWidth);
    tui_textUserName->setPlaceholderText(tr("Enter username"));
    tui_textUserName->show();
    tui_textUserName->setFocus();

    // Label and LineEdit for Password
    tui_labelUserPass = new ui::QLabel();
    tui_labelUserPass->setPosition(5, labelCol);
    tui_labelUserPass->setText(gui.labelUserPass->text());
    tui_labelUserPass->show();

    tui_textUserPass = new ui::QLineEdit();
    tui_textUserPass->setPosition(5, fieldCol);
    tui_textUserPass->setWidth(fieldWidth);
    tui_textUserPass->setEchoMode(QLineEdit::Password);
    tui_textUserPass->setPlaceholderText(tr("Enter password"));
    tui_textUserPass->show();

    // Label and LineEdit for Password Confirmation
    tui_labelUserPass2 = new ui::QLabel();
    tui_labelUserPass2->setPosition(7, labelCol);
    tui_labelUserPass2->setText(gui.labelUserPass2->text());
    tui_labelUserPass2->show();

    tui_textUserPass2 = new ui::QLineEdit();
    tui_textUserPass2->setPosition(7, fieldCol);
    tui_textUserPass2->setWidth(fieldWidth);
    tui_textUserPass2->setEchoMode(QLineEdit::Password);
    tui_textUserPass2->setPlaceholderText(tr("Re-enter password"));
    tui_textUserPass2->show();

    // Checkbox for root account
    tui_checkRootAccount = new ui::QCheckBox();
    tui_checkRootAccount->setPosition(9, labelCol);
    tui_checkRootAccount->setText(gui.boxRootAccount->title());
    tui_checkRootAccount->setChecked(false);
    tui_checkRootAccount->show();

    tui_labelRootPass = new ui::QLabel();
    tui_labelRootPass->setPosition(11, labelCol);
    tui_labelRootPass->setText(gui.labelRootPass->text());
    tui_labelRootPass->show();

    tui_textRootPass = new ui::QLineEdit();
    tui_textRootPass->setPosition(11, fieldCol);
    tui_textRootPass->setWidth(fieldWidth);
    tui_textRootPass->setEchoMode(QLineEdit::Password);
    tui_textRootPass->show();

    tui_labelRootPass2 = new ui::QLabel();
    tui_labelRootPass2->setPosition(13, labelCol);
    tui_labelRootPass2->setText(gui.labelRootPass2->text());
    tui_labelRootPass2->show();

    tui_textRootPass2 = new ui::QLineEdit();
    tui_textRootPass2->setPosition(13, fieldCol);
    tui_textRootPass2->setWidth(fieldWidth);
    tui_textRootPass2->setEchoMode(QLineEdit::Password);
    tui_textRootPass2->show();

    tui_checkAutoLogin = new ui::QCheckBox();
    tui_checkAutoLogin->setPosition(15, labelCol);
    tui_checkAutoLogin->setText(gui.checkAutoLogin->text());
    tui_checkAutoLogin->setChecked(false);
    tui_checkAutoLogin->show();

    tui_checkSaveDesktop = new ui::QCheckBox();
    tui_checkSaveDesktop->setPosition(17, labelCol);
    tui_checkSaveDesktop->setText(gui.checkSaveDesktop->text());
    tui_checkSaveDesktop->setChecked(false);
    tui_checkSaveDesktop->show();

    tui_userError.clear();
    tui_confirmEmptyUserPass = false;
    tui_confirmEmptyRootPass = false;
}

void MInstall::setupPageOldHomeTUI() noexcept
{
    if (!ui::Context::isTUI()) {
        return;
    }

    qInfo() << "Setting up TUI widgets for pageOldHome...";

    // Title label
    tui_labelOldHomeTitle = new ui::QLabel();
    tui_labelOldHomeTitle->setPosition(2, 2);
    tui_labelOldHomeTitle->setText(tr("Old Home Directory"));
    tui_labelOldHomeTitle->show();

    // Info label explaining the situation
    tui_labelOldHomeInfo = new ui::QLabel();
    tui_labelOldHomeInfo->setPosition(4, 2);
    tui_labelOldHomeInfo->setText(tr("A home directory already exists for this user name.\nChoose what to do with it:"));
    tui_labelOldHomeInfo->show();

    // Radio button: Re-use existing home
    tui_radioOldHomeUse = new ui::QRadioButton();
    tui_radioOldHomeUse->setPosition(7, 2);
    tui_radioOldHomeUse->setText(tr("Re-use it for this installation"));
    tui_radioOldHomeUse->setChecked(gui.radioOldHomeUse->isChecked());
    tui_radioOldHomeUse->show();

    // Radio button: Rename and create new
    tui_radioOldHomeSave = new ui::QRadioButton();
    tui_radioOldHomeSave->setPosition(8, 2);
    tui_radioOldHomeSave->setText(tr("Rename it and create a new directory"));
    tui_radioOldHomeSave->setChecked(gui.radioOldHomeSave->isChecked());
    tui_radioOldHomeSave->show();

    // Radio button: Delete and create new
    tui_radioOldHomeDelete = new ui::QRadioButton();
    tui_radioOldHomeDelete->setPosition(9, 2);
    tui_radioOldHomeDelete->setText(tr("Delete it and create a new directory"));
    tui_radioOldHomeDelete->setChecked(gui.radioOldHomeDelete->isChecked());
    tui_radioOldHomeDelete->show();

    tui_oldHomeError.clear();
}

void MInstall::setupPageTipsTUI() noexcept
{
    if (!ui::Context::isTUI()) {
        return;
    }

    qInfo() << "Setting up TUI widgets for pageTips...";

    // Title label
    tui_labelTipsTitle = new ui::QLabel();
    tui_labelTipsTitle->setPosition(2, 2);
    tui_labelTipsTitle->setText(tr("Installation in Progress"));
    tui_labelTipsTitle->show();

    // Info label
    tui_labelTipsInfo = new ui::QLabel();
    tui_labelTipsInfo->setPosition(4, 2);
    tui_labelTipsInfo->setText(tr("The system is being installed.\nThis may take several minutes."));
    tui_labelTipsInfo->show();
    
    // Reboot checkbox
    tui_checkTipsReboot = new ui::QCheckBox();
    tui_checkTipsReboot->setPosition(14, 2);
    tui_checkTipsReboot->setText(tr("Reboot automatically when installation completes"));
    tui_checkTipsReboot->setChecked(false);  // Disabled by default
    tui_checkTipsReboot->show();
}

void MInstall::renderPageSplash() noexcept
{
    if (!ui::Context::isTUI()) {
        return;
    }
    
    if (tui_labelSplash) {
        auto* tuiWidget = dynamic_cast<qtui::TLabel*>(tui_labelSplash->tuiWidget());
        if (tuiWidget) {
            tuiWidget->render();
        }
    }
    
    // Show instruction at bottom
    mvprintw(12, 10, "Press ENTER or SPACE to continue...");
}

void MInstall::renderPageEnd() noexcept
{
    if (!ui::Context::isTUI()) {
        return;  // Only render in TUI mode
    }

    // Note: Reboot checkbox removed from END page (now on TIPS page)
    
    if (tui_textReminders) {
        auto* tuiWidget = dynamic_cast<qtui::TLabel*>(tui_textReminders->tuiWidget());
        if (tuiWidget) {
            tuiWidget->render();
        }
    }

    mvprintw(20, 2, "ENTER: exit");
}

void MInstall::renderPageTerms() noexcept
{
    if (!ui::Context::isTUI()) {
        return;
    }

    if (tui_textCopyright) {
        auto* tuiWidget = dynamic_cast<qtui::TLabel*>(tui_textCopyright->tuiWidget());
        if (tuiWidget) {
            tuiWidget->render();
        }
    }

}

void MInstall::renderPageInstallation() noexcept
{
    if (!ui::Context::isTUI()) {
        return;
    }

    syncInstallationTuiFromGui();

    // Render label
    if (tui_labelInstallType) {
        auto* tuiWidget = dynamic_cast<qtui::TLabel*>(tui_labelInstallType->tuiWidget());
        if (tuiWidget) {
            tuiWidget->render();
        }
    }

    // Render radio buttons
    if (tui_radioEntireDrive) {
        auto* tuiWidget = dynamic_cast<qtui::TRadioButton*>(tui_radioEntireDrive->tuiWidget());
        if (tuiWidget) {
            tuiWidget->render();
        }
    }

    if (tui_radioCustomPart) {
        auto* tuiWidget = dynamic_cast<qtui::TRadioButton*>(tui_radioCustomPart->tuiWidget());
        if (tuiWidget) {
            tuiWidget->render();
        }
    }

    if (tui_radioReplace) {
        auto* tuiWidget = dynamic_cast<qtui::TRadioButton*>(tui_radioReplace->tuiWidget());
        if (tuiWidget) {
            tuiWidget->render();
        }
    }

    if (tui_checkDualDrive) {
        auto* tuiWidget = dynamic_cast<qtui::TCheckBox*>(tui_checkDualDrive->tuiWidget());
        if (tuiWidget) {
            tuiWidget->render();
        }
    }
    if (tui_labelDriveSystem) {
        auto* tuiWidget = dynamic_cast<qtui::TLabel*>(tui_labelDriveSystem->tuiWidget());
        if (tuiWidget) {
            tuiWidget->render();
        }
    }
    if (tui_comboDriveSystem) {
        auto* tuiWidget = dynamic_cast<qtui::TComboBox*>(tui_comboDriveSystem->tuiWidget());
        if (tuiWidget) {
            tuiWidget->render();
        }
    }
    if (tui_labelDriveHome) {
        auto* tuiWidget = dynamic_cast<qtui::TLabel*>(tui_labelDriveHome->tuiWidget());
        if (tuiWidget) {
            tuiWidget->render();
        }
    }
    if (tui_comboDriveHome) {
        auto* tuiWidget = dynamic_cast<qtui::TComboBox*>(tui_comboDriveHome->tuiWidget());
        if (tuiWidget) {
            tuiWidget->render();
        }
    }
    if (tui_sliderPart) {
        auto* tuiWidget = dynamic_cast<qtui::TSlider*>(tui_sliderPart->tuiWidget());
        if (tuiWidget) {
            tuiWidget->render();
        }
    }
    if (tui_checkEncryptAuto) {
        auto* tuiWidget = dynamic_cast<qtui::TCheckBox*>(tui_checkEncryptAuto->tuiWidget());
        if (tuiWidget) {
            tuiWidget->render();
        }
    }

    // Instructions
    mvprintw(18, 4, "TAB: navigate | SPACE: toggle | LEFT/RIGHT: slider | ENTER: continue");

    // Render any open combo popup LAST so it appears on top of other widgets
    if (tui_comboDriveSystem) {
        auto* tuiWidget = dynamic_cast<qtui::TComboBox*>(tui_comboDriveSystem->tuiWidget());
        if (tuiWidget && tuiWidget->isPopupVisible()) {
            tuiWidget->renderPopup();
        }
    }
    if (tui_comboDriveHome) {
        auto* tuiWidget = dynamic_cast<qtui::TComboBox*>(tui_comboDriveHome->tuiWidget());
        if (tuiWidget && tuiWidget->isPopupVisible()) {
            tuiWidget->renderPopup();
        }
    }
}

void MInstall::renderPageConfirm() noexcept
{
    if (!ui::Context::isTUI()) {
        return;
    }

    // Render title
    mvprintw(2, 2, "Installation Confirmation");
    mvprintw(3, 2, "=========================");

    // Show installation settings
    int line = 5;
    int maxY = 0;
    int maxX = 0;
    getmaxyx(stdscr, maxY, maxX);
    auto safePrint = [&](int row, int col, const char *fmt, const char *value) {
        if (row >= 0 && row < maxY) {
            mvprintw(row, col, fmt, value);
        }
    };
    auto safePrintLine = [&](int row, int col, const char *text) {
        if (row >= 0 && row < maxY) {
            mvprintw(row, col, "%s", text);
        }
    };
    if (gui.radioEntireDrive->isChecked()) {
        safePrintLine(line++, 2, "Mode: Use entire drive");
        // Get selected drive name
        const QString driveName = gui.comboDriveSystem->currentText();
        if (!driveName.isEmpty()) {
            const QByteArray driveUtf8 = driveName.toUtf8();
            safePrint(line++, 2, "Drive: %s", driveUtf8.constData());
        }
        if (gui.checkDualDrive->isChecked()) {
            const QString homeDriver = gui.comboDriveHome->currentText();
            if (!homeDriver.isEmpty()) {
                const QByteArray homeUtf8 = homeDriver.toUtf8();
                safePrint(line++, 2, "Home Drive: %s", homeUtf8.constData());
            }
        }
        if (gui.checkEncryptAuto->isChecked()) {
            safePrintLine(line++, 2, "Encryption: Enabled");
        }
    } else if (gui.radioReplace->isChecked()) {
        safePrintLine(line++, 2, "Mode: Replace existing installation");
        // Get selected installation
        if (replacer && replacer->installationCount() > 0) {
            size_t count = replacer->installationCount();
            int safeIndex = tui_focusReplace;
            if (safeIndex < 0) {
                safeIndex = 0;
            } else if (count > 0 && safeIndex >= static_cast<int>(count)) {
                safeIndex = static_cast<int>(count) - 1;
            }
            const QString release = replacer->installationRelease(static_cast<size_t>(safeIndex));
            if (!release.isEmpty()) {
                const QByteArray releaseUtf8 = release.toUtf8();
                safePrint(line++, 2, "Replacing: %s", releaseUtf8.constData());
            }
        }
    } else if (gui.radioCustomPart->isChecked()) {
        safePrintLine(line++, 2, "Mode: Custom partitioning");
    }

    line++;
    safePrintLine(line++, 2, "WARNING: All data on the selected drive will be ERASED!");
    line++;
    safePrintLine(line++, 2, "Press ENTER to begin installation");
    safePrintLine(line++, 2, "Press BACKSPACE to go back");
}

void MInstall::renderPageBoot() noexcept
{
    if (!ui::Context::isTUI()) {
        return;
    }

    if (tui_comboBoot) {
        if (tui_comboBoot->count() != gui.comboBoot->count()) {
            tui_comboBoot->clear();
            for (int i = 0; i < gui.comboBoot->count(); ++i) {
                tui_comboBoot->addItem(gui.comboBoot->itemText(i), gui.comboBoot->itemData(i));
            }
        }
        tui_comboBoot->setCurrentIndex(gui.comboBoot->currentIndex());
    }
    if (tui_checkBootInstall) {
        tui_checkBootInstall->setChecked(gui.boxBoot->isChecked());
    }
    if (tui_checkBootHostSpecific) {
        tui_checkBootHostSpecific->setChecked(gui.checkBootHostSpecific->isChecked());
    }

    const bool bootEnabled = gui.boxBoot->isChecked();
    if (tui_labelBootLocation) tui_labelBootLocation->setEnabled(bootEnabled);
    if (tui_comboBoot) tui_comboBoot->setEnabled(bootEnabled);
    if (tui_radioBootMBR) tui_radioBootMBR->setEnabled(bootEnabled);
    if (tui_radioBootPBR) tui_radioBootPBR->setEnabled(bootEnabled);
    if (tui_radioBootESP) tui_radioBootESP->setEnabled(bootEnabled);
    if (tui_checkBootHostSpecific) tui_checkBootHostSpecific->setEnabled(bootEnabled);
    if (tui_radioBootMBR) tui_radioBootMBR->setChecked(gui.radioBootMBR->isChecked());
    if (tui_radioBootPBR) tui_radioBootPBR->setChecked(gui.radioBootPBR->isChecked());
    if (tui_radioBootESP) tui_radioBootESP->setChecked(gui.radioBootESP->isChecked());

    // Render title
    if (tui_labelBootTitle) {
        auto* tuiWidget = dynamic_cast<qtui::TLabel*>(tui_labelBootTitle->tuiWidget());
        if (tuiWidget) {
            tuiWidget->render();
        }
    }

    if (tui_checkBootInstall) {
        auto* tuiWidget = dynamic_cast<qtui::TCheckBox*>(tui_checkBootInstall->tuiWidget());
        if (tuiWidget) {
            tuiWidget->render();
        }
    }
    if (tui_labelBootLocation) {
        auto* tuiWidget = dynamic_cast<qtui::TLabel*>(tui_labelBootLocation->tuiWidget());
        if (tuiWidget) {
            tuiWidget->render();
        }
    }
    if (tui_comboBoot) {
        auto* tuiWidget = dynamic_cast<qtui::TComboBox*>(tui_comboBoot->tuiWidget());
        if (tuiWidget) {
            tuiWidget->render();
        }
    }

    // Render radio buttons
    if (tui_radioBootMBR) {
        auto* tuiWidget = dynamic_cast<qtui::TRadioButton*>(tui_radioBootMBR->tuiWidget());
        if (tuiWidget) {
            tuiWidget->render();
        }
    }

    if (tui_radioBootPBR) {
        auto* tuiWidget = dynamic_cast<qtui::TRadioButton*>(tui_radioBootPBR->tuiWidget());
        if (tuiWidget) {
            tuiWidget->render();
        }
    }

    if (tui_radioBootESP) {
        auto* tuiWidget = dynamic_cast<qtui::TRadioButton*>(tui_radioBootESP->tuiWidget());
        if (tuiWidget) {
            tuiWidget->render();
        }
    }

    if (tui_checkBootHostSpecific) {
        auto* tuiWidget = dynamic_cast<qtui::TCheckBox*>(tui_checkBootHostSpecific->tuiWidget());
        if (tuiWidget) {
            tuiWidget->render();
        }
    }

    // Instructions
    mvprintw(16, 4, "TAB/UP/DOWN: navigate | SPACE: toggle/select | ENTER: continue");

    // Render any open combo popup LAST so it appears on top of other widgets
    if (tui_comboBoot) {
        auto* tuiWidget = dynamic_cast<qtui::TComboBox*>(tui_comboBoot->tuiWidget());
        if (tuiWidget && tuiWidget->isPopupVisible()) {
            tuiWidget->renderPopup();
        }
    }
}

void MInstall::renderPageNetwork() noexcept
{
    if (!ui::Context::isTUI()) {
        return;
    }

    if (tui_checkSamba) {
        tui_checkSamba->setChecked(gui.checkSamba->isChecked());
        tui_checkSamba->setEnabled(gui.checkSamba->isEnabled());
    }
    if (tui_labelComputerGroup) tui_labelComputerGroup->setEnabled(gui.textComputerGroup->isEnabled());
    if (tui_textComputerGroup) tui_textComputerGroup->setEnabled(gui.textComputerGroup->isEnabled());

    // Render labels
    if (tui_labelComputerName) {
        auto* tuiWidget = dynamic_cast<qtui::TLabel*>(tui_labelComputerName->tuiWidget());
        if (tuiWidget) {
            tuiWidget->render();
        }
    }

    if (tui_labelComputerDomain) {
        auto* tuiWidget = dynamic_cast<qtui::TLabel*>(tui_labelComputerDomain->tuiWidget());
        if (tuiWidget) {
            tuiWidget->render();
        }
    }

    // Render line edits (sync from GUI when unfocused, or when TUI text is still empty)
    if (tui_textComputerName && (tui_focusNetwork != 0 || tui_textComputerName->text().isEmpty())) {
        tui_textComputerName->setText(gui.textComputerName->text());
    }
    if (tui_textComputerName) {
        auto* tuiWidget = dynamic_cast<qtui::TLineEdit*>(tui_textComputerName->tuiWidget());
        if (tuiWidget) {
            tuiWidget->render();
        }
    }

    if (tui_textComputerDomain && (tui_focusNetwork != 1 || tui_textComputerDomain->text().isEmpty())) {
        tui_textComputerDomain->setText(gui.textComputerDomain->text());
    }
    if (tui_textComputerDomain) {
        auto* tuiWidget = dynamic_cast<qtui::TLineEdit*>(tui_textComputerDomain->tuiWidget());
        if (tuiWidget) {
            tuiWidget->render();
        }
    }

    if (tui_labelComputerGroup) {
        auto* tuiWidget = dynamic_cast<qtui::TLabel*>(tui_labelComputerGroup->tuiWidget());
        if (tuiWidget) {
            tuiWidget->render();
        }
    }
    if (tui_textComputerGroup && (tui_focusNetwork != 2 || tui_textComputerGroup->text().isEmpty())) {
        tui_textComputerGroup->setText(gui.textComputerGroup->text());
    }
    if (tui_textComputerGroup) {
        auto* tuiWidget = dynamic_cast<qtui::TLineEdit*>(tui_textComputerGroup->tuiWidget());
        if (tuiWidget) {
            tuiWidget->render();
        }
    }
    if (tui_checkSamba) {
        auto* tuiWidget = dynamic_cast<qtui::TCheckBox*>(tui_checkSamba->tuiWidget());
        if (tuiWidget) {
            tuiWidget->render();
        }
    }

    // Instructions
    if (!tui_networkError.isEmpty()) {
        mvprintw(11, 2, "%s", tui_networkError.toUtf8().constData());
    }
    mvprintw(12, 2, "TAB to switch fields, SPACE to toggle, ENTER to continue");
}

void MInstall::renderPageLocalization() noexcept
{
    if (!ui::Context::isTUI()) {
        return;
    }

    if (tui_comboLocale && tui_comboLocale->count() != gui.comboLocale->count()) {
        tui_comboLocale->clear();
        for (int i = 0; i < gui.comboLocale->count(); ++i) {
            tui_comboLocale->addItem(gui.comboLocale->itemText(i), gui.comboLocale->itemData(i));
        }
    }
    if (tui_comboTimeArea && tui_comboTimeArea->count() != gui.comboTimeArea->count()) {
        tui_comboTimeArea->clear();
        for (int i = 0; i < gui.comboTimeArea->count(); ++i) {
            tui_comboTimeArea->addItem(gui.comboTimeArea->itemText(i), gui.comboTimeArea->itemData(i));
        }
    }
    if (tui_comboTimeZone && tui_comboTimeZone->count() != gui.comboTimeZone->count()) {
        tui_comboTimeZone->clear();
        for (int i = 0; i < gui.comboTimeZone->count(); ++i) {
            tui_comboTimeZone->addItem(gui.comboTimeZone->itemText(i), gui.comboTimeZone->itemData(i));
        }
    }

    if (tui_comboLocale) {
        tui_comboLocale->setCurrentIndex(gui.comboLocale->currentIndex());
    }
    if (tui_comboTimeArea) {
        tui_comboTimeArea->setCurrentIndex(gui.comboTimeArea->currentIndex());
    }
    if (tui_comboTimeZone) {
        tui_comboTimeZone->setCurrentIndex(gui.comboTimeZone->currentIndex());
    }
    if (tui_checkLocalClock) {
        tui_checkLocalClock->setChecked(gui.checkLocalClock->isChecked());
    }
    if (tui_radioClock24) {
        tui_radioClock24->setChecked(gui.radioClock24->isChecked());
    }
    if (tui_radioClock12) {
        tui_radioClock12->setChecked(gui.radioClock12->isChecked());
    }

    // Render title
    if (tui_labelLocaleTitle) {
        auto* tuiWidget = dynamic_cast<qtui::TLabel*>(tui_labelLocaleTitle->tuiWidget());
        if (tuiWidget) {
            tuiWidget->render();
        }
    }

    if (tui_labelLocale) {
        auto* tuiWidget = dynamic_cast<qtui::TLabel*>(tui_labelLocale->tuiWidget());
        if (tuiWidget) {
            tuiWidget->render();
        }
    }
    if (tui_comboLocale) {
        auto* tuiWidget = dynamic_cast<qtui::TComboBox*>(tui_comboLocale->tuiWidget());
        if (tuiWidget) {
            tuiWidget->render();
        }
    }
    if (tui_labelTimeArea) {
        auto* tuiWidget = dynamic_cast<qtui::TLabel*>(tui_labelTimeArea->tuiWidget());
        if (tuiWidget) {
            tuiWidget->render();
        }
    }
    if (tui_comboTimeArea) {
        auto* tuiWidget = dynamic_cast<qtui::TComboBox*>(tui_comboTimeArea->tuiWidget());
        if (tuiWidget) {
            tuiWidget->render();
        }
    }
    if (tui_labelTimeZone) {
        auto* tuiWidget = dynamic_cast<qtui::TLabel*>(tui_labelTimeZone->tuiWidget());
        if (tuiWidget) {
            tuiWidget->render();
        }
    }
    if (tui_comboTimeZone) {
        auto* tuiWidget = dynamic_cast<qtui::TComboBox*>(tui_comboTimeZone->tuiWidget());
        if (tuiWidget) {
            tuiWidget->render();
        }
    }
    if (tui_checkLocalClock) {
        auto* tuiWidget = dynamic_cast<qtui::TCheckBox*>(tui_checkLocalClock->tuiWidget());
        if (tuiWidget) {
            tuiWidget->render();
        }
    }
    if (tui_labelClockFormat) {
        auto* tuiWidget = dynamic_cast<qtui::TLabel*>(tui_labelClockFormat->tuiWidget());
        if (tuiWidget) {
            tuiWidget->render();
        }
    }
    if (tui_radioClock24) {
        auto* tuiWidget = dynamic_cast<qtui::TRadioButton*>(tui_radioClock24->tuiWidget());
        if (tuiWidget) {
            tuiWidget->render();
        }
    }
    if (tui_radioClock12) {
        auto* tuiWidget = dynamic_cast<qtui::TRadioButton*>(tui_radioClock12->tuiWidget());
        if (tuiWidget) {
            tuiWidget->render();
        }
    }

    // Instructions
    mvprintw(14, 2, "TAB/UP/DOWN: navigate | SPACE: expand | ENTER: select/continue");

    // Render any open combo popup LAST so it appears on top of other widgets
    if (tui_comboLocale) {
        auto* tuiWidget = dynamic_cast<qtui::TComboBox*>(tui_comboLocale->tuiWidget());
        if (tuiWidget && tuiWidget->isPopupVisible()) {
            tuiWidget->renderPopup();
        }
    }
    if (tui_comboTimeArea) {
        auto* tuiWidget = dynamic_cast<qtui::TComboBox*>(tui_comboTimeArea->tuiWidget());
        if (tuiWidget && tuiWidget->isPopupVisible()) {
            tuiWidget->renderPopup();
        }
    }
    if (tui_comboTimeZone) {
        auto* tuiWidget = dynamic_cast<qtui::TComboBox*>(tui_comboTimeZone->tuiWidget());
        if (tuiWidget && tuiWidget->isPopupVisible()) {
            tuiWidget->renderPopup();
        }
    }
}

void MInstall::renderPageServices() noexcept
{
    if (!ui::Context::isTUI()) {
        return;
    }

    buildServicesTui();
    syncServicesTuiFromGui();

    // Render title
    if (tui_labelServicesTitle) {
        auto* tuiWidget = dynamic_cast<qtui::TLabel*>(tui_labelServicesTitle->tuiWidget());
        if (tuiWidget) {
            tuiWidget->render();
        }
    }

    for (auto *label : tui_serviceCategoryLabels) {
        auto* tuiWidget = dynamic_cast<qtui::TLabel*>(label->tuiWidget());
        if (tuiWidget) {
            tuiWidget->render();
        }
    }
    for (const auto &item : tui_serviceItems) {
        if (item.checkbox) {
            auto* tuiWidget = dynamic_cast<qtui::TCheckBox*>(item.checkbox->tuiWidget());
            if (tuiWidget) {
                tuiWidget->render();
            }
        }
    }

    if (tui_serviceItems.isEmpty() && tui_labelServicesInfo) {
        auto* tuiWidget = dynamic_cast<qtui::TLabel*>(tui_labelServicesInfo->tuiWidget());
        if (tuiWidget) {
            tuiWidget->render();
        }
    }

    mvprintw(22, 2, "UP/DOWN: navigate | SPACE: toggle | ENTER: continue");
}

void MInstall::renderPageReplace() noexcept
{
    if (!ui::Context::isTUI()) {
        return;
    }

    // Render title
    if (tui_labelReplaceTitle) {
        auto* tuiWidget = dynamic_cast<qtui::TLabel*>(tui_labelReplaceTitle->tuiWidget());
        if (tuiWidget) {
            tuiWidget->render();
        }
    }

    if (tui_replaceScanning) {
        mvprintw(4, 2, "Detected installations:");
        mvprintw(6, 4, "Scanning...");
        mvprintw(8, 2, "Please wait...");
        return;
    }

    // Display list of detected installations
    mvprintw(4, 2, "Detected installations:");

    size_t count = replacer ? replacer->installationCount() : 0;
    if (count == 0) {
        mvprintw(6, 4, "(No existing installations found)");
        mvprintw(8, 2, "Press 'r' to rescan, Backspace to go back");
    } else {
        for (size_t i = 0; i < count; ++i) {
            QString release = replacer->installationRelease(i);
            const char* marker = (static_cast<int>(i) == tui_focusReplace) ? ">" : " ";
            if (static_cast<int>(i) == tui_focusReplace) {
                attron(A_REVERSE);
            }
            mvprintw(6 + static_cast<int>(i), 4, "%s %zu. %s", marker, i + 1, release.toUtf8().constData());
            if (static_cast<int>(i) == tui_focusReplace) {
                attroff(A_REVERSE);
            }
        }

        // Render checkbox
        if (tui_checkReplacePackages) {
            auto* tuiWidget = dynamic_cast<qtui::TCheckBox*>(tui_checkReplacePackages->tuiWidget());
            if (tuiWidget) {
                tuiWidget->render();
            }
        }

        mvprintw(17, 2, "UP/DOWN to select, SPACE to toggle option, ENTER to continue, 'r' to rescan");
    }
}

bool MInstall::processDeferredActions() noexcept
{
    if (!ui::Context::isTUI()) {
        return false;
    }

    if (tui_replaceScanPending && replacer) {
        tui_replaceScanPending = false;
        replacer->scan(true, true);
        tui_replaceScanning = false;
        tui_focusReplace = 0;
        return true;
    }
    if (tui_deferredPage >= 0) {
        const int target = tui_deferredPage;
        tui_deferredPage = -1;
        gotoPage(target);
        return true;
    }
    return false;
}

void MInstall::renderPagePartitions() noexcept
{
    if (!ui::Context::isTUI()) {
        return;
    }

    // Render title
    if (tui_labelPartitionsInfo) {
        auto* tuiWidget = dynamic_cast<qtui::TLabel*>(tui_labelPartitionsInfo->tuiWidget());
        if (tuiWidget) {
            tuiWidget->render();
        }
    }

    // Build device list
    std::vector<PartMan::Device*> devices;
    if (partman) {
        for (PartMan::Iterator it(*partman); PartMan::Device *device = *it; it.next()) {
            if (device->type == PartMan::Device::DRIVE ||
                device->type == PartMan::Device::PARTITION ||
                device->type == PartMan::Device::VIRTUAL ||
                device->type == PartMan::Device::SUBVOLUME) {
                devices.push_back(device);
            }
        }
    }

    // Ensure selection is within bounds
    if (tui_partitionRow >= static_cast<int>(devices.size())) {
        tui_partitionRow = devices.empty() ? 0 : static_cast<int>(devices.size()) - 1;
    }

    // Column headers
    const int colDevice = 2;
    const int colSize = 18;
    const int colUseFor = 30;
    const int colLabel = 44;
    const int colFormat = 60;
    const int colEncrypt = 72;

    attron(A_BOLD | A_UNDERLINE);
    mvprintw(3, colDevice, "Device");
    mvprintw(3, colSize, "Size");
    mvprintw(3, colUseFor, "Use For");
    mvprintw(3, colLabel, "Label");
    mvprintw(3, colFormat, "Format");
    mvprintw(3, colEncrypt, "Encrypt");
    attroff(A_BOLD | A_UNDERLINE);

    // Calculate visible rows
    const int startRow = 4;
    const int maxVisibleRows = 12;

    // Adjust scroll
    if (tui_partitionRow < tui_partitionScroll) {
        tui_partitionScroll = tui_partitionRow;
    } else if (tui_partitionRow >= tui_partitionScroll + maxVisibleRows) {
        tui_partitionScroll = tui_partitionRow - maxVisibleRows + 1;
    }

    // Render device rows
    for (int i = 0; i < maxVisibleRows && (tui_partitionScroll + i) < static_cast<int>(devices.size()); ++i) {
        int idx = tui_partitionScroll + i;
        PartMan::Device *dev = devices[idx];
        int row = startRow + i;
        bool selected = (idx == tui_partitionRow);

        // Clear the row
        move(row, colDevice);
        clrtoeol();

        // Indentation based on device type
        int indent = 0;
        QString prefix;
        if (dev->type == PartMan::Device::PARTITION || dev->type == PartMan::Device::VIRTUAL) {
            indent = 2;
            prefix = "├─";
        } else if (dev->type == PartMan::Device::SUBVOLUME) {
            indent = 4;
            prefix = "└─";
        }

        // Device name
        QString devName = prefix + dev->name;
        mvprintw(row, colDevice + indent, "%-14s", devName.left(14).toUtf8().constData());

        // Size
        if (dev->size > 0) {
            QString sizeStr;
            if (dev->size >= TB) {
                sizeStr = QString::number(dev->size / (double)TB, 'f', 1) + " TB";
            } else if (dev->size >= GB) {
                sizeStr = QString::number(dev->size / (double)GB, 'f', 1) + " GB";
            } else if (dev->size >= MB) {
                sizeStr = QString::number(dev->size / (double)MB, 'f', 1) + " MB";
            } else {
                sizeStr = QString::number(dev->size / (double)KB, 'f', 0) + " KB";
            }
            if (selected && tui_partitionCol == PART_COL_SIZE) {
                attron(A_REVERSE);
            }
            mvprintw(row, colSize, "%-10s", sizeStr.toUtf8().constData());
            if (selected && tui_partitionCol == PART_COL_SIZE) {
                attroff(A_REVERSE);
            }
        }

        // Use For (mount point)
        if (dev->type != PartMan::Device::DRIVE) {
            QString useFor = dev->usefor.isEmpty() ? "-" : dev->usefor;
            if (selected && tui_partitionCol == PART_COL_USEFOR) {
                attron(A_REVERSE);
            }
            mvprintw(row, colUseFor, "%-12s", useFor.left(12).toUtf8().constData());
            if (selected && tui_partitionCol == PART_COL_USEFOR) {
                attroff(A_REVERSE);
            }
        }

        // Label
        if (dev->type != PartMan::Device::DRIVE) {
            QString label;
            bool editableLabel = (dev->format != "PRESERVE"_L1 && dev->format != "DELETE"_L1)
                && (dev->type == PartMan::Device::SUBVOLUME || !dev->usefor.isEmpty());
            if (editableLabel) {
                label = dev->label;
            } else {
                label = dev->curLabel;
            }
            if (label.isEmpty()) label = "-";
            if (selected && tui_partitionCol == PART_COL_LABEL) {
                attron(A_REVERSE);
            }
            mvprintw(row, colLabel, "%-12s", label.left(12).toUtf8().constData());
            if (selected && tui_partitionCol == PART_COL_LABEL) {
                attroff(A_REVERSE);
            }
        }

        // Format
        if (dev->type != PartMan::Device::DRIVE) {
            QString fmt = dev->format.isEmpty() ? (dev->curFormat.isEmpty() ? "-" : dev->curFormat) : dev->format;
            if (selected && tui_partitionCol == PART_COL_FORMAT) {
                attron(A_REVERSE);
            }
            mvprintw(row, colFormat, "%-10s", fmt.left(10).toUtf8().constData());
            if (selected && tui_partitionCol == PART_COL_FORMAT) {
                attroff(A_REVERSE);
            }
        }

        // Encrypt
        if (dev->type == PartMan::Device::PARTITION && dev->canEncrypt()) {
            mvprintw(row, colEncrypt, "[%c]", dev->encrypt ? 'X' : ' ');
        }

    }

    // Scroll indicators
    if (tui_partitionScroll > 0) {
        mvprintw(startRow - 1, 70, "▲ more");
    }
    if (tui_partitionScroll + maxVisibleRows < static_cast<int>(devices.size())) {
        mvprintw(startRow + maxVisibleRows, 70, "▼ more");
    }

    // Show editing dropdown if in edit mode
    tui_partitionPopupVisible = false;
    if (tui_partitionEditing && tui_partitionRow < static_cast<int>(devices.size())
        && !tui_partitionEditIsLabel && !tui_partitionEditIsSize) {
        PartMan::Device *dev = devices[tui_partitionRow];
        QStringList options;
        int popupCol = (tui_partitionCol == PART_COL_USEFOR) ? colUseFor : colFormat;

        if (tui_partitionCol == PART_COL_USEFOR) {
            options = dev->allowedUsesFor();
            options.prepend(u"-"_s);
        } else {
            options = dev->allowedFormats();
        }

        // Draw popup box
        int popupRow = startRow + (tui_partitionRow - tui_partitionScroll) + 1;
        int popupHeight = qMin(static_cast<int>(options.size()), 8);
        int scrollOffset = 0;

        // Ensure popup doesn't go off screen
        if (popupRow + popupHeight > 16) {
            popupRow = 16 - popupHeight;
        }

        if (options.size() > 8) {
            scrollOffset = qMax(0, tui_partitionEditIndex - 4);
            scrollOffset = qMin(scrollOffset, static_cast<int>(options.size()) - 8);
        }

        tui_partitionPopupVisible = true;
        tui_partitionPopupRow = popupRow;
        tui_partitionPopupCol = popupCol;
        tui_partitionPopupHeight = popupHeight;
        tui_partitionPopupScroll = scrollOffset;

        // Draw popup border and options
        attron(A_REVERSE);
        for (int i = 0; i < popupHeight; ++i) {
            int optIdx = scrollOffset + i;

            if (optIdx < options.size()) {
                bool highlighted = (optIdx == tui_partitionEditIndex);
                if (highlighted) {
                    attroff(A_REVERSE);
                    attron(A_BOLD);
                }
                mvprintw(popupRow + i, popupCol, "%-14s", options[optIdx].left(14).toUtf8().constData());
                if (highlighted) {
                    attroff(A_BOLD);
                    attron(A_REVERSE);
                }
            }
        }
        attroff(A_REVERSE);
    }

    const int sizeEditWidth = 8;
    if (tui_partitionEditing && tui_partitionEditIsSize && tui_partitionSizeEdit
        && tui_partitionRow < static_cast<int>(devices.size())) {
        const int editRow = startRow + (tui_partitionRow - tui_partitionScroll);
        tui_partitionSizeEdit->setPosition(editRow, colSize);
        tui_partitionSizeEdit->setWidth(sizeEditWidth);
        tui_partitionSizeEdit->show();
        tui_partitionSizeEdit->setFocus(true);
        tui_partitionSizeEdit->render();
        const char *unitText = (tui_partitionSizeUnit == SIZE_UNIT_GB) ? "GB" : "MB";
        mvprintw(editRow, colSize + sizeEditWidth + 3, "%s", unitText);
    } else if (tui_partitionSizeEdit) {
        tui_partitionSizeEdit->setFocus(false);
        tui_partitionSizeEdit->hide();
    }

    if (tui_partitionEditing && tui_partitionEditIsLabel && tui_partitionLabelEdit
        && tui_partitionRow < static_cast<int>(devices.size())) {
        const int editRow = startRow + (tui_partitionRow - tui_partitionScroll);
        tui_partitionLabelEdit->setPosition(editRow, colLabel);
        tui_partitionLabelEdit->show();
        tui_partitionLabelEdit->setFocus(true);
        tui_partitionLabelEdit->render();
    } else if (tui_partitionLabelEdit) {
        tui_partitionLabelEdit->setFocus(false);
        tui_partitionLabelEdit->hide();
    }

    if (tui_buttonPartitionsApply) {
        tui_buttonPartitionsApply->setFocus(tui_focusPartitions == 1);
        tui_buttonPartitionsApply->render();
    }

    // Instructions and action keys
    if (tui_partitionUnlocking) {
        move(17, 0); clrtoeol();
        move(18, 0); clrtoeol();
        move(19, 0); clrtoeol();

        mvprintw(17, 2, "Password:");
        if (tui_unlockPassEdit) {
            tui_unlockPassEdit->setPosition(17, 12);
            tui_unlockPassEdit->show();
            tui_unlockPassEdit->setFocus(tui_unlockFocusPass);
            tui_unlockPassEdit->render();
        }
        if (!tui_unlockFocusPass) attron(A_REVERSE);
        mvprintw(17, 38, "[%c] Add to crypttab", tui_unlockAddCrypttab ? 'X' : ' ');
        if (!tui_unlockFocusPass) attroff(A_REVERSE);

        mvprintw(18, 2, "ENTER: unlock | TAB: switch focus | ESC: cancel");

        if (!tui_unlockError.isEmpty()) {
            mvprintw(19, 2, "%s", tui_unlockError.left(74).toUtf8().constData());
        }
    } else {
        if (tui_unlockPassEdit) {
            tui_unlockPassEdit->setFocus(false);
            tui_unlockPassEdit->hide();
        }
        move(19, 0); clrtoeol();

        if (tui_partitionEditing) {
            if (tui_partitionEditIsSize) {
                mvprintw(17, 2, "Type number | G/M: switch units | X: MAX | ENTER: apply | ESC: cancel");
            } else {
                mvprintw(17, 2, "UP/DOWN: select option | ENTER: apply | ESC: cancel");
            }
            move(18, 2); clrtoeol();
        } else {
            mvprintw(17, 2, "UP/DOWN: select | LEFT/RIGHT: column | ENTER: edit | SPACE: toggle encrypt");

            PartMan::Device *selectedDevice = nullptr;
            if (tui_partitionRow < static_cast<int>(devices.size())) {
                selectedDevice = devices[tui_partitionRow];
            }
            const bool canClear = selectedDevice && selectedDevice->type == PartMan::Device::DRIVE && !selectedDevice->isLocked();
            const bool canAdd = selectedDevice && !selectedDevice->flags.oldLayout
                && selectedDevice->driveFreeSpace(true) >= 1 * MB;
            const bool canDelete = selectedDevice && selectedDevice->type != PartMan::Device::DRIVE
                && !selectedDevice->flags.oldLayout;
            const bool canNewSubvol = selectedDevice && selectedDevice->isVolume()
                && selectedDevice->finalFormat() == "btrfs"_L1;
            const bool canScanSubvols = canNewSubvol && !selectedDevice->willFormat();
            const bool canUnlock = selectedDevice
                && selectedDevice->type == PartMan::Device::PARTITION
                && selectedDevice->flags.oldLayout
                && selectedDevice->curFormat == "crypto_LUKS"_L1
                && !selectedDevice->mapCount;
            const bool canLock = selectedDevice
                && selectedDevice->type == PartMan::Device::VIRTUAL
                && selectedDevice->flags.volCrypto;
            const bool canCrypttab = selectedDevice
                && (selectedDevice->encrypt
                    || (selectedDevice->flags.oldLayout && selectedDevice->curFormat == "crypto_LUKS"_L1));

            int col = 2;
            auto printAction = [&](const char *text, bool enabled) {
                if (!enabled) attron(A_DIM);
                mvprintw(18, col, "%s", text);
                if (!enabled) attroff(A_DIM);
                col += static_cast<int>(std::strlen(text));
            };

            printAction("C: clear drive", canClear);
            printAction(" | A: add partition", canAdd);
            printAction(" | D: delete", canDelete);
            printAction(" | N: new subvol", canNewSubvol);
            printAction(" | S: scan subvols", canScanSubvols);
            printAction(" | R: reload", true);
            printAction(" | P: partition manager", true);
            printAction(" | TAB: focus apply", true);
            if (canUnlock) printAction(" | U: unlock", true);
            if (canLock) printAction(" | L: lock", true);
            if (canCrypttab) printAction(" | T: crypttab", true);
        }
    }
}

void MInstall::renderPageSwap() noexcept
{
    if (!ui::Context::isTUI()) {
        return;
    }

    syncSwapTuiFromGui();

    // Render title
    if (tui_labelSwapTitle) {
        auto* tuiWidget = dynamic_cast<qtui::TLabel*>(tui_labelSwapTitle->tuiWidget());
        if (tuiWidget) {
            tuiWidget->render();
        }
    }

    if (tui_checkSwapFile) {
        auto* tuiWidget = dynamic_cast<qtui::TCheckBox*>(tui_checkSwapFile->tuiWidget());
        if (tuiWidget) {
            tuiWidget->render();
        }
    }

    if (tui_labelSwapLocation) {
        auto* tuiWidget = dynamic_cast<qtui::TLabel*>(tui_labelSwapLocation->tuiWidget());
        if (tuiWidget) {
            tuiWidget->render();
        }
    }
    if (tui_textSwapFile) {
        auto* tuiWidget = dynamic_cast<qtui::TLineEdit*>(tui_textSwapFile->tuiWidget());
        if (tuiWidget) {
            tuiWidget->render();
        }
    }

    if (tui_labelSwapSize) {
        auto* tuiWidget = dynamic_cast<qtui::TLabel*>(tui_labelSwapSize->tuiWidget());
        if (tuiWidget) {
            tuiWidget->render();
        }
    }
    if (tui_textSwapSize) {
        auto* tuiWidget = dynamic_cast<qtui::TLineEdit*>(tui_textSwapSize->tuiWidget());
        if (tuiWidget) {
            tuiWidget->render();
        }
    }
    if (tui_labelSwapMax) {
        auto* tuiWidget = dynamic_cast<qtui::TLabel*>(tui_labelSwapMax->tuiWidget());
        if (tuiWidget) {
            tuiWidget->render();
        }
    }
    if (tui_labelSwapReset) {
        auto* tuiWidget = dynamic_cast<qtui::TLabel*>(tui_labelSwapReset->tuiWidget());
        if (tuiWidget) {
            tuiWidget->render();
        }
    }
    if (tui_checkHibernation) {
        auto* tuiWidget = dynamic_cast<qtui::TCheckBox*>(tui_checkHibernation->tuiWidget());
        if (tuiWidget) {
            tuiWidget->render();
        }
    }

    if (tui_labelZramTitle) {
        auto* tuiWidget = dynamic_cast<qtui::TLabel*>(tui_labelZramTitle->tuiWidget());
        if (tuiWidget) {
            tuiWidget->render();
        }
    }
    if (tui_checkZram) {
        auto* tuiWidget = dynamic_cast<qtui::TCheckBox*>(tui_checkZram->tuiWidget());
        if (tuiWidget) {
            tuiWidget->render();
        }
    }
    if (tui_radioZramPercent) {
        auto* tuiWidget = dynamic_cast<qtui::TRadioButton*>(tui_radioZramPercent->tuiWidget());
        if (tuiWidget) {
            tuiWidget->render();
        }
    }
    if (tui_textZramPercent) {
        auto* tuiWidget = dynamic_cast<qtui::TLineEdit*>(tui_textZramPercent->tuiWidget());
        if (tuiWidget) {
            tuiWidget->render();
        }
    }
    if (tui_labelZramRecPercent) {
        auto* tuiWidget = dynamic_cast<qtui::TLabel*>(tui_labelZramRecPercent->tuiWidget());
        if (tuiWidget) {
            tuiWidget->render();
        }
    }
    if (tui_radioZramSize) {
        auto* tuiWidget = dynamic_cast<qtui::TRadioButton*>(tui_radioZramSize->tuiWidget());
        if (tuiWidget) {
            tuiWidget->render();
        }
    }
    if (tui_textZramSize) {
        auto* tuiWidget = dynamic_cast<qtui::TLineEdit*>(tui_textZramSize->tuiWidget());
        if (tuiWidget) {
            tuiWidget->render();
        }
    }
    if (tui_labelZramRecSize) {
        auto* tuiWidget = dynamic_cast<qtui::TLabel*>(tui_labelZramRecSize->tuiWidget());
        if (tuiWidget) {
            tuiWidget->render();
        }
    }

    mvprintw(20, 2, "TAB/UP/DOWN: navigate | SPACE: toggle | R: reset size");
    mvprintw(21, 2, "ENTER: continue | Backspace: go back");
}

void MInstall::renderPageEncryption() noexcept
{
    if (!ui::Context::isTUI()) {
        return;
    }

    if (tui_textCryptoPass && tui_focusEncryption != 0) {
        tui_textCryptoPass->setText(gui.textCryptoPass->text());
    }
    if (tui_textCryptoPass2 && tui_focusEncryption != 1) {
        tui_textCryptoPass2->setText(gui.textCryptoPass2->text());
    }

    if (tui_labelEncryptionTitle) {
        auto* tuiWidget = dynamic_cast<qtui::TLabel*>(tui_labelEncryptionTitle->tuiWidget());
        if (tuiWidget) {
            tuiWidget->render();
        }
    }
    if (tui_labelCryptoPass) {
        auto* tuiWidget = dynamic_cast<qtui::TLabel*>(tui_labelCryptoPass->tuiWidget());
        if (tuiWidget) {
            tuiWidget->render();
        }
    }
    if (tui_textCryptoPass) {
        auto* tuiWidget = dynamic_cast<qtui::TLineEdit*>(tui_textCryptoPass->tuiWidget());
        if (tuiWidget) {
            tuiWidget->render();
        }
    }
    if (tui_labelCryptoPass2) {
        auto* tuiWidget = dynamic_cast<qtui::TLabel*>(tui_labelCryptoPass2->tuiWidget());
        if (tuiWidget) {
            tuiWidget->render();
        }
    }
    if (tui_textCryptoPass2) {
        auto* tuiWidget = dynamic_cast<qtui::TLineEdit*>(tui_textCryptoPass2->tuiWidget());
        if (tuiWidget) {
            tuiWidget->render();
        }
    }

    if (!tui_encryptionError.isEmpty()) {
        mvprintw(8, 2, "%s", tui_encryptionError.toUtf8().constData());
    }
    mvprintw(9, 2, "TAB to switch fields, ENTER to continue");
}

void MInstall::renderPageUserAccounts() noexcept
{
    if (!ui::Context::isTUI()) {
        return;
    }

    if (tui_checkRootAccount) {
        tui_checkRootAccount->setChecked(gui.boxRootAccount->isChecked());
    }
    if (tui_checkAutoLogin) {
        tui_checkAutoLogin->setChecked(gui.checkAutoLogin->isChecked());
    }
    if (tui_checkSaveDesktop) {
        tui_checkSaveDesktop->setChecked(gui.checkSaveDesktop->isChecked());
    }

    const bool rootEnabled = gui.boxRootAccount->isChecked();
    if (tui_labelRootPass) tui_labelRootPass->setEnabled(rootEnabled);
    if (tui_textRootPass) tui_textRootPass->setEnabled(rootEnabled);
    if (tui_labelRootPass2) tui_labelRootPass2->setEnabled(rootEnabled);
    if (tui_textRootPass2) tui_textRootPass2->setEnabled(rootEnabled);

    if (tui_textUserName && tui_focusUserAccounts != 0) {
        tui_textUserName->setText(gui.textUserName->text());
    }
    if (tui_textUserPass && tui_focusUserAccounts != 1) {
        tui_textUserPass->setText(gui.textUserPass->text());
    }
    if (tui_textUserPass2 && tui_focusUserAccounts != 2) {
        tui_textUserPass2->setText(gui.textUserPass2->text());
    }
    if (tui_textRootPass && tui_focusUserAccounts != 4) {
        tui_textRootPass->setText(gui.textRootPass->text());
    }
    if (tui_textRootPass2 && tui_focusUserAccounts != 5) {
        tui_textRootPass2->setText(gui.textRootPass2->text());
    }

    // Render labels
    if (tui_labelUserName) {
        auto* tuiWidget = dynamic_cast<qtui::TLabel*>(tui_labelUserName->tuiWidget());
        if (tuiWidget) {
            tuiWidget->render();
        }
    }

    if (tui_labelUserPass) {
        auto* tuiWidget = dynamic_cast<qtui::TLabel*>(tui_labelUserPass->tuiWidget());
        if (tuiWidget) {
            tuiWidget->render();
        }
    }

    if (tui_labelUserPass2) {
        auto* tuiWidget = dynamic_cast<qtui::TLabel*>(tui_labelUserPass2->tuiWidget());
        if (tuiWidget) {
            tuiWidget->render();
        }
    }

    // Render line edits
    if (tui_textUserName) {
        auto* tuiWidget = dynamic_cast<qtui::TLineEdit*>(tui_textUserName->tuiWidget());
        if (tuiWidget) {
            tuiWidget->render();
        }
    }

    if (tui_textUserPass) {
        auto* tuiWidget = dynamic_cast<qtui::TLineEdit*>(tui_textUserPass->tuiWidget());
        if (tuiWidget) {
            tuiWidget->render();
        }
    }

    if (tui_textUserPass2) {
        auto* tuiWidget = dynamic_cast<qtui::TLineEdit*>(tui_textUserPass2->tuiWidget());
        if (tuiWidget) {
            tuiWidget->render();
        }
    }

    if (tui_checkRootAccount) {
        auto* tuiWidget = dynamic_cast<qtui::TCheckBox*>(tui_checkRootAccount->tuiWidget());
        if (tuiWidget) {
            tuiWidget->render();
        }
    }
    if (tui_labelRootPass) {
        auto* tuiWidget = dynamic_cast<qtui::TLabel*>(tui_labelRootPass->tuiWidget());
        if (tuiWidget) {
            tuiWidget->render();
        }
    }
    if (tui_textRootPass) {
        auto* tuiWidget = dynamic_cast<qtui::TLineEdit*>(tui_textRootPass->tuiWidget());
        if (tuiWidget) {
            tuiWidget->render();
        }
    }
    if (tui_labelRootPass2) {
        auto* tuiWidget = dynamic_cast<qtui::TLabel*>(tui_labelRootPass2->tuiWidget());
        if (tuiWidget) {
            tuiWidget->render();
        }
    }
    if (tui_textRootPass2) {
        auto* tuiWidget = dynamic_cast<qtui::TLineEdit*>(tui_textRootPass2->tuiWidget());
        if (tuiWidget) {
            tuiWidget->render();
        }
    }

    if (tui_checkAutoLogin) {
        auto* tuiWidget = dynamic_cast<qtui::TCheckBox*>(tui_checkAutoLogin->tuiWidget());
        if (tuiWidget) {
            tuiWidget->render();
        }
    }
    if (tui_checkSaveDesktop) {
        auto* tuiWidget = dynamic_cast<qtui::TCheckBox*>(tui_checkSaveDesktop->tuiWidget());
        if (tuiWidget) {
            tuiWidget->render();
        }
    }

    // Instructions
    if (!tui_userError.isEmpty()) {
        mvprintw(18, 2, "%s", tui_userError.toUtf8().constData());
    }
    mvprintw(19, 2, "TAB to switch fields, SPACE to toggle, ENTER for next field/continue");
}

void MInstall::renderPageOldHome() noexcept
{
    if (!ui::Context::isTUI()) {
        return;
    }

    // Render title
    if (tui_labelOldHomeTitle) {
        auto* tuiWidget = dynamic_cast<qtui::TLabel*>(tui_labelOldHomeTitle->tuiWidget());
        if (tuiWidget) {
            tuiWidget->render();
        }
    }

    // Render info
    if (tui_labelOldHomeInfo) {
        if (!gui.labelOldHome->text().isEmpty()) {
            tui_labelOldHomeInfo->setText(gui.labelOldHome->text());
        }
        auto* tuiWidget = dynamic_cast<qtui::TLabel*>(tui_labelOldHomeInfo->tuiWidget());
        if (tuiWidget) {
            tuiWidget->render();
        }
    }

    // Render radio buttons
    if (tui_radioOldHomeUse) {
        auto* tuiWidget = dynamic_cast<qtui::TRadioButton*>(tui_radioOldHomeUse->tuiWidget());
        if (tuiWidget) {
            tuiWidget->render();
        }
    }

    if (tui_radioOldHomeSave) {
        auto* tuiWidget = dynamic_cast<qtui::TRadioButton*>(tui_radioOldHomeSave->tuiWidget());
        if (tuiWidget) {
            tuiWidget->render();
        }
    }

    if (tui_radioOldHomeDelete) {
        auto* tuiWidget = dynamic_cast<qtui::TRadioButton*>(tui_radioOldHomeDelete->tuiWidget());
        if (tuiWidget) {
            tuiWidget->render();
        }
    }

    // Instructions
    if (!tui_oldHomeError.isEmpty()) {
        mvprintw(11, 2, "%s", tui_oldHomeError.toUtf8().constData());
    }
    mvprintw(12, 2, "UP/DOWN to navigate, SPACE to select, ENTER to continue");
}

void MInstall::renderPageTips() noexcept
{
    if (!ui::Context::isTUI()) {
        return;
    }

    // Render title
    if (tui_labelTipsTitle) {
        auto* tuiWidget = dynamic_cast<qtui::TLabel*>(tui_labelTipsTitle->tuiWidget());
        if (tuiWidget) {
            tuiWidget->render();
        }
    }

    // Render info
    if (tui_labelTipsInfo) {
        auto* tuiWidget = dynamic_cast<qtui::TLabel*>(tui_labelTipsInfo->tuiWidget());
        if (tuiWidget) {
            tuiWidget->render();
        }
    }

    // Progress bar + status on same line
    const int barWidth = 40;
    const int maxVal = gui.progInstall->maximum() > 0 ? gui.progInstall->maximum() : 100;
    const int value = qBound(0, gui.progInstall->value(), maxVal);
    const int filled = (value * barWidth) / maxVal;
    const int percent = (value * 100) / maxVal;
    
    QString status = gui.progInstall->format();
    if (status.contains("%p"_L1)) {
        status.replace("%p"_L1, QString::number(percent));
    }
    if (status.isEmpty()) {
        status = tr("Installing...");
    }
    
    // Add spinner inside the progress bar between filled and empty portions
    const char spinnerChars[] = "|/-\\";
    const char spin = spinnerChars[tui_spinnerTick % 4];
    
    QString bar = "["_L1 + QString(filled, '#') + spin + QString(barWidth - filled, ' ') + "]"_L1;
    
    // Clear and print: [####/           ] 25% - Status message
    int maxY, maxX;
    getmaxyx(stdscr, maxY, maxX);
    (void)maxY;
    QString fullLine = QString("%1 %2").arg(bar).arg(status);
    // Pad with spaces to clear the rest of the line
    int lineLen = fullLine.length();
    if (lineLen < maxX - 4) {
        fullLine += QString(maxX - 4 - lineLen, ' ');
    }
    mvprintw(10, 2, "%s", fullLine.toUtf8().constData());
    
    // Render reboot checkbox with focus
    if (tui_checkTipsReboot) {
        auto* tuiWidget = dynamic_cast<qtui::TCheckBox*>(tui_checkTipsReboot->tuiWidget());
        if (tuiWidget) {
            tuiWidget->setFocus(true);  // Always keep focus on checkbox so SPACE works
            tuiWidget->render();
        }
    }
}

void MInstall::renderCurrentPage() noexcept
{
    if (!ui::Context::isTUI()) {
        return;  // Only for TUI mode
    }

    // Render based on current page
    if (currentPageIndex == Step::SPLASH) {
        renderPageSplash();
    } else if (currentPageIndex == Step::TERMS) {
        renderPageTerms();
    } else if (currentPageIndex == Step::INSTALLATION) {
        renderPageInstallation();
    } else if (currentPageIndex == Step::REPLACE) {
        renderPageReplace();
    } else if (currentPageIndex == Step::PARTITIONS) {
        renderPagePartitions();
    } else if (currentPageIndex == Step::ENCRYPTION) {
        renderPageEncryption();
    } else if (currentPageIndex == Step::CONFIRM) {
        renderPageConfirm();
    } else if (currentPageIndex == Step::BOOT) {
        renderPageBoot();
    } else if (currentPageIndex == Step::SWAP) {
        renderPageSwap();
    } else if (currentPageIndex == Step::NETWORK) {
        renderPageNetwork();
    } else if (currentPageIndex == Step::LOCALIZATION) {
        renderPageLocalization();
    } else if (currentPageIndex == Step::SERVICES) {
        renderPageServices();
    } else if (currentPageIndex == Step::USER_ACCOUNTS) {
        renderPageUserAccounts();
    } else if (currentPageIndex == Step::OLD_HOME) {
        renderPageOldHome();
    } else if (currentPageIndex == Step::TIPS) {
        renderPageTips();
    } else if (currentPageIndex == Step::END) {
        renderPageEnd();
    } else {
        mvprintw(5, 5, "Page %d - TUI rendering not yet implemented", currentPageIndex);
    }
}

void MInstall::handleInput(int key) noexcept
{
    if (!ui::Context::isTUI()) {
        return;
    }

    // Handle input based on current page
    if (currentPageIndex == Step::END) {
        // pageEnd has only checkbox (label is not interactive)
        // Space or Enter to toggle checkbox
        if (tui_checkExitReboot) {
            auto* checkbox = dynamic_cast<qtui::TCheckBox*>(tui_checkExitReboot->tuiWidget());
            if (checkbox && key == ' ') {
                checkbox->toggle();
            }
        }
        if (key == '\n' || key == KEY_ENTER) {
            tui_exitRequested = true;
        }
    } else if (currentPageIndex == Step::TERMS) {
        // pageTerms: Enter/Space to accept and go to next page
        if (key == ' ' || key == '\n' || key == KEY_ENTER) {
            gotoPage(currentPageIndex + 1);
        }
    } else if (currentPageIndex == Step::SPLASH) {
        // pageSplash: Enter/Space to continue to terms page
        if (key == ' ' || key == '\n' || key == KEY_ENTER) {
            gotoPage(currentPageIndex + 1);
        }
    } else if (currentPageIndex == Step::INSTALLATION) {
        qtui::TRadioButton* radios[3] = {nullptr, nullptr, nullptr};
        qtui::TCheckBox* dualDrive = nullptr;
        qtui::TComboBox* comboSystem = nullptr;
        qtui::TComboBox* comboHome = nullptr;
        qtui::TSlider* slider = nullptr;
        qtui::TCheckBox* encryptAuto = nullptr;

        if (tui_radioEntireDrive) {
            radios[0] = dynamic_cast<qtui::TRadioButton*>(tui_radioEntireDrive->tuiWidget());
        }
        if (tui_radioCustomPart) {
            radios[1] = dynamic_cast<qtui::TRadioButton*>(tui_radioCustomPart->tuiWidget());
        }
        if (tui_radioReplace) {
            radios[2] = dynamic_cast<qtui::TRadioButton*>(tui_radioReplace->tuiWidget());
        }
        if (tui_checkDualDrive) {
            dualDrive = dynamic_cast<qtui::TCheckBox*>(tui_checkDualDrive->tuiWidget());
        }
        if (tui_comboDriveSystem) {
            comboSystem = dynamic_cast<qtui::TComboBox*>(tui_comboDriveSystem->tuiWidget());
        }
        if (tui_comboDriveHome) {
            comboHome = dynamic_cast<qtui::TComboBox*>(tui_comboDriveHome->tuiWidget());
        }
        if (tui_sliderPart) {
            slider = dynamic_cast<qtui::TSlider*>(tui_sliderPart->tuiWidget());
        }
        if (tui_checkEncryptAuto) {
            encryptAuto = dynamic_cast<qtui::TCheckBox*>(tui_checkEncryptAuto->tuiWidget());
        }

        auto isFocusable = [&](int index) -> bool {
            switch (index) {
                case 0: return radios[0] && radios[0]->isEnabled();
                case 1: return radios[1] && radios[1]->isEnabled();
                case 2: return radios[2] && radios[2]->isEnabled();
                case 3: return dualDrive && dualDrive->isEnabled();
                case 4: return comboSystem && comboSystem->isEnabled();
                case 5: return comboHome && comboHome->isEnabled();
                case 6: return slider && slider->isEnabled() && slider->isVisible();
                case 7: return encryptAuto && encryptAuto->isEnabled();
                default: return false;
            }
        };

        auto moveFocus = [&](int delta) {
            int idx = tui_focusInstallationField;
            for (int i = 0; i < 8; ++i) {
                idx = (idx + delta + 8) % 8;
                if (isFocusable(idx)) {
                    tui_focusInstallationField = idx;
                    break;
                }
            }
        };

        auto applyFocus = [&]() {
            if (radios[0]) radios[0]->setFocus(tui_focusInstallationField == 0);
            if (radios[1]) radios[1]->setFocus(tui_focusInstallationField == 1);
            if (radios[2]) radios[2]->setFocus(tui_focusInstallationField == 2);
            if (dualDrive) dualDrive->setFocus(tui_focusInstallationField == 3);
            if (comboSystem) comboSystem->setFocus(tui_focusInstallationField == 4);
            if (comboHome) comboHome->setFocus(tui_focusInstallationField == 5);
            if (slider) slider->setFocus(tui_focusInstallationField == 6);
            if (encryptAuto) encryptAuto->setFocus(tui_focusInstallationField == 7);
        };

        auto ensureFocus = [&]() {
            if (!isFocusable(tui_focusInstallationField)) {
                moveFocus(1);
            }
        };

        const bool installFocusIsSlider = (tui_focusInstallationField == 6);
        if (key == TUI_KEY_ALT_LEFT || key == KEY_BACKSPACE) {
            gotoPage(currentPageIndex - 1);
            return;
        } else if (key == '\t' || (key == KEY_DOWN && !installFocusIsSlider)) {
            moveFocus(1);
        } else if (key == KEY_UP && !installFocusIsSlider) {
            moveFocus(-1);
        } else if (key == ' ') {
            if (tui_focusInstallationField <= 2 && radios[tui_focusInstallationField]) {
                radios[tui_focusInstallationField]->setChecked(true);
                if (tui_focusInstallationField == 0) gui.radioEntireDrive->setChecked(true);
                if (tui_focusInstallationField == 1) gui.radioCustomPart->setChecked(true);
                if (tui_focusInstallationField == 2) gui.radioReplace->setChecked(true);
                tui_focusInstallation = tui_focusInstallationField;
                syncInstallationTuiFromGui();
            } else if (tui_focusInstallationField == 3 && dualDrive) {
                dualDrive->toggle();
                gui.checkDualDrive->setChecked(dualDrive->isChecked());
                syncInstallationTuiFromGui();
            } else if (tui_focusInstallationField == 7 && encryptAuto) {
                encryptAuto->toggle();
                gui.checkEncryptAuto->setChecked(encryptAuto->isChecked());
                syncInstallationTuiFromGui();
            }
        } else if (tui_focusInstallationField == 4 && comboSystem) {
            comboSystem->handleKey(key);
            gui.comboDriveSystem->setCurrentIndex(comboSystem->currentIndex());
            syncInstallationTuiFromGui();
        } else if (tui_focusInstallationField == 5 && comboHome) {
            comboHome->handleKey(key);
            gui.comboDriveHome->setCurrentIndex(comboHome->currentIndex());
            syncInstallationTuiFromGui();
        } else if (tui_focusInstallationField == 6 && slider) {
            slider->handleKey(key);
            gui.spinRoot->setValue(slider->value());
            syncInstallationTuiFromGui();
        }

        if (key == '\n' || key == KEY_ENTER) {
            if (tui_focusInstallationField == 4 && comboSystem && comboSystem->isPopupVisible()) {
                comboSystem->handleKey(key);
            } else if (tui_focusInstallationField == 5 && comboHome && comboHome->isPopupVisible()) {
                comboHome->handleKey(key);
            } else {
                gotoPage(currentPageIndex + 1);
            }
        }

        ensureFocus();
        applyFocus();
    } else if (currentPageIndex == Step::REPLACE) {
        // pageReplace: Select installation to replace
        size_t count = replacer ? replacer->installationCount() : 0;

        if (key == TUI_KEY_ALT_LEFT) {
            gotoPage(Step::INSTALLATION);
        } else if (key == KEY_UP && tui_focusReplace > 0) {
            tui_focusReplace--;
        } else if (key == KEY_DOWN && tui_focusReplace < static_cast<int>(count) - 1) {
            tui_focusReplace++;
        } else if (key == ' ') {
            // Space toggles upgrade packages checkbox
            if (tui_checkReplacePackages) {
                auto* checkbox = dynamic_cast<qtui::TCheckBox*>(tui_checkReplacePackages->tuiWidget());
                if (checkbox) {
                    checkbox->toggle();
                    // Sync with GUI checkbox
                    gui.checkReplacePackages->setChecked(checkbox->isChecked());
                }
            }
        } else if (key == 'r' || key == 'R') {
            // Rescan for installations
            if (replacer) {
                tui_replaceScanning = true;
                tui_replaceScanPending = true;
            }
            tui_focusReplace = 0;
        } else if (key == '\n' || key == KEY_ENTER) {
            if (count > 0 && replacer) {
                // Set selected installation in GUI widget for validation
                replacer->setSelectedInstallation(tui_focusReplace);
                gotoPage(Step::CONFIRM);
            }
        } else if (key == KEY_BACKSPACE) {
            gotoPage(Step::INSTALLATION);
        }
    } else if (currentPageIndex == Step::PARTITIONS) {
        // Build device list for navigation
        std::vector<PartMan::Device*> devices;
        if (partman) {
            for (PartMan::Iterator it(*partman); PartMan::Device *device = *it; it.next()) {
                if (device->type == PartMan::Device::DRIVE ||
                    device->type == PartMan::Device::PARTITION ||
                    device->type == PartMan::Device::VIRTUAL ||
                    device->type == PartMan::Device::SUBVOLUME) {
                    devices.push_back(device);
                }
            }
        }

        const int deviceCount = static_cast<int>(devices.size());
        PartMan::Device *selectedDevice = (tui_partitionRow < deviceCount) ? devices[tui_partitionRow] : nullptr;

        if (key == TUI_KEY_ALT_LEFT) {
            gotoPage(Step::INSTALLATION);
        } else if (key == '\t' || key == KEY_BTAB) {
            tui_focusPartitions = (tui_focusPartitions + 1) % 2;
            if (tui_buttonPartitionsApply) {
                tui_buttonPartitionsApply->setFocus(tui_focusPartitions == 1);
            }
        } else if (tui_focusPartitions == 1 && tui_buttonPartitionsApply) {
            if (tui_buttonPartitionsApply->handleKey(key)) {
                // apply handled by button click signal
            }
        } else if (tui_partitionUnlocking) {
            if (key == 27) { // ESC
                tui_partitionUnlocking = false;
                tui_unlockDevice = nullptr;
                tui_unlockError.clear();
            } else if (key == '\t') {
                tui_unlockFocusPass = !tui_unlockFocusPass;
            } else if (key == ' ' && !tui_unlockFocusPass) {
                tui_unlockAddCrypttab = !tui_unlockAddCrypttab;
            } else if (key == '\n' || key == KEY_ENTER) {
                if (tui_unlockFocusPass) {
                    if (!tui_unlockPassEdit || tui_unlockPassEdit->text().isEmpty()) {
                        beep();
                    } else if (tui_unlockDevice && crypto && partman) {
                        const QByteArray password = tui_unlockPassEdit->text().toUtf8();
                        if (crypto->open(tui_unlockDevice, password)) {
                            tui_unlockDevice->usefor.clear();
                            tui_unlockDevice->addToCrypttab = tui_unlockAddCrypttab;
                            partman->scanVirtualDevices(true);
                            tui_partitionUnlocking = false;
                            tui_unlockDevice = nullptr;
                            tui_unlockError.clear();
                        } else {
                            tui_unlockError = tr("Could not unlock device. Incorrect password?");
                        }
                    }
                } else {
                    tui_unlockAddCrypttab = !tui_unlockAddCrypttab;
                }
            } else if (tui_unlockFocusPass && tui_unlockPassEdit) {
                tui_unlockPassEdit->handleKey(key);
            }
        } else if (tui_partitionEditing && selectedDevice) {
            // Handle editing navigation first so arrows don't move the row
            if (tui_partitionEditIsSize) {
                auto formatSizeValue = [](double value) -> QString {
                    double rounded = std::round(value * 10.0) / 10.0;
                    if (std::fabs(rounded - std::round(rounded)) < 0.05) {
                        return QString::number(static_cast<long long>(std::llround(rounded)));
                    }
                    return QString::number(rounded, 'f', 1);
                };

                if (key == KEY_BACKSPACE || key == 127 || key == 8) {
                    if (tui_partitionSizeEdit) {
                        tui_partitionSizeEdit->handleKey(key);
                    }
                } else if (key == 'x' || key == 'X') {
                    if (tui_partitionSizeEdit) {
                        tui_partitionSizeEdit->setText("MAX");
                    }
                } else if (key == 'g' || key == 'G' || key == 'm' || key == 'M') {
                    const bool toGb = (key == 'g' || key == 'G');
                    if (tui_partitionSizeEdit) {
                        QString text = tui_partitionSizeEdit->text().trimmed();
                        if (!text.compare("MAX"_L1, Qt::CaseInsensitive)) {
                            tui_partitionSizeUnit = toGb ? SIZE_UNIT_GB : SIZE_UNIT_MB;
                            return;
                        }
                        text = text.replace("MB"_L1, ""_L1, Qt::CaseInsensitive)
                                   .replace("GB"_L1, ""_L1, Qt::CaseInsensitive)
                                   .replace(" "_L1, ""_L1)
                                   .trimmed();
                        bool ok = false;
                        double value = text.toDouble(&ok);
                        if (ok) {
                            if (toGb && tui_partitionSizeUnit == SIZE_UNIT_MB) {
                                value = value / 1024.0;
                            } else if (!toGb && tui_partitionSizeUnit == SIZE_UNIT_GB) {
                                value = value * 1024.0;
                            }
                            tui_partitionSizeUnit = toGb ? SIZE_UNIT_GB : SIZE_UNIT_MB;
                            tui_partitionSizeEdit->setText(formatSizeValue(value));
                        } else {
                            tui_partitionSizeUnit = toGb ? SIZE_UNIT_GB : SIZE_UNIT_MB;
                        }
                    }
                } else if (key == 27) { // ESC
                    tui_partitionEditing = false;
                    tui_partitionEditIsSize = false;
                    if (tui_partitionSizeEdit) {
                        tui_partitionSizeEdit->setText(tui_partitionSizeOriginal);
                    }
                } else if (key == '\n' || key == KEY_ENTER) {
                    if (!partman || !selectedDevice) {
                        beep();
                        tui_partitionEditing = false;
                        tui_partitionEditIsSize = false;
                        return;
                    }
                    if (tui_partitionSizeEdit) {
                        QString text = tui_partitionSizeEdit->text().trimmed();
                        double sizeMb = -1.0;
                        if (text.compare("MAX"_L1, Qt::CaseInsensitive) == 0) {
                            sizeMb = 0.0;
                        } else {
                            QString work = text;
                            QChar suffix;
                            work.replace(" "_L1, ""_L1);
                            if (work.endsWith("MB"_L1, Qt::CaseInsensitive)) {
                                suffix = 'M';
                                work.chop(2);
                            } else if (work.endsWith("GB"_L1, Qt::CaseInsensitive)) {
                                suffix = 'G';
                                work.chop(2);
                            } else if (!work.isEmpty()) {
                                suffix = work.at(work.size() - 1);
                                if (suffix.isLetter()) {
                                    work.chop(1);
                                }
                            }
                            bool ok = false;
                            double value = work.toDouble(&ok);
                            if (ok) {
                                PartitionSizeUnit unit = tui_partitionSizeUnit;
                                if (suffix == 'g' || suffix == 'G') unit = SIZE_UNIT_GB;
                                else if (suffix == 'm' || suffix == 'M') unit = SIZE_UNIT_MB;
                                sizeMb = (unit == SIZE_UNIT_GB) ? (value * 1024.0) : value;
                            }
                        }
                        if (sizeMb >= 0.0) {
                            if (!partman->changeBegin(selectedDevice)) {
                                beep();
                                return;
                            }
                            long long sizeBytes = static_cast<long long>(std::llround(sizeMb)) * MB;
                            if (sizeMb == 0.0) {
                                sizeBytes = selectedDevice->driveFreeSpace();
                            } else {
                                const long long maxBytes = selectedDevice->driveFreeSpace();
                                if (sizeBytes > maxBytes) {
                                    sizeBytes = maxBytes;
                                }
                            }
                            selectedDevice->size = sizeBytes;
                            partman->changeEnd();
                            if (partman->index(selectedDevice).isValid()) {
                                for (PartMan::Iterator it(*partman); PartMan::Device *subvol = *it; it.next()) {
                                    if (subvol->type == PartMan::Device::SUBVOLUME && subvol->parent() == selectedDevice) {
                                        subvol->size = selectedDevice->size;
                                    }
                                }
                            }
                            tui_partitionEditing = false;
                            tui_partitionEditIsSize = false;
                            tui_partitionCol = PART_COL_USEFOR;
                        } else {
                            beep();
                        }
                    }
                } else if (tui_partitionSizeEdit) {
                    if ((key >= '0' && key <= '9') || key == '.' || key == ',') {
                        if (key == ',') {
                            key = '.';
                        }
                        tui_partitionSizeEdit->handleKey(key);
                    } else {
                        beep();
                    }
                }
            } else if (tui_partitionEditIsLabel) {
                if (key == KEY_BACKSPACE || key == 127 || key == 8) {
                    if (tui_partitionLabelEdit) {
                        tui_partitionLabelEdit->handleKey(key);
                    }
                } else if (key == 27) { // ESC
                    tui_partitionEditing = false;
                    tui_partitionEditIsLabel = false;
                    if (tui_partitionLabelEdit) {
                        tui_partitionLabelEdit->setText(tui_partitionLabelOriginal);
                    }
                } else if (key == '\n' || key == KEY_ENTER) {
                    if (tui_partitionLabelEdit) {
                        partman->changeBegin(selectedDevice);
                        selectedDevice->label = tui_partitionLabelEdit->text().trimmed();
                        partman->changeEnd();
                    }
                    tui_partitionEditing = false;
                    tui_partitionEditIsLabel = false;
                } else if (tui_partitionLabelEdit) {
                    tui_partitionLabelEdit->handleKey(key);
                }
            } else {
                if (key == KEY_UP && tui_partitionEditIndex > 0) {
                    tui_partitionEditIndex--;
                } else if (key == KEY_DOWN) {
                    QStringList options;
                    if (tui_partitionCol == PART_COL_USEFOR) {
                        options = selectedDevice->allowedUsesFor();
                        options.prepend(u"-"_s);
                    } else {
                        options = selectedDevice->allowedFormats();
                    }
                    if (tui_partitionEditIndex < options.size() - 1) {
                        tui_partitionEditIndex++;
                    }
                } else if (key == KEY_LEFT || key == KEY_RIGHT) {
                    int nextCol = (key == KEY_LEFT) ? PART_COL_USEFOR : PART_COL_FORMAT;
                    if (nextCol != tui_partitionCol) {
                        tui_partitionCol = nextCol;
                        if (tui_partitionCol == PART_COL_USEFOR) {
                            QStringList uses = selectedDevice->allowedUsesFor();
                            uses.prepend(u"-"_s);
                            if (selectedDevice->usefor.isEmpty()) {
                                tui_partitionEditIndex = 0;
                            } else {
                                tui_partitionEditIndex = uses.indexOf(selectedDevice->usefor);
                                if (tui_partitionEditIndex < 0) tui_partitionEditIndex = 0;
                            }
                        } else if (tui_partitionCol == PART_COL_FORMAT) {
                            QStringList formats = selectedDevice->allowedFormats();
                            QString curFmt = selectedDevice->format.isEmpty() ? selectedDevice->curFormat : selectedDevice->format;
                            tui_partitionEditIndex = formats.indexOf(curFmt);
                            if (tui_partitionEditIndex < 0) tui_partitionEditIndex = 0;
                        }
                    }
                } else if (key == 27) { // ESC
                    tui_partitionEditing = false;
                    tui_partitionEditIsLabel = false;
                    tui_partitionEditIsSize = false;
                } else if (key == '\n' || key == KEY_ENTER) {
                    // Apply edit
                    partman->changeBegin(selectedDevice);
                    if (tui_partitionCol == PART_COL_USEFOR) {
                        QStringList uses = selectedDevice->allowedUsesFor();
                        uses.prepend(u"-"_s);
                        if (tui_partitionEditIndex < uses.size()) {
                            if (tui_partitionEditIndex == 0) {
                                selectedDevice->usefor.clear();
                            } else {
                                selectedDevice->usefor = uses[tui_partitionEditIndex];
                            }
                        }
                    } else if (tui_partitionCol == PART_COL_FORMAT) {
                        QStringList formats = selectedDevice->allowedFormats();
                        if (tui_partitionEditIndex < formats.size()) {
                            selectedDevice->format = formats[tui_partitionEditIndex];
                        }
                    }
                    partman->changeEnd();
                    tui_partitionEditing = false;
                    tui_partitionEditIsLabel = false;
                    tui_partitionEditIsSize = false;
                }
            }
        } else if (key == KEY_UP && tui_partitionRow > 0) {
            tui_partitionRow--;
            tui_partitionEditing = false;
            tui_partitionEditIsLabel = false;
            tui_partitionEditIsSize = false;
            tui_focusPartitions = 0;
            if (tui_partitionRow < deviceCount) selectedDevice = devices[tui_partitionRow];
            int maxCol = (selectedDevice && selectedDevice->type != PartMan::Device::DRIVE) ? PART_COL_FORMAT : PART_COL_SIZE;
            if (tui_partitionCol > maxCol) tui_partitionCol = maxCol;
        } else if (key == KEY_DOWN && tui_partitionRow < deviceCount - 1) {
            tui_partitionRow++;
            tui_partitionEditing = false;
            tui_partitionEditIsLabel = false;
            tui_partitionEditIsSize = false;
            tui_focusPartitions = 0;
            if (tui_partitionRow < deviceCount) selectedDevice = devices[tui_partitionRow];
            int maxCol = (selectedDevice && selectedDevice->type != PartMan::Device::DRIVE) ? PART_COL_FORMAT : PART_COL_SIZE;
            if (tui_partitionCol > maxCol) tui_partitionCol = maxCol;
        } else if (key == KEY_LEFT) {
            int maxCol = (selectedDevice && selectedDevice->type != PartMan::Device::DRIVE) ? PART_COL_FORMAT : PART_COL_SIZE;
            if (tui_partitionCol > 0) {
                tui_partitionCol--;
            }
            tui_focusPartitions = 0;
            tui_partitionEditIsLabel = false;
            tui_partitionEditIsSize = false;
            if (tui_partitionCol > maxCol) tui_partitionCol = maxCol;
        } else if (key == KEY_RIGHT) {
            int maxCol = (selectedDevice && selectedDevice->type != PartMan::Device::DRIVE) ? PART_COL_FORMAT : PART_COL_SIZE;
            if (tui_partitionCol < maxCol) {
                tui_partitionCol++;
            }
            tui_focusPartitions = 0;
            tui_partitionEditIsLabel = false;
            tui_partitionEditIsSize = false;
        } else if (key == ' ' && selectedDevice) {
            // Toggle encrypt for partitions
            if (selectedDevice->type == PartMan::Device::PARTITION && selectedDevice->canEncrypt()) {
                partman->changeBegin(selectedDevice);
                selectedDevice->encrypt = !selectedDevice->encrypt;
                partman->changeEnd();
            }
        } else if ((key == '\n' || key == KEY_ENTER) && selectedDevice) {
            if (selectedDevice->type != PartMan::Device::DRIVE) {
                if (!tui_partitionEditing) {
                    // Start editing - get current options
                    if (tui_partitionCol == PART_COL_SIZE) {
                        if (selectedDevice->type == PartMan::Device::PARTITION && !selectedDevice->flags.oldLayout
                            && tui_partitionSizeEdit) {
                            tui_partitionEditing = true;
                            tui_partitionEditIsLabel = false;
                            tui_partitionEditIsSize = true;
                            double sizeMb = static_cast<double>(selectedDevice->size) / MB;
                            auto formatSizeValue = [](double value) -> QString {
                                double rounded = std::round(value * 10.0) / 10.0;
                                if (std::fabs(rounded - std::round(rounded)) < 0.05) {
                                    return QString::number(static_cast<long long>(std::llround(rounded)));
                                }
                                return QString::number(rounded, 'f', 1);
                            };
                            if (sizeMb >= 1024.0) {
                                tui_partitionSizeUnit = SIZE_UNIT_GB;
                                tui_partitionSizeOriginal = formatSizeValue(sizeMb / 1024.0);
                            } else {
                                tui_partitionSizeUnit = SIZE_UNIT_MB;
                                tui_partitionSizeOriginal = formatSizeValue(sizeMb);
                            }
                            tui_partitionSizeEdit->setText(tui_partitionSizeOriginal);
                        }
                    } else if (tui_partitionCol == PART_COL_LABEL) {
                        bool editableLabel = (selectedDevice->format != "PRESERVE"_L1 && selectedDevice->format != "DELETE"_L1)
                            && (selectedDevice->type == PartMan::Device::SUBVOLUME || !selectedDevice->usefor.isEmpty());
                        if (editableLabel && tui_partitionLabelEdit) {
                            tui_partitionEditing = true;
                            tui_partitionEditIsLabel = true;
                            tui_partitionLabelOriginal = selectedDevice->label;
                            tui_partitionLabelEdit->setText(selectedDevice->label);
                        }
                    } else {
                        tui_partitionEditing = true;
                        tui_partitionEditIsLabel = false;
                        tui_partitionEditIsSize = false;
                        tui_partitionEditIndex = 0;
                        if (tui_partitionCol == PART_COL_USEFOR) {
                            // Use For column - find current index ("-" at index 0 for empty)
                            QStringList uses = selectedDevice->allowedUsesFor();
                            uses.prepend(u"-"_s);
                            if (selectedDevice->usefor.isEmpty()) {
                                tui_partitionEditIndex = 0;
                            } else {
                                tui_partitionEditIndex = uses.indexOf(selectedDevice->usefor);
                                if (tui_partitionEditIndex < 0) tui_partitionEditIndex = 0;
                            }
                        } else if (tui_partitionCol == PART_COL_FORMAT) {
                            // Format column - find current index
                            QStringList formats = selectedDevice->allowedFormats();
                            QString curFmt = selectedDevice->format.isEmpty() ? selectedDevice->curFormat : selectedDevice->format;
                            tui_partitionEditIndex = formats.indexOf(curFmt);
                            if (tui_partitionEditIndex < 0) tui_partitionEditIndex = 0;
                        }
                    }
                }
            } else {
                // For drives, try to proceed to next page
                gotoAfterPartitionsTui();
            }
        } else if (key == 'c' || key == 'C') {
            // Clear drive
            if (selectedDevice && selectedDevice->type == PartMan::Device::DRIVE) {
                gui.treePartitions->setCurrentIndex(partman->index(selectedDevice));
                gui.pushPartClear->click();
            }
        } else if (key == 'a' || key == 'A') {
            // Add partition
            if (selectedDevice) {
                gui.treePartitions->setCurrentIndex(partman->index(selectedDevice));
                gui.pushPartAdd->click();
            }
        } else if (key == 'd' || key == 'D') {
            // Delete partition
            if (selectedDevice && selectedDevice->type != PartMan::Device::DRIVE) {
                if (!partman || !partman->removeDevice(selectedDevice)) {
                    beep();
                }
            } else {
                beep();
            }
        } else if (key == 'n' || key == 'N') {
            // New btrfs subvolume
            if (selectedDevice && partman
                && selectedDevice->isVolume()
                && selectedDevice->finalFormat() == "btrfs"_L1) {
                if (partman->newSubvolume(selectedDevice)) {
                    PartMan::Device *newSel = partman->selectedDevice();
                    if (newSel) {
                        devices.clear();
                        for (PartMan::Iterator it(*partman); PartMan::Device *device = *it; it.next()) {
                            if (device->type == PartMan::Device::DRIVE ||
                                device->type == PartMan::Device::PARTITION ||
                                device->type == PartMan::Device::VIRTUAL ||
                                device->type == PartMan::Device::SUBVOLUME) {
                                devices.push_back(device);
                            }
                        }
                        for (int i = 0; i < static_cast<int>(devices.size()); ++i) {
                            if (devices[i] == newSel) {
                                tui_partitionRow = i;
                                break;
                            }
                        }
                        tui_partitionCol = PART_COL_USEFOR;
                        tui_partitionEditing = false;
                        tui_partitionEditIsLabel = false;
                        tui_focusPartitions = 0;
                    }
                } else {
                    beep();
                }
            } else {
                beep();
            }
        } else if (key == 's' || key == 'S') {
            // Scan btrfs subvolumes
            if (selectedDevice && partman
                && selectedDevice->isVolume()
                && selectedDevice->finalFormat() == "btrfs"_L1
                && !selectedDevice->willFormat()) {
                if (!partman->scanSubvolumesFor(selectedDevice)) {
                    beep();
                }
            } else {
                beep();
            }
        } else if (key == 'r' || key == 'R') {
            // Reload
            gui.pushPartReload->click();
            tui_partitionRow = 0;
        } else if (key == 'p' || key == 'P') {
            // Launch partition manager
            endwin();
            int result = system("cfdisk");
            (void)result;
            refresh();
            clear();
            gui.pushPartReload->click();
            tui_partitionRow = 0;
        } else if (key == 'u' || key == 'U') {
            // Unlock LUKS partition
            if (selectedDevice
                && selectedDevice->type == PartMan::Device::PARTITION
                && selectedDevice->flags.oldLayout
                && selectedDevice->curFormat == "crypto_LUKS"_L1
                && !selectedDevice->mapCount) {
                tui_partitionUnlocking = true;
                tui_partitionEditing = false;
                tui_unlockDevice = selectedDevice;
                tui_unlockAddCrypttab = true;
                tui_unlockFocusPass = true;
                tui_unlockError.clear();
                if (tui_unlockPassEdit) tui_unlockPassEdit->setText(QString());
            } else {
                beep();
            }
        } else if (key == 'l' || key == 'L') {
            // Lock LUKS virtual volume
            if (selectedDevice
                && selectedDevice->type == PartMan::Device::VIRTUAL
                && selectedDevice->flags.volCrypto
                && crypto && partman) {
                if (!crypto->close(selectedDevice)) {
                    beep();
                }
                partman->scanVirtualDevices(true);
            } else {
                beep();
            }
        } else if (key == 't' || key == 'T') {
            // Toggle add-to-crypttab for LUKS partition
            if (selectedDevice
                && (selectedDevice->encrypt
                    || (selectedDevice->flags.oldLayout
                        && selectedDevice->curFormat == "crypto_LUKS"_L1))) {
                selectedDevice->addToCrypttab = !selectedDevice->addToCrypttab;
            } else {
                beep();
            }
        }
    } else if (currentPageIndex == Step::ENCRYPTION) {
        qtui::TLineEdit* passFields[2] = {nullptr, nullptr};

        if (tui_textCryptoPass) {
            passFields[0] = dynamic_cast<qtui::TLineEdit*>(tui_textCryptoPass->tuiWidget());
        }
        if (tui_textCryptoPass2) {
            passFields[1] = dynamic_cast<qtui::TLineEdit*>(tui_textCryptoPass2->tuiWidget());
        }

        if (key != '\n' && key != KEY_ENTER && key != TUI_KEY_ALT_LEFT) {
            tui_encryptionError.clear();
        }

        if (key == TUI_KEY_ALT_LEFT) {
            gotoPage(currentPageIndex - 1);
            return;
        } else if (key == 27) {
            tui_focusEncryption = (tui_focusEncryption + 1) % 2;
        } else if (key == '\t' || key == KEY_DOWN) {
            tui_focusEncryption = (tui_focusEncryption + 1) % 2;
        } else if (key == KEY_UP) {
            tui_focusEncryption = (tui_focusEncryption + 1) % 2;
        } else if (key == '\n' || key == KEY_ENTER) {
            const QString pass1 = gui.textCryptoPass->text();
            const QString pass2 = gui.textCryptoPass2->text();
            const bool match = !pass1.isEmpty() && pass1 == pass2;
            if (match && (!crypto || crypto->valid())) {
                gotoPage(currentPageIndex + 1);
            } else {
                tui_encryptionError = tr("Passwords must match and not be empty.");
            }
        } else if (passFields[tui_focusEncryption]) {
            passFields[tui_focusEncryption]->handleKey(key);
            if (tui_focusEncryption == 0) {
                gui.textCryptoPass->setText(passFields[0]->text());
            } else if (tui_focusEncryption == 1) {
                gui.textCryptoPass2->setText(passFields[1]->text());
            }
        }

        for (int i = 0; i < 2; ++i) {
            if (passFields[i]) {
                passFields[i]->setFocus(i == tui_focusEncryption);
            }
        }
    } else if (currentPageIndex == Step::CONFIRM) {
        // pageConfirm: Enter to continue to configuration, ESC or backspace to go back
        if (key == TUI_KEY_ALT_LEFT) {
            gotoPage(currentPageIndex - 1);
        } else if (key == '\n' || key == KEY_ENTER) {
            gotoPage(currentPageIndex + 1);
        } else if (key == KEY_BACKSPACE) {
            gotoPage(Step::INSTALLATION);  // Go back to installation type selection
        }
    } else if (currentPageIndex == Step::BOOT) {
        qtui::TCheckBox* bootInstall = tui_checkBootInstall
            ? dynamic_cast<qtui::TCheckBox*>(tui_checkBootInstall->tuiWidget())
            : nullptr;
        qtui::TRadioButton* radios[3] = {nullptr, nullptr, nullptr};
        qtui::TComboBox* comboBoot = tui_comboBoot
            ? dynamic_cast<qtui::TComboBox*>(tui_comboBoot->tuiWidget())
            : nullptr;
        qtui::TCheckBox* hostSpecific = tui_checkBootHostSpecific
            ? dynamic_cast<qtui::TCheckBox*>(tui_checkBootHostSpecific->tuiWidget())
            : nullptr;

        if (tui_radioBootMBR) {
            radios[0] = dynamic_cast<qtui::TRadioButton*>(tui_radioBootMBR->tuiWidget());
        }
        if (tui_radioBootPBR) {
            radios[1] = dynamic_cast<qtui::TRadioButton*>(tui_radioBootPBR->tuiWidget());
        }
        if (tui_radioBootESP) {
            radios[2] = dynamic_cast<qtui::TRadioButton*>(tui_radioBootESP->tuiWidget());
        }

        auto isFocusable = [&](int index) -> bool {
            switch (index) {
                case 0: return bootInstall && bootInstall->isEnabled();
                case 1: return radios[0] && radios[0]->isEnabled();
                case 2: return radios[1] && radios[1]->isEnabled();
                case 3: return radios[2] && radios[2]->isEnabled();
                case 4: return comboBoot && comboBoot->isEnabled();
                case 5: return hostSpecific && hostSpecific->isEnabled();
                default: return false;
            }
        };

        auto moveFocus = [&](int delta) {
            int idx = tui_focusBootField;
            for (int i = 0; i < 6; ++i) {
                idx = (idx + delta + 6) % 6;
                if (isFocusable(idx)) {
                    tui_focusBootField = idx;
                    break;
                }
            }
        };

        auto applyFocus = [&]() {
            if (bootInstall) bootInstall->setFocus(tui_focusBootField == 0);
            if (radios[0]) radios[0]->setFocus(tui_focusBootField == 1);
            if (radios[1]) radios[1]->setFocus(tui_focusBootField == 2);
            if (radios[2]) radios[2]->setFocus(tui_focusBootField == 3);
            if (comboBoot) comboBoot->setFocus(tui_focusBootField == 4);
            if (hostSpecific) hostSpecific->setFocus(tui_focusBootField == 5);
        };
        auto ensureFocus = [&]() {
            if (!isFocusable(tui_focusBootField)) {
                moveFocus(1);
            }
        };

        if (key == TUI_KEY_ALT_LEFT || key == KEY_BACKSPACE) {
            gotoPage(Step::CONFIRM);
            return;
        } else if (key == '\t' || key == KEY_DOWN) {
            moveFocus(1);
        } else if (key == KEY_UP) {
            moveFocus(-1);
        } else if (key == ' ') {
            if (tui_focusBootField == 0 && bootInstall) {
                bootInstall->toggle();
                gui.boxBoot->setChecked(bootInstall->isChecked());
            } else if (tui_focusBootField >= 1 && tui_focusBootField <= 3) {
                const int idx = tui_focusBootField - 1;
                if (radios[idx]) {
                    radios[idx]->setChecked(true);
                    if (idx == 0) gui.radioBootMBR->setChecked(true);
                    if (idx == 1) gui.radioBootPBR->setChecked(true);
                    if (idx == 2) gui.radioBootESP->setChecked(true);
                }
            } else if (tui_focusBootField == 5 && hostSpecific) {
                hostSpecific->toggle();
                gui.checkBootHostSpecific->setChecked(hostSpecific->isChecked());
            }
        } else if (tui_focusBootField == 4 && comboBoot) {
            comboBoot->handleKey(key);
            gui.comboBoot->setCurrentIndex(comboBoot->currentIndex());
        }

        if (key == '\n' || key == KEY_ENTER) {
            if (tui_focusBootField == 4 && comboBoot && comboBoot->isPopupVisible()) {
                comboBoot->handleKey(key);
            } else {
                gotoPage(Step::SWAP);
            }
        }

        ensureFocus();
        applyFocus();
    } else if (currentPageIndex == Step::SWAP) {
        qtui::TCheckBox* swapCheck = tui_checkSwapFile
            ? dynamic_cast<qtui::TCheckBox*>(tui_checkSwapFile->tuiWidget())
            : nullptr;
        qtui::TLineEdit* swapFileEdit = tui_textSwapFile
            ? dynamic_cast<qtui::TLineEdit*>(tui_textSwapFile->tuiWidget())
            : nullptr;
        qtui::TLineEdit* swapSizeEdit = tui_textSwapSize
            ? dynamic_cast<qtui::TLineEdit*>(tui_textSwapSize->tuiWidget())
            : nullptr;
        qtui::TCheckBox* hibernationCheck = tui_checkHibernation
            ? dynamic_cast<qtui::TCheckBox*>(tui_checkHibernation->tuiWidget())
            : nullptr;
        qtui::TCheckBox* zramCheck = tui_checkZram
            ? dynamic_cast<qtui::TCheckBox*>(tui_checkZram->tuiWidget())
            : nullptr;
        qtui::TRadioButton* zramPercentRadio = tui_radioZramPercent
            ? dynamic_cast<qtui::TRadioButton*>(tui_radioZramPercent->tuiWidget())
            : nullptr;
        qtui::TLineEdit* zramPercentEdit = tui_textZramPercent
            ? dynamic_cast<qtui::TLineEdit*>(tui_textZramPercent->tuiWidget())
            : nullptr;
        qtui::TRadioButton* zramSizeRadio = tui_radioZramSize
            ? dynamic_cast<qtui::TRadioButton*>(tui_radioZramSize->tuiWidget())
            : nullptr;
        qtui::TLineEdit* zramSizeEdit = tui_textZramSize
            ? dynamic_cast<qtui::TLineEdit*>(tui_textZramSize->tuiWidget())
            : nullptr;

        auto isFocusable = [&](int index) -> bool {
            switch (index) {
                case 0: return swapCheck && swapCheck->isEnabled();
                case 1: return swapFileEdit && swapFileEdit->isEnabled();
                case 2: return swapSizeEdit && swapSizeEdit->isEnabled();
                case 3: return hibernationCheck && hibernationCheck->isEnabled();
                case 4: return zramCheck && zramCheck->isEnabled();
                case 5: return zramPercentRadio && zramPercentRadio->isEnabled();
                case 6: return zramPercentEdit && zramPercentEdit->isEnabled();
                case 7: return zramSizeRadio && zramSizeRadio->isEnabled();
                case 8: return zramSizeEdit && zramSizeEdit->isEnabled();
                default: return false;
            }
        };

        auto moveFocus = [&](int delta) {
            int idx = tui_focusSwap;
            for (int i = 0; i < 9; ++i) {
                idx = (idx + delta + 9) % 9;
                if (isFocusable(idx)) {
                    tui_focusSwap = idx;
                    break;
                }
            }
        };

        auto applyFocus = [&]() {
            if (swapCheck) swapCheck->setFocus(tui_focusSwap == 0);
            if (swapFileEdit) swapFileEdit->setFocus(tui_focusSwap == 1);
            if (swapSizeEdit) swapSizeEdit->setFocus(tui_focusSwap == 2);
            if (hibernationCheck) hibernationCheck->setFocus(tui_focusSwap == 3);
            if (zramCheck) zramCheck->setFocus(tui_focusSwap == 4);
            if (zramPercentRadio) zramPercentRadio->setFocus(tui_focusSwap == 5);
            if (zramPercentEdit) zramPercentEdit->setFocus(tui_focusSwap == 6);
            if (zramSizeRadio) zramSizeRadio->setFocus(tui_focusSwap == 7);
            if (zramSizeEdit) zramSizeEdit->setFocus(tui_focusSwap == 8);
        };

        auto ensureFocus = [&]() {
            if (!isFocusable(tui_focusSwap)) {
                moveFocus(1);
            }
        };

        const bool swapFocusIsEdit = (tui_focusSwap == 1 || tui_focusSwap == 2 || tui_focusSwap == 6 || tui_focusSwap == 8);
        if (key == TUI_KEY_ALT_LEFT || (key == KEY_BACKSPACE && !swapFocusIsEdit)) {
            gotoPage(Step::BOOT);
            return;
        }
        if (key == 27 && swapFocusIsEdit) {
            moveFocus(1);
            ensureFocus();
            applyFocus();
            return;
        }
        if (key == '\n' || key == KEY_ENTER) {
            gotoPage(Step::SERVICES);
            return;
        }

        if (key == '\t' || key == KEY_DOWN) {
            moveFocus(1);
        } else if (key == KEY_UP) {
            moveFocus(-1);
        } else if (key == 'r' || key == 'R') {
            gui.pushSwapSizeReset->click();
            syncSwapTuiFromGui();
        } else if (key == ' ') {
            if (tui_focusSwap == 0 && swapCheck) {
                swapCheck->toggle();
                gui.boxSwapFile->setChecked(swapCheck->isChecked());
                if (swapman) {
                    swapman->updateBounds();
                }
                syncSwapTuiFromGui();
            } else if (tui_focusSwap == 3 && hibernationCheck) {
                hibernationCheck->toggle();
                gui.checkHibernation->setChecked(hibernationCheck->isChecked());
                syncSwapTuiFromGui();
            } else if (tui_focusSwap == 4 && zramCheck) {
                zramCheck->toggle();
                gui.boxSwapZram->setChecked(zramCheck->isChecked());
                syncSwapTuiFromGui();
            } else if (tui_focusSwap == 5 && zramPercentRadio) {
                zramPercentRadio->setChecked(true);
                gui.radioZramPercent->setChecked(true);
                syncSwapTuiFromGui();
            } else if (tui_focusSwap == 7 && zramSizeRadio) {
                zramSizeRadio->setChecked(true);
                gui.radioZramSize->setChecked(true);
                syncSwapTuiFromGui();
            }
        } else if (tui_focusSwap == 1 && swapFileEdit) {
            swapFileEdit->handleKey(key);
            gui.textSwapFile->setText(swapFileEdit->text());
            if (swapman) {
                swapman->updateBounds();
            }
            syncSwapTuiFromGui();
        } else if (tui_focusSwap == 2 && swapSizeEdit) {
            swapSizeEdit->handleKey(key);
            bool ok = false;
            const int value = swapSizeEdit->text().toInt(&ok);
            if (ok && value > 0) {
                gui.spinSwapSize->setValue(value);
            }
            syncSwapTuiFromGui();
        } else if (tui_focusSwap == 6 && zramPercentEdit) {
            zramPercentEdit->handleKey(key);
            bool ok = false;
            const int value = zramPercentEdit->text().toInt(&ok);
            if (ok && value > 0) {
                gui.spinZramPercent->setValue(value);
            }
            syncSwapTuiFromGui();
        } else if (tui_focusSwap == 8 && zramSizeEdit) {
            zramSizeEdit->handleKey(key);
            bool ok = false;
            const int value = zramSizeEdit->text().toInt(&ok);
            if (ok && value > 0) {
                gui.spinZramSize->setValue(value);
            }
            syncSwapTuiFromGui();
        }

        ensureFocus();
        applyFocus();
    } else if (currentPageIndex == Step::NETWORK) {
        qtui::TLineEdit* fields[3] = {nullptr, nullptr, nullptr};
        qtui::TCheckBox* samba = nullptr;

        if (tui_textComputerName) {
            fields[0] = dynamic_cast<qtui::TLineEdit*>(tui_textComputerName->tuiWidget());
        }
        if (tui_textComputerDomain) {
            fields[1] = dynamic_cast<qtui::TLineEdit*>(tui_textComputerDomain->tuiWidget());
        }
        if (tui_textComputerGroup) {
            fields[2] = dynamic_cast<qtui::TLineEdit*>(tui_textComputerGroup->tuiWidget());
        }
        if (tui_checkSamba) {
            samba = dynamic_cast<qtui::TCheckBox*>(tui_checkSamba->tuiWidget());
        }

        auto isFocusable = [&](int index) -> bool {
            if (index >= 0 && index < 3) {
                return fields[index] && fields[index]->isEnabled();
            }
            if (index == 3) {
                return samba && samba->isEnabled();
            }
            return false;
        };

        auto moveFocus = [&](int delta) {
            int idx = tui_focusNetwork;
            for (int i = 0; i < 4; ++i) {
                idx = (idx + delta + 4) % 4;
                if (isFocusable(idx)) {
                    tui_focusNetwork = idx;
                    break;
                }
            }
        };

        const bool focusIsEdit = (tui_focusNetwork >= 0 && tui_focusNetwork <= 2);
        if (key != '\n' && key != KEY_ENTER && key != TUI_KEY_ALT_LEFT) {
            tui_networkError.clear();
        }

        if (key == TUI_KEY_ALT_LEFT || (key == KEY_BACKSPACE && !focusIsEdit)) {
            gotoPage(currentPageIndex - 1);
            return;
        } else if (key == 27 && focusIsEdit) {
            moveFocus(1);
            return;
        } else if (key == '\t' || key == KEY_DOWN) {
            moveFocus(1);
        } else if (key == KEY_UP) {
            moveFocus(-1);
        } else if (key == ' ' && tui_focusNetwork == 3 && samba) {
            samba->toggle();
            gui.checkSamba->setChecked(samba->isChecked());
        } else if (key == '\n' || key == KEY_ENTER) {
            static const QRegularExpression nametest(u"[^0-9a-zA-Z-.]|^[.-]|[.-]$|\\.\\."_s);
            if (gui.textComputerName->text().isEmpty()) {
                tui_networkError = tr("Please enter a computer name.");
                tui_focusNetwork = 0;
            } else if (gui.textComputerName->text().contains(nametest)) {
                tui_networkError = tr("The computer name contains invalid characters.");
                tui_focusNetwork = 0;
            } else if (gui.textComputerDomain->text().isEmpty()) {
                tui_networkError = tr("Please enter a domain name.");
                tui_focusNetwork = 1;
            } else if (gui.textComputerDomain->text().contains(nametest)) {
                tui_networkError = tr("The computer domain contains invalid characters.");
                tui_focusNetwork = 1;
            } else if (gui.textComputerGroup->isEnabled() && gui.textComputerGroup->text().isEmpty()) {
                tui_networkError = tr("Please enter a workgroup.");
                tui_focusNetwork = 2;
            } else {
                if (!gui.textComputerGroup->isEnabled()) {
                    gui.textComputerGroup->clear();
                }
                gotoPage(currentPageIndex + 1);
            }
        } else if (tui_focusNetwork < 3 && fields[tui_focusNetwork]) {
            fields[tui_focusNetwork]->handleKey(key);
            if (tui_focusNetwork == 0) gui.textComputerName->setText(fields[0]->text());
            if (tui_focusNetwork == 1) gui.textComputerDomain->setText(fields[1]->text());
            if (tui_focusNetwork == 2) gui.textComputerGroup->setText(fields[2]->text());
        }

        if (!isFocusable(tui_focusNetwork)) {
            moveFocus(1);
        }

        for (int i = 0; i < 3; ++i) {
            if (fields[i]) {
                fields[i]->setFocus(i == tui_focusNetwork);
            }
        }
        if (samba) {
            samba->setFocus(tui_focusNetwork == 3);
        }
    } else if (currentPageIndex == Step::LOCALIZATION) {
        // pageLocalization: TAB to switch controls, UP/DOWN to select items
        qtui::TComboBox* combos[3] = {nullptr, nullptr, nullptr};
        qtui::TCheckBox* localClock = nullptr;
        qtui::TRadioButton* radios[2] = {nullptr, nullptr};

        if (tui_comboLocale) {
            combos[0] = dynamic_cast<qtui::TComboBox*>(tui_comboLocale->tuiWidget());
        }
        if (tui_comboTimeArea) {
            combos[1] = dynamic_cast<qtui::TComboBox*>(tui_comboTimeArea->tuiWidget());
        }
        if (tui_comboTimeZone) {
            combos[2] = dynamic_cast<qtui::TComboBox*>(tui_comboTimeZone->tuiWidget());
        }
        if (tui_checkLocalClock) {
            localClock = dynamic_cast<qtui::TCheckBox*>(tui_checkLocalClock->tuiWidget());
        }
        if (tui_radioClock24) {
            radios[0] = dynamic_cast<qtui::TRadioButton*>(tui_radioClock24->tuiWidget());
        }
        if (tui_radioClock12) {
            radios[1] = dynamic_cast<qtui::TRadioButton*>(tui_radioClock12->tuiWidget());
        }

        auto isFocusable = [&](int index) -> bool {
            switch (index) {
                case 0: return combos[0] && combos[0]->isEnabled();
                case 1: return combos[1] && combos[1]->isEnabled();
                case 2: return combos[2] && combos[2]->isEnabled();
                case 3: return localClock && localClock->isEnabled();
                case 4: return radios[0] && radios[0]->isEnabled();
                case 5: return radios[1] && radios[1]->isEnabled();
                default: return false;
            }
        };

        auto moveFocus = [&](int delta) {
            int idx = tui_focusLocalization;
            for (int i = 0; i < 6; ++i) {
                idx = (idx + delta + 6) % 6;
                if (isFocusable(idx)) {
                    tui_focusLocalization = idx;
                    break;
                }
            }
        };

        auto syncTimeZoneList = [&]() {
            if (!tui_comboTimeZone) {
                return;
            }
            if (tui_comboTimeZone->count() != gui.comboTimeZone->count()) {
                tui_comboTimeZone->clear();
                for (int i = 0; i < gui.comboTimeZone->count(); ++i) {
                    tui_comboTimeZone->addItem(gui.comboTimeZone->itemText(i), gui.comboTimeZone->itemData(i));
                }
            }
            tui_comboTimeZone->setCurrentIndex(gui.comboTimeZone->currentIndex());
        };

        const bool comboFocused = (tui_focusLocalization <= 2 && combos[tui_focusLocalization]);
        const bool comboPopupVisible = comboFocused && combos[tui_focusLocalization]->isPopupVisible();

        if (key == TUI_KEY_ALT_LEFT || key == KEY_BACKSPACE) {
            gotoPage(currentPageIndex - 1);
            return;
        } else if (comboFocused && (comboPopupVisible || key == KEY_UP || key == KEY_DOWN
            || key == ' ' || key == '\n' || key == KEY_ENTER)) {
            const bool wasPopupVisible = comboPopupVisible;
            combos[tui_focusLocalization]->handleKey(key);
            if (tui_focusLocalization == 0 && tui_comboLocale && gui.comboLocale) {
                gui.comboLocale->setCurrentIndex(tui_comboLocale->currentIndex());
                if (tui_radioClock24) tui_radioClock24->setChecked(gui.radioClock24->isChecked());
                if (tui_radioClock12) tui_radioClock12->setChecked(gui.radioClock12->isChecked());
            } else if (tui_focusLocalization == 1 && tui_comboTimeArea && gui.comboTimeArea) {
                gui.comboTimeArea->setCurrentIndex(tui_comboTimeArea->currentIndex());
                syncTimeZoneList();
            } else if (tui_focusLocalization == 2 && tui_comboTimeZone && gui.comboTimeZone) {
                gui.comboTimeZone->setCurrentIndex(tui_comboTimeZone->currentIndex());
            }
            // If popup was visible and we just selected an item, don't advance page
            if (wasPopupVisible && (key == '\n' || key == KEY_ENTER)) {
                return;
            }
        } else if (key == '\t') {
            moveFocus(1);
        } else if (key == KEY_DOWN) {
            moveFocus(1);
        } else if (key == KEY_UP) {
            moveFocus(-1);
        } else if (key == ' ') {
            if (tui_focusLocalization == 3 && localClock) {
                localClock->toggle();
                gui.checkLocalClock->setChecked(localClock->isChecked());
            } else if (tui_focusLocalization == 4 && radios[0]) {
                radios[0]->setChecked(true);
                gui.radioClock24->setChecked(true);
            } else if (tui_focusLocalization == 5 && radios[1]) {
                radios[1]->setChecked(true);
                gui.radioClock12->setChecked(true);
            }
        }

        if (key == '\n' || key == KEY_ENTER) {
            const bool popupVisible = (combos[0] && combos[0]->isPopupVisible())
                || (combos[1] && combos[1]->isPopupVisible())
                || (combos[2] && combos[2]->isPopupVisible());
            if (!popupVisible) {
                gotoPage(currentPageIndex + 1);
            }
        }

        if (!isFocusable(tui_focusLocalization)) {
            moveFocus(1);
        }

        if (combos[0]) combos[0]->setFocus(tui_focusLocalization == 0);
        if (combos[1]) combos[1]->setFocus(tui_focusLocalization == 1);
        if (combos[2]) combos[2]->setFocus(tui_focusLocalization == 2);
        if (localClock) localClock->setFocus(tui_focusLocalization == 3);
        if (radios[0]) radios[0]->setFocus(tui_focusLocalization == 4);
        if (radios[1]) radios[1]->setFocus(tui_focusLocalization == 5);
    } else if (currentPageIndex == Step::SERVICES) {
        buildServicesTui();
        syncServicesTuiFromGui();

        if (tui_serviceItems.isEmpty()) {
            if (key == TUI_KEY_ALT_LEFT || key == KEY_BACKSPACE) {
                gotoPage(Step::LOCALIZATION);
            } else if (key == '\n' || key == KEY_ENTER) {
                gotoPage(currentPageIndex + 1);
            }
            return;
        }

        auto isFocusable = [&](int index) -> bool {
            return index >= 0 && index < tui_serviceItems.size();
        };

        auto moveFocus = [&](int delta) {
            int idx = tui_focusServices;
            for (int i = 0; i < tui_serviceItems.size(); ++i) {
                idx = (idx + delta + tui_serviceItems.size()) % tui_serviceItems.size();
                if (isFocusable(idx)) {
                    tui_focusServices = idx;
                    break;
                }
            }
        };

        if (key == TUI_KEY_ALT_LEFT || key == KEY_BACKSPACE) {
            gotoPage(Step::LOCALIZATION);
        } else if (key == KEY_UP) {
            moveFocus(-1);
        } else if (key == KEY_DOWN || key == '\t') {
            moveFocus(1);
        } else if (key == ' ') {
            auto &entry = tui_serviceItems[tui_focusServices];
            if (entry.checkbox && entry.item) {
                auto* checkbox = dynamic_cast<qtui::TCheckBox*>(entry.checkbox->tuiWidget());
                if (checkbox) {
                    checkbox->toggle();
                    entry.item->setCheckState(0, checkbox->isChecked() ? Qt::Checked : Qt::Unchecked);
                }
            }
        } else if (key == '\n' || key == KEY_ENTER) {
            gotoPage(currentPageIndex + 1);
        }

        for (int i = 0; i < tui_serviceItems.size(); ++i) {
            auto &entry = tui_serviceItems[i];
            if (entry.checkbox) {
                auto* checkbox = dynamic_cast<qtui::TCheckBox*>(entry.checkbox->tuiWidget());
                if (checkbox) {
                    checkbox->setFocus(i == tui_focusServices);
                }
            }
        }
    } else if (currentPageIndex == Step::USER_ACCOUNTS) {
        qtui::TLineEdit* edits[5] = {nullptr, nullptr, nullptr, nullptr, nullptr};
        qtui::TCheckBox* rootCheck = nullptr;
        qtui::TCheckBox* autoLogin = nullptr;
        qtui::TCheckBox* saveDesktop = nullptr;

        if (tui_textUserName) {
            edits[0] = dynamic_cast<qtui::TLineEdit*>(tui_textUserName->tuiWidget());
        }
        if (tui_textUserPass) {
            edits[1] = dynamic_cast<qtui::TLineEdit*>(tui_textUserPass->tuiWidget());
        }
        if (tui_textUserPass2) {
            edits[2] = dynamic_cast<qtui::TLineEdit*>(tui_textUserPass2->tuiWidget());
        }
        if (tui_textRootPass) {
            edits[3] = dynamic_cast<qtui::TLineEdit*>(tui_textRootPass->tuiWidget());
        }
        if (tui_textRootPass2) {
            edits[4] = dynamic_cast<qtui::TLineEdit*>(tui_textRootPass2->tuiWidget());
        }
        if (tui_checkRootAccount) {
            rootCheck = dynamic_cast<qtui::TCheckBox*>(tui_checkRootAccount->tuiWidget());
        }
        if (tui_checkAutoLogin) {
            autoLogin = dynamic_cast<qtui::TCheckBox*>(tui_checkAutoLogin->tuiWidget());
        }
        if (tui_checkSaveDesktop) {
            saveDesktop = dynamic_cast<qtui::TCheckBox*>(tui_checkSaveDesktop->tuiWidget());
        }

        auto isFocusable = [&](int index) -> bool {
            switch (index) {
                case 0: return edits[0] && edits[0]->isEnabled();
                case 1: return edits[1] && edits[1]->isEnabled();
                case 2: return edits[2] && edits[2]->isEnabled();
                case 3: return rootCheck && rootCheck->isEnabled();
                case 4: return edits[3] && edits[3]->isEnabled();
                case 5: return edits[4] && edits[4]->isEnabled();
                case 6: return autoLogin && autoLogin->isEnabled();
                case 7: return saveDesktop && saveDesktop->isEnabled();
                default: return false;
            }
        };

        auto moveFocus = [&](int delta) {
            int idx = tui_focusUserAccounts;
            for (int i = 0; i < 8; ++i) {
                idx = (idx + delta + 8) % 8;
                if (isFocusable(idx)) {
                    tui_focusUserAccounts = idx;
                    break;
                }
            }
        };

        const bool userFocusIsEdit = (tui_focusUserAccounts == 0 || tui_focusUserAccounts == 1
            || tui_focusUserAccounts == 2 || tui_focusUserAccounts == 4 || tui_focusUserAccounts == 5);

        if (key != '\n' && key != KEY_ENTER && key != TUI_KEY_ALT_LEFT) {
            tui_userError.clear();
        }

        if (key == TUI_KEY_ALT_LEFT || (key == KEY_BACKSPACE && !userFocusIsEdit)) {
            gotoPage(currentPageIndex - 1);
            return;
        } else if (key == 27 && userFocusIsEdit) {
            moveFocus(1);
            return;
        } else if (key == '\t' || key == KEY_DOWN) {
            moveFocus(1);
        } else if (key == KEY_UP) {
            moveFocus(-1);
        } else if (key == ' ' && rootCheck && tui_focusUserAccounts == 3) {
            rootCheck->toggle();
            gui.boxRootAccount->setChecked(rootCheck->isChecked());
            tui_confirmEmptyRootPass = false;
        } else if (key == ' ' && autoLogin && tui_focusUserAccounts == 6) {
            autoLogin->toggle();
            gui.checkAutoLogin->setChecked(autoLogin->isChecked());
        } else if (key == ' ' && saveDesktop && tui_focusUserAccounts == 7) {
            saveDesktop->toggle();
            gui.checkSaveDesktop->setChecked(saveDesktop->isChecked());
        } else if (key == '\n' || key == KEY_ENTER) {
            const QString &userName = gui.textUserName->text();
            const QString userPass1 = gui.textUserPass->text();
            const QString userPass2 = gui.textUserPass2->text();
            const QString rootPass1 = gui.textRootPass->text();
            const QString rootPass2 = gui.textRootPass2->text();
            const bool rootEnabled = gui.boxRootAccount->isChecked();
            const bool allRequiredFilled = !userName.isEmpty()
                && !userPass1.isEmpty()
                && !userPass2.isEmpty()
                && (!rootEnabled || (!rootPass1.isEmpty() && !rootPass2.isEmpty()));

            if (!allRequiredFilled) {
                auto isRequiredField = [&](int index) -> bool {
                    return index == 0 || index == 1 || index == 2 || (rootEnabled && (index == 4 || index == 5));
                };
                auto isFieldEmpty = [&](int index) -> bool {
                    switch (index) {
                        case 0: return gui.textUserName->text().isEmpty();
                        case 1: return gui.textUserPass->text().isEmpty();
                        case 2: return gui.textUserPass2->text().isEmpty();
                        case 4: return gui.textRootPass->text().isEmpty();
                        case 5: return gui.textRootPass2->text().isEmpty();
                        default: return false;
                    }
                };
                for (int i = 1; i <= 8; ++i) {
                    const int idx = (tui_focusUserAccounts + i) % 8;
                    if (isRequiredField(idx) && isFieldEmpty(idx)) {
                        tui_focusUserAccounts = idx;
                        break;
                    }
                }
                if (isRequiredField(tui_focusUserAccounts) && isFieldEmpty(tui_focusUserAccounts)) {
                    tui_userError = tr("Please fill in all required fields before continuing.");
                }
            } else {
                const bool userMatch = (userPass1 == userPass2);
                const bool rootMatch = (rootPass1 == rootPass2);
                static const QRegularExpression usertest(u"^[a-zA-Z_][a-zA-Z0-9_-]*[$]?$"_s);

                if (!userName.contains(usertest)) {
                    tui_userError = tr("The user name cannot contain special characters or spaces.");
                    tui_focusUserAccounts = 0;
                } else if (!userMatch) {
                    tui_userError = tr("Please ensure the passwords match.");
                    tui_focusUserAccounts = 1;
                } else if (rootEnabled && !rootMatch) {
                    tui_userError = tr("Please ensure the passwords match.");
                    tui_focusUserAccounts = 4;
                } else {
                    QFile file(u"/etc/passwd"_s);
                    bool inUse = false;
                    if (file.open(QFile::ReadOnly | QFile::Text)) {
                        const QByteArray match = QStringLiteral("%1:").arg(userName).toUtf8();
                        while (!file.atEnd()) {
                            if (file.readLine().startsWith(match)) {
                                inUse = true;
                                break;
                            }
                        }
                    }
                    if (inUse) {
                        tui_userError = tr("The chosen user name is in use.");
                        tui_focusUserAccounts = 0;
                    } else if (!automatic && userPass1.isEmpty()) {
                        if (!tui_confirmEmptyUserPass) {
                            tui_userError = tr("You did not provide a passphrase for %1.").arg(userName);
                            tui_confirmEmptyUserPass = true;
                            tui_focusUserAccounts = 1;
                        } else {
                            gotoPage(currentPageIndex + 1);
                        }
                    } else if (!automatic && rootEnabled && rootPass1.isEmpty()) {
                        if (!tui_confirmEmptyRootPass) {
                            tui_userError = tr("You did not provide a password for the root account.");
                            tui_confirmEmptyRootPass = true;
                            tui_focusUserAccounts = 4;
                        } else {
                            gotoPage(currentPageIndex + 1);
                        }
                    } else {
                        gotoPage(currentPageIndex + 1);
                    }
                }
            }
        } else if (tui_focusUserAccounts >= 0 && tui_focusUserAccounts <= 2) {
            const int idx = tui_focusUserAccounts;
            if (edits[idx]) {
                edits[idx]->handleKey(key);
                if (idx == 0) gui.textUserName->setText(edits[0]->text());
                if (idx == 1) gui.textUserPass->setText(edits[1]->text());
                if (idx == 2) gui.textUserPass2->setText(edits[2]->text());
                if (idx == 0 || idx == 1 || idx == 2) {
                    tui_confirmEmptyUserPass = false;
                }
            }
        } else if (tui_focusUserAccounts == 4 && edits[3]) {
            edits[3]->handleKey(key);
            gui.textRootPass->setText(edits[3]->text());
            tui_confirmEmptyRootPass = false;
        } else if (tui_focusUserAccounts == 5 && edits[4]) {
            edits[4]->handleKey(key);
            gui.textRootPass2->setText(edits[4]->text());
            tui_confirmEmptyRootPass = false;
        }

        if (!isFocusable(tui_focusUserAccounts)) {
            moveFocus(1);
        }

        for (int i = 0; i < 3; ++i) {
            if (edits[i]) {
                edits[i]->setFocus(i == tui_focusUserAccounts);
            }
        }
        if (rootCheck) rootCheck->setFocus(tui_focusUserAccounts == 3);
        if (edits[3]) edits[3]->setFocus(tui_focusUserAccounts == 4);
        if (edits[4]) edits[4]->setFocus(tui_focusUserAccounts == 5);
        if (autoLogin) autoLogin->setFocus(tui_focusUserAccounts == 6);
        if (saveDesktop) saveDesktop->setFocus(tui_focusUserAccounts == 7);
    } else if (currentPageIndex == Step::OLD_HOME) {
        // pageOldHome: UP/DOWN to navigate, SPACE to select
        qtui::TRadioButton* radios[3] = {nullptr, nullptr, nullptr};

        if (tui_radioOldHomeUse) {
            radios[0] = dynamic_cast<qtui::TRadioButton*>(tui_radioOldHomeUse->tuiWidget());
        }
        if (tui_radioOldHomeSave) {
            radios[1] = dynamic_cast<qtui::TRadioButton*>(tui_radioOldHomeSave->tuiWidget());
        }
        if (tui_radioOldHomeDelete) {
            radios[2] = dynamic_cast<qtui::TRadioButton*>(tui_radioOldHomeDelete->tuiWidget());
        }

        if (key != '\n' && key != KEY_ENTER && key != TUI_KEY_ALT_LEFT) {
            tui_oldHomeError.clear();
        }

        if (key == TUI_KEY_ALT_LEFT || key == KEY_BACKSPACE) {
            gotoPage(Step::USER_ACCOUNTS);
            return;
        } else if (key == KEY_UP && tui_focusOldHome > 0) {
            tui_focusOldHome--;
        } else if (key == KEY_DOWN && tui_focusOldHome < 2) {
            tui_focusOldHome++;
        } else if (key == ' ') {
            // Space selects the current radio button
            if (radios[tui_focusOldHome]) {
                radios[tui_focusOldHome]->setChecked(true);
                // Sync to GUI
                if (tui_focusOldHome == 0) {
                    gui.radioOldHomeUse->setChecked(true);
                } else if (tui_focusOldHome == 1) {
                    gui.radioOldHomeSave->setChecked(true);
                } else if (tui_focusOldHome == 2) {
                    gui.radioOldHomeDelete->setChecked(true);
                }
            }
        } else if (key == '\n' || key == KEY_ENTER) {
            if (!gui.radioOldHomeUse->isChecked()
                && !gui.radioOldHomeSave->isChecked()
                && !gui.radioOldHomeDelete->isChecked()) {
                tui_oldHomeError = tr("Please select an option to continue.");
            } else {
                gotoPage(currentPageIndex + 1);
            }
        }

        // Update focus display
        for (int i = 0; i < 3; ++i) {
            if (radios[i]) {
                radios[i]->setFocus(i == tui_focusOldHome);
            }
        }
    } else if (currentPageIndex == Step::TIPS) {
        // pageTips: Allow toggling reboot checkbox during installation
        qtui::TCheckBox* rebootCheck = tui_checkTipsReboot
            ? dynamic_cast<qtui::TCheckBox*>(tui_checkTipsReboot->tuiWidget())
            : nullptr;
        
        if (rebootCheck && rebootCheck->handleKey(key)) {
            // handleKey already toggles the checkbox for SPACE/ENTER
            gui.checkExitReboot->setChecked(rebootCheck->isChecked());
        } else if (key == '\n' || key == KEY_ENTER) {
            if (phase == PH_FINISHED) {
                // Sync checkbox state to GUI widget
                if (rebootCheck) {
                    gui.checkExitReboot->setChecked(rebootCheck->isChecked());
                }
                // If reboot is checked, reboot directly; otherwise go to END page
                if (gui.checkExitReboot->isChecked()) {
                    runShutdown(u"reboot"_s);
                } else {
                    gotoPage(Step::END);
                }
            }
        }
    }

    // Future: Add input handling for other pages
}

void MInstall::handleMouse(int mouseY, int mouseX, int mouseState) noexcept
{
    if (!ui::Context::isTUI()) {
        return;
    }

    if (!(mouseState & (BUTTON1_CLICKED | BUTTON1_DOUBLE_CLICKED))) {
        return;
    }

    if (currentPageIndex == Step::END) {
        if (tui_checkExitReboot) {
            auto* checkbox = dynamic_cast<qtui::TCheckBox*>(tui_checkExitReboot->tuiWidget());
            if (checkbox && checkbox->handleMouse(mouseY, mouseX)) {
                checkbox->setFocus(true);
            }
        }
    } else if (currentPageIndex == Step::TIPS) {
        // Handle mouse clicks on reboot checkbox
        if (tui_checkTipsReboot) {
            auto* checkbox = dynamic_cast<qtui::TCheckBox*>(tui_checkTipsReboot->tuiWidget());
            if (checkbox && checkbox->handleMouse(mouseY, mouseX)) {
                // handleMouse already toggles the checkbox internally
                gui.checkExitReboot->setChecked(checkbox->isChecked());
            }
        }
    } else if (currentPageIndex == Step::INSTALLATION) {
        qtui::TRadioButton* radios[3] = {nullptr, nullptr, nullptr};
        qtui::TCheckBox* dualDrive = nullptr;
        qtui::TComboBox* comboSystem = nullptr;
        qtui::TComboBox* comboHome = nullptr;
        qtui::TSlider* slider = nullptr;
        qtui::TCheckBox* encryptAuto = nullptr;

        if (tui_radioEntireDrive) {
            radios[0] = dynamic_cast<qtui::TRadioButton*>(tui_radioEntireDrive->tuiWidget());
        }
        if (tui_radioCustomPart) {
            radios[1] = dynamic_cast<qtui::TRadioButton*>(tui_radioCustomPart->tuiWidget());
        }
        if (tui_radioReplace) {
            radios[2] = dynamic_cast<qtui::TRadioButton*>(tui_radioReplace->tuiWidget());
        }
        if (tui_checkDualDrive) {
            dualDrive = dynamic_cast<qtui::TCheckBox*>(tui_checkDualDrive->tuiWidget());
        }
        if (tui_comboDriveSystem) {
            comboSystem = dynamic_cast<qtui::TComboBox*>(tui_comboDriveSystem->tuiWidget());
        }
        if (tui_comboDriveHome) {
            comboHome = dynamic_cast<qtui::TComboBox*>(tui_comboDriveHome->tuiWidget());
        }
        if (tui_sliderPart) {
            slider = dynamic_cast<qtui::TSlider*>(tui_sliderPart->tuiWidget());
        }
        if (tui_checkEncryptAuto) {
            encryptAuto = dynamic_cast<qtui::TCheckBox*>(tui_checkEncryptAuto->tuiWidget());
        }

        for (int i = 0; i < 3; ++i) {
            if (radios[i] && radios[i]->handleMouse(mouseY, mouseX)) {
                tui_focusInstallationField = i;
                if (i == 0) gui.radioEntireDrive->setChecked(true);
                if (i == 1) gui.radioCustomPart->setChecked(true);
                if (i == 2) gui.radioReplace->setChecked(true);
                tui_focusInstallation = i;
                syncInstallationTuiFromGui();
                break;
            }
        }

        if (dualDrive && dualDrive->handleMouse(mouseY, mouseX)) {
            tui_focusInstallationField = 3;
            gui.checkDualDrive->setChecked(dualDrive->isChecked());
            syncInstallationTuiFromGui();
        } else if (comboSystem && comboSystem->handleMouse(mouseY, mouseX)) {
            tui_focusInstallationField = 4;
            gui.comboDriveSystem->setCurrentIndex(comboSystem->currentIndex());
        } else if (comboHome && comboHome->handleMouse(mouseY, mouseX)) {
            tui_focusInstallationField = 5;
            gui.comboDriveHome->setCurrentIndex(comboHome->currentIndex());
        } else if (slider && slider->handleMouse(mouseY, mouseX)) {
            tui_focusInstallationField = 6;
            gui.spinRoot->setValue(slider->value());
            syncInstallationTuiFromGui();
        } else if (encryptAuto && encryptAuto->handleMouse(mouseY, mouseX)) {
            tui_focusInstallationField = 7;
            gui.checkEncryptAuto->setChecked(encryptAuto->isChecked());
        }

        if (radios[0]) radios[0]->setFocus(tui_focusInstallationField == 0);
        if (radios[1]) radios[1]->setFocus(tui_focusInstallationField == 1);
        if (radios[2]) radios[2]->setFocus(tui_focusInstallationField == 2);
        if (dualDrive) dualDrive->setFocus(tui_focusInstallationField == 3);
        if (comboSystem) comboSystem->setFocus(tui_focusInstallationField == 4);
        if (comboHome) comboHome->setFocus(tui_focusInstallationField == 5);
        if (slider) slider->setFocus(tui_focusInstallationField == 6);
        if (encryptAuto) encryptAuto->setFocus(tui_focusInstallationField == 7);
    } else if (currentPageIndex == Step::REPLACE) {
        size_t count = replacer ? replacer->installationCount() : 0;
        if (count > 0 && mouseY >= 6 && mouseY < 6 + static_cast<int>(count)) {
            int idx = mouseY - 6;
            if (idx >= 0 && idx < static_cast<int>(count)) {
                tui_focusReplace = idx;
            }
        }
        if (tui_checkReplacePackages) {
            auto* checkbox = dynamic_cast<qtui::TCheckBox*>(tui_checkReplacePackages->tuiWidget());
            if (checkbox && checkbox->handleMouse(mouseY, mouseX)) {
                gui.checkReplacePackages->setChecked(checkbox->isChecked());
            }
        }
    } else if (currentPageIndex == Step::PARTITIONS) {
        const int startRow = 4;
        const int maxVisibleRows = 12;
        const int colSize = 18;
        const int colUseFor = 30;
        const int colLabel = 44;
        const int colFormat = 60;
        const int colEncrypt = 72;

        std::vector<PartMan::Device*> devices;
        if (partman) {
            for (PartMan::Iterator it(*partman); PartMan::Device *device = *it; it.next()) {
                if (device->type == PartMan::Device::DRIVE ||
                    device->type == PartMan::Device::PARTITION ||
                    device->type == PartMan::Device::VIRTUAL ||
                    device->type == PartMan::Device::SUBVOLUME) {
                    devices.push_back(device);
                }
            }
        }

        if (tui_partitionEditing && tui_partitionPopupVisible && !tui_partitionEditIsLabel && !tui_partitionEditIsSize) {
            const int popupWidth = 14;
            if (mouseY >= tui_partitionPopupRow && mouseY < tui_partitionPopupRow + tui_partitionPopupHeight
                && mouseX >= tui_partitionPopupCol && mouseX < tui_partitionPopupCol + popupWidth) {
                int optIdx = tui_partitionPopupScroll + (mouseY - tui_partitionPopupRow);
                if (optIdx >= 0) {
                    PartMan::Device *selectedDevice = (tui_partitionRow < static_cast<int>(devices.size()))
                        ? devices[tui_partitionRow]
                        : nullptr;
                    if (selectedDevice) {
                        QStringList options = (tui_partitionCol == PART_COL_USEFOR)
                            ? selectedDevice->allowedUsesFor()
                            : selectedDevice->allowedFormats();
                        if (optIdx < options.size()) {
                            tui_partitionEditIndex = optIdx;
                            partman->changeBegin(selectedDevice);
                            if (tui_partitionCol == PART_COL_USEFOR) {
                                selectedDevice->usefor = options[tui_partitionEditIndex];
                            } else {
                                selectedDevice->format = options[tui_partitionEditIndex];
                            }
                            partman->changeEnd();
                            tui_partitionEditing = false;
                            tui_partitionEditIsLabel = false;
                            tui_partitionEditIsSize = false;
                        }
                    }
                }
                return;
            }
        }

        if (tui_buttonPartitionsApply) {
            if (tui_buttonPartitionsApply->handleMouse(mouseY, mouseX)) {
                tui_focusPartitions = 1;
                tui_buttonPartitionsApply->setFocus(true);
                return;
            }
        }

        if (mouseY >= startRow && mouseY < startRow + maxVisibleRows) {
            int idx = tui_partitionScroll + (mouseY - startRow);
            if (idx >= 0 && idx < static_cast<int>(devices.size())) {
                tui_partitionRow = idx;
                tui_partitionEditing = false;
                tui_partitionEditIsLabel = false;
                tui_partitionEditIsSize = false;
                tui_focusPartitions = 0;
                if (mouseX >= colSize && mouseX < colUseFor) {
                    tui_partitionCol = PART_COL_SIZE;
                } else if (mouseX >= colUseFor && mouseX < colLabel) {
                    tui_partitionCol = PART_COL_USEFOR;
                } else if (mouseX >= colLabel && mouseX < colFormat) {
                    tui_partitionCol = PART_COL_LABEL;
                } else if (mouseX >= colFormat && mouseX < colEncrypt) {
                    tui_partitionCol = PART_COL_FORMAT;
                }

                if (mouseState & BUTTON1_DOUBLE_CLICKED) {
                    PartMan::Device *selectedDevice = devices[idx];
                    if (selectedDevice->type != PartMan::Device::DRIVE) {
                        if (tui_partitionCol == PART_COL_SIZE) {
                            if (selectedDevice->type == PartMan::Device::PARTITION && !selectedDevice->flags.oldLayout
                                && tui_partitionSizeEdit) {
                                tui_partitionEditing = true;
                                tui_partitionEditIsLabel = false;
                                tui_partitionEditIsSize = true;
                                tui_partitionSizeOriginal = QString::number(selectedDevice->size / MB);
                                tui_partitionSizeEdit->setText(tui_partitionSizeOriginal);
                            }
                        } else if (tui_partitionCol == PART_COL_LABEL) {
                            bool editableLabel = (selectedDevice->format != "PRESERVE"_L1 && selectedDevice->format != "DELETE"_L1)
                                && (selectedDevice->type == PartMan::Device::SUBVOLUME || !selectedDevice->usefor.isEmpty());
                            if (editableLabel && tui_partitionLabelEdit) {
                                tui_partitionEditing = true;
                                tui_partitionEditIsLabel = true;
                                tui_partitionLabelOriginal = selectedDevice->label;
                                tui_partitionLabelEdit->setText(selectedDevice->label);
                            }
                        } else {
                            tui_partitionEditing = true;
                            tui_partitionEditIsLabel = false;
                            tui_partitionEditIsSize = false;
                            tui_partitionEditIndex = 0;
                            if (tui_partitionCol == PART_COL_USEFOR) {
                                QStringList uses = selectedDevice->allowedUsesFor();
                                tui_partitionEditIndex = uses.indexOf(selectedDevice->usefor);
                                if (tui_partitionEditIndex < 0) tui_partitionEditIndex = 0;
                            } else if (tui_partitionCol == PART_COL_FORMAT) {
                                QStringList formats = selectedDevice->allowedFormats();
                                QString curFmt = selectedDevice->format.isEmpty() ? selectedDevice->curFormat : selectedDevice->format;
                                tui_partitionEditIndex = formats.indexOf(curFmt);
                                if (tui_partitionEditIndex < 0) tui_partitionEditIndex = 0;
                            }
                        }
                    } else {
                        gotoAfterPartitionsTui();
                    }
                }
            }
        }
    } else if (currentPageIndex == Step::ENCRYPTION) {
        qtui::TLineEdit* passFields[2] = {nullptr, nullptr};

        if (tui_textCryptoPass) {
            passFields[0] = dynamic_cast<qtui::TLineEdit*>(tui_textCryptoPass->tuiWidget());
        }
        if (tui_textCryptoPass2) {
            passFields[1] = dynamic_cast<qtui::TLineEdit*>(tui_textCryptoPass2->tuiWidget());
        }

        for (int i = 0; i < 2; ++i) {
            if (passFields[i] && passFields[i]->handleMouse(mouseY, mouseX)) {
                tui_focusEncryption = i;
                break;
            }
        }

        for (int i = 0; i < 2; ++i) {
            if (passFields[i]) {
                passFields[i]->setFocus(i == tui_focusEncryption);
            }
        }
    } else if (currentPageIndex == Step::SWAP) {
        qtui::TCheckBox* swapCheck = tui_checkSwapFile
            ? dynamic_cast<qtui::TCheckBox*>(tui_checkSwapFile->tuiWidget())
            : nullptr;
        qtui::TLineEdit* swapFileEdit = tui_textSwapFile
            ? dynamic_cast<qtui::TLineEdit*>(tui_textSwapFile->tuiWidget())
            : nullptr;
        qtui::TLineEdit* swapSizeEdit = tui_textSwapSize
            ? dynamic_cast<qtui::TLineEdit*>(tui_textSwapSize->tuiWidget())
            : nullptr;
        qtui::TCheckBox* hibernationCheck = tui_checkHibernation
            ? dynamic_cast<qtui::TCheckBox*>(tui_checkHibernation->tuiWidget())
            : nullptr;
        qtui::TCheckBox* zramCheck = tui_checkZram
            ? dynamic_cast<qtui::TCheckBox*>(tui_checkZram->tuiWidget())
            : nullptr;
        qtui::TRadioButton* zramPercentRadio = tui_radioZramPercent
            ? dynamic_cast<qtui::TRadioButton*>(tui_radioZramPercent->tuiWidget())
            : nullptr;
        qtui::TLineEdit* zramPercentEdit = tui_textZramPercent
            ? dynamic_cast<qtui::TLineEdit*>(tui_textZramPercent->tuiWidget())
            : nullptr;
        qtui::TRadioButton* zramSizeRadio = tui_radioZramSize
            ? dynamic_cast<qtui::TRadioButton*>(tui_radioZramSize->tuiWidget())
            : nullptr;
        qtui::TLineEdit* zramSizeEdit = tui_textZramSize
            ? dynamic_cast<qtui::TLineEdit*>(tui_textZramSize->tuiWidget())
            : nullptr;

        auto applyFocus = [&]() {
            if (swapCheck) swapCheck->setFocus(tui_focusSwap == 0);
            if (swapFileEdit) swapFileEdit->setFocus(tui_focusSwap == 1);
            if (swapSizeEdit) swapSizeEdit->setFocus(tui_focusSwap == 2);
            if (hibernationCheck) hibernationCheck->setFocus(tui_focusSwap == 3);
            if (zramCheck) zramCheck->setFocus(tui_focusSwap == 4);
            if (zramPercentRadio) zramPercentRadio->setFocus(tui_focusSwap == 5);
            if (zramPercentEdit) zramPercentEdit->setFocus(tui_focusSwap == 6);
            if (zramSizeRadio) zramSizeRadio->setFocus(tui_focusSwap == 7);
            if (zramSizeEdit) zramSizeEdit->setFocus(tui_focusSwap == 8);
        };

        if (swapCheck && swapCheck->handleMouse(mouseY, mouseX)) {
            tui_focusSwap = 0;
            gui.boxSwapFile->setChecked(swapCheck->isChecked());
            if (swapman) {
                swapman->updateBounds();
            }
            syncSwapTuiFromGui();
        } else if (swapFileEdit && swapFileEdit->handleMouse(mouseY, mouseX)) {
            tui_focusSwap = 1;
        } else if (swapSizeEdit && swapSizeEdit->handleMouse(mouseY, mouseX)) {
            tui_focusSwap = 2;
        } else if (hibernationCheck && hibernationCheck->handleMouse(mouseY, mouseX)) {
            tui_focusSwap = 3;
            gui.checkHibernation->setChecked(hibernationCheck->isChecked());
            syncSwapTuiFromGui();
        } else if (zramCheck && zramCheck->handleMouse(mouseY, mouseX)) {
            tui_focusSwap = 4;
            gui.boxSwapZram->setChecked(zramCheck->isChecked());
            syncSwapTuiFromGui();
        } else if (zramPercentRadio && zramPercentRadio->handleMouse(mouseY, mouseX)) {
            tui_focusSwap = 5;
            gui.radioZramPercent->setChecked(true);
            syncSwapTuiFromGui();
        } else if (zramPercentEdit && zramPercentEdit->handleMouse(mouseY, mouseX)) {
            tui_focusSwap = 6;
        } else if (zramSizeRadio && zramSizeRadio->handleMouse(mouseY, mouseX)) {
            tui_focusSwap = 7;
            gui.radioZramSize->setChecked(true);
            syncSwapTuiFromGui();
        } else if (zramSizeEdit && zramSizeEdit->handleMouse(mouseY, mouseX)) {
            tui_focusSwap = 8;
        }

        applyFocus();
    } else if (currentPageIndex == Step::NETWORK) {
        qtui::TLineEdit* fields[3] = {nullptr, nullptr, nullptr};
        qtui::TCheckBox* samba = nullptr;

        if (tui_textComputerName) {
            fields[0] = dynamic_cast<qtui::TLineEdit*>(tui_textComputerName->tuiWidget());
        }
        if (tui_textComputerDomain) {
            fields[1] = dynamic_cast<qtui::TLineEdit*>(tui_textComputerDomain->tuiWidget());
        }
        if (tui_textComputerGroup) {
            fields[2] = dynamic_cast<qtui::TLineEdit*>(tui_textComputerGroup->tuiWidget());
        }
        if (tui_checkSamba) {
            samba = dynamic_cast<qtui::TCheckBox*>(tui_checkSamba->tuiWidget());
        }

        for (int i = 0; i < 3; ++i) {
            if (fields[i] && fields[i]->handleMouse(mouseY, mouseX)) {
                tui_focusNetwork = i;
                tui_networkError.clear();
                break;
            }
        }
        if (samba && samba->handleMouse(mouseY, mouseX)) {
            tui_focusNetwork = 3;
            gui.checkSamba->setChecked(samba->isChecked());
            tui_networkError.clear();
        }

        for (int i = 0; i < 3; ++i) {
            if (fields[i]) {
                fields[i]->setFocus(i == tui_focusNetwork);
            }
        }
        if (samba) {
            samba->setFocus(tui_focusNetwork == 3);
        }
    } else if (currentPageIndex == Step::LOCALIZATION) {
        qtui::TComboBox* combos[3] = {nullptr, nullptr, nullptr};
        qtui::TCheckBox* localClock = nullptr;
        qtui::TRadioButton* radios[2] = {nullptr, nullptr};

        if (tui_comboLocale) {
            combos[0] = dynamic_cast<qtui::TComboBox*>(tui_comboLocale->tuiWidget());
        }
        if (tui_comboTimeArea) {
            combos[1] = dynamic_cast<qtui::TComboBox*>(tui_comboTimeArea->tuiWidget());
        }
        if (tui_comboTimeZone) {
            combos[2] = dynamic_cast<qtui::TComboBox*>(tui_comboTimeZone->tuiWidget());
        }
        if (tui_checkLocalClock) {
            localClock = dynamic_cast<qtui::TCheckBox*>(tui_checkLocalClock->tuiWidget());
        }
        if (tui_radioClock24) {
            radios[0] = dynamic_cast<qtui::TRadioButton*>(tui_radioClock24->tuiWidget());
        }
        if (tui_radioClock12) {
            radios[1] = dynamic_cast<qtui::TRadioButton*>(tui_radioClock12->tuiWidget());
        }

        for (int i = 0; i < 3; ++i) {
            if (combos[i] && combos[i]->handleMouse(mouseY, mouseX)) {
                tui_focusLocalization = i;
                if (i == 0) gui.comboLocale->setCurrentIndex(combos[0]->currentIndex());
                if (i == 1) gui.comboTimeArea->setCurrentIndex(combos[1]->currentIndex());
                if (i == 2) gui.comboTimeZone->setCurrentIndex(combos[2]->currentIndex());
                if (i == 1 && tui_comboTimeZone) {
                    if (tui_comboTimeZone->count() != gui.comboTimeZone->count()) {
                        tui_comboTimeZone->clear();
                        for (int j = 0; j < gui.comboTimeZone->count(); ++j) {
                            tui_comboTimeZone->addItem(gui.comboTimeZone->itemText(j), gui.comboTimeZone->itemData(j));
                        }
                    }
                    tui_comboTimeZone->setCurrentIndex(gui.comboTimeZone->currentIndex());
                }
                break;
            }
        }
        if (localClock && localClock->handleMouse(mouseY, mouseX)) {
            tui_focusLocalization = 3;
            gui.checkLocalClock->setChecked(localClock->isChecked());
        }
        for (int i = 0; i < 2; ++i) {
            if (radios[i] && radios[i]->handleMouse(mouseY, mouseX)) {
                tui_focusLocalization = 4 + i;
                if (i == 0) gui.radioClock24->setChecked(true);
                if (i == 1) gui.radioClock12->setChecked(true);
                break;
            }
        }

        for (int i = 0; i < 3; ++i) {
            if (combos[i]) {
                combos[i]->setFocus(i == tui_focusLocalization);
            }
        }
        if (localClock) localClock->setFocus(tui_focusLocalization == 3);
        if (radios[0]) radios[0]->setFocus(tui_focusLocalization == 4);
        if (radios[1]) radios[1]->setFocus(tui_focusLocalization == 5);
    } else if (currentPageIndex == Step::SERVICES) {
        buildServicesTui();
        syncServicesTuiFromGui();
        for (int i = 0; i < tui_serviceItems.size(); ++i) {
            auto &entry = tui_serviceItems[i];
            if (entry.checkbox && entry.checkbox->tuiWidget()) {
                auto* checkbox = dynamic_cast<qtui::TCheckBox*>(entry.checkbox->tuiWidget());
                if (checkbox && checkbox->handleMouse(mouseY, mouseX)) {
                    tui_focusServices = i;
                    entry.item->setCheckState(0, checkbox->isChecked() ? Qt::Checked : Qt::Unchecked);
                    break;
                }
            }
        }
        for (int i = 0; i < tui_serviceItems.size(); ++i) {
            auto &entry = tui_serviceItems[i];
            if (entry.checkbox) {
                auto* checkbox = dynamic_cast<qtui::TCheckBox*>(entry.checkbox->tuiWidget());
                if (checkbox) {
                    checkbox->setFocus(i == tui_focusServices);
                }
            }
        }
    } else if (currentPageIndex == Step::BOOT) {
        qtui::TCheckBox* bootInstall = tui_checkBootInstall
            ? dynamic_cast<qtui::TCheckBox*>(tui_checkBootInstall->tuiWidget())
            : nullptr;
        qtui::TRadioButton* radios[3] = {nullptr, nullptr, nullptr};
        qtui::TComboBox* comboBoot = tui_comboBoot
            ? dynamic_cast<qtui::TComboBox*>(tui_comboBoot->tuiWidget())
            : nullptr;
        qtui::TCheckBox* hostSpecific = tui_checkBootHostSpecific
            ? dynamic_cast<qtui::TCheckBox*>(tui_checkBootHostSpecific->tuiWidget())
            : nullptr;

        if (tui_radioBootMBR) {
            radios[0] = dynamic_cast<qtui::TRadioButton*>(tui_radioBootMBR->tuiWidget());
        }
        if (tui_radioBootPBR) {
            radios[1] = dynamic_cast<qtui::TRadioButton*>(tui_radioBootPBR->tuiWidget());
        }
        if (tui_radioBootESP) {
            radios[2] = dynamic_cast<qtui::TRadioButton*>(tui_radioBootESP->tuiWidget());
        }

        if (bootInstall && bootInstall->handleMouse(mouseY, mouseX)) {
            tui_focusBootField = 0;
            gui.boxBoot->setChecked(bootInstall->isChecked());
        } else if (comboBoot && comboBoot->handleMouse(mouseY, mouseX)) {
            tui_focusBootField = 4;
            gui.comboBoot->setCurrentIndex(comboBoot->currentIndex());
        } else if (hostSpecific && hostSpecific->handleMouse(mouseY, mouseX)) {
            tui_focusBootField = 5;
            gui.checkBootHostSpecific->setChecked(hostSpecific->isChecked());
        } else {
            for (int i = 0; i < 3; ++i) {
                if (radios[i] && radios[i]->handleMouse(mouseY, mouseX)) {
                    tui_focusBootField = i + 1;
                    if (i == 0) gui.radioBootMBR->setChecked(true);
                    if (i == 1) gui.radioBootPBR->setChecked(true);
                    if (i == 2) gui.radioBootESP->setChecked(true);
                    break;
                }
            }
        }

        if (bootInstall) bootInstall->setFocus(tui_focusBootField == 0);
        if (radios[0]) radios[0]->setFocus(tui_focusBootField == 1);
        if (radios[1]) radios[1]->setFocus(tui_focusBootField == 2);
        if (radios[2]) radios[2]->setFocus(tui_focusBootField == 3);
        if (comboBoot) comboBoot->setFocus(tui_focusBootField == 4);
        if (hostSpecific) hostSpecific->setFocus(tui_focusBootField == 5);
    } else if (currentPageIndex == Step::USER_ACCOUNTS) {
        qtui::TLineEdit* edits[5] = {nullptr, nullptr, nullptr, nullptr, nullptr};
        qtui::TCheckBox* rootCheck = nullptr;
        qtui::TCheckBox* autoLogin = nullptr;
        qtui::TCheckBox* saveDesktop = nullptr;

        if (tui_textUserName) {
            edits[0] = dynamic_cast<qtui::TLineEdit*>(tui_textUserName->tuiWidget());
        }
        if (tui_textUserPass) {
            edits[1] = dynamic_cast<qtui::TLineEdit*>(tui_textUserPass->tuiWidget());
        }
        if (tui_textUserPass2) {
            edits[2] = dynamic_cast<qtui::TLineEdit*>(tui_textUserPass2->tuiWidget());
        }
        if (tui_textRootPass) {
            edits[3] = dynamic_cast<qtui::TLineEdit*>(tui_textRootPass->tuiWidget());
        }
        if (tui_textRootPass2) {
            edits[4] = dynamic_cast<qtui::TLineEdit*>(tui_textRootPass2->tuiWidget());
        }
        if (tui_checkRootAccount) {
            rootCheck = dynamic_cast<qtui::TCheckBox*>(tui_checkRootAccount->tuiWidget());
        }
        if (tui_checkAutoLogin) {
            autoLogin = dynamic_cast<qtui::TCheckBox*>(tui_checkAutoLogin->tuiWidget());
        }
        if (tui_checkSaveDesktop) {
            saveDesktop = dynamic_cast<qtui::TCheckBox*>(tui_checkSaveDesktop->tuiWidget());
        }

        for (int i = 0; i < 5; ++i) {
            if (edits[i] && edits[i]->handleMouse(mouseY, mouseX)) {
                tui_focusUserAccounts = (i < 3) ? i : (i == 3 ? 4 : 5);
                tui_userError.clear();
                if (i < 3) {
                    tui_confirmEmptyUserPass = false;
                } else {
                    tui_confirmEmptyRootPass = false;
                }
                break;
            }
        }
        if (rootCheck && rootCheck->handleMouse(mouseY, mouseX)) {
            tui_focusUserAccounts = 3;
            gui.boxRootAccount->setChecked(rootCheck->isChecked());
            tui_confirmEmptyRootPass = false;
            tui_userError.clear();
        } else if (autoLogin && autoLogin->handleMouse(mouseY, mouseX)) {
            tui_focusUserAccounts = 6;
            gui.checkAutoLogin->setChecked(autoLogin->isChecked());
        } else if (saveDesktop && saveDesktop->handleMouse(mouseY, mouseX)) {
            tui_focusUserAccounts = 7;
            gui.checkSaveDesktop->setChecked(saveDesktop->isChecked());
        }

        for (int i = 0; i < 3; ++i) {
            if (edits[i]) {
                edits[i]->setFocus(i == tui_focusUserAccounts);
            }
        }
        if (rootCheck) rootCheck->setFocus(tui_focusUserAccounts == 3);
        if (edits[3]) edits[3]->setFocus(tui_focusUserAccounts == 4);
        if (edits[4]) edits[4]->setFocus(tui_focusUserAccounts == 5);
        if (autoLogin) autoLogin->setFocus(tui_focusUserAccounts == 6);
        if (saveDesktop) saveDesktop->setFocus(tui_focusUserAccounts == 7);
    } else if (currentPageIndex == Step::OLD_HOME) {
        qtui::TRadioButton* radios[3] = {nullptr, nullptr, nullptr};

        if (tui_radioOldHomeUse) {
            radios[0] = dynamic_cast<qtui::TRadioButton*>(tui_radioOldHomeUse->tuiWidget());
        }
        if (tui_radioOldHomeSave) {
            radios[1] = dynamic_cast<qtui::TRadioButton*>(tui_radioOldHomeSave->tuiWidget());
        }
        if (tui_radioOldHomeDelete) {
            radios[2] = dynamic_cast<qtui::TRadioButton*>(tui_radioOldHomeDelete->tuiWidget());
        }

        for (int i = 0; i < 3; ++i) {
            if (radios[i] && radios[i]->handleMouse(mouseY, mouseX)) {
                tui_focusOldHome = i;
                if (i == 0) gui.radioOldHomeUse->setChecked(true);
                if (i == 1) gui.radioOldHomeSave->setChecked(true);
                if (i == 2) gui.radioOldHomeDelete->setChecked(true);
                tui_oldHomeError.clear();
                break;
            }
        }

        for (int i = 0; i < 3; ++i) {
            if (radios[i]) {
                radios[i]->setFocus(i == tui_focusOldHome);
            }
        }
    }
}
