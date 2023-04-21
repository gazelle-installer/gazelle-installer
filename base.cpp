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
#include <unistd.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <QDebug>
#include <QFileInfo>
#include <QCryptographicHash>
#include <QDirIterator>
#include <QMessageBox>
#include "base.h"

Base::Base(MProcess &mproc, PartMan &pman, Ui::MeInstall &ui,
    const QSettings &appConf, const QCommandLineParser &appArgs)
    : proc(mproc), gui(ui), partman(pman)
{
    mediacheck = appArgs.isSet("media-check");
    if (!mediacheck) nomediacheck = appArgs.isSet("no-media-check");
    populateMediaMounts = appConf.value("POPULATE_MEDIA_MOUNTPOINTS").toBool();
    nocopy = appArgs.isSet("nocopy");
    sync = appArgs.isSet("sync");
}

void Base::scanMedia()
{
    QSettings liveInfo("/live/config/initrd.out", QSettings::IniFormat);
    const QString &sqpath = '/' + liveInfo.value("SQFILE_PATH", "antiX").toString();
    const QString &sqtoram = liveInfo.value("TORAM_MP", "/live/to-ram").toString() + sqpath;
    const QString &sqloc = liveInfo.value("SQFILE_DIR", "/live/boot-dev/antiX").toString();

    // Check the installation media for errors (skip if not required).
    bool checkmd5 = false;
    if (!mediacheck) {
        QFile file("/live/config/proc-cmdline");
        if (file.open(QFile::ReadOnly | QFile::Text)) {
            while (!file.atEnd()) {
                if(file.readLine().trimmed() == "checkmd5") {
                    checkmd5 = true;
                    break;
                }
            }
        }
    }
    if(checkmd5) proc.log("No media check (checkmd5)");
    else if(nomediacheck) proc.log("No media check");
    else {
        const QString &sqfile = liveInfo.value("SQFILE_NAME", "linuxfs").toString();
        checkMediaMD5(QFile::exists(sqtoram+'/'+sqfile) ? sqtoram : sqloc, sqfile);
    }

    bootSource = "/live/aufs/boot";
    rootSources << "/live/aufs/bin" << "/live/aufs/dev"
        << "/live/aufs/etc" << "/live/aufs/lib" << "/live/aufs/libx32" << "/live/aufs/lib64"
        << "/live/aufs/media" << "/live/aufs/mnt" << "/live/aufs/opt" << "/live/aufs/root"
        << "/live/aufs/sbin" << "/live/aufs/usr" << "/live/aufs/var" << "/live/aufs/home";

    // calculate required disk space
    proc.exec("du", {"-scb", bootSource}, nullptr, true);
    partman.bootSpaceNeeded = proc.readOut(true).section('\n', -1).section('\t', 0, 0).toLongLong();

    bool floatOK = false;
    QString infile = sqtoram + "/linuxfs.info";
    if (!QFile::exists(infile)) infile = sqloc + "/linuxfs.info";
    if (QFile::exists(infile)) {
        const QSettings squashInfo(infile, QSettings::IniFormat);
        partman.rootSpaceNeeded = squashInfo.value("UncompressedSizeKB").toFloat(&floatOK) * KB;
    }
    if (!floatOK) {
        rootSources.prepend("-scb");
        proc.exec("du", rootSources, nullptr, true);
        rootSources.removeFirst();
        partman.rootSpaceNeeded = proc.readOut(true).section('\n', -1).section('\t', 0, 0).toLongLong();
    }
    qDebug() << "Image size:" << partman.rootSpaceNeeded << floatOK << infile;
    // Account for persistent root.
    if (QFileInfo::exists("/live/perist-root")) {
        proc.shell("df /live/persist-root --output=used --total |tail -n1", nullptr, true);
        const long long rootfs_size = proc.readOut().toLongLong() * KB;
        qDebug() << "rootfs size is " << rootfs_size;
        // probaby conservative, as rootfs will likely have some overlap with linuxfs.
        partman.rootSpaceNeeded += rootfs_size;
    }

    qDebug() << "Minimum space:" << partman.bootSpaceNeeded << "(boot)," << partman.rootSpaceNeeded << "(root)";
}

