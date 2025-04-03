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
#include <QMessageBox>
#include "checkmd5.h"
#include "msettings.h"

using namespace Qt::Literals::StringLiterals;

CheckMD5::CheckMD5(MProcess &mproc, QLabel *splash) noexcept
    : proc(mproc), labelSplash(splash)
{
}

void CheckMD5::check()
{
    const QString osplash = labelSplash->text();
    const QString &nsplash = tr("Checking installation media.")
        + "<br/><font size=2>%1% - "_L1 + tr("Press ESC to skip.") + "</font>"_L1;
    labelSplash->setText(nsplash.arg(0));

    checking = true;
    const MIni liveInfo(u"/live/config/initrd.out"_s, MIni::ReadOnly);
    const QString &sqtoram = liveInfo.getString(u"TORAM_MP"_s, u"/live/to-ram"_s)
        + '/' + liveInfo.getString(u"SQFILE_PATH"_s, u"antiX"_s);
    const QString &sqfile = liveInfo.getString(u"SQFILE_NAME"_s, u"linuxfs"_s);
    const QString &path = QFile::exists(sqtoram+'/'+sqfile)
        ? sqtoram : liveInfo.getString(u"SQFILE_DIR"_s, u"/live/boot-dev/antiX"_s);

    off_t btotal = 0;
    struct HashTarget {
        QString path;
        QByteArray hash;
        off_t size;
        size_t blksize;
    };
    std::vector<HashTarget> targets;
    static const char *failmsg = QT_TR_NOOP("The installation media is corrupt.");
    // Obtain a list of MD5 hashes and their files.
    QDirIterator it(path, {u"*.md5"_s}, QDir::Files);
    QStringList missing(sqfile);
    if (!QFile::exists(u"/live/config/did-toram"_s) || QFile::exists(u"/live/config/toram-all"_s)) {
        missing << u"vmlinuz"_s << u"initrd.gz"_s;
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
                    .blksize = static_cast<size_t>(sb.st_blksize)
                };
                if (bufsize < target.blksize) {
                    bufsize = target.blksize;
                }
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
        auto logEntry = proc.log("Check MD5: "_L1 + target.path, MProcess::LOG_EXEC);
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
    if (!checking) {
        proc.log(u"Check halted"_s, MProcess::LOG_FAIL);
    }
    checking = false;
}

void CheckMD5::halt(bool silent) noexcept
{
    if (!checking) return;
    if (!silent) {
        QMessageBox msgbox(labelSplash);
        msgbox.setIcon(QMessageBox::Warning);
        msgbox.setText(tr("The installation media is still being checked."));
        msgbox.setInformativeText(tr("Are you sure you want to skip checking the installation media?"));
        msgbox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
        msgbox.setDefaultButton(QMessageBox::No);
        if(msgbox.exec() != QMessageBox::Yes) return;
    }
    checking = false;
}
