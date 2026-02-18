/***************************************************************************\
 * Media integrity check. This should be done at boot time instead of here.
 *
 *   Copyright (C) 2023 by AK-47
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
\***************************************************************************/

#define _FILE_OFFSET_BITS 64
#define _DEFAULT_SOURCE
#include <utility>
#include <vector>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <QtConcurrent>
#include <QFutureWatcher>
#include <QFileInfo>
#include <QCryptographicHash>
#include <QDirIterator>
#include <QMessageBox>
#include "checkmd5.h"
#include "msettings.h"

using namespace Qt::Literals::StringLiterals;

CheckMD5::CheckMD5(MProcess &mproc, QLabel *splash) noexcept
    : proc(mproc), labelSplash(splash)
{
    oldSplashText = labelSplash->text();

    connect(&watcher, &QFutureWatcher<CheckResult>::progressValueChanged, this, &CheckMD5::watcher_progressValueChanged);
    connect(&watcher, &QFutureWatcher<CheckResult>::resultReadyAt, this, &CheckMD5::watcher_resultReadyAt);
    connect(&watcher, &QFutureWatcher<CheckResult>::finished, this, &CheckMD5::watcher_finished);
    watcher.setFuture(QtConcurrent::run(&CheckMD5::check, this));
}
CheckMD5::~CheckMD5() noexcept
{
    if(!watcher.isFinished()) {
        halt(true);
        QEventLoop eloop;
        connect(&watcher, &QFutureWatcher<CheckResult>::finished, &eloop, &QEventLoop::quit);
        eloop.exec();
    }
}

void CheckMD5::wait()
{
    if(!watcher.isFinished()) {
        QEventLoop eloop;
        connect(&watcher, &QFutureWatcher<CheckResult>::finished, &eloop, &QEventLoop::quit);
        eloop.exec();
    }
    qApp->processEvents();
    if (status == MProcess::STATUS_CRITICAL) {
        throw(QT_TR_NOOP("The installation media is corrupt."));
    }
}

void CheckMD5::halt(bool silent) noexcept
{
    if (!silent && !watcher.isCanceled()) {
        QMessageBox msgbox(labelSplash);
        msgbox.setIcon(QMessageBox::Warning);
        msgbox.setText(tr("The installation media is still being checked."));
        msgbox.setInformativeText(tr("Are you sure you want to skip checking the installation media?"));
        msgbox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
        msgbox.setDefaultButton(QMessageBox::No);
        if(msgbox.exec() != QMessageBox::Yes) return;
    }
    watcher.cancel();
    status = MProcess::STATUS_ERROR;
    proc.log(u"Check MD5 Halted"_s, MProcess::LOG_FAIL);
}

