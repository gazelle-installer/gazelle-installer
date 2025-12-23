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
#include <cctype>
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
    liveToInstalled = appConf.getString(u"LiveToInstalledScript"_s, u"/usr/bin/live-to-installed"_s);
    if (!QFileInfo(liveToInstalled).isExecutable() && QFileInfo(u"/usr/sbin/live-to-installed"_s).isExecutable()) {
        liveToInstalled = u"/usr/sbin/live-to-installed"_s;
    }

    archLive = QFileInfo::exists(u"/run/archiso/airootfs"_s);
    if (archLive && QFileInfo(u"/usr/bin/live-to-installed"_s).isExecutable()) {
        liveToInstalled = u"/usr/bin/live-to-installed"_s;
    }
    rootBase = archLive ? u"/"_s : u"/live/aufs"_s;
    homeSource = rootBase + u"/home"_s;

    bootSource = rootBase + u"/boot"_s;
    const auto addIfExists = [this](const QString &path) {
        if (QFileInfo::exists(path)) rootSources << path;
    };
    addIfExists(rootBase + u"/bin"_s);
    if (!archLive) {
        addIfExists(rootBase + u"/dev"_s);
    }
    addIfExists(rootBase + u"/etc"_s);
    addIfExists(rootBase + u"/lib"_s);
    if (!archLive) {
        addIfExists(rootBase + u"/media"_s);
        addIfExists(rootBase + u"/mnt"_s);
    }
    addIfExists(rootBase + u"/root"_s);
    addIfExists(rootBase + u"/sbin"_s);
    addIfExists(rootBase + u"/usr"_s);
    addIfExists(rootBase + u"/var"_s);
    addIfExists(rootBase + u"/initrd.img"_s);
    addIfExists(rootBase + u"/vmlinuz"_s);
    addIfExists(rootBase + u"/libx32"_s);
    addIfExists(rootBase + u"/lib64"_s);
    addIfExists(rootBase + u"/opt"_s);

    MProcess::Section sect(proc, QT_TR_NOOP("Cannot access installation media."));
    if (pretend) sect.setExceptionMode(nullptr);

    const MIni liveInfo(u"/live/config/initrd.out"_s, MIni::ReadOnly);
    const QString &sqpath = '/' + liveInfo.getString(u"SQFILE_PATH"_s, u"antiX"_s);
    const QString &sqtoram = liveInfo.getString(u"TORAM_MP"_s, u"/live/to-ram"_s) + sqpath;
    QString infile;
    if (archLive) {
        infile = u"/run/archiso/bootmnt/arch/x86_64/airootfs.md5"_s; // fall back to du if metadata is unavailable
    } else {
        infile = sqtoram + "/linuxfs.info"_L1;
        if (!QFile::exists(infile)) {
            const QString &sqloc = liveInfo.getString(u"SQFILE_DIR"_s, u"/live/boot-dev/antiX"_s);
            infile = sqloc + "/linuxfs.info"_L1;
        }
    }
    MIni squashInfo(infile, MIni::ReadOnly);

    // Default EFI System Partition size.
    partman.volSpecs[u"ESP"_s].preferred = 256*MB;

    PartMan::VolumeSpec &vspecRoot = partman.volSpecs[u"/"_s];
    PartMan::VolumeSpec &vspecBoot = partman.volSpecs[u"/boot"_s];
    PartMan::VolumeSpec &vspecHome = partman.volSpecs[u"/home"_s];

    // Root space required.
    MSettings::ValState rootNumState = MSettings::VAL_NOTFOUND;
    vspecRoot.image = squashInfo.getFloat(u"UncompressedSizeKB"_s, 0.0, &rootNumState) * KB;
    if (rootNumState != MSettings::VAL_OK) {
        QStringList duSources;
        duSources << u"-scb"_s;
        for (const QString &src : std::as_const(rootSources)) {
            if (QFileInfo::exists(src)) duSources << src;
            else qDebug() << "Skip missing source for du:" << src;
        }
        proc.exec(u"du"_s, duSources, nullptr, true);
        vspecRoot.image = proc.readOut(true).section('\n', -1).section('\t', 0, 0).toLongLong();
    }
    qDebug() << "Basic image:" << vspecRoot.image << rootNumState << infile;

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
    if (statvfs(rootBase.toUtf8().constData(), &svfs) == 0) {
        sourceInodes = svfs.f_files - svfs.f_ffree; // Will also be used for progress bar.
    }
    vspecRoot.image += (sourceInodes * BASE_BLOCK);
    vspecRoot.minimum = vspecRoot.image + (vspecRoot.image / 20); // +5% for general FS shenanigans.
    vspecRoot.preferred = vspecRoot.minimum + bufferRoot;
    qDebug() << "Source inodes:" << sourceInodes << "Assumed block size:" << BASE_BLOCK;

    getVolumeSpec(squashInfo, u"/boot"_s, bootSource);
    // Include 1 backup and 1 new initrd image.
    vspecBoot.minimum *= 3;
    vspecBoot.preferred *= 3;
    if (vspecBoot.preferred < 1*GB) {
        vspecBoot.preferred = 1*GB;
    }

    // Account for /home on personal snapshots
    if (QFileInfo::exists(homeSource)) {
        getVolumeSpec(squashInfo, u"/home"_s, homeSource);
        vspecHome.preferred += bufferHome;
        rootSources.append(homeSource);
    }

    // Subtract components from the root if obtained the "short way".
    if (rootNumState == MSettings::VAL_OK) {
        vspecRoot.image -= (vspecBoot.image + vspecHome.image);
        vspecRoot.minimum -= (vspecBoot.minimum + vspecHome.minimum);
        vspecRoot.preferred -= (vspecBoot.preferred + vspecHome.preferred);
    }

    qDebug() << "Minimum space:" << vspecBoot.minimum << "(boot),"
        << vspecRoot.minimum << "(root)," << vspecHome.minimum << "(home)";
}

