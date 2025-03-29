/***************************************************************************
 * Core class - Common module for various important utilities.
 *
 *   Copyright (C) 2025 by AK-47
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
 *
 * This file is part of the gazelle-installer.
 ***************************************************************************/
#include <sys/stat.h>
#include <QListWidget>
#include <QFile>
#include <QDir>
#include <QTimer>
#include <QEventLoop>
#include <QDebug>

#include "mprocess.h"
#include "core.h"

Core::Core(MProcess &mproc) : proc(mproc)
{
}

// Common functions that are traditionally carried out by processes.

void Core::sleep(const int msec, const bool silent) noexcept
{
    QListWidgetItem *logEntry = nullptr;
    if (!silent) {
        ++sleepcount;
        logEntry = proc.log(QString("SLEEP: %1ms").arg(msec), MProcess::LOG_EXEC, false);
        qDebug().nospace() << "Sleep #" << sleepcount << ": " << msec << "ms";
    }
    QTimer cstimer(&proc);
    QEventLoop eloop(&proc);
    QObject::connect(&cstimer, &QTimer::timeout, &eloop, &QEventLoop::quit);
    cstimer.start(msec);
    const int rc = eloop.exec();
    if (!silent) {
        qDebug().nospace() << "Sleep #" << sleepcount << ": exit " << rc;
        proc.log(logEntry, rc == 0 ? MProcess::STATUS_OK : MProcess::STATUS_CRITICAL);
    }
}
bool Core::mkpath(const QString &path, mode_t mode, bool force)
{
    if (proc.checkHalt()) return false;
    if (!force && QFileInfo::exists(path)) return true;

    QListWidgetItem *logEntry = proc.log("MKPATH: "+path, MProcess::LOG_EXEC, false);
    bool rc = QDir().mkpath(path);
    if (mode && rc) rc = (chmod(path.toUtf8().constData(), mode) == 0);
    qDebug() << (rc ? "MkPath(SUCCESS):" : "MkPath(FAILURE):") << path;
    proc.log(logEntry, rc ? MProcess::STATUS_OK : MProcess::STATUS_CRITICAL);

    if(!rc) {
        MProcess::Section sect(proc);
        const char *const fmsg = sect.failMessage();
        if (fmsg) throw fmsg;
    }
    return rc;
}

// System environment detection

const QString &Core::detectArch()
{
    if (testArch.isEmpty()) {
        if (proc.exec("uname", {"-m"}, nullptr, true)) testArch = proc.readOut();
        qDebug() << "Detect arch:" << testArch;
    }
    return testArch;
}
int Core::detectEFI(bool noTest)
{
    if (testEFI < 0) {
        testEFI = 0;
        QFile file("/sys/firmware/efi/fw_platform_size");
        if (file.open(QFile::ReadOnly | QFile::Text)) {
            testEFI = file.readLine().toInt();
            file.close();
        }
        qDebug() << "Detect EFI:" << testEFI;
    }
    if (!noTest && testEFI == 64) {
        // Bad combination: 32-bit OS on 64-bit EFI.
        if (detectArch()=="i686") return 0;
    }
    return testEFI;
}
bool Core::detectVirtualBox()
{
    if (testVirtualBox < 0) {
        MProcess::Section sect(proc);
        sect.setExceptionMode(false);
        const bool rc = proc.shell("lspci -n | grep -qE '80ee:beef|80ee:cafe'");
        qDebug() << "Detect VirtualBox:" << rc;
        testVirtualBox = rc ? 1 : 0;
    }
    return (testVirtualBox != 0);
}
