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
#include <QDebug>
#include <QFile>
#include <QLibraryInfo>
#include <QLocale>
#include <QLoggingCategory>
#include <QMessageBox>
#include <QScreen>
#include <QString>
#include <QStringList>
#include <QSettings>
#include <QTranslator>
#include <QLockFile>

#include "minstall.h"
#include "version.h"

static QFile logFile("/var/log/minstall.log");

void messageHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg);

int main(int argc, char *argv[])
{
    const bool defskin = (!getenv("QT_QPA_PLATFORMTHEME") && !getenv("XDG_CURRENT_DESKTOP"));
    if (defskin) {
        // The default style in the OOBE environment is hideous and unusable.
        QApplication::setStyle("cde"); // Qt docs say do this before the QApplication instance.
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
    if (defskin) a.setStyleSheet("QDialog { border: 2px ridge gray; }");
    a.setApplicationVersion(VERSION);
    a.setWindowIcon(QIcon("/usr/share/gazelle-installer-data/logo.png"));

#if QT_VERSION >= QT_VERSION_CHECK(6,0,0)
    const QString &transpath = QLibraryInfo::path(QLibraryInfo::TranslationsPath);
#else
    const QString &transpath = QLibraryInfo::location(QLibraryInfo::TranslationsPath);
#endif
    QTranslator qtTran;
    if (qtTran.load(QLocale::system(), "qt", "_", transpath)) {
        a.installTranslator(&qtTran);
    }
    QTranslator qtBaseTran;
    if (qtBaseTran.load(QLocale::system(), "qtbase", "_", transpath)) {
        a.installTranslator(&qtBaseTran);
    }
    QTranslator appTran;
    if (appTran.load(QLocale::system(), "gazelle-installer", "_", "/usr/share/gazelle-installer/locale")) {
        a.installTranslator(&appTran);
    }

    QCommandLineParser parser;
    parser.setApplicationDescription(QObject::tr("Customizable GUI installer for MX Linux and antiX Linux"));
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addOptions({{"auto", QObject::tr("Installs automatically using the configuration file (more information below).\n"
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
    parser.addPositionalArgument("config-file", QObject::tr("Load a configuration file as specified by <config-file>."), "<config-file>");
    parser.process(a);

    if (parser.positionalArguments().size() > 1) {
        qDebug() << QObject::tr("Too many arguments. Please check the command format by running the program with --help");
        return EXIT_FAILURE;
    }

    QSettings appConf("/usr/share/gazelle-installer-data/installer.conf", QSettings::NativeFormat);
    a.setApplicationDisplayName(QObject::tr("%1 Installer").arg(appConf.value("PROJECT_NAME").toString()));

    // The lock is released when this object is destroyed.
    QLockFile lockfile("/var/lock/gazelle-installer.lock");
    if (!parser.isSet("pretend")) {
        // Set Lock or exit if lockfile is present.
        if (!lockfile.tryLock()) {
            QMessageBox::critical(nullptr, QString(),
                QObject::tr("The installer won't launch because it appears to be running already in the background.\n\n"
                    "Please close it if possible, or run 'pkill minstall' in terminal."));
            return EXIT_FAILURE;
        }
        // Alert the user if not running as root.
        if (getuid() != 0) {
            QMessageBox::critical(nullptr, QString(),
                QObject::tr("This operation requires root access."));
            return EXIT_FAILURE;
        }
    }

    if (logFile.open(QFile::Append | QFile::Text)) {
        qInstallMessageHandler(messageHandler);
    } else {
        qDebug() << "Cannot write to installer log:" << logFile.fileName();
    }

    QString cfgfile;
    if (parser.isSet("config") || parser.positionalArguments().size() == 1) {
        if (parser.positionalArguments().size() == 1) { // use config file if passed as argument
            cfgfile = parser.positionalArguments().at(0);
        } else if (parser.isSet("config")) { // use default config file if no argument
            cfgfile = "/etc/minstall.conf";
        }
        // give error message and exit if no config file found
        if (! QFile::exists(cfgfile)) {
            QMessageBox::warning(nullptr, QString(),
                QObject::tr("Configuration file (%1) not found.").arg(cfgfile));
            return EXIT_FAILURE;
        }
    }

    // main routine
    qDebug() << "Installer version:" << VERSION;
    MInstall minstall(appConf, parser, cfgfile);
    const QRect &geo = a.primaryScreen()->availableGeometry();
    if (parser.isSet("oobe")) {
        minstall.setGeometry(0,0,geo.width()/1.5,geo.height()/1.5);
    }
    minstall.move((geo.width() - minstall.width()) / 2, (geo.height() - minstall.height()) / 2);
    minstall.show();
    return a.exec();
}

// The implementation of the handler
void messageHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
    // Write to terminal
    QTextStream term_out(stdout);
    term_out << msg << Qt::endl;

    // Open stream file writes
    QTextStream out(&logFile);

    // Write the date of recording
    out << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz ");
    // By type determine to what level belongs message
    switch (type)
    {
    case QtInfoMsg:     out << "INF "; break;
    case QtDebugMsg:    out << "DBG "; break;
    case QtWarningMsg:  out << "WRN "; break;
    case QtCriticalMsg: out << "CRT "; break;
    case QtFatalMsg:    out << "FTL "; break;
    }
    // Write to the output category of the message and the message itself
    out << context.category << ": " << msg << Qt::endl;
}