void Base::getVolumeSpec(MIni &squashInfo, const QString &volume, const QString &source) const
{
    PartMan::VolumeSpec &vspec = partman.volSpecs[volume];
    squashInfo.setGroup(volume);

    MSettings::ValState valState = MSettings::VAL_NOTFOUND;
    if (!QFileInfo::exists(source)) {
        vspec.image = vspec.minimum = vspec.preferred = 0;
        qDebug() << "Skip missing source" << source << "for volume" << volume;
        return;
    }

    vspec.image = squashInfo.getFloat(u"UncompressedSizeKB"_s, 0.0, &valState) * KB;
    if (valState != MSettings::VAL_OK) {
        proc.exec(u"du"_s, {u"-scb"_s, source}, nullptr, true);
        vspec.image = proc.readOut(true).section('\n', -1).section('\t', 0, 0).toLongLong();
    }
    long long inodes = squashInfo.getInteger(u"Inodes"_s, 0, &valState);
    if (valState != MSettings::VAL_OK) {
        proc.exec(u"du"_s, {u"-sc"_s, u"--inodes"_s, source}, nullptr, true);
        inodes = proc.readOut(true).section('\n', -1).section('\t', 0, 0).toLongLong();
    }

    vspec.image += (inodes * BASE_BLOCK);
    vspec.minimum = vspec.image + (vspec.image / 20); // +5% for general FS shenanigans.
    vspec.preferred = vspec.minimum;

    qDebug() << volume << "image:" << vspec.image << "minimum:" << vspec.minimum
        << "preferred:" << vspec.preferred << "inodes:" << inodes;
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
    if (mount->curFormat == "ext3"_L1 || mount->curFormat == "ext4"_L1
        || mount->curFormat == "xfs"_L1) {
        opts += ",norecovery"_L1;
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
    const bool hasDesktopMenu =
        QFileInfo::exists(u"/mnt/antiX/usr/bin/desktop-menu"_s) ||
        (!archLive && QFileInfo::exists(u"/mnt/antiX/usr/sbin/desktop-menu"_s));
    if (hasDesktopMenu) {
        proc.exec(u"chroot"_s, {u"/mnt/antiX"_s, u"desktop-menu"_s, u"--write-out-global"_s});
    } else {
        qDebug() << "Skip desktop-menu (not installed in target)";
    }

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
    if (!skiphome && !QFileInfo(u"/live/linux/home/demo"_s).isDir()) {
        proc.exec(u"rm"_s, {u"-rf"_s, u"/mnt/antiX/home/demo"_s});
    }

    // create a /etc/machine-id file and /var/lib/dbus/machine-id file
    sect.setRoot("/mnt/antiX");
    proc.exec(u"rm"_s, {u"-f"_s, u"/var/lib/dbus/machine-id"_s, u"/etc/machine-id"_s});
    proc.exec(u"dbus-uuidgen"_s, {u"--ensure=/etc/machine-id"_s});
    proc.exec(u"dbus-uuidgen"_s, {u"--ensure"_s});
    sect.setRoot(nullptr);

    // Disable VirtualBox Guest Additions if not running in VirtualBox.
    if(!core.detectVirtualBox()) {
        const QStringList vboxMoves = {
            u"/mnt/antiX/etc/rc5.d/S*virtualbox-guest-utils /mnt/antiX/etc/rc5.d/K01virtualbox-guest-utils"_s,
            u"/mnt/antiX/etc/rc4.d/S*virtualbox-guest-utils /mnt/antiX/etc/rc4.d/K01virtualbox-guest-utils"_s,
            u"/mnt/antiX/etc/rc3.d/S*virtualbox-guest-utils /mnt/antiX/etc/rc3.d/K01virtualbox-guest-utils"_s,
            u"/mnt/antiX/etc/rc2.d/S*virtualbox-guest-utils /mnt/antiX/etc/rc2.d/K01virtualbox-guest-utils"_s,
            u"/mnt/antiX/etc/rcS.d/S*virtualbox-guest-x11 /mnt/antiX/etc/rcS.d/K21virtualbox-guest-x11"_s
        };
        for (const QString &move : vboxMoves) {
            proc.shell(u"for f in "_s + move.section(' ', 0, 0)
                + u"; do [ -e \"$f\" ] && mv -f \"$f\" "_s
                + move.section(' ', 1, 1) + u"; done"_s);
        }
    }

    // if PopulateMediaMountPoints is true in gazelle-installer-data, then use the --mntpnt switch
    if (populateMediaMounts) {
        proc.shell(u"make-fstab -O --install=/mnt/antiX --mntpnt=/media"_s);
    } else {
        // Otherwise, clean /media folder - modification to preserve points that are still mounted.
    QDir mediaDir(u"/mnt/antiX/media"_s);
    if (mediaDir.exists()) {
        const QStringList entries = mediaDir.entryList({u"sd*"_s}, QDir::Dirs | QDir::NoDotAndDotDot);
        for (const QString &entry : entries) {
            proc.exec(u"rmdir"_s, {u"--ignore-fail-on-non-empty"_s, mediaDir.filePath(entry)});
        }
    }
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
    const bool useRsync = sync || archLive;
    const bool trackRsyncProgress = useRsync && archLive;
    if (useRsync) {
        prog = "rsync"_L1;
        if (sync) args << u"--delete"_s;
        if (archLive) {
            args << u"--exclude=/dev/*"_s
                << u"--exclude=/proc/*"_s
                << u"--exclude=/sys/*"_s
                << u"--exclude=/run/*"_s
                << u"--exclude=/tmp/*"_s
                << u"--exclude=/mnt/*"_s
                << u"--exclude=/media/*"_s
                << u"--exclude=/lost+found"_s
                << u"--info=progress2"_s
                << u"--outbuf=L"_s;
        }
        if (skiphome) {
            args << u"--filter"_s << u"protect home/*"_s;
        }
    }
    QStringList sources(rootSources);
    if (skiphome && !sync) {
        sources.removeAll(homeSource);
    }
    args << bootSource << sources << u"/mnt/antiX"_s;
    const long copySteps = trackRsyncProgress ? 100 : sourceInodes;
    proc.advance(80, copySteps);
    proc.status(tr("Copying new system"));
    // Placed here so the progress bar moves to the right position before the next step.

    const QString &joined = MProcess::joinCommand(prog, args);
    qDebug().noquote() << "Exec COPY:" << joined;
    QListWidgetItem *logEntry = proc.log(joined, MProcess::LOG_EXEC, false);

    QEventLoop eloop;
    QObject::connect(&proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), &eloop, &QEventLoop::quit);
    QObject::connect(&proc, &QProcess::readyRead, &eloop, &QEventLoop::quit);
    QObject::connect(&proc, &QProcess::readyReadStandardError, &eloop, &QEventLoop::quit);
    proc.start(prog, args);
    long ncopy = 0;
    int rsyncPercent = -1;
    QByteArray stderrBuffer;
    const auto parseRsyncPercentLine = [](const QByteArray &line) {
        const int toChkPos = line.indexOf("to-chk=");
        if (toChkPos >= 0) {
            const int remainStart = toChkPos + 7;
            const int slashPos = line.indexOf('/', remainStart);
            if (slashPos > remainStart) {
                bool okRemain = false;
                const int remaining = line.mid(remainStart, slashPos - remainStart).toInt(&okRemain);
                int totalEnd = slashPos + 1;
                while (totalEnd < line.size()
                    && std::isdigit(static_cast<unsigned char>(line.at(totalEnd)))) {
                    ++totalEnd;
                }
                bool okTotal = false;
                const int total = line.mid(slashPos + 1, totalEnd - (slashPos + 1)).toInt(&okTotal);
                if (okRemain && okTotal && total > 0) {
                    const long long done = static_cast<long long>(total) - remaining;
                    int pct = static_cast<int>((done * 100LL) / total);
                    if (pct < 0) pct = 0;
                    if (pct > 100) pct = 100;
                    return pct;
                }
            }
        }
        const int pctPos = line.lastIndexOf('%');
        if (pctPos <= 0) return -1;
        int start = pctPos - 1;
        while (start >= 0 && std::isdigit(static_cast<unsigned char>(line.at(start)))) {
            --start;
        }
        ++start;
        if (start >= pctPos) return -1;
        bool ok = false;
        const int pct = QByteArray(line.mid(start, pctPos - start)).toInt(&ok);
        return ok ? pct : -1;
    };
    QByteArray rsyncStdoutBuffer;
    QByteArray rsyncStderrBuffer;
    const auto updateRsyncPercent = [&parseRsyncPercentLine](const QByteArray &chunk, QByteArray &buffer, int &pctOut) {
        buffer.append(chunk);
        buffer.replace('\r', '\n');
        const int lastNewline = buffer.lastIndexOf('\n');
        if (lastNewline < 0) return;
        const QByteArray complete = buffer.left(lastNewline);
        buffer = buffer.mid(lastNewline + 1);
        const QList<QByteArray> lines = complete.split('\n');
        for (const QByteArray &line : lines) {
            const int pct = parseRsyncPercentLine(line);
            if (pct >= 0) pctOut = pct;
        }
    };
    const auto isProgressLine = [](const QByteArray &line) {
        return line.contains('%') && (line.contains("to-chk=") || line.contains("xfr#"));
    };
    while (proc.state() != QProcess::NotRunning) {
        eloop.exec();
        const QByteArray stdoutChunk = proc.readAllStandardOutput();
        if (!stdoutChunk.isEmpty()) {
            ncopy += stdoutChunk.count('\n');
            if (trackRsyncProgress) {
                updateRsyncPercent(stdoutChunk, rsyncStdoutBuffer, rsyncPercent);
            }
        }
        const QByteArray stderrChunk = proc.readAllStandardError();
        if (!stderrChunk.isEmpty()) {
            if (trackRsyncProgress) {
                updateRsyncPercent(stderrChunk, rsyncStderrBuffer, rsyncPercent);
                QByteArray cleaned = stderrChunk;
                cleaned.replace('\r', '\n');
                const QList<QByteArray> lines = cleaned.split('\n');
                for (const QByteArray &line : lines) {
                    if (!line.isEmpty() && !isProgressLine(line)) {
                        stderrBuffer.append(line);
                        stderrBuffer.append('\n');
                    }
                }
            } else {
                stderrBuffer.append(stderrChunk);
            }
        }
        if (trackRsyncProgress) {
            if (rsyncPercent >= 0) proc.status(rsyncPercent);
        } else {
            proc.status(ncopy);
        }
    }
    proc.disconnect(&eloop);

    const QByteArray &StdErr = stderrBuffer;
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
