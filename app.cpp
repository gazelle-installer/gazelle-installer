/****************************************************************************
 *   Copyright (C) 2003-2009 by Warren Woodford
 *   Heavily edited, with permision, by anticapitalista for antiX 2011-2014.
 *   Heavily revised by dolphin oracle, adrian, and anticaptialista 2018.
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
 ***************************************************************************/

#include <cstdlib>
#include <cstdio>
#include <clocale>
#include <unistd.h>
#include <atomic>
#include <thread>
#include <chrono>

#include <QApplication>
#include <QCommandLineParser>
#include <QDateTime>
#include <QtDebug>
#include <QFile>
#include <QLibraryInfo>
#include <QLocale>
#include <QLoggingCategory>
#include <QMessageBox>
#include <QScreen>
#include <QString>
#include <QStringList>
#include <QTranslator>
#include <QLockFile>

#include "minstall.h"
#include "msettings.h"
#include "src/ui/context.h"
#include "src/ui/qmessagebox.h"

// ncurses for TUI mode
#include <ncurses.h>

// VERSION should come from compiler flags.
#ifndef VERSION
    #define VERSION "?.?.?.?"
#endif

using namespace Qt::Literals::StringLiterals;

static void messageHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg);
static QtMessageHandler origHandler = nullptr;
static bool suppressConsoleLogs = false;

