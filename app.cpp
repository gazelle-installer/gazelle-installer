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
#include <unistd.h>

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
#include "version.h"

using namespace Qt::Literals::StringLiterals;

static void messageHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg);
static QtMessageHandler origHandler = nullptr;

int main(int argc, char *argv[])
{
    const bool defskin = (!getenv("QT_QPA_PLATFORMTHEME") && !getenv("XDG_CURRENT_DESKTOP"));
    if (defskin) {
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
    if (defskin) a.setStyleSheet(u"QDialog { border: 2px ridge gray; }"_s);
    a.setApplicationVersion(QStringLiteral(VERSION));
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
            "Not recommended unless the installation media is guaranteed to be free from errors.")}});
    parser.addPositionalArgument(u"config-file"_s, QObject::tr("Load a configuration file as specified by <config-file>."), u"<config-file>"_s);
    parser.process(a);

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
        QMessageBox msgbox;
        msgbox.setIcon(QMessageBox::Critical);
        // Set Lock or exit if lockfile is present.
        if (!lockfile.tryLock()) {
            msgbox.setText(QObject::tr("The installer appears to be running already."));
            msgbox.setInformativeText(QObject::tr("Please close it if possible, or run 'pkill minstall' in terminal."));
            msgbox.exec();
            return EXIT_FAILURE;
        }
        // Alert the user if not running as root.
        if (getuid() != 0) {
            msgbox.setText(QObject::tr("This operation requires root access."));
            msgbox.exec();
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
            QMessageBox msgbox;
            msgbox.setIcon(QMessageBox::Warning);
            msgbox.setText(QObject::tr("Configuration file (%1) not found.").arg(cfgfile));
            msgbox.exec();
            return EXIT_FAILURE;
        }
    }

    // main routine
    qInfo() << "Installer version:" << VERSION;
    MInstall minstall(appConf, parser, cfgfile);
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
    if (origHandler) {
        (*origHandler)(type, context, msg);
    }
}
