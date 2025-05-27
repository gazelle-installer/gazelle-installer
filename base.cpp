/***************************************************************************
 * Base operating system software installation and configuration.
 *
 *   Copyright (C) 2022-2023 by AK-47, along with transplanted code:
 *    - Copyright (C) 2003-2010 by Warren Woodford
 *    - Heavily edited, with permision, by anticapitalista for antiX 2011-2014.
 *    - Heavily revised by dolphin oracle, adrian, and anticaptialista 2018.
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
 ****************************************************************************/
#include <unistd.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include "mprocess.h"
#include "core.h"
#include "msettings.h"
#include "partman.h"
#include "base.h"

using namespace Qt::Literals::StringLiterals;

#define BASE_BLOCK  (4*KB)

Base::Base(MProcess &mproc, Core &mcore, PartMan &pman,
    MIni &appConf, const QCommandLineParser &appArgs)
    : proc(mproc), core(mcore), partman(pman)
{
    pretend = appArgs.isSet(u"pretend"_s);
    appConf.setSection(u"Storage"_s);
    populateMediaMounts = appConf.getBoolean(u"PopulateMediaMountPoints"_s);
    sync = appArgs.isSet(u"sync"_s);
    bufferRoot = appConf.getInteger(u"RootBuffer"_s, 1024) * MB;
    bufferHome = appConf.getInteger(u"HomeBuffer"_s, 1024) * MB;
    appConf.setSection();
    liveToInstalled = appConf.getString(u"LiveToInstalledScript"_s, u"/usr/sbin/live-to-installed"_s);

    bootSource = "/live/aufs/boot"_L1;
    rootSources << u"/live/aufs/bin"_s << u"/live/aufs/dev"_s
        << u"/live/aufs/etc"_s << u"/live/aufs/lib"_s
        << u"/live/aufs/media"_s << u"/live/aufs/mnt"_s << u"/live/aufs/root"_s
        << u"/live/aufs/sbin"_s << u"/live/aufs/usr"_s << u"/live/aufs/var"_s << u"/live/aufs/home"_s;
    if (QFileInfo::exists(u"/live/aufs/libx32"_s)){
        rootSources << u"/live/aufs/libx32"_s ;
    }
    if (QFileInfo::exists(u"/live/aufs/lib64"_s)){
        rootSources << u"/live/aufs/lib64"_s ;
    }
    if (QFileInfo::exists(u"/live/aufs/opt"_s)){
        rootSources << u"/live/aufs/opt"_s ;
    }
    PartMan::VolumeSpec &vspecRoot = partman.volSpecs[u"/"_s];
    PartMan::VolumeSpec &vspecBoot = partman.volSpecs[u"/boot"_s];
    PartMan::VolumeSpec &vspecHome = partman.volSpecs[u"/home"_s];

    MProcess::Section sect(proc, QT_TR_NOOP("Cannot access installation media."));
    if (pretend) sect.setExceptionMode(nullptr);

    // Boot space required.
    proc.exec(u"du"_s, {u"-scb"_s, bootSource}, nullptr, true);
    vspecBoot.image = proc.readOut(true).section('\n', -1).section('\t', 0, 0).toLongLong();
    vspecBoot.minimum = vspecBoot.image * 3; // Include 1 backup and 1 new initrd image.
    vspecBoot.preferred = vspecBoot.minimum;
    if (vspecBoot.preferred < 1*GB) vspecBoot.preferred = 1*GB;

    // Root space required.
    const MIni liveInfo(u"/live/config/initrd.out"_s, MIni::ReadOnly);
    const QString &sqpath = '/' + liveInfo.getString(u"SQFILE_PATH"_s, u"antiX"_s);
    const QString &sqtoram = liveInfo.getString(u"TORAM_MP"_s, u"/live/to-ram"_s) + sqpath;
    const QString &sqloc = liveInfo.getString(u"SQFILE_DIR"_s, u"/live/boot-dev/antiX"_s);
    bool floatOK = false;
    QString infile = sqtoram + "/linuxfs.info"_L1;
    if (!QFile::exists(infile)) infile = sqloc + "/linuxfs.info"_L1;
    if (QFile::exists(infile)) {
        const MIni squashInfo(infile, MIni::ReadOnly);
        vspecRoot.image = squashInfo.getString(u"UncompressedSizeKB"_s).toFloat(&floatOK) * KB;
    }
    if (!floatOK) {
        rootSources.prepend(u"-scb"_s);
        proc.exec(u"du"_s, rootSources, nullptr, true);
        rootSources.removeFirst();
        vspecRoot.image = proc.readOut(true).section('\n', -1).section('\t', 0, 0).toLongLong();
    }
    qDebug() << "Basic image:" << vspecRoot.image << floatOK << infile;

    // Account for persistent root.
    if (QFileInfo::exists(u"/live/perist-root"_s)) {
        proc.shell(u"df /live/persist-root --output=used --total |tail -n1"_s, nullptr, true);
        const long long rootfs_size = proc.readOut().toLongLong() * KB;
        qDebug() << "rootfs size is " << rootfs_size;
        // probaby conservative, as rootfs will likely have some overlap with linuxfs.
        vspecRoot.image += rootfs_size;
    }
    // Account for file system formatting
    struct statvfs svfs;
    if (statvfs("/live/linux", &svfs) == 0) {
        sourceInodes = svfs.f_files - svfs.f_ffree; // Will also be used for progress bar.
    }
    vspecRoot.image += (sourceInodes * BASE_BLOCK);
    vspecRoot.minimum = vspecRoot.image + (vspecRoot.image / 20); // +5% for general FS shenanigans.
    vspecRoot.preferred = vspecRoot.minimum + bufferRoot;
    qDebug() << "Source inodes:" << sourceInodes << "Assumed block size:" << BASE_BLOCK;

    vspecHome.minimum = 64*MB; // TODO: Set minimum home dynamically.
    vspecHome.preferred = vspecHome.minimum + bufferHome;
    qDebug() << "Minimum space:" << vspecBoot.image << "(boot)," << vspecRoot.image << "(root)";
}