void Base::checkMediaMD5(const QString &path, const QString &sqfs)
{
    checking = true;
    const QString osplash = gui.labelSplash->text();
    const QString &nsplash = tr("Checking installation media.")
        + "<br/><font size=2>%1% - " + tr("Press ESC to skip.") + "</font>";
    gui.labelSplash->setText(nsplash.arg(0));
    qint64 btotal = 0;
    struct FileHash {
        QString path;
        QByteArray hash;
    };
    QList<FileHash> hashes;
    static const char *failmsg = QT_TR_NOOP("The installation media is corrupt.");
    // Obtain a list of MD5 hashes and their files.
    QDirIterator it(path, {"*.md5"}, QDir::Files);
    QStringList missing(sqfs);
    if (!QFile::exists("/live/config/did-toram") || QFile::exists("/live/config/toram-all")) {
        missing << "vmlinuz" << "initrd.gz";
    }
    while (it.hasNext()) {
        QFile file(it.next());
        if (!file.open(QFile::ReadOnly | QFile::Text)) throw failmsg;
        while (!file.atEnd()) {
            QString line(file.readLine().trimmed());
            const QString &fname = line.section(' ', 1, 1, QString::SectionSkipEmpty);
            missing.removeOne(fname);
            FileHash sfhash = {
                .path = it.path() + '/' + fname,
                .hash = QByteArray::fromHex(line.section(' ', 0, 0, QString::SectionSkipEmpty).toUtf8())
            };
            hashes.append(sfhash);
            btotal += QFileInfo(sfhash.path).size();
            qApp->processEvents();
        }
    }
    if(!missing.isEmpty()) throw failmsg;
    // Check the MD5 hash of each file.
    const size_t bufsize = 65536;
    std::unique_ptr<char[]> buf(new char[bufsize]);
    qint64 bprog = 0;
    for(const FileHash &fh : hashes) {
        QListWidgetItem *logEntry = proc.log("Check MD5: " + fh.path);
        QFile file(fh.path);
        if (!file.open(QFile::ReadOnly)) throw failmsg;
        QCryptographicHash hash(QCryptographicHash::Md5);
        while (checking && !file.atEnd()) {
            const qint64 rlen = file.read(buf.get(), bufsize);
            if(rlen < 0) throw failmsg;
            hash.addData(buf.get(), rlen);
            bprog += rlen;
            gui.labelSplash->setText(nsplash.arg((bprog * 100) / btotal));
            qApp->processEvents();
        }
        if (!checking) break;
        if (hash.result() != fh.hash) throw failmsg;
        proc.log(logEntry);
    }
    gui.labelSplash->setText(osplash);
    if (!checking) proc.log("Check halted");
    checking = false;
}

void Base::haltCheck(bool silent)
{
    if (!checking) return;
    if (!silent) {
        QMessageBox::StandardButton rc = QMessageBox::warning(gui.boxMain, QString(),
            tr("Are you sure you want to skip checking the installation media?"),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if(rc != QMessageBox::Yes) return;
    }
    checking = false;
}

void Base::install()
{
    proc.log(__PRETTY_FUNCTION__, MProcess::Section);
    if (proc.halted()) return;
    proc.advance(1, 2);

    if (!partman.willFormat("/")) {
        // if root was not formatted and not using --sync option then re-use it
        // remove all folders in root except for /home
        proc.status(tr("Deleting old system"));
        proc.shell("find /mnt/antiX -mindepth 1 -maxdepth 1 ! -name home -exec rm -r {} \\;");

        if (proc.exitStatus() != QProcess::NormalExit) {
            throw QT_TR_NOOP("Failed to delete old system on destination.");
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

    copyLinux();

    proc.advance(1, 1);
    proc.status(tr("Fixing configuration"));
    mkdir("/mnt/antiX/tmp", 01777);
    chmod("/mnt/antiX/tmp", 01777);

    // Copy live set up to install and clean up.
    proc.shell("/usr/sbin/live-to-installed /mnt/antiX");
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
        proc.exec("/bin/rm", {"-rf", "/mnt/antiX/home/demo"});

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

    partman.installTabs();
    // if POPULATE_MEDIA_MOUNTPOINTS is true in gazelle-installer-data, then use the --mntpnt switch
    if (populateMediaMounts) {
        proc.shell("/sbin/make-fstab -O --install=/mnt/antiX --mntpnt=/media");
    } else {
        // Otherwise, clean /media folder - modification to preserve points that are still mounted.
        proc.shell("/bin/rmdir --ignore-fail-on-non-empty /mnt/antiX/media/sd*");
    }
}

void Base::copyLinux()
{
    proc.log(__PRETTY_FUNCTION__, MProcess::Section);
    if (proc.halted()) return;

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
    if (nocopy) return; // Skip copying on purpose

    const QString &joined = MProcess::joinCommand(prog, args);
    qDebug().noquote() << "Exec COPY:" << joined;
    QListWidgetItem *logEntry = proc.log(joined, MProcess::Exec);

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
    QObject::disconnect(&proc, &QProcess::readyRead, nullptr, nullptr);
    QObject::disconnect(&proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), nullptr, nullptr);

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
        throw QT_TR_NOOP("Failed to copy the new system.");
    }
    proc.log(logEntry, proc.exitCode() ? 0 : 1);
}