int main(int argc, char *argv[])
{
    // Check for --tui flag BEFORE creating QApplication to avoid X11/Wayland dependency
    bool isTuiMode = false;
    for (int i = 1; i < argc; ++i) {
        if (QString::fromUtf8(argv[i]) == u"--tui"_s || QString::fromUtf8(argv[i]) == u"-tui"_s) {
            isTuiMode = true;
            break;
        }
    }

    // Auto-detect TUI mode if no display server is available
    if (!isTuiMode && !getenv("DISPLAY") && !getenv("WAYLAND_DISPLAY")) {
        isTuiMode = true;
    }

    // If TUI mode, set Qt to use offscreen platform (no X11/Wayland needed)
    if (isTuiMode) {
        qputenv("QT_QPA_PLATFORM", "offscreen");
    }

    const bool defskin = (!getenv("QT_QPA_PLATFORMTHEME") && !getenv("XDG_CURRENT_DESKTOP"));
    if (defskin && !isTuiMode) {
        // The default style in the OOBE environment is hideous and unusable.
        QApplication::setStyle(u"cde"_s); // Qt docs say do this before the QApplication instance.
        QPalette pal;
        pal.setColor(QPalette::Window, Qt::black);
        pal.setColor(QPalette::WindowText, Qt::white);
        pal.setColor(QPalette::Base, Qt::black);
        pal.setColor(QPalette::AlternateBase, Qt::black);
        pal.setColor(QPalette::Text, Qt::white);
        pal.setColor(QPalette::Button, Qt::black);
        pal.setColor(QPalette::ButtonText, Qt::white);
        pal.setColor(QPalette::BrightText, Qt::white);
        pal.setColor(QPalette::Disabled, QPalette::Text, Qt::darkGray);
        pal.setColor(QPalette::Disabled, QPalette::WindowText, Qt::darkGray);
        pal.setColor(QPalette::Disabled, QPalette::ButtonText, Qt::darkGray);
        pal.setColor(QPalette::Highlight, Qt::lightGray);
        pal.setColor(QPalette::HighlightedText, Qt::black);
        pal.setColor(QPalette::ToolTipBase, Qt::black);
        pal.setColor(QPalette::ToolTipText, Qt::white);
        pal.setColor(QPalette::Link, Qt::cyan);
        QApplication::setPalette(pal); // Qt docs say do this after setting the style.
    }

    QApplication a(argc, argv);
    if (defskin && !isTuiMode) a.setStyleSheet(u"QDialog { border: 2px ridge gray; }"_s);
    //a.setWindowIcon(QIcon(u"/usr/share/gazelle-installer-data/logo.png"_s));

    const QString &transpath = QLibraryInfo::path(QLibraryInfo::TranslationsPath);
    QTranslator qtTran;
    if (qtTran.load(QLocale::system(), u"qt"_s, u"_"_s, transpath)) {
        a.installTranslator(&qtTran);
    }
    QTranslator qtBaseTran;
    if (qtBaseTran.load(QLocale::system(), u"qtbase"_s, u"_"_s, transpath)) {
        a.installTranslator(&qtBaseTran);
    }
    QTranslator appTran;
    if (appTran.load(QLocale::system(), u"gazelle-installer"_s, u"_"_s, u"/usr/share/gazelle-installer/locale"_s)) {
        a.installTranslator(&appTran);
    }

    a.setApplicationDisplayName(QObject::tr("Gazelle Installer"));
    a.setApplicationVersion(QStringLiteral(VERSION));
    QCommandLineParser parser;
    parser.setApplicationDescription(QObject::tr("Customizable GUI installer for MX Linux and antiX Linux"));
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addOptions({{"advanced", QObject::tr("Enable advanced settings, even in regular installation mode.")},
        {"auto", QObject::tr("Installs automatically using the configuration file (more information below).\n"
            "-- WARNING: potentially dangerous option, it will wipe the partition(s) automatically.")},
        {"brave", QObject::tr("Overrules sanity checks on partitions and drives, causing them to be displayed.\n"
            "-- WARNING: this can break things, use it only if you don't care about data on drive.")},
        {{"c" , "config"}, QObject::tr("Load a configuration file as specified by <config-file>.\n"
            "By default /etc/minstall.conf is used.\n"
            "This configuration can be used with --auto for an unattended installation.\n"
            "The installer creates (or overwrites) /mnt/antiX/etc/minstall.conf and saves a copy to /etc/minstalled.conf for future use.\n"
            "The installer will not write any passwords or ignored settings to the new configuration file.\n"
            "Please note, this is experimental. Future installer versions may break compatibility with existing configuration files.")},
        {{"f", "poweroff"}, QObject::tr("Shutdown automatically when done installing.")},
        {{"m", "mount-keep"}, QObject::tr("Do not unmount /mnt/antiX or close any of the associated LUKS containers when finished.")},
        {{"o", "oem"}, QObject::tr("Install the operating system, delaying prompts for user-specific options until the first reboot.\n"
            "Upon rebooting, the installer will be run with --oobe so that the user can provide these details.\n"
            "This is useful for OEM installations, selling or giving away a computer with an OS pre-loaded on it.")},
        {"oobe", QObject::tr("Out Of the Box Experience option.\n"
            "This will start automatically if installed with --oem option.")},
        {"pretend", QObject::tr("Test mode for GUI, you can advance to different screens without actially installing.")},
        {{"r", "reboot"}, QObject::tr("Reboots automatically when done installing.")},
        {{"s", "sync"}, QObject::tr("Installing with rsync instead of cp on custom partitioning.\n"
            "-- doesn't format /root and it doesn't work with encryption.")},
        {"media-check", QObject::tr("Always check the installation media at the beginning.")},
        {"no-media-check", QObject::tr("Do not check the installation media at the beginning.\n"
            "Not recommended unless the installation media is guaranteed to be free from errors.")},
        {"tui", QObject::tr("Force TUI (Text User Interface) mode instead of GUI.")},
        {"test-page", QObject::tr("Jump directly to a specific page for testing (use with --pretend).\n"
            "Page names: splash, terms, installation, partitions, confirm, boot, services, user, end"), QObject::tr("<page-name>")}});
    parser.addPositionalArgument(u"config-file"_s, QObject::tr("Load a configuration file as specified by <config-file>."), u"<config-file>"_s);
    parser.process(a);
    
    // Force TUI mode if --tui flag is present or no display server detected
    if (isTuiMode || parser.isSet(u"tui"_s)) {
        ui::Context::forceTUI();
        suppressConsoleLogs = true;  // Suppress console output for clean TUI
    }

    if (parser.positionalArguments().size() > 1) {
        qDebug() << QObject::tr("Too many arguments. Please check the command format by running the program with --help");
        return EXIT_FAILURE;
    }

    QString confPath = u"/usr/share/gazelle-installer-data/installer.conf"_s;
    if (QFile::exists(u"/etc/gazelle-installer-data/installer.conf"_s)) {
        confPath = u"/etc/gazelle-installer-data/installer.conf"_s;
    }
    MIni appConf(confPath, MIni::ReadOnly);
    a.setWindowIcon(QIcon(appConf.getString("LOGO-IMAGE", "/usr/share/gazelle-installer-data/logo.png")));
    a.setApplicationDisplayName(QObject::tr("%1 Installer").arg(appConf.getString(u"Name"_s)));

    // The lock is released when this object is destroyed.
    QLockFile lockfile(u"/var/lock/gazelle-installer.lock"_s);
    if (!parser.isSet(u"pretend"_s)) {
        // Set Lock or exit if lockfile is present.
        if (!lockfile.tryLock()) {
            if (ui::Context::isGUI()) {
                QMessageBox msgbox;
                msgbox.setIcon(QMessageBox::Critical);
                msgbox.setText(QObject::tr("The installer appears to be running already."));
                msgbox.setInformativeText(QObject::tr("Please close it if possible, or run 'pkill minstall' in terminal."));
                msgbox.exec();
            } else {
                // TUI mode - use wrapper
                ui::QMessageBox::critical(nullptr, 
                    QObject::tr("Error"),
                    QObject::tr("The installer appears to be running already.\nPlease close it if possible, or run 'pkill minstall' in terminal."));
            }
            return EXIT_FAILURE;
        }
        // Alert the user if not running as root.
        if (getuid() != 0) {
            if (ui::Context::isGUI()) {
                QMessageBox msgbox;
                msgbox.setIcon(QMessageBox::Critical);
                msgbox.setText(QObject::tr("This operation requires root access."));
                msgbox.exec();
            } else {
                // TUI mode - use wrapper
                ui::QMessageBox::critical(nullptr,
                    QObject::tr("Error"),
                    QObject::tr("This operation requires root access."));
            }
            return EXIT_FAILURE;
        }
    }

    origHandler = qInstallMessageHandler(messageHandler);

    QString cfgfile;
    if (parser.isSet(u"config"_s) || parser.positionalArguments().size() == 1) {
        if (parser.positionalArguments().size() == 1) { // use config file if passed as argument
            cfgfile = parser.positionalArguments().at(0);
        } else if (parser.isSet(u"config"_s)) { // use default config file if no argument
            cfgfile = u"/etc/minstall.conf"_s;
        }
        // give error message and exit if no config file found
        if (! QFile::exists(cfgfile)) {
            if (ui::Context::isGUI()) {
                QMessageBox msgbox;
                msgbox.setIcon(QMessageBox::Warning);
                msgbox.setText(QObject::tr("Configuration file (%1) not found.").arg(cfgfile));
                msgbox.exec();
            } else {
                ui::QMessageBox::critical(nullptr,
                    QObject::tr("Error"),
                    QObject::tr("Configuration file (%1) not found.").arg(cfgfile));
            }
            return EXIT_FAILURE;
        }
    }

    // main routine
    qInfo() << "Installer version:" << VERSION;

    // For TUI mode, initialize ncurses first and show loading spinner
    std::atomic<bool> startupComplete{false};
    if (ui::Context::isTUI()) {
        // Set locale for proper UTF-8 support in ncurses
        std::setlocale(LC_ALL, "");
        initscr();
        start_color();
        cbreak();
        noecho();
        keypad(stdscr, TRUE);
        curs_set(0);

        // Show loading message with spinner
        const char* spinChars = "|/-\\";
        int spinIdx = 0;
        auto spinnerThread = std::thread([&]() {
            while (!startupComplete.load()) {
                clear();
                mvprintw(0, 0, "Gazelle Installer (TUI Mode)");
                mvprintw(1, 0, "============================");
                mvprintw(10, 30, "Loading... %c", spinChars[spinIdx % 4]);
                mvprintw(12, 25, "Scanning disk drives...");
                refresh();
                spinIdx++;
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
        });

        MInstall minstall(appConf, parser, cfgfile);
        startupComplete.store(true);
        spinnerThread.join();

        // Handle --test-page option
        if (parser.isSet(u"test-page"_s) && parser.isSet(u"pretend"_s)) {
            QString pageName = parser.value(u"test-page"_s).toLower();
            int targetPage = -1;
            if (pageName == u"splash"_s) targetPage = 0;
            else if (pageName == u"terms"_s) targetPage = 1;
            else if (pageName == u"installation"_s) targetPage = 2;
            else if (pageName == u"replace"_s) targetPage = 3;
            else if (pageName == u"partitions"_s) targetPage = 4;
            else if (pageName == u"encryption"_s) targetPage = 5;
            else if (pageName == u"confirm"_s) targetPage = 6;
            else if (pageName == u"boot"_s) targetPage = 7;
            else if (pageName == u"swap"_s) targetPage = 8;
            else if (pageName == u"services"_s) targetPage = 9;
            else if (pageName == u"network"_s) targetPage = 10;
            else if (pageName == u"localization"_s) targetPage = 11;
            else if (pageName == u"user"_s) targetPage = 12;
            else if (pageName == u"oldhome"_s) targetPage = 13;
            else if (pageName == u"tips"_s) targetPage = 14;
            else if (pageName == u"end"_s) targetPage = 15;
            if (targetPage >= 0) {
                minstall.gotoPage(targetPage);
            }
        }

        // TUI event loop
        mousemask(ALL_MOUSE_EVENTS | REPORT_MOUSE_POSITION, nullptr);
        init_pair(1, COLOR_WHITE, COLOR_BLUE);
        init_pair(2, COLOR_BLACK, COLOR_WHITE);
        init_pair(3, COLOR_YELLOW, COLOR_BLUE);

        bool running = true;
        int exitCode = EXIT_SUCCESS;

        while (running) {
            clear();
            // Show header with Alt+Left status
            if (minstall.canGoBack()) {
                mvprintw(0, 0, "Gazelle Installer (TUI Mode) - Press Ctrl-C to quit (Alt+Left = Back)");
            } else {
                attron(COLOR_PAIR(2));
                mvprintw(0, 0, "Gazelle Installer (TUI Mode) - Press Ctrl-C to quit");
                attroff(COLOR_PAIR(2));
                attron(A_DIM);
                printw(" (Alt+Left unavailable)");
                attroff(A_DIM);
            }
            mvprintw(1, 0, "========================================================");
            minstall.renderCurrentPage();
            int maxY, maxX;
            getmaxyx(stdscr, maxY, maxX);
            (void)maxX;
            
            // Show different footer based on page
            if (minstall.getCurrentPage() == 14) {  // Step::TIPS
                mvprintw(maxY - 1, 0, "Installation in progress - please wait");
            } else {
                // Render Previous button (Ctrl-P)
                move(maxY - 1, 0);
                if (minstall.canGoBack()) {
                    printw("[ Previous (Ctrl-P) ]");
                } else {
                    attron(A_DIM);
                    printw("[ Previous (Ctrl-P) ]");
                    attroff(A_DIM);
                }

                printw(" ");

                // Render Next button (Ctrl-N)
                if (minstall.canGoNext()) {
                    printw("[ Next (Ctrl-N) ]");
                } else {
                    attron(A_DIM);
                    printw("[ Next (Ctrl-N) ]");
                    attroff(A_DIM);
                }

                printw(" | SPACE: Toggle | Ctrl-C: Quit");
            }
            refresh();

            if (minstall.processDeferredActions()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(150));
                continue;
            }

            timeout(-1);
            int ch = getch();

            if (ch == 27) {
                timeout(0);
                int ch1 = getch();
                if (ch1 == ERR) {
                    // Standalone Esc - let the page handle it (close popups, exit fields, etc.)
                    minstall.handleInput(27);
                } else if (ch1 == '[') {
                    int ch2 = getch();
                    int ch3 = getch();
                    int ch4 = getch();
                    int ch5 = getch();
                    if (ch2 == '1' && ch3 == ';' && ch4 == '3' && ch5 == 'D') {
                        minstall.handleInput(MInstall::TUI_KEY_ALT_LEFT);
                    }
                }
                // Ignore unrecognized escape sequences
                timeout(-1);
            } else if (ch == KEY_MOUSE) {
                MEVENT event;
                if (getmouse(&event) == OK) {
                    // Check if click was on Previous or Next buttons in footer
                    int maxY, maxX;
                    getmaxyx(stdscr, maxY, maxX);
                    (void)maxX;

                    if (event.y == maxY - 1 && (event.bstate & (BUTTON1_CLICKED | BUTTON1_DOUBLE_CLICKED))) {
                        // Previous button is at columns 0-20: "[ Previous (Ctrl-P) ]"
                        if (event.x >= 0 && event.x <= 20) {
                            if (minstall.canGoBack()) {
                                minstall.handleInput(MInstall::TUI_KEY_ALT_LEFT);
                            }
                        }
                        // Next button is at columns 22-38: "[ Next (Ctrl-N) ]"
                        else if (event.x >= 22 && event.x <= 38) {
                            if (minstall.canGoNext()) {
                                // Simulate Enter key to advance
                                minstall.handleInput('\n');
                            }
                        }
                        // If not on buttons, pass to page-specific mouse handler
                        else {
                            minstall.handleMouse(event.y, event.x, event.bstate);
                        }
                    } else {
                        // Not in footer, pass to page-specific mouse handler
                        minstall.handleMouse(event.y, event.x, event.bstate);
                    }
                }
            } else if (ch == 16) {
                // Ctrl-P = Previous
                if (minstall.canGoBack()) {
                    minstall.handleInput(MInstall::TUI_KEY_ALT_LEFT);
                }
            } else if (ch == 14) {
                // Ctrl-N = Next
                if (minstall.canGoNext()) {
                    minstall.handleInput('\n');
                }
            } else {
                minstall.handleInput(ch);
            }

            if (minstall.shouldExit()) {
                running = false;
            }
        }

        endwin();
        return exitCode;
    }

    MInstall minstall(appConf, parser, cfgfile);

    // Handle --test-page option for testing
    if (parser.isSet(u"test-page"_s) && parser.isSet(u"pretend"_s)) {
        QString pageName = parser.value(u"test-page"_s).toLower();
        int targetPage = -1;
        
        if (pageName == u"splash"_s) targetPage = 0;  // Step::SPLASH
        else if (pageName == u"terms"_s) targetPage = 1;  // Step::TERMS
        else if (pageName == u"installation"_s) targetPage = 2;  // Step::INSTALLATION
        else if (pageName == u"replace"_s) targetPage = 3;  // Step::REPLACE
        else if (pageName == u"partitions"_s) targetPage = 4;  // Step::PARTITIONS
        else if (pageName == u"encryption"_s) targetPage = 5;  // Step::ENCRYPTION
        else if (pageName == u"confirm"_s) targetPage = 6;  // Step::CONFIRM
        else if (pageName == u"boot"_s) targetPage = 7;  // Step::BOOT
        else if (pageName == u"swap"_s) targetPage = 8;  // Step::SWAP
        else if (pageName == u"services"_s) targetPage = 9;  // Step::SERVICES
        else if (pageName == u"network"_s) targetPage = 10;  // Step::NETWORK
        else if (pageName == u"localization"_s) targetPage = 11;  // Step::LOCALIZATION
        else if (pageName == u"user"_s) targetPage = 12;  // Step::USER_ACCOUNTS
        else if (pageName == u"oldhome"_s) targetPage = 13;  // Step::OLD_HOME
        else if (pageName == u"tips"_s) targetPage = 14;  // Step::TIPS
        else if (pageName == u"end"_s) targetPage = 15;  // Step::END
        
        if (targetPage >= 0) {
            qInfo() << "Jumping to test page:" << pageName << "(index" << targetPage << ")";
            minstall.gotoPage(targetPage);
        }
    }
    
    if (ui::Context::isGUI()) {
        // GUI mode - standard Qt event loop
        const QRect &geo = a.primaryScreen()->availableGeometry();
        int width = 800;
        int height = 600;
        if (geo.width() > 1200) {
            width = geo.width()/1.5;
            if (width > 1280) width = 1280; //  1920 / 1.5
        }
        if (geo.height() > 900){
            height = geo.height()/1.5;
            if (height > 720) height = 720; // 1080 / 1.5
        }
        minstall.setGeometry(0,0,width,height);
        minstall.move((geo.width() - minstall.width()) / 2, (geo.height() - minstall.height()) / 2);
        minstall.show();
        return a.exec();
    }

    // Should not reach here - TUI returns early, GUI returns from a.exec()
    return EXIT_FAILURE;
}

// Qt log message handler
void messageHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
    static constexpr char logfile[] = "/var/log/minstall.log";
    static FILE *log = fopen(logfile, "a");
    static bool nolog = false;
    if (log) {
        // QtDebugMsg = 0, QtWarningMsg = 1, QtCriticalMsg = 2, QtFatalMsg = 3, QtInfoMsg = 4
        static const char *chtype[] = { "DBG", "WRN", "CRT", "FTL", "INF" };
        fprintf(log, "%s %s %s: %s\n",
            QDateTime::currentDateTime().toString().toUtf8().constData(),
            chtype[type], context.category, msg.toUtf8().constData());
        fflush(log);
    } else if (!nolog) {
        fprintf(stderr, "Cannot write to installer log: %s\n", logfile);
        nolog = true;
    }
    // Call the original handler which should print text to the console.
    if (!suppressConsoleLogs && origHandler) {
        (*origHandler)(type, context, msg);
    }
}