bool Base::saveHomeBasic() noexcept
{
    MProcess::Section sect(proc, nullptr);
    proc.log(__PRETTY_FUNCTION__, MProcess::LOG_MARKER);
    QString homedir(u"/"_s);
    const PartMan::Device *mount = partman.findByMount(u"/home"_s);
    if (mount == nullptr) {
        mount = partman.findByMount(u"/"_s); // Must have '/' at this point.
        homedir = "/home"_L1;
    }
    assert(mount != nullptr);
    if (mount->willFormat()) return true;
    const QString &homedev = mount->mappedDevice();

    // Just in case the device or mount point is in use elsewhere.
    proc.exec(u"umount"_s, {u"-q"_s, homedev});
    proc.exec(u"umount"_s, {u"-q"_s, u"/mnt/antiX"_s});

    // Store a listing of /home to compare with the user name given later.
    mkdir("/mnt/antiX", 0755);
    QString opts = u"ro"_s;
    if (mount->type == PartMan::Device::SUBVOLUME) {
        opts += ",subvol="_L1 + mount->curLabel;
    }
    bool ok = proc.exec(u"mount"_s, {u"-o"_s, opts, homedev, u"/mnt/antiX"_s});
    if (ok) {
        QDir hd("/mnt/antiX"_L1 + homedir);
        ok = hd.exists() && hd.isReadable();
        homes = hd.entryList(QDir::Dirs);
        proc.exec(u"umount"_s, {u"-l"_s, u"/mnt/antiX"_s});
    }
    return ok;
}

