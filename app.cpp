//
//   Copyright (C) 2003-2009 by Warren Woodford
//   Heavily edited, with permision, by anticapitalista for antiX 2011-2014.
//   Heavily revised by dolphin oracle, adrian, and anticaptialista 2018.
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

#include <cstdio>
#include <unistd.h>

#include <QApplication>
#include <QCommandLineParser>
#include <QDateTime>
#include <QDebug>
#include <QDesktopWidget>
#include <QDir>
#include <QFile>
#include <QFont>
#include <QLibraryInfo>
#include <QLocale>
#include <QLoggingCategory>
#include <QMessageBox>
#include <QScopedPointer>
#include <QString>
#include <QStringList>
#include <QTranslator>

#include "minstall.h"
#include "version.h"

static QScopedPointer<QFile> logFile;

void messageHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg);

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    a.setApplicationVersion(VERSION);

    QCommandLineParser parser;
    parser.setApplicationDescription(QApplication::tr("Customizable GUI installer for MX Linux and antiX Linux"));
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addOptions({{"auto", QApplication::tr("Installs automatically using the configuration file (more information below).\n"
                                                 "-- WARNING: potentially dangerous option, it will wipe the partition(s) automatically.")},
                       {"brave", QApplication::tr("Overrules sanity checks on partitions and drives, causing them to be displayed.\n"
                                                  "-- WARNING: this can break things, use it only if you don't care about data on drive.")},
                       {{"c" , "config"}, QApplication::tr("Load a configuration file as specified by <config-file>.\n"
                                                           "By default /etc/minstall.conf is used.\n"
                                                           "This configuration can be used with --auto for an unattended installation.\n"
                                                           "The installer creates (or overwrites) /mnt/antiX/etc/minstall.conf and saves a copy to /etc/minstalled.conf for future use.\n"
                                                           "The installer will not write any passwords or ignored settings to the new configuration file.\n"
                                                           "Please note, this is experimental. Future installer versions may break compatibility with existing configuration files.")},
                       {"gpt-override", QApplication::tr("Always use GPT when doing a whole-drive installation regardlesss of capacity.\n"
                                                          "Without this option, GPT will only be used on drives with at least 2TB capacity.\n"
                                                          "GPT is always used on whole-drive installations on UEFI systems regardless of capacity, even without this option.")},
                       {{"m", "mount-keep"}, QApplication::tr("Do not unmount /mnt/antiX or close any of the associated LUKS containers when finished.")},
                       {{"n", "nocopy"}, QApplication::tr("Another testing mode for installer, partitions/drives are going to be FORMATED, it will skip copying the files.")},
                       {{"o", "oem"}, QApplication::tr("Install the operating system, delaying prompts for user-specific options until the first reboot.\n"
                                                       "Upon rebooting, the installer will be run with --oobe so that the user can provide these details.\n"
                                                       "This is useful for OEM installations, selling or giving away a computer with an OS pre-loaded on it.")},
                       {"oobe", QApplication::tr("Out Of the Box Experience option.\n"
                                                 "This will start automatically if installed with --oem option.")},
                       {{"p", "pretend"}, QApplication::tr("Test mode for GUI, you can advance to different screens without actially installing.")},
                       {{"s", "sync"}, QApplication::tr("Installing with rsync instead of cp on custom partitioning.\n"
                                                        "-- doesn't format /root and it doesn't work with encryption.")}});
    parser.addPositionalArgument("config-file", QApplication::tr("Load a configuration file as specified by <config-file>."), "<config-file>");
    parser.process(a);

    if (parser.positionalArguments().size() > 1) {
        qDebug() << "Too many arguments, please check the command format by running the program with --help";
        return EXIT_FAILURE;
    }

    a.setWindowIcon(QIcon("/usr/share/gazelle-installer-data/logo.png"));

    // Set the logging files
    const QString logFileName("/var/log/minstall.log");
    logFile.reset(new QFile(logFileName));
    // Open the file logging
    if (logFile.data()->open(QFile::Append | QFile::Text)) {
        // Set handler
        qInstallMessageHandler(messageHandler);
    } else {
        qDebug() << "Cannot write to installer log:" << logFileName;
    }

    QTranslator qtTran;
    if (qtTran.load(QLocale::system(), "qt", "_", QLibraryInfo::location(QLibraryInfo::TranslationsPath)))
        a.installTranslator(&qtTran);

    QTranslator qtBaseTran;
    if (qtBaseTran.load("qtbase_" + QLocale::system().name(), QLibraryInfo::location(QLibraryInfo::TranslationsPath)))
        a.installTranslator(&qtBaseTran);

    QTranslator appTran;
    if (appTran.load(QString("gazelle-installer_") + QLocale::system().name(), "/usr/share/gazelle-installer/locale"))
        a.installTranslator(&appTran);

    // alert the user if not running as root
    if (!parser.isSet("pretend") && getuid()!=0) {
        QApplication::beep();
        QMessageBox::critical(nullptr, QString(),
            QApplication::tr("This operation requires root access."));
        return EXIT_FAILURE;
    }

    QString cfgfile;
    if (parser.isSet("config") || parser.positionalArguments().size() == 1) {
        if (parser.positionalArguments().size() == 1)  // use config file if passed as argument
            cfgfile = parser.positionalArguments().at(0);
        else if (parser.isSet("config")) // use default config file if no argument
            cfgfile = "/etc/minstall.conf";

        // give error message and exit if no config file found
        if (! QFile::exists(cfgfile)) {
            QMessageBox::warning(nullptr, QString(),
                                 QApplication::tr("Configuration file (%1) not found.").arg(cfgfile));
            return EXIT_FAILURE;
        }
    }

    // main routine
    qDebug() << "Installer version:" << VERSION;
    MInstall minstall(parser, cfgfile);
    const QRect &geo = a.desktop()->availableGeometry(&minstall);
    if (parser.isSet("oobe")) minstall.setGeometry(geo);
    else minstall.move((geo.width()-minstall.width())/2, (geo.height()-minstall.height())/2);
    minstall.show();
    return a.exec();
}

// The implementation of the handler
void messageHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
    // Write to terminal
    QTextStream term_out(stdout);
    term_out << msg << endl;

    // Open stream file writes
    QTextStream out(logFile.data());

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
    out << context.category << ": " << msg << endl;
}