void CheckMD5::check(QPromise<CheckResult> &promise) const noexcept
{
    promise.setProgressRange(0, 100);
    promise.setProgressValue(0);
    QString archIsoBase;
    QString archSqName;
    const QStringList archBases = {
        u"/run/archiso/bootmnt/arch/x86_64"_s, // common Arch iso path
        u"/run/archiso/copytoram"_s            // fallback for toram option
    };
    const QStringList archNames = {u"airootfs.sfs"_s, u"airootfs.img"_s};
    for (const QString &candidate : archBases) {
        for (const QString &fname : archNames) {
            if (QFileInfo::exists(candidate + u'/' + fname)) {
                archIsoBase = candidate;
                archSqName = fname;
                break;
            }
        }
        if (!archIsoBase.isEmpty()) break;
    }
    const bool archMedia = !archIsoBase.isEmpty();

    QString path;
    QString sqfile;
    if (archMedia) {
        // Arch ISO layout uses airootfs.sfs alongside its checksum.
        path = archIsoBase;
        sqfile = archSqName.isEmpty() ? u"airootfs.sfs"_s : archSqName;
    } else {
        const MIni liveInfo(u"/live/config/initrd.out"_s, MIni::ReadOnly);
        const QString &sqtoram = liveInfo.getString(u"TORAM_MP"_s, u"/live/to-ram"_s)
            + '/' + liveInfo.getString(u"SQFILE_PATH"_s, u"antiX"_s);
        sqfile = liveInfo.getString(u"SQFILE_NAME"_s, u"linuxfs"_s);
        path = QFile::exists(sqtoram+'/'+sqfile)
            ? sqtoram : liveInfo.getString(u"SQFILE_DIR"_s, u"/live/boot-dev/antiX"_s);
    }

    off_t btotal = 0;
    struct HashTarget {
        QString path;
        QByteArray hash;
        off_t size;
        size_t blksize;
    };
    std::vector<HashTarget> targets;
    // For Arch, prefer SHA-512 checksums; fall back to MD5 if unavailable.
    const bool useSha512 = archMedia && QDirIterator(path, {u"*.sha512"_s}, QDir::Files).hasNext();
    // Obtain a list of hashes and their files.
    QDirIterator it(path, {useSha512 ? u"*.sha512"_s : u"*.md5"_s}, QDir::Files);
    QStringList missing(sqfile);
    if (!archMedia && (!QFile::exists(u"/live/config/did-toram"_s) || QFile::exists(u"/live/config/toram-all"_s))) {
        missing << u"vmlinuz"_s << u"initrd.gz"_s;
    }
    size_t bufsize = 0;
    // Minimum optimum block size for hash processing is 64 bytes (MD5) or 128 bytes (SHA-512).
    const long pagesize = std::max<long>(sysconf(_SC_PAGESIZE), 64);

    while (it.hasNext()) {
        QFile file(it.next());
        if (!file.open(QFile::ReadOnly | QFile::Text)) {
            promise.emplaceResult(file.fileName(), CheckState::ERROR);
        }
        while (!file.atEnd()) {
            QString line(file.readLine().trimmed());
            const QString &fname = line.section(' ', 1, 1, QString::SectionSkipEmpty);
            const QString &fpath = it.path() + '/' + fname;
            missing.removeOne(fname);
            struct stat sb;
            if (stat(fpath.toUtf8().constData(), &sb) == 0) {
                HashTarget target = {
                    .path = fpath,
                    .hash = QByteArray::fromHex(line.section(' ', 0, 0, QString::SectionSkipEmpty).toUtf8()),
                    .size = sb.st_size,
                    .blksize = static_cast<size_t>(((sb.st_blksize + (pagesize - 1)) / pagesize) * pagesize)
                };
                if (bufsize < target.blksize) {
                    bufsize = target.blksize;
                }
                targets.push_back(target);
                btotal += target.size;
            } else {
                promise.emplaceResult(fpath, CheckState::ERROR);
            }
        }
    }
    for (const QString &mpath : missing) {
        promise.emplaceResult(mpath, CheckState::MISSING);
    }

    // Check the hash of each file.
    std::unique_ptr<char[]> buf(new char[bufsize]);
    void *const buffer = std::aligned_alloc(static_cast<size_t>(pagesize), bufsize);
    if (!buffer) {
        promise.emplaceResult(QString(), CheckState::ERROR);
        return;
    }

    qint64 bprog = 0;
    int progress = 0;
    assert(btotal != 0);
    for(const auto &target : targets) {
        if (promise.isCanceled()) break;
        promise.emplaceResult(target.path, CheckState::STARTED);

        const int fd = open(target.path.toUtf8().constData(), O_RDONLY);
        if (fd == -1) {
            promise.emplaceResult(target.path, CheckState::ERROR);
            break;
        }
        posix_fadvise(fd, 0, 0, POSIX_FADV_SEQUENTIAL);

        QCryptographicHash hash(useSha512 ? QCryptographicHash::Sha512 : QCryptographicHash::Md5);
        bool okIO = true;
        for (off_t remain = target.size; remain>0 && !promise.isCanceled();) {
            const ssize_t rlen = read(fd, buffer, target.blksize);
            if(rlen < 0) {
                promise.emplaceResult(target.path, CheckState::ERROR);
                okIO = false;
                break;
            }
            hash.addData({static_cast<const char *>(buffer), rlen});
            remain -= rlen;
            bprog += rlen;
            // For performance reasons, update the progress only if the PERCENTAGE has changed.
            const int newprog = (100*bprog) / btotal;
            if(newprog != progress) {
                progress = newprog;
                promise.setProgressValue(progress);
            }
        }

        close(fd);
        if (!promise.isCanceled() && okIO) {
            promise.emplaceResult(target.path,
                (hash.result() == target.hash ? CheckState::GOOD : CheckState::BAD));
        }
    }

    std::free(buffer);
}

// Progress indication and logging
void CheckMD5::watcher_progressValueChanged(int progress) noexcept
{
    if (progress < 0) return;
    static const QString &nsplash = tr("Checking installation media.")
        + "<br/><font size=2>%1% - "_L1 + tr("Press ESC to skip.") + "</font>"_L1;
    labelSplash->setText(nsplash.arg(progress));
}
void CheckMD5::watcher_resultReadyAt(int index) noexcept
{
    const CheckResult &result = watcher.resultAt(index);
    if(result.state == STARTED) {
        logEntry = proc.log("Check MD5: "_L1 + result.path, MProcess::LOG_EXEC);
    } else {
        if(result.state == GOOD) {
            status = MProcess::STATUS_OK;
            qDebug().noquote() << "Check MD5 Good:" << result.path;
        } else {
            watcher.cancel();
            status = MProcess::STATUS_CRITICAL;
            switch(result.state) {
                case BAD:
                    proc.log("Check MD5 Bad: "_L1 + result.path, MProcess::LOG_FAIL);
                    break;
                case ERROR:
                    proc.log("Check MD5 Error: "_L1 + result.path, MProcess::LOG_FAIL);
                    break;
                case MISSING:
                    proc.log("Check MD5 Missing: "_L1 + result.path, MProcess::LOG_FAIL);
                    break;
                default:
                    std::unreachable();
            }
        }
        proc.log(logEntry, status);
    }
}
void CheckMD5::watcher_finished() noexcept
{
    logEntry = proc.log(u"Check MD5 Finished"_s, MProcess::LOG_LOG);
    proc.log(logEntry, status);
    labelSplash->setText(oldSplashText);
}
