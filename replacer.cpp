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
#include <QFile>
#include <QFileInfo>
#include <QHash>
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
        scan(true, true);
    });

    ui.boxReplaceOptions->hide(); // TODO: Delete when implemented.
}

void Replacer::scan(bool full, bool allowUnlock) noexcept
{
        gui.tableExistInst->setEnabled(false);
    while (gui.tableExistInst->rowCount() > 0) {
        gui.tableExistInst->removeRow(0);
    }

    long long minSpace = partman->volSpecTotal(u"/"_s, QStringList()).minimum;
    bases.clear();
    bool rescan = false;
    for (PartMan::Iterator it(*partman); PartMan::Device *device = *it; it.next()) {
        if (device->type == PartMan::Device::PARTITION && device->size >= minSpace
            && (!device->flags.bootRoot || installFromRootDevice)) {
            if (device->curFormat == "crypto_LUKS"_L1 && device->mapCount == 0) {
                gui.radioReplace->setEnabled(true);
                if (!allowUnlock) continue;
                if (partman->promptUnlock(device)) {
                    rescan = true;
                    break;
                }
                continue;
            }
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

    gui.tableExistInst->setEnabled(!bases.empty());
    if (rescan) {
        scan(full, allowUnlock);
        return;
    }

    if (gui.tableExistInst->columnCount() > 0) {
        gui.tableExistInst->resizeColumnToContents(0);
        for (int col = 1; col < gui.tableExistInst->columnCount(); ++col) {
            gui.tableExistInst->resizeColumnToContents(col);
        }
    }
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
    QString location = rbase.devpath;
    if (!rbase.physdev.isEmpty() && rbase.physdev != rbase.devpath) {
        location += QStringLiteral(" [%1]").arg(rbase.physdev);
    }
    twit->setText(0, tr("Replace the installation in %1 (%2)").arg(location, rbase.release));
    return true;
}
bool Replacer::preparePartMan() const noexcept
{
    const int currow = gui.tableExistInst->currentRow();
    assert(gui.tableExistInst->rowCount() == bases.size());
    assert(currow >= 0 && currow < (int)bases.size());

    partman->scan();
    for (PartMan::Iterator it(*partman); PartMan::Device *dev = *it; it.next()) {
        if (dev->type == PartMan::Device::PARTITION && dev->curFormat == "crypto_LUKS"_L1
            && dev->mapCount == 0) {
            if (!partman->promptUnlock(dev)) return false;
            partman->scan(dev);
        }
    }
    // Populate layout usage information based on the selected existing installation.
    const auto &rbase = bases.at(currow);

    auto setMount = [&](PartMan::Device *device, const QString &dir) {
        if (!device) return;
        auto assign = [&](PartMan::Device *target, const QString &fmt) {
            if (!target) return;
            partman->changeBegin(target);
            target->usefor = dir;
            if (!dir.isEmpty() && !fmt.isEmpty()) target->format = fmt;
            partman->changeEnd();
        };

        PartMan::Device *volume = device;
        if (device->type == PartMan::Device::VIRTUAL && device->origin) volume = device->origin;
        if (!volume) return;

        if (!dir.isEmpty()) {
            for (PartMan::Iterator it(*partman); PartMan::Device *other = *it; it.next()) {
                if (other == volume) continue;
                if (other->usefor == dir) {
                    partman->changeBegin(other);
                    other->usefor.clear();
                    partman->changeEnd();
                }
            }
        }

        QString newFormat = volume->format.isEmpty() ? volume->curFormat : volume->format;
        if (device->type == PartMan::Device::VIRTUAL && !device->curFormat.isEmpty()) newFormat = device->curFormat;

        assign(volume, newFormat);
        if (volume != device) assign(device, newFormat);
        if (!volume->map.isEmpty()) {
            if (PartMan::Device *virt = partman->findByPath(u"/dev/mapper/"_s + volume->map)) assign(virt, newFormat);
        }
    };

    auto markPreserve = [&](PartMan::Device *device) {
        if (!device) return;
        auto preserve = [&](PartMan::Device *target) {
            if (!target) return;
            partman->changeBegin(target);
            target->format = "PRESERVE"_L1;
            partman->changeEnd();
        };
        preserve(device);
        if (device->type == PartMan::Device::VIRTUAL && device->origin) preserve(device->origin);
        else if (!device->map.isEmpty()) preserve(partman->findByPath(u"/dev/mapper/"_s + device->map));
    };

    auto resolveFsDevice = [&](const QString &source) -> PartMan::Device * {
        QString fs = source.trimmed();
        if (fs.isEmpty()) return nullptr;
        if (fs.startsWith(u"UUID="_s)) {
            const QString uuid = fs.mid(5);
            for (PartMan::Iterator it(*partman); PartMan::Device *dev = *it; it.next()) {
                if (dev->uuid.compare(uuid, Qt::CaseInsensitive) == 0) return dev;
            }
            return nullptr;
        }
        if (fs.startsWith(u"PARTUUID="_s)) {
            const QString partuuid = fs.mid(9);
            for (PartMan::Iterator it(*partman); PartMan::Device *dev = *it; it.next()) {
                if (dev->partuuid.compare(partuuid, Qt::CaseInsensitive) == 0) return dev;
            }
            return nullptr;
        }
        if (fs.startsWith(u"LABEL="_s)) {
            const QString label = fs.mid(6);
            for (PartMan::Iterator it(*partman); PartMan::Device *dev = *it; it.next()) {
                if (dev->label.compare(label, Qt::CaseInsensitive) == 0
                    || dev->curLabel.compare(label, Qt::CaseInsensitive) == 0) return dev;
            }
            return nullptr;
        }
        if (fs.startsWith(u"PARTLABEL="_s)) {
            const QString partlabel = fs.mid(10);
            for (PartMan::Iterator it(*partman); PartMan::Device *dev = *it; it.next()) {
                if (dev->partlabel.compare(partlabel, Qt::CaseInsensitive) == 0) return dev;
            }
            return nullptr;
        }
        QFileInfo info(fs);
        if (!fs.startsWith(u"/dev/"_s) && info.exists()) {
            const QString canon = info.canonicalFilePath();
            if (!canon.isEmpty()) fs = canon;
        }
        PartMan::Device *dev = partman->findByPath(fs);
        if (!dev && fs.startsWith(u"/dev/mapper/"_s)) {
            dev = partman->findByPath(fs.mid(5));
        }
        if (dev && dev->type == PartMan::Device::VIRTUAL && dev->origin) return dev->origin;
        return dev;
    };

    for (const auto &mount : rbase.mounts) {
        PartMan::Device *dev = resolveFsDevice(mount.fsname);
        if (!dev) continue;
        setMount(dev, mount.dir);
        if (mount.dir == "/"_L1) {
            if (!rbase.homeSeparate) markPreserve(dev);
        } else if (mount.dir.startsWith(u"/"_s)) {
            markPreserve(dev);
        }
    }

    auto dedupeMounts = [&]() {
        QHash<QString, PartMan::Device *> owners;
        for (PartMan::Iterator it(*partman); PartMan::Device *node = *it; it.next()) {
            if (node->usefor.isEmpty()) continue;
            if (!node->usefor.startsWith('/')) continue;
            auto found = owners.find(node->usefor);
            if (found == owners.end()) {
                owners.insert(node->usefor, node);
                continue;
            }
            if (found.value() == node) continue;
            partman->changeBegin(node);
            node->usefor.clear();
            partman->changeEnd();
        }
    };
    dedupeMounts();

    auto ensureMountFromFstab = [&](const QString &dir) {
        if (partman->findByMount(dir)) return;
        for (const auto &mount : rbase.mounts) {
            if (mount.dir == dir) {
                if (PartMan::Device *dev = resolveFsDevice(mount.fsname)) {
                    setMount(dev, dir);
                }
                return;
            }
        }
    };
    ensureMountFromFstab(u"/boot/efi"_s);
    ensureMountFromFstab(u"/boot"_s);
    if (!partman->findByMount(u"/boot/efi"_s)) {
        for (PartMan::Iterator it(*partman); PartMan::Device *dev = *it; it.next()) {
            if (dev->flags.sysEFI && dev->type == PartMan::Device::PARTITION) {
                setMount(dev, u"/boot/efi"_s);
                break;
            }
        }
    }
    if (!partman->findByMount(u"/boot"_s)) {
        auto hasBootLabel = [](const QString &label) -> bool {
            return !label.isEmpty() && label.contains(u"boot"_s, Qt::CaseInsensitive);
        };
        auto looksLikeBoot = [&](PartMan::Device *dev) -> bool {
            return hasBootLabel(dev->curLabel) || hasBootLabel(dev->label) || hasBootLabel(dev->partlabel);
        };
        PartMan::Device *cand = nullptr;
        for (PartMan::Iterator it(*partman); PartMan::Device *dev = *it; it.next()) {
            if (dev->type != PartMan::Device::PARTITION) continue;
            if (dev->flags.sysEFI) continue;
            const QString fmt = dev->format.isEmpty() ? dev->curFormat : dev->format;
            if (!fmt.startsWith(u"ext"_s, Qt::CaseInsensitive)) continue;
            if (dev->flags.bootRoot) continue;
            if (!looksLikeBoot(dev)) continue;
            cand = dev;
            break;
        }
        if (cand) setMount(cand, u"/boot"_s);
    }

    auto ensureMount = [&](const QString &dir, auto &&predicate) {
        if (partman->findByMount(dir)) return;
        for (PartMan::Iterator it(*partman); PartMan::Device *dev = *it; it.next()) {
            if (predicate(dev)) {
                setMount(dev, dir);
                partman->changeBegin(dev);
                dev->format = dev->format.isEmpty() ? dev->curFormat : dev->format;
                partman->changeEnd();
                break;
            }
        }
    };
    ensureMount(u"/boot/efi"_s, [](PartMan::Device *dev) {
        return dev->flags.sysEFI;
    });
    ensureMount(u"/boot"_s, [](PartMan::Device *dev) {
        return dev->usefor.isEmpty() && dev->format.startsWith(u"ext"_s, Qt::CaseInsensitive)
            && !dev->flags.sysEFI && dev->type == PartMan::Device::PARTITION;
    });

    auto resolveCryptDevice = resolveFsDevice;

    for (const auto &crypt : rbase.crypts) {
        PartMan::Device *cryptdev = resolveCryptDevice(crypt.encdev);
        if (!cryptdev) continue;
        partman->changeBegin(cryptdev);
        cryptdev->addToCrypttab = true;
        if (!crypt.volume.isEmpty() && (cryptdev->map.isEmpty() || cryptdev->map == crypt.volume)) {
            cryptdev->map = crypt.volume;
        }
        partman->changeEnd();
    }

    return partman->validate(true);
}

Replacer::RootBase::RootBase(MProcess &proc, PartMan::Device *device) noexcept
{
    MProcess::Section sect(proc, nullptr);
    proc.log(__PRETTY_FUNCTION__, MProcess::LOG_MARKER);

    QString mountpoint;
    bool premounted = false;
    devpath = device->mappedDevice();
    physdev = device->path;
    if (devpath.isEmpty() || !QFile::exists(devpath)) return;
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

    if (!mountpoint.isEmpty()) {
        if (premounted) proc.exec(u"umount"_s, {mountpoint});
        else if (!proc.exec(u"umount"_s, {mountpoint})) {
            proc.exec(u"umount"_s, {u"-l"_s, mountpoint});
        }
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
