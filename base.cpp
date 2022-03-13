/***************************************************************************
 * Base operating system software installation and configuration.
 *
 *   Copyright (C) 2022 by AK-47, along with transplanted code:
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
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <QDebug>
#include <QFileInfo>
#include "base.h"

Base::Base(MProcess &mproc, PartMan &pman, Ui::MeInstall &ui,
    const QSettings &appConf, const QCommandLineParser &appArgs)
    : QObject(ui.boxMain), proc(mproc), gui(ui), partman(pman)
{
    populateMediaMounts = appConf.value("POPULATE_MEDIA_MOUNTPOINTS").toBool();
    nocopy = appArgs.isSet("nocopy");
    sync = appArgs.isSet("sync");

    bootSource = "/live/aufs/boot";
    rootSources << "/live/aufs/bin" << "/live/aufs/dev"
        << "/live/aufs/etc" << "/live/aufs/lib" << "/live/aufs/libx32" << "/live/aufs/lib64"
        << "/live/aufs/media" << "/live/aufs/mnt" << "/live/aufs/opt" << "/live/aufs/root"
        << "/live/aufs/sbin" << "/live/aufs/usr" << "/live/aufs/var" << "/live/aufs/home";

    // calculate required disk space
    proc.exec("du", {"-sb", bootSource}, nullptr, true);
    partman.bootSpaceNeeded = proc.readOut().section('\t', 0, 0).toLongLong();
    const long long spaceBlock = 134217728; // 128MB
    partman.bootSpaceNeeded += 2*spaceBlock - (partman.bootSpaceNeeded % spaceBlock);

    QSettings livesettings("/live/config/initrd.out", QSettings::NativeFormat);
    QString SQFILE_FULL = livesettings.value("SQFILE_FULL", "/live/boot-dev/antiX/linuxfs").toString();
    if (!QFile::exists(SQFILE_FULL + ".info")) {
        SQFILE_FULL = livesettings.value("TORAM_MP", "/live/to-ram").toString() + "/antiX/linuxfs";
    }
    qDebug() << "linuxfs file is at : " << SQFILE_FULL;
    const QSettings squashinfo(SQFILE_FULL + ".info");
    bool floatOK = false;
    partman.rootSpaceNeeded = 1024 * squashinfo.value("UncompressedSizeKB").toFloat(&floatOK);
    if (!floatOK) {
        //conservative but fast. Factors are same as used in live-remaster. Using "du -sb ..." here is so slow, people thought the installer crashed.
        //get compression factor by reading the linuxfs squasfs file, if available
        long long compression_factor;
        int linuxfs_compression_type = 0; //default conservative
        if (QFileInfo::exists(SQFILE_FULL)) {
            proc.shell("dd if=" + SQFILE_FULL + " bs=1 skip=20 count=2 status=none"
                " 2>/dev/null | od -An -tdI", nullptr, true);
            linuxfs_compression_type = proc.readOut().toInt();
        }
        // gzip, xz, or lz4
        switch (linuxfs_compression_type) {
            case 1: // gzip
                compression_factor = 30;
                break;
            case 2: // lzo, not used by antiX
            case 3: // lzma, not used by antiX
            case 5: // lz4
                compression_factor = 42;
                break;
            case 4: // xz
            default: // anythng else or linuxfs not reachable (toram), should be pretty conservative
                compression_factor = 25;
        }
        qDebug() << "linuxfs compression type is" << linuxfs_compression_type << "compression factor is" << compression_factor;
        proc.shell("df /live/linux --output=used --total |tail -n1", nullptr, true);
        partman.rootSpaceNeeded = (proc.readOut().toLongLong() * 1024 * 100) / compression_factor;
        partman.rootSpaceNeeded += 1024 * 1024 * 1024; // 1GB safety factor
    }
    qDebug() << "linuxfs file size is " << partman.rootSpaceNeeded;
    // Account for persistent root.
    if (QFileInfo::exists("/live/perist-root")) {
        proc.shell("df /live/persist-root --output=used --total |tail -n1", nullptr, true);
        const long long rootfs_size = proc.readOut().toLongLong() * 1024;
        qDebug() << "rootfs size is " << rootfs_size;
        // probaby conservative, as rootfs will likely have some overlap with linuxfs.
        partman.rootSpaceNeeded += rootfs_size;
    }
    // Account for file tails.
    struct statvfs svfs;
    if (statvfs("/live/linux", &svfs) == 0) {
        partman.rootSpaceNeeded += (svfs.f_files - svfs.f_ffree) * svfs.f_bsize;
    }

    qDebug() << "Minimum space:" << partman.bootSpaceNeeded << "(boot)," << partman.rootSpaceNeeded << "(root)";

}

bool Base::install()
{
    proc.log(__PRETTY_FUNCTION__);
    if (proc.halted()) return false;
    proc.advance(1, 2);

    if (!partman.willFormat("/")) {
        // if root was not formatted and not using --sync option then re-use it
        // remove all folders in root except for /home
        proc.status(tr("Deleting old system"));
        proc.shell("find /mnt/antiX -mindepth 1 -maxdepth 1 ! -name home -exec rm -r {} \\;");

        if (proc.exitStatus() != QProcess::NormalExit) {
            failure = tr("Failed to delete old system on destination.");
            return false;
        }
    }

    // make empty dirs for opt, dev, proc, sys, run,
    // home already done
    proc.status(tr("Creating system directories"));
    mkdir("/mnt/antiX/opt", 0755);
    mkdir("/mnt/antiX/dev", 0755);
    mkdir("/mnt/antiX/proc", 0755);
    mkdir("/mnt/antiX/sys", 0755);
    mkdir("/mnt/antiX/run", 0755);

    if (!copyLinux()) return false;

    proc.advance(1, 1);
    proc.status(tr("Fixing configuration"));
    mkdir("/mnt/antiX/tmp", 01777);
    chmod("/mnt/antiX/tmp", 01777);

    // Copy live set up to install and clean up.
    proc.shell("/usr/sbin/live-to-installed /mnt/antiX");
    qDebug() << "Desktop menu";
    proc.exec("chroot", {"/mnt/antiX", "desktop-menu", "--write-out-global"});

    // if POPULATE_MEDIA_MOUNTPOINTS is true in gazelle-installer-data, then use the --mntpnt switch
    partman.makeFstab(populateMediaMounts);
    if (!partman.fixCryptoSetup()) {
        failure = tr("Failed to finalize encryption setup.");
        return false;
    }
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
        proc.exec("/bin/rm", {"-rf", "/mnt/antiX/home/demo"});

    // if POPULATE_MEDIA_MOUNTPOINTS is true in gazelle-installer-data, don't clean /media folder
    // modification to preserve points that are still mounted.
    if (!populateMediaMounts) {
        proc.shell("/bin/rmdir --ignore-fail-on-non-empty /mnt/antiX/media/sd*");
    }

    // guess localtime vs UTC
    proc.shell("guess-hwclock", nullptr, true);
    if (proc.readOut() == "localtime") gui.checkLocalClock->setChecked(true);

    // create a /etc/machine-id file and /var/lib/dbus/machine-id file
    proc.exec("/bin/mount", {"--rbind", "--make-rslave", "/dev", "/mnt/antiX/dev"});
    proc.setChRoot("/mnt/antiX");
    proc.exec("rm", {"/var/lib/dbus/machine-id", "/etc/machine-id"});
    proc.exec("dbus-uuidgen", {"--ensure=/etc/machine-id"});
    proc.exec("dbus-uuidgen", {"--ensure"});
    proc.setChRoot();
    proc.exec("/bin/umount", {"-R", "/mnt/antiX/dev"});

    // Disable VirtualBox Guest Additions if not running in VirtualBox.
    if(!proc.shell("lspci -n | grep -qE '80ee:beef|80ee:cafe'")) {
        proc.shell("/bin/mv -f /mnt/antiX/etc/rc5.d/S*virtualbox-guest-utils /mnt/antiX/etc/rc5.d/K01virtualbox-guest-utils >/dev/null 2>&1");
        proc.shell("/bin/mv -f /mnt/antiX/etc/rc4.d/S*virtualbox-guest-utils /mnt/antiX/etc/rc4.d/K01virtualbox-guest-utils >/dev/null 2>&1");
        proc.shell("/bin/mv -f /mnt/antiX/etc/rc3.d/S*virtualbox-guest-utils /mnt/antiX/etc/rc3.d/K01virtualbox-guest-utils >/dev/null 2>&1");
        proc.shell("/bin/mv -f /mnt/antiX/etc/rc2.d/S*virtualbox-guest-utils /mnt/antiX/etc/rc2.d/K01virtualbox-guest-utils >/dev/null 2>&1");
        proc.shell("/bin/mv -f /mnt/antiX/etc/rcS.d/S*virtualbox-guest-x11 /mnt/antiX/etc/rcS.d/K21virtualbox-guest-x11 >/dev/null 2>&1");
    }

    return true;
}

bool Base::copyLinux()
{
    proc.log(__PRETTY_FUNCTION__);
    if (proc.halted()) return false;

    // copy most except usr, mnt and home
    // must copy boot even if saving, the new files are required
    // media is already ok, usr will be done next, home will be done later
    // setup and start the process
    QString prog = "/bin/cp";
    QStringList args("-av");
    if (sync) {
        prog = "rsync";
        args << "--delete";
        if (!partman.willFormat("/")) args << "--filter" << "protect home/*";
    }
    args << bootSource << rootSources << "/mnt/antiX";
    struct statvfs svfs;
    fsfilcnt_t sourceInodes = 1;
    if (statvfs("/live/linux", &svfs) == 0) {
        sourceInodes = svfs.f_files - svfs.f_ffree;
    }
    proc.advance(80, sourceInodes);
    proc.status(tr("Copying new system"));
    // Placed here so the progress bar moves to the right position before the next step.
    if (proc.halted()) return false;
    if (nocopy) return true; // Skip copying on purpose

    const QString &joined = MProcess::joinCommand(prog, args);
    qDebug().noquote() << "Exec COPY:" << joined;
    QListWidgetItem *logEntry = proc.log(joined, MProcess::Exec);

    QEventLoop eloop;
    connect(&proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), &eloop, &QEventLoop::quit);
    connect(&proc, &QProcess::readyRead, &eloop, &QEventLoop::quit);
    proc.start(prog, args);
    long ncopy = 0;
    while (proc.state() != QProcess::NotRunning) {
        eloop.exec();
        ncopy += proc.readAllStandardOutput().count('\n');
        proc.status(ncopy);
    }
    disconnect(&proc, &QProcess::readyRead, nullptr, nullptr);
    disconnect(&proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), nullptr, nullptr);

    const QByteArray &StdErr = proc.readAllStandardError();
    if (!StdErr.isEmpty()) {
        qDebug() << "SErr COPY:" << StdErr;
        QFont logFont = logEntry->font();
        logFont.setItalic(true);
        logEntry->setFont(logFont);
    }
    qDebug() << "Exit COPY:" << proc.exitCode() << proc.exitStatus();
    if (proc.exitStatus() != QProcess::NormalExit) {
        proc.log(logEntry, -1);
        failure = tr("Failed to copy the new system.");
        return false;
    }
    proc.log(logEntry, proc.exitCode() ? 0 : 1);

    return !proc.halted(); // Reduces domino effect if the copy is aborted.
}
