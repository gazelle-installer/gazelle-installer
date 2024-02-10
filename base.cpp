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
#include <QMessageBox>
#include "mprocess.h"
#include "msettings.h"
#include "partman.h"
#include "base.h"

#define BASE_BLOCK  (4*KB)

Base::Base(MProcess &mproc, PartMan &pman, const QSettings &appConf, const QCommandLineParser &appArgs)
    : proc(mproc), partman(pman)
{
    pretend = appArgs.isSet("pretend");
    populateMediaMounts = appConf.value("POPULATE_MEDIA_MOUNTPOINTS").toBool();
    sync = appArgs.isSet("sync");
    bufferRoot = appConf.value("ROOT_BUFFER", 1024).toLongLong() * MB;
    bufferHome = appConf.value("HOME_BUFFER", 1024).toLongLong() * MB;

    bootSource = "/live/aufs/boot";
    rootSources << "/live/aufs/bin" << "/live/aufs/dev"
        << "/live/aufs/etc" << "/live/aufs/lib"
        << "/live/aufs/media" << "/live/aufs/mnt" << "/live/aufs/root"
        << "/live/aufs/sbin" << "/live/aufs/usr" << "/live/aufs/var" << "/live/aufs/home";
    if (QFileInfo::exists("/live/aufs/libx32")){
        rootSources << "/live/aufs/libx32" ;
    }
    if (QFileInfo::exists("/live/aufs/lib64")){
        rootSources << "/live/aufs/lib64" ;
    }
    if (QFileInfo::exists("/live/aufs/opt")){
        rootSources << "/live/aufs/opt" ;
    }
    PartMan::VolumeSpec &vspecRoot = partman.volSpecs["/"];
    PartMan::VolumeSpec &vspecBoot = partman.volSpecs["/boot"];
    PartMan::VolumeSpec &vspecHome = partman.volSpecs["/home"];

    MProcess::Section sect(proc, QT_TR_NOOP("Cannot access installation media."));
    if (pretend) sect.setExceptionMode(nullptr);

    // Boot space required.
    proc.exec("du", {"-scb", bootSource}, nullptr, true);
    vspecBoot.image = proc.readOut(true).section('\n', -1).section('\t', 0, 0).toLongLong();
    vspecBoot.minimum = vspecBoot.image * 3; // Include 1 backup and 1 new initrd image.
    vspecBoot.preferred = vspecBoot.minimum;
    if (vspecBoot.preferred < 1*GB) vspecBoot.preferred = 1*GB;

    // Root space required.
    QSettings liveInfo("/live/config/initrd.out", QSettings::IniFormat);
    const QString &sqpath = '/' + liveInfo.value("SQFILE_PATH", "antiX").toString();
    const QString &sqtoram = liveInfo.value("TORAM_MP", "/live/to-ram").toString() + sqpath;
    const QString &sqloc = liveInfo.value("SQFILE_DIR", "/live/boot-dev/antiX").toString();
    bool floatOK = false;
    QString infile = sqtoram + "/linuxfs.info";
    if (!QFile::exists(infile)) infile = sqloc + "/linuxfs.info";
    if (QFile::exists(infile)) {
        const QSettings squashInfo(infile, QSettings::IniFormat);
        vspecRoot.image = squashInfo.value("UncompressedSizeKB").toFloat(&floatOK) * KB;
    }
    if (!floatOK) {
        rootSources.prepend("-scb");
        proc.exec("du", rootSources, nullptr, true);
        rootSources.removeFirst();
        vspecRoot.image = proc.readOut(true).section('\n', -1).section('\t', 0, 0).toLongLong();
    }
    qDebug() << "Basic image:" << vspecRoot.image << floatOK << infile;

    // Account for persistent root.
    if (QFileInfo::exists("/live/perist-root")) {
        proc.shell("df /live/persist-root --output=used --total |tail -n1", nullptr, true);
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
    QString homedir("/");
    const auto fit = partman.mounts.find("/home");
    const PartMan::Device *mount = nullptr;
    if (fit != partman.mounts.cend() && !fit->second->willFormat()) {
        mount = fit->second;
    } else {
        mount = partman.mounts.at("/"); // Must have '/' at this point.
        homedir = "/home";
    }
    assert(mount != nullptr);
    if (mount->willFormat()) return true;
    const QString &homedev = mount->mappedDevice();

    // Just in case the device or mount point is in use elsewhere.
    proc.exec("umount", {"-q", homedev});
    proc.exec("umount", {"-q", "/mnt/antiX"});

    // Store a listing of /home to compare with the user name given later.
    mkdir("/mnt/antiX", 0755);
    QString opts = "ro";
    if (mount->type == PartMan::Device::SUBVOLUME) opts += ",subvol="+mount->curLabel;
    bool ok = proc.exec("mount", {"-o", opts, homedev, "/mnt/antiX"});
    if (ok) {
        QDir hd("/mnt/antiX" + homedir);
        ok = hd.exists() && hd.isReadable();
        homes = hd.entryList(QDir::Dirs);
        proc.exec("umount", {"-l", "/mnt/antiX"});
    }
    return ok;
}

void Base::install()
{
    proc.log(__PRETTY_FUNCTION__, MProcess::LOG_MARKER);
    if (proc.halted()) return;
    proc.advance(1, 1);

    const PartMan::Device *mount = partman.mounts.at("/");
    const bool skiphome = !mount->willFormat();
    if (skiphome) {
        MProcess::Section sect(proc, QT_TR_NOOP("Failed to delete old system on destination."));
        // if root was not formatted and not using --sync option then re-use it
        // remove all folders in root except for /home
        proc.status(tr("Deleting old system"));
        QStringList findargs({"/mnt/antiX", "-mindepth", "1", "-xdev"});
        // Preserve mount points of other file systems.
        for (auto &it : partman.mounts) {
            if (it.first.at(0) == '/' && it.first != "/") {
                findargs << "!" << "-path" << "/mnt/antiX" + it.first;
            }
        }
        // Preserve /home even if not a separate file system.
        if (partman.mounts.count("/home") < 1) {
            findargs << "!" << "-path" << "/mnt/antiX/home/*";
            findargs << "!" << "-path" << "/mnt/antiX/home";
        }
        findargs << "-delete";
        proc.exec("find", findargs);
    }

    copyLinux(skiphome);

    MProcess::Section sect(proc, QT_TR_NOOP("Failed to set the system configuration."));
    proc.advance(1, 1);
    proc.status(tr("Setting system configuration"));
    // make empty dir for opt. home already done.
    proc.mkpath("/mnt/antiX/opt", 0755);
    proc.mkpath("/mnt/antiX/tmp", 01777, true);
    // Fix live setup to install before creating a chroot environment.
    proc.shell("/usr/sbin/live-to-installed /mnt/antiX");
    if (!partman.installTabs()) throw sect.failMessage();
    // Create a chroot environment.
    proc.exec("mount", {"--mkdir", "--rbind", "--make-rslave", "/dev", "/mnt/antiX/dev"});
    proc.exec("mount", {"--mkdir", "--rbind", "--make-rslave", "/sys", "/mnt/antiX/sys"});
    proc.exec("mount", {"--mkdir", "--rbind", "/proc", "/mnt/antiX/proc"});
    proc.exec("mount", {"--mkdir", "-t", "tmpfs", "-o", "size=100m,nodev,mode=755", "tmpfs", "/mnt/antiX/run"});
    proc.exec("mount", {"--mkdir", "--rbind", "/run/udev", "/mnt/antiX/run/udev"});

    sect.setExceptionMode(nullptr);
    proc.status();

    qDebug() << "Desktop menu";
    proc.exec("chroot", {"/mnt/antiX", "desktop-menu", "--write-out-global"});

    // Disable hibernation inside initramfs.
    if (partman.isEncrypt("SWAP")) {
        proc.exec("touch", {"/mnt/antiX/initramfs-tools/conf.d/resume"});
        QFile file("/mnt/antiX/etc/initramfs-tools/conf.d/resume");
        if (file.open(QIODevice::WriteOnly)) {
            QTextStream out(&file);
            out << "RESUME=none";
        }
        file.close();
    }

    //remove home unless a demo home is found in remastered linuxfs
    if (!QFileInfo("/live/linux/home/demo").isDir())
        proc.exec("rm", {"-rf", "/mnt/antiX/home/demo"});

    // create a /etc/machine-id file and /var/lib/dbus/machine-id file
    sect.setRoot("/mnt/antiX");
    proc.exec("rm", {"/var/lib/dbus/machine-id", "/etc/machine-id"});
    proc.exec("dbus-uuidgen", {"--ensure=/etc/machine-id"});
    proc.exec("dbus-uuidgen", {"--ensure"});
    sect.setRoot(nullptr);

    // Disable VirtualBox Guest Additions if not running in VirtualBox.
    if(!proc.shell("lspci -n | grep -qE '80ee:beef|80ee:cafe'")) {
        proc.shell("mv -f /mnt/antiX/etc/rc5.d/S*virtualbox-guest-utils /mnt/antiX/etc/rc5.d/K01virtualbox-guest-utils >/dev/null 2>&1");
        proc.shell("mv -f /mnt/antiX/etc/rc4.d/S*virtualbox-guest-utils /mnt/antiX/etc/rc4.d/K01virtualbox-guest-utils >/dev/null 2>&1");
        proc.shell("mv -f /mnt/antiX/etc/rc3.d/S*virtualbox-guest-utils /mnt/antiX/etc/rc3.d/K01virtualbox-guest-utils >/dev/null 2>&1");
        proc.shell("mv -f /mnt/antiX/etc/rc2.d/S*virtualbox-guest-utils /mnt/antiX/etc/rc2.d/K01virtualbox-guest-utils >/dev/null 2>&1");
        proc.shell("mv -f /mnt/antiX/etc/rcS.d/S*virtualbox-guest-x11 /mnt/antiX/etc/rcS.d/K21virtualbox-guest-x11 >/dev/null 2>&1");
    }

    // if POPULATE_MEDIA_MOUNTPOINTS is true in gazelle-installer-data, then use the --mntpnt switch
    if (populateMediaMounts) {
        proc.shell("make-fstab -O --install=/mnt/antiX --mntpnt=/media");
    } else {
        // Otherwise, clean /media folder - modification to preserve points that are still mounted.
        proc.shell("rmdir --ignore-fail-on-non-empty /mnt/antiX/media/sd*");
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
    QString prog = "cp";
    QStringList args("-av");
    if (sync) {
        prog = "rsync";
        args << "--delete";
        if (skiphome) args << "--filter" << "protect home/*";
    }
    args << bootSource << rootSources << "/mnt/antiX";
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
