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
#include <vector>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <QFileInfo>
#include <QCryptographicHash>
#include <QDirIterator>
#include <QSettings>
#include <QMessageBox>
#include "checkmd5.h"

CheckMD5::CheckMD5(MProcess &mproc, QLabel *splash) noexcept
    : proc(mproc), labelSplash(splash)
{
}

void CheckMD5::check()
{
    const QString osplash = labelSplash->text();
    const QString &nsplash = tr("Checking installation media.")
        + "<br/><font size=2>%1% - " + tr("Press ESC to skip.") + "</font>";
    labelSplash->setText(nsplash.arg(0));

    checking = true;
    QSettings liveInfo("/live/config/initrd.out", QSettings::IniFormat);
    const QString &sqtoram = liveInfo.value("TORAM_MP", "/live/to-ram").toString()
        + '/' + liveInfo.value("SQFILE_PATH", "antiX").toString();
    const QString &sqfile = liveInfo.value("SQFILE_NAME", "linuxfs").toString();
    const QString &path = QFile::exists(sqtoram+'/'+sqfile)
        ? sqtoram : liveInfo.value("SQFILE_DIR", "/live/boot-dev/antiX").toString();

    off_t btotal = 0;
    struct HashTarget {
        QString path;
        QByteArray hash;
        off_t size;
        blksize_t blksize;
    };
    std::vector<HashTarget> targets;
    static const char *failmsg = QT_TR_NOOP("The installation media is corrupt.");
    // Obtain a list of MD5 hashes and their files.
    QDirIterator it(path, {"*.md5"}, QDir::Files);
    QStringList missing(sqfile);
    if (!QFile::exists("/live/config/did-toram") || QFile::exists("/live/config/toram-all")) {
        missing << "vmlinuz" << "initrd.gz";
    }
    size_t bufsize = 0;
    while (it.hasNext()) {
        QFile file(it.next());
        if (!file.open(QFile::ReadOnly | QFile::Text)) throw failmsg;
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
                    .blksize = sb.st_blksize
                };
                bufsize = std::max<size_t>(bufsize, sb.st_blksize);
                targets.push_back(target);
                btotal += target.size;
            } else {
                throw failmsg;
            }
            qApp->processEvents();
        }
    }
    if(!missing.isEmpty()) throw failmsg;

    // Check the MD5 hash of each file.
    std::unique_ptr<char[]> buf(new char[bufsize]);
    qint64 bprog = 0;
    for(const auto &target : targets) {
        auto logEntry = proc.log("Check MD5: " + target.path, MProcess::LOG_EXEC);
        const int fd = open(target.path.toUtf8().constData(), O_RDONLY);
        MProcess::Status status = MProcess::STATUS_OK;
        if (fd > 0) {
            posix_fadvise(fd, 0, 0, POSIX_FADV_SEQUENTIAL);
            QCryptographicHash hash(QCryptographicHash::Md5);
            for (off_t remain = target.size; remain>0 && checking;) {
                const ssize_t rlen = read(fd, buf.get(), target.blksize);
                if(rlen < 0) {
                    status = MProcess::STATUS_CRITICAL;
                    break;
                }
                hash.addData({buf.get(), rlen});
                remain -= rlen;
                bprog += rlen;
                labelSplash->setText(nsplash.arg((100*bprog) / btotal));
                qApp->processEvents();
            }
            close(fd);

            if (!checking) status = MProcess::STATUS_ERROR; // Halted
            else if (hash.result() != target.hash) {
                status = MProcess::STATUS_CRITICAL; // I/O error or hash mismatch
            }
        } else {
            status = MProcess::STATUS_CRITICAL;
        }
        proc.log(logEntry, status);
        if (status == MProcess::STATUS_CRITICAL) throw failmsg;
        else if (!checking) break;
    }

    labelSplash->setText(osplash);
    if (!checking) proc.log("Check halted", MProcess::LOG_FAIL);
    checking = false;
}

void CheckMD5::halt(bool silent) noexcept
{
    if (!checking) return;
    if (!silent) {
        QMessageBox::StandardButton rc = QMessageBox::warning(labelSplash, QString(),
            tr("Are you sure you want to skip checking the installation media?"),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if(rc != QMessageBox::Yes) return;
    }
    checking = false;
}
