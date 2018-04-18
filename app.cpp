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

#include <unistd.h>

#include <QApplication>
#include <QFont>
#include <QString>
#include <QLocale>
#include <QTranslator>
#include <QMessageBox>
#include <QFile>
#include <QDebug>

#include "mmain.h"



int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    a.setWindowIcon(QIcon("/usr/share/icons/msystem.png"));

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
        return 1;
    }

    // check if 32bit on 64 bit UEFI
    if (system("uname -m | grep -q i686") == 0 && system("grep -q 64 /sys/firmware/efi/fw_platform_size") == 0)
    {
        int ans = QMessageBox::question(0, QString::null, QApplication::tr("You are running 32bit OS started in 64 bit UEFI mode, the system will not be able to boot"
                                                                           " unless you select Legacy Boot or similar at restart.\n"
                                                                           "We recommend you quit now and restart in Legacy Boot\n\n"
                                                                           "Do you want to continue the installation?"),
                                    QApplication::tr("Yes"), QApplication::tr("No"));
        if (ans != 0) {
            return 1;
        }
    }

    if (getuid() == 0) {
        MMain mmain(a.arguments());
        mmain.show();
        return a.exec();
    } else {
        QApplication::beep();
        QMessageBox::critical(0, QString::null,
                              QApplication::tr("You must run this app as root."));
        return 1;
    }
}


