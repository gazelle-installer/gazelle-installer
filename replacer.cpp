/***************************************************************************
 * Operating system replacement (or reinstall or upgrade)
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
 ****************************************************************************/
#include <mntent.h>
#include <QMessageBox>
#include "mprocess.h"
#include "partman.h"
#include "msettings.h"
#include "replacer.h"

using namespace Qt::Literals::StringLiterals;

Replacer::Replacer(class MProcess &mproc, class PartMan *pman, Ui::MeInstall &ui, class MIni &appConf)
    : proc(mproc), partman(pman), gui(ui)
{
    appConf.setSection(u"Storage"_s);
    installFromRootDevice = appConf.getBoolean(u"InstallFromRootDevice"_s);
    appConf.setSection();
    connect(gui.pushReplaceScan, &QPushButton::clicked, this, [this](bool) noexcept {
        scan(true);
    });
}

void Replacer::scan(bool full) noexcept
{
    gui.radioReplace->setEnabled(false);
    gui.tableExistInst->setEnabled(false);
    while (gui.tableExistInst->rowCount() > 0) {
        gui.tableExistInst->removeRow(0);
    }

    long long minSpace = partman->volSpecTotal(u"/"_s, QStringList()).minimum;
    bases.clear();
    for (PartMan::Iterator it(*partman); PartMan::Device *device = *it; it.next()) {
        if (device->type == PartMan::Device::PARTITION && device->size >= minSpace
            && (!device->flags.bootRoot || installFromRootDevice)) {
            gui.radioReplace->setEnabled(true);
            if(full) {
                const auto &rbase = bases.emplace_back(proc, device);
                if(rbase.ok) {
                    const int newrow = gui.tableExistInst->rowCount();
                    gui.tableExistInst->insertRow(newrow);
                    gui.tableExistInst->setItem(newrow, 0, new QTableWidgetItem(rbase.devpath));
                    gui.tableExistInst->setItem(newrow, 1, new QTableWidgetItem(rbase.release));
                } else {
                    bases.erase(bases.end());
                }
            } else {
                break; // Not a full scan, so only need to check if there is any possible replacement.
            }
        }
    }

    gui.tableExistInst->setEnabled(bases.size() > 0);
}

bool Replacer::validate(bool automatic) const noexcept
{
    const int currow = gui.tableExistInst->currentRow();
    assert(gui.tableExistInst->rowCount() == bases.size());
    if (currow < 0 || currow >= (int)bases.size()) {
        if (!automatic) {
            QMessageBox::critical(gui.boxMain, QString(), tr("No existing installation selected."));
        }
        return false;
    }
    const auto &rbase = bases.at(currow);

    gui.treeConfirm->clear();
    QTreeWidgetItem *twit = new QTreeWidgetItem(gui.treeConfirm);
    twit->setText(0, tr("Replace the installation in %1 (%2)").arg(rbase.devpath, rbase.release));
    return true;
}
bool Replacer::preparePartMan() const noexcept
{
    const int currow = gui.tableExistInst->currentRow();
    assert(gui.tableExistInst->rowCount() == bases.size());
    assert(currow >= 0 && currow < (int)bases.size());

    partman->scan();
    // Populate layout usage information based on the selected existing installation.
    const auto &rbase = bases.at(currow);
    for(const auto &mount : rbase.mounts) {
        for (PartMan::Iterator it(*partman); *it; it.next()) {
            PartMan::Device *dev = *it;
            if ((mount.fsname.startsWith("UUID="_L1) && dev->uuid == mount.fsname.mid(5))
                || (mount.fsname.startsWith("LABEL="_L1) && dev->curLabel == mount.fsname.mid(6))
                || (mount.fsname == dev->path)) {
                partman->changeBegin(dev);
                dev->usefor = mount.dir;
                if (mount.dir == "/"_L1 && !rbase.homeSeparate) {
                    dev->format = "PRESERVE"_L1;
                }
                partman->changeEnd();
            }
        }
    }
    return partman->validate(true);
}

Replacer::RootBase::RootBase(MProcess &proc, PartMan::Device *device) noexcept
{
    MProcess::Section sect(proc, nullptr);
    proc.log(__PRETTY_FUNCTION__, MProcess::LOG_MARKER);

    QString mountpoint;
    bool premounted = false;
    devpath = device->path;
    if (proc.exec(u"findmnt"_s, {u"-AfncoTARGET"_s, u"--source"_s, devpath}, nullptr, true)) {
        mountpoint = proc.readOut();
        premounted = !mountpoint.isEmpty();
    }
    if (!premounted) {
        mountpoint = "/mnt/temp"_L1;
        ok = proc.exec(u"mount"_s, {u"-o"_s, u"ro"_s, devpath, u"-m"_s, mountpoint});
        if (!ok) return;
    }

    // Extract the release from lsb-release.
    MIni lsbrel(mountpoint + "/etc/lsb-release"_L1, MIni::OpenMode::ReadOnly);
    release = lsbrel.getString(u"PRETTY_NAME"_s);

    if (!release.isEmpty()) {
        // Parse fstab
        const QString &fstname(mountpoint + "/etc/fstab"_L1);
        FILE *fstab = setmntent(fstname.toUtf8().constData(), "r");
        if (fstab) {
            for (struct mntent *mntent = getmntent(fstab); mntent != nullptr; mntent = getmntent(fstab)) {
                mounts.emplace_back(mntent);
                if(strcmp(mntent->mnt_dir, "/home") == 0) {
                    homeSeparate = true;
                }
                ok = true; // At least one mount point.
            }
            endmntent(fstab);
        }
    }

    if (ok) {
        // Parse crypttab
        QFile crypttab(mountpoint + "/etc/crypttab"_L1);
        if (crypttab.open(QFile::ReadOnly | QFile::Text)) {
            while (!crypttab.atEnd()) {
                const QByteArray &line = crypttab.readLine().simplified();
                if (line.isEmpty() || line.startsWith('#')) continue;
                const auto &cryptent = crypts.emplace_back(line);
                if (cryptent.volume.isEmpty()) {
                    ok = false;
                    break;
                }
            }
        }
    }

    if (premounted && !mountpoint.isEmpty()) {
        proc.exec(u"umount"_s, {mountpoint});
    }
}

// struct mntent converted into Qt-friendly form.
Replacer::RootBase::MountEntry::MountEntry(struct mntent *mntent)
    : fsname(mntent->mnt_fsname), dir(mntent->mnt_dir), type(mntent->mnt_type),
    opts(mntent->mnt_opts), freq(mntent->mnt_freq), passno(mntent->mnt_passno)
{
}
// crypttab entry structure
Replacer::RootBase::CryptEntry::CryptEntry(const QByteArray &line)
{
    const QList<QByteArray> &fields = line.split(' ');
    if(fields.count() < 2) return;
    volume = fields.at(0);
    encdev = fields.at(1);
    keyfile = fields.value(2);
    options = fields.value(3);
}
