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

using namespace Qt::Literals::StringLiterals;

Core::Core(MProcess &mproc) : proc(mproc)
{
    containsSystemd = QFileInfo(u"/usr/usr/bin/systemctl"_s).isExecutable() // Online
        || QFileInfo(u"/live/aufs/usr/bin/systemctl"_s).isExecutable(); // Offline

    // Online
    if (QFile::exists(u"/etc/service"_s) && QFile::exists(u"/lib/runit/runit-init"_s)) {
        containsRunit = true;
    }
    // Offline
    if (QFile::exists(u"/live/aufs/etc/service"_s) && QFile::exists(u"/live/aufs/sbin/runit"_s)) {
        containsRunit = true;
    }
}

// Common functions that are traditionally carried out by processes.

void Core::sleep(const int msec, const bool silent) noexcept
{
    QListWidgetItem *logEntry = nullptr;
    if (!silent) {
        ++sleepcount;
        logEntry = proc.log(QStringLiteral("SLEEP: %1ms").arg(msec), MProcess::LOG_EXEC, false);
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
bool Core::mkpath(const QString &path, mode_t mode, bool force) const
{
    if (proc.checkHalt()) return false;
    if (!force && QFileInfo::exists(path)) return true;

    QListWidgetItem *logEntry = proc.log("MKPATH: "_L1 + path, MProcess::LOG_EXEC, false);
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
        if (proc.exec(u"uname"_s, {u"-m"_s}, nullptr, true)) testArch = proc.readOut();
        qDebug() << "Detect arch:" << testArch;
    }
    return testArch;
}
int Core::detectEFI(bool noTest)
{
    if (testEFI < 0) {
        testEFI = 0;
        QFile file(u"/sys/firmware/efi/fw_platform_size"_s);
        if (file.open(QFile::ReadOnly | QFile::Text)) {
            testEFI = file.readLine().toInt();
            file.close();
        }
        qDebug() << "Detect EFI:" << testEFI;
    }
    if (!noTest && testEFI == 64) {
        // Bad combination: 32-bit OS on 64-bit EFI.
        if (detectArch() == "i686"_L1) return 0;
    }
    return testEFI;
}
bool Core::detectVirtualBox()
{
    if (testVirtualBox < 0) {
        MProcess::Section sect(proc);
        sect.setExceptionStrict(false);
        const bool rc = proc.shell(u"lspci -n | grep -qE '80ee:beef|80ee:cafe'"_s);
        qDebug() << "Detect VirtualBox:" << rc;
        testVirtualBox = rc ? 1 : 0;
    }
    return (testVirtualBox != 0);
}

// Services
void Core::setService(const QString &service, bool enabled) const
{
    qDebug() << "Set service:" << service << enabled;
    MProcess::Section sect(proc);
    const QString chroot(sect.root());

    proc.exec(u"update-rc.d"_s, {u"-f"_s, service, enabled?u"defaults"_s:u"remove"_s});
    if (containsSystemd) {
        if (enabled) {
            proc.exec(u"systemctl"_s, {u"unmask"_s, service});
            proc.exec(u"systemctl"_s, {u"enable"_s, service});
        } else {
            proc.exec(u"systemctl"_s, {u"disable"_s, service});
            proc.exec(u"systemctl"_s, {u"mask"_s, service});
        }
    }
    if (containsRunit) {
        if (enabled) {
            QFile::remove(chroot + "/etc/sv/"_L1 + service + "/down"_L1);
            if (!QFile::exists(chroot + "/etc/sv/"_L1 + service)) {
                mkpath(chroot + "/etc/sv/"_L1 + service);
                proc.exec(u"ln"_s, {u"-fs"_s, "/etc/sv/"_L1 + service, u"/etc/service/"_s});
            }
        } else {
            if (!QFile::exists(chroot + "/etc/sv/"_L1 + service)) {
                mkpath(chroot + "/etc/sv/"_L1 + service);
                proc.exec(u"unlink"_s, {"/etc/sv/"_L1 + service, u"/etc/service/"_s});
            }
            proc.exec(u"touch"_s, {"/etc/sv/"_L1 + service + u"/down"_s});
        }
    }
}