void Base::install()
{
    proc.log(__PRETTY_FUNCTION__, MProcess::LOG_MARKER);
    if (proc.halted()) return;
    proc.advance(1, 1);

    const PartMan::Device *rootdev = partman.findByMount(u"/"_s);
    assert(rootdev != nullptr);
    const bool skiphome = !rootdev->willFormat();
    if (skiphome) {
        MProcess::Section sect(proc, QT_TR_NOOP("Failed to delete old system on destination."));
        // if root was not formatted and not using --sync option then re-use it
        // remove all folders in root except for /home
        proc.status(tr("Deleting old system"));
        QStringList findargs({u"/mnt/antiX"_s, u"-mindepth"_s, u"1"_s, u"-xdev"_s});
        // Preserve mount points of other file systems.
        bool homeSeparate = false;
        for (PartMan::Iterator it(partman); *it; it.next()) {
            const QString &mount = (*it)->mountPoint();
            if (mount != "/" && mount.startsWith('/')) {
                findargs << u"!"_s << u"-path"_s << "/mnt/antiX"_L1 + mount;
            } else if (mount == "/home"_L1) {
                homeSeparate = true;
            }
        }
        // Preserve /home even if not a separate file system.
        if (!homeSeparate) {
            findargs << u"!"_s << u"-path"_s << u"/mnt/antiX/home/*"_s;
            findargs << u"!"_s << u"-path"_s << u"/mnt/antiX/home"_s;
        }
        findargs << u"-delete"_s;
        sect.setExceptionStrict(false);
        proc.exec(u"find"_s, findargs);
    }

    copyLinux(skiphome);

    MProcess::Section sect(proc, QT_TR_NOOP("Failed to set the system configuration."));
    proc.advance(1, 1);
    proc.status(tr("Setting system configuration"));
    // make empty dir for opt. home already done.
    core.mkpath(u"/mnt/antiX/opt"_s, 0755);
    core.mkpath(u"/mnt/antiX/tmp"_s, 01777, true);
    // Fix live setup to install before creating a chroot environment.
    // allow custom live-to-installed script
    proc.shell(liveToInstalled + " /mnt/antiX"_L1);
    if (!partman.installTabs()) throw sect.failMessage();
    // Create a chroot environment.
    proc.exec(u"mount"_s, {u"--mkdir"_s, u"--rbind"_s, u"--make-rslave"_s, u"/dev"_s, u"/mnt/antiX/dev"_s});
    proc.exec(u"mount"_s, {u"--mkdir"_s, u"--rbind"_s, u"--make-rslave"_s, u"/sys"_s, u"/mnt/antiX/sys"_s});
    proc.exec(u"mount"_s, {u"--mkdir"_s, u"--rbind"_s, u"/proc"_s, u"/mnt/antiX/proc"_s});
    proc.exec(u"mount"_s, {u"--mkdir"_s, u"-t"_s, u"tmpfs"_s, u"-o"_s, u"size=100m,nodev,mode=755"_s, u"tmpfs"_s, u"/mnt/antiX/run"_s});
    proc.exec(u"mount"_s, {u"--mkdir"_s, u"--rbind"_s, u"/run/udev"_s, u"/mnt/antiX/run/udev"_s});

    sect.setExceptionMode(nullptr);
    proc.status();

    qDebug() << "Desktop menu";
    proc.exec(u"chroot"_s, {u"/mnt/antiX"_s, u"desktop-menu"_s, u"--write-out-global"_s});

    // Disable hibernation inside initramfs.
    for (PartMan::Iterator it(partman); PartMan::Device *dev = *it; it.next()) {
        if (dev->mountPoint() == "swap"_L1 && dev->willEncrypt()) {
            proc.exec(u"touch"_s, {u"/mnt/antiX/initramfs-tools/conf.d/resume"_s});
            QFile file(u"/mnt/antiX/etc/initramfs-tools/conf.d/resume"_s);
            if (file.open(QIODevice::WriteOnly)) {
                QTextStream out(&file);
                out << "RESUME=none";
            }
            file.close();
            break;
        }
    }

    //remove home unless a demo home is found in remastered linuxfs
    if (!QFileInfo(u"/live/linux/home/demo"_s).isDir())
        proc.exec(u"rm"_s, {u"-rf"_s, u"/mnt/antiX/home/demo"_s});

    // create a /etc/machine-id file and /var/lib/dbus/machine-id file
    sect.setRoot("/mnt/antiX");
    proc.exec(u"rm"_s, {u"/var/lib/dbus/machine-id"_s, u"/etc/machine-id"_s});
    proc.exec(u"dbus-uuidgen"_s, {u"--ensure=/etc/machine-id"_s});
    proc.exec(u"dbus-uuidgen"_s, {u"--ensure"_s});
    sect.setRoot(nullptr);

    // Disable VirtualBox Guest Additions if not running in VirtualBox.
    if(!core.detectVirtualBox()) {
        proc.shell(u"mv -f /mnt/antiX/etc/rc5.d/S*virtualbox-guest-utils /mnt/antiX/etc/rc5.d/K01virtualbox-guest-utils >/dev/null 2>&1"_s);
        proc.shell(u"mv -f /mnt/antiX/etc/rc4.d/S*virtualbox-guest-utils /mnt/antiX/etc/rc4.d/K01virtualbox-guest-utils >/dev/null 2>&1"_s);
        proc.shell(u"mv -f /mnt/antiX/etc/rc3.d/S*virtualbox-guest-utils /mnt/antiX/etc/rc3.d/K01virtualbox-guest-utils >/dev/null 2>&1"_s);
        proc.shell(u"mv -f /mnt/antiX/etc/rc2.d/S*virtualbox-guest-utils /mnt/antiX/etc/rc2.d/K01virtualbox-guest-utils >/dev/null 2>&1"_s);
        proc.shell(u"mv -f /mnt/antiX/etc/rcS.d/S*virtualbox-guest-x11 /mnt/antiX/etc/rcS.d/K21virtualbox-guest-x11 >/dev/null 2>&1"_s);
    }

    // if PopulateMediaMountPoints is true in gazelle-installer-data, then use the --mntpnt switch
    if (populateMediaMounts) {
        proc.shell(u"make-fstab -O --install=/mnt/antiX --mntpnt=/media"_s);
    } else {
        // Otherwise, clean /media folder - modification to preserve points that are still mounted.
        proc.shell(u"rmdir --ignore-fail-on-non-empty /mnt/antiX/media/sd*"_s);
    }
}

