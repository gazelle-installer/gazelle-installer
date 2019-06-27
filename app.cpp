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
#include <QDesktopWidget>
#include <QDateTime>
#include <QFont>
#include <QString>
#include <QLocale>
#include <QLoggingCategory>
#include <QTranslator>
#include <QMessageBox>
#include <QFile>
#include <QScopedPointer>
#include <QDebug>

#include "minstall.h"
#include "version.h"

QScopedPointer<QFile> logFile;

void messageHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg);
void printHelp();

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    if (a.arguments().contains("--help") || a.arguments().contains("-h") ) {
        printHelp();
        return EXIT_SUCCESS;
    }

    if (a.arguments().contains("--version") || a.arguments().contains("-v") ) {
       qDebug() << "Installer version:" << VERSION;
       return EXIT_SUCCESS;
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
    qtTran.load(QString("qt_") + QLocale::system().name());
    a.installTranslator(&qtTran);

    QTranslator appTran;
    appTran.load(QString("gazelle-installer_") + QLocale::system().name(), "/usr/share/gazelle-installer/locale");
    a.installTranslator(&appTran);

    //exit if "minstall" is already running
    if (system("ps -C minstall | sed '0,/minstall/{s/minstall//}' | grep minstall") == 0) {
        QMessageBox::critical(0, QString::null,
                              QApplication::tr("The installer won't launch because it appears to be running already in the background.\n\n"
                                               "Please close it if possible, or run 'pkill minstall' in terminal."));
        return EXIT_FAILURE;
    }

    // check if 32bit on 64 bit UEFI
    if (system("uname -m | grep -q i686") == 0 && system("grep -q 64 /sys/firmware/efi/fw_platform_size") == 0)
    {
        int ans = QMessageBox::question(0, QString::null, QApplication::tr("You are running 32bit OS started in 64 bit UEFI mode, the system will not be able to boot"
                                                                           " unless you select Legacy Boot or similar at restart.\n"
                                                                           "We recommend you quit now and restart in Legacy Boot\n\n"
                                                                           "Do you want to continue the installation?"),
                                    QMessageBox::Yes, QMessageBox::No);
        if (ans != QMessageBox::Yes) {
            return EXIT_FAILURE;
        }
    }

    // alert the user if not running as root
    if (getuid() != 0) {
        QApplication::beep();
        const QString &msg = QApplication::tr("You must run this app as root.");
        if (a.arguments().contains("--pretend") || a.arguments().contains("-p")) {
            QMessageBox::warning(0, QString::null, msg);
        } else {
            QMessageBox::critical(0, QString::null, msg);
            return EXIT_FAILURE;
        }
    }

    // config file
    QString cfgfile("/.minstall");
    int cfgindex = a.arguments().lastIndexOf("--config");
    if (cfgindex >= 0 && cfgindex < (a.arguments().count() - 1)) {
        const QString clicfgfile(a.arguments().at(cfgindex + 1));
        if (QFile::exists(clicfgfile)) cfgfile = clicfgfile;
        else {
            QMessageBox::warning(0, QString::null,
                QApplication::tr("Configuration file (%1) not found.").arg(clicfgfile));
        }
    }

    // main routine
    qDebug() << "Installer version:" << VERSION;
    MInstall minstall(a.arguments(), cfgfile);
    const QRect &geo = a.desktop()->availableGeometry(&minstall);
    minstall.move((geo.width()-minstall.width())/2, (geo.height()-minstall.height())/2);
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
    default:            out << "OTH"; break;
    }
    // Write to the output category of the message and the message itself
    out << context.category << ": "
        << msg << endl;
    out.flush();    // Clear the buffered data
}

// print CLI help info
void printHelp()
{
    printf("Here are some CLI options you can use. Please read the description carefully and be aware that these are experimental options.\n"
           "\n"
           "Usage: minstall [<options>]\n"
           "       minstall [<options>] --config <config-file>\n"
           "\n"
           "Options:\n"
           "  --config       Load a configuration file as specified by <config-file>.\n"
           "                 By default (without this option) /.minstall is used.\n"
           "  --auto         Installs automatically using the configuration file (more information below).\n"
           "                 -- WARNING: potentially dangerous option, it will wipe the partition(s) automatically.\n"
           "  --brave        Overrules sanity checks on partitions and drives, causing them to be displayed.\n"
           "                 -- WARNING: this can break things, use it only if you don't care about data on drive.\n"
           "  -n --nocopy    Another testing mode for installer, partitions/drives are going to be FORMATED, it will skip copying the files.\n"
           "  -p --pretend   Test mode for GUI, you can advance to different screens without actially installing.\n"
           "  -s --sync      Installing with rsync instead of cp on custom partitioning.\n"
           "                 -- doesn't format /root and it doesn't work with encryption.\n"
           "  -v --version   Show version information.\n"
           "\n"
           "Configuration File:\n"
           "  If /.minstall is present, the installer will load whatever configuration is stored inside.\n"
           "  The use of -c or --config makes the installer load <config-file> instead.\n"
           "  This configuration can be used with --auto for an unattended installation.\n"
           "  The installer creates (or overwrites) /.minstalled and saves a copy to /mnt/antiX/.minstall for future use.\n"
           "  The installer will not write any passwords or ignored settings to the new configuration file.\n"
           "  Please note, this is experimental. Future installer versions may break compatibility with existing configuration files.\n");
}
