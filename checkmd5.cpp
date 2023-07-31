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
#include <vector>
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

    qint64 btotal = 0;
    struct FileHash {
        QString path;
        QByteArray hash;
    };
    std::vector<FileHash> hashes;
    static const char *failmsg = QT_TR_NOOP("The installation media is corrupt.");
    // Obtain a list of MD5 hashes and their files.
    QDirIterator it(path, {"*.md5"}, QDir::Files);
    QStringList missing(sqfile);
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
            hashes.push_back(sfhash);
            btotal += QFileInfo(sfhash.path).size();
            qApp->processEvents();
        }
    }
    if(!missing.isEmpty()) throw failmsg;
    // Check the MD5 hash of each file.
    const size_t bufsize = 65536;
    std::unique_ptr<char[]> buf(new char[bufsize]);
    qint64 bprog = 0;
    for(const auto &fh : hashes) {
        auto logEntry = proc.log("Check MD5: " + fh.path, MProcess::LOG_EXEC);
        QFile file(fh.path);
        MProcess::Status status = MProcess::STATUS_OK;
        if (!file.open(QFile::ReadOnly)) status = MProcess::STATUS_CRITICAL;
        else {
            QCryptographicHash hash(QCryptographicHash::Md5);
            while (checking && !file.atEnd()) {
                const qint64 rlen = file.read(buf.get(), bufsize);
                if(rlen < 0) break; // I/O error
                hash.addData(buf.get(), rlen);
                bprog += rlen;
                labelSplash->setText(nsplash.arg((bprog * 100) / btotal));
                qApp->processEvents();
            }

            if (!checking) status = MProcess::STATUS_ERROR; // Halted
            else if (!file.atEnd() || hash.result() != fh.hash) {
                status = MProcess::STATUS_CRITICAL; // I/O error or hash mismatch
            }
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