void Base::copyLinux(bool skiphome)
{
    proc.log(__PRETTY_FUNCTION__, MProcess::LOG_MARKER);
    if (proc.halted()) return;

    // copy most except usr, mnt and home
    // must copy boot even if saving, the new files are required
    // media is already ok, usr will be done next, home will be done later
    // setup and start the process
    QString prog = u"cp"_s;
    QStringList args("-av");
    if (sync) {
        prog = "rsync"_L1;
        args << u"--delete"_s;
        if (skiphome) {
            args << u"--filter"_s << u"protect home/*"_s;
        }
    }
    args << bootSource << rootSources << u"/mnt/antiX"_s;
    proc.advance(80, sourceInodes);
    proc.status(tr("Copying new system"));
    // Placed here so the progress bar moves to the right position before the next step.

    const QString &joined = MProcess::joinCommand(prog, args);
    qDebug().noquote() << "Exec COPY:" << joined;
    QListWidgetItem *logEntry = proc.log(joined, MProcess::LOG_EXEC, false);

    QEventLoop eloop;
    QObject::connect(&proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), &eloop, &QEventLoop::quit);
    QObject::connect(&proc, &QProcess::readyRead, &eloop, &QEventLoop::quit);
    proc.start(prog, args);
    long ncopy = 0;
    while (proc.state() != QProcess::NotRunning) {
        eloop.exec();
        ncopy += proc.readAllStandardOutput().count('\n');
        proc.status(ncopy);
    }
    proc.disconnect(&eloop);

    const QByteArray &StdErr = proc.readAllStandardError();
    if (!StdErr.isEmpty()) {
        qDebug() << "SErr COPY:" << StdErr;
        QFont logFont = logEntry->font();
        logFont.setItalic(true);
        logEntry->setFont(logFont);
    }
    qDebug() << "Exit COPY:" << proc.exitCode() << proc.exitStatus();
    if (proc.exitStatus() != QProcess::NormalExit) {
        proc.log(logEntry, MProcess::STATUS_CRITICAL);
        throw QT_TR_NOOP("Failed to copy the new system.");
    }
    proc.log(logEntry, proc.exitCode() ? MProcess::STATUS_ERROR : MProcess::STATUS_OK);
}
