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
#include <QInputDialog>
#include <QRegularExpression>
#include "mprocess.h"
#include "partman.h"
#include "msettings.h"
#include "crypto.h"
#include "replacer.h"

using namespace Qt::Literals::StringLiterals;

Replacer::Replacer(class MProcess &mproc, class PartMan *pman, Ui::MeInstall &ui, Crypto &cman, class MIni &appConf)
    : proc(mproc), partman(pman), gui(ui), crypto(cman)
{
    appConf.setSection(u"Storage"_s);
    installFromRootDevice = appConf.getBoolean(u"InstallFromRootDevice"_s);
    appConf.setSection();
    connect(gui.pushReplaceScan, &QPushButton::clicked, this, &Replacer::pushReplaceScan_clicked);

    ui.boxReplaceOptions->hide(); // TODO: Delete when implemented.
}
Replacer::~Replacer() {
    clean();
}

void Replacer::scan(bool full, bool allowUnlock) noexcept
{
    // clear() erases the headers, and clearContents() keeps the cells.
    while (gui.tableExistInst->rowCount() > 0) {
        gui.tableExistInst->removeRow(0);
    }

    long long minSpace = partman->volSpecTotal(u"/"_s, false).minimum;
    bases.clear();
    bool cryptoPrompted = false;
    bool cryptoAny = false;
    bool cryptoUnlocked = false;
    for (PartMan::Iterator it(*partman); PartMan::Device *device = *it; it.next()) {
        if (device->type == PartMan::Device::PARTITION && device->size >= minSpace
            && (!device->flags.bootRoot || installFromRootDevice)) {
            gui.radioReplace->setEnabled(true);
            if (!full) {
                break; // Not a full scan, so only need to check if there is any possible replacement.
            }

            // Attempt to unlock encrypted containers (read-only) for scanning.
            bool unlocked = false;
            if (device->curFormat == "crypto_LUKS"_L1 && device->mapCount == 0) {
                if (allowUnlock && !cryptoPrompted) {
                    cryptoPrompted = true;
                    allowUnlock = promptPass();
                }
                if (!allowUnlock) continue;
                // Prompted and allowed to unlock.
                cryptoAny = true;
                if (crypto.open(device, cryptoPass, true)) {
                    unlocked = true;
                    cryptoUnlocked = true;
                } else {
                    continue; // Unable to open this LUKS container.
                }
            }

            // Obtain required information and add to the list if it is a supported installation.
            const auto &rbase = bases.emplace_back(proc, device);
            if(rbase.ok) {
                const int newrow = gui.tableExistInst->rowCount();
                gui.tableExistInst->insertRow(newrow);
                gui.tableExistInst->setItem(newrow, 0, new QTableWidgetItem(device->friendlyName()));
                gui.tableExistInst->setItem(newrow, 1, new QTableWidgetItem(rbase.release));
            } else {
                bases.pop_back();
            }

            // Lock encrypted volume if it was unlocked in this loop.
            if (unlocked) {
                crypto.close(device);
            }
        }
    }

    if (gui.tableExistInst->columnCount() > 0) {
        gui.tableExistInst->resizeColumnToContents(0);
        for (int col = 1; col < gui.tableExistInst->columnCount(); ++col) {
            gui.tableExistInst->resizeColumnToContents(col);
        }
    }

    if (cryptoAny && !cryptoUnlocked) {
        QMessageBox msgbox(gui.boxMain);
        msgbox.setIcon(QMessageBox::Information);
        msgbox.setText(tr("Could not open any of the locked encrypted containers."));
        msgbox.setInformativeText(tr("Possible incorrect password."
                                     " Press the 'Scan' button to try again with a different password."));
        msgbox.exec();
    }
}

void Replacer::clean() noexcept
{
    cryptoPass.fill('#'); // Wipe the secret.
    // Close every encrypted container opened by the replacer.
    for (RootBase &rbase : bases) {
        closeEncrypted(rbase);
    }
    // Completely refresh the partition tree.
    partman->scan();
}

bool Replacer::validate() const noexcept
{
    const int currow = gui.tableExistInst->currentRow();
    assert(gui.tableExistInst->rowCount() == bases.size());
    if (currow < 0 || currow >= (int)bases.size()) {
        QMessageBox::critical(gui.boxMain, QString(), tr("No existing installation selected."));
        return false;
    }
    return true;
}

bool Replacer::preparePartMan() noexcept
{
    const int currow = gui.tableExistInst->currentRow();
    assert(gui.tableExistInst->rowCount() == bases.size());
    assert(currow >= 0 && currow < (int)bases.size());
    RootBase &rbase = bases[currow];

    partman->scan();
    if (!openEncrypted(rbase)) {
        closeEncrypted(rbase); // Clean up any that were successfully opened.
        return false;
    }
    partman->scanVirtualDevices(true);

    // Populate layout usage information based on the selected existing installation.

    auto ensureBtrfsSubvolume = [&](PartMan::Device *device, const RootBase::MountEntry &mount) -> PartMan::Device * {
        if (!device) return nullptr;
        if (!mount.isBtrfs || mount.subvol.isEmpty()) return device;

        PartMan::Device *partition = device;
        if (partition->type == PartMan::Device::SUBVOLUME) partition = partition->parent();

        auto matchesLabel = [&](PartMan::Device *subvol) -> bool {
            if (subvol->label.compare(mount.subvol, Qt::CaseInsensitive) == 0) return true;
            if (!subvol->curLabel.isEmpty()
                && subvol->curLabel.compare(mount.subvol, Qt::CaseInsensitive) == 0) return true;
            return false;
        };

        PartMan::Device *subvol = nullptr;
        for (PartMan::Iterator it(*partman); PartMan::Device *candidate = *it; it.next()) {
            if (!candidate || candidate->parent() != partition) continue;
            if (candidate->type != PartMan::Device::SUBVOLUME) continue;
            if (matchesLabel(candidate)) {
                subvol = candidate;
                break;
            }
        }
        if (!subvol) {
            subvol = new PartMan::Device(PartMan::Device::SUBVOLUME, partition);
            subvol->flags.oldLayout = true;
            subvol->label = mount.subvol;
            subvol->curLabel = mount.subvol;
            subvol->format = "PRESERVE"_L1;
            subvol->dump = mount.freq != 0;
            subvol->pass = mount.passno;
            partman->notifyChange(subvol);
        }
        if (mount.dir == "/"_L1) subvol->setActive(true);
        return subvol;
    };

    auto finalizeDevice = [&](PartMan::Device *dev, const RootBase::MountEntry *mountInfo) -> PartMan::Device * {
        if (!dev) return nullptr;
        if (mountInfo && mountInfo->isBtrfs) {
            PartMan::Device *candidate = dev;
            if (candidate) {
                PartMan::Device *resolved = ensureBtrfsSubvolume(candidate, *mountInfo);
                if (resolved) dev = resolved;
            }
        }
        return dev;
    };

    auto setMount = [&](PartMan::Device *device, const QString &dir, const RootBase::MountEntry *mountInfo) {
        if (!device) return;
        PartMan::Device *volume = device;
        if (!volume) return;
        const bool isBtrfsSubvolume = mountInfo && mountInfo->isBtrfs && device->type == PartMan::Device::SUBVOLUME;
        QString usefor = dir;
        if (mountInfo && mountInfo->type.compare(u"swap"_s, Qt::CaseInsensitive) == 0) {
            usefor = u"SWAP"_s;
        } else if (usefor.compare(u"swap"_s, Qt::CaseInsensitive) == 0) {
            usefor = u"SWAP"_s;
        } else if (mountInfo && mountInfo->type.compare(u"none"_s, Qt::CaseInsensitive) == 0
                   && mountInfo->fsname.compare(u"swap"_s, Qt::CaseInsensitive) == 0) {
            usefor = u"SWAP"_s;
        }
        auto assignUsefor = [&](PartMan::Device *target, const QString &fmt, bool applyProps, bool canSetUse) {
            if (!target) return;
            partman->changeBegin(target);
            if (canSetUse) {
                target->usefor = usefor;
            } else if (target->usefor == usefor) {
                target->usefor.clear();
            }
            if (!dir.isEmpty() && !fmt.isEmpty()) target->format = fmt;
            if (applyProps && mountInfo) {
                target->dump = mountInfo->freq != 0;
                target->pass = mountInfo->passno;
                if (target->type == PartMan::Device::SUBVOLUME && mountInfo->isBtrfs) {
                    QStringList opts = mountInfo->optionList;
                    for (int ix = opts.count() - 1; ix >= 0; --ix) {
                        const QString trimmed = opts.at(ix).trimmed();
                        if (trimmed.startsWith(u"subvol="_s, Qt::CaseInsensitive)
                            || trimmed.startsWith(u"subvolid="_s, Qt::CaseInsensitive)) {
                            opts.removeAt(ix);
                        } else {
                            opts[ix] = trimmed;
                        }
                    }
                    target->options = opts.join(','_L1);
                } else {
                    target->options = mountInfo->opts.trimmed();
                }
            }
            partman->changeEnd();
        };

        if (!usefor.isEmpty()) {
            for (PartMan::Iterator it(*partman); PartMan::Device *other = *it; it.next()) {
                if (other == volume) continue;
                if (other->usefor == usefor) {
                    partman->changeBegin(other);
                    other->usefor.clear();
                    partman->changeEnd();
                }
            }
        }

        QString newFormat = volume->format.isEmpty() ? volume->curFormat : volume->format;
        if (device->type == PartMan::Device::VIRTUAL && !device->curFormat.isEmpty()) newFormat = device->curFormat;

        assignUsefor(volume, newFormat, device == volume, !(isBtrfsSubvolume && volume == device->parent()));
        if (volume != device) assignUsefor(device, newFormat, true, true);
        if (!volume->map.isEmpty()) {
            if (PartMan::Device *virt = partman->findByPath(u"/dev/mapper/"_s + volume->map)) {
                const bool setUse = !(isBtrfsSubvolume && virt == volume);
                assignUsefor(virt, newFormat, device == virt, setUse);
            }
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
        if (device->type == PartMan::Device::SUBVOLUME) preserve(device->parent());
        else if (!device->map.isEmpty()) preserve(partman->findByPath(u"/dev/mapper/"_s + device->map));
    };

    auto resolveFsDevice = [&](const QString &source, const RootBase::MountEntry *mountInfo) -> PartMan::Device * {
        QString fs = source.trimmed();
        PartMan::Device *dev = resolveDevSource(fs);
        if (!dev && fs.startsWith(u"/dev/mapper/"_s)) {
            dev = partman->findByPath(fs.mid(5));
        }
        if (!dev && mountInfo && mountInfo->isBtrfs) {
            if (!rbase.devpath.isEmpty()) dev = partman->findByPath(rbase.devpath);
            if (!dev && !rbase.physdev.isEmpty()) dev = partman->findByPath(rbase.physdev);
        }
        return finalizeDevice(dev, mountInfo);
    };

    for (const auto &mount : rbase.mounts) {
        PartMan::Device *dev = resolveFsDevice(mount.fsname, &mount);
        if (!dev) continue;
        qDebug() << "DEV" << dev->path << dev->mappedDevice() << mount.dir << mount.type << mount.fsname;
        setMount(dev, mount.dir, &mount);
        if (mount.dir == "/"_L1) {
            if (!rbase.homeSeparate) markPreserve(dev);
        } else if (mount.dir.startsWith(u"/"_s)) {
            markPreserve(dev);
        } else if (mount.type.compare(u"swap"_s, Qt::CaseInsensitive) == 0
                   || mount.dir.compare(u"swap"_s, Qt::CaseInsensitive) == 0) {
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
            PartMan::Device *current = found.value();
            if (current == node) continue;
            auto clearUsefor = [&](PartMan::Device *target) {
                if (!target) return;
                partman->changeBegin(target);
                target->usefor.clear();
                partman->changeEnd();
            };
            if (node->type == PartMan::Device::SUBVOLUME && current->type != PartMan::Device::SUBVOLUME) {
                clearUsefor(current);
                found.value() = node;
            } else {
                clearUsefor(node);
            }
        }
    };
    dedupeMounts();

    for (const auto &mount : rbase.mounts) {
        if (!mount.dir.startsWith(u"/"_s)) continue;
        if (partman->findByMount(mount.dir)) continue;
        if (PartMan::Device *dev = resolveFsDevice(mount.fsname, &mount)) {
            setMount(dev, mount.dir, &mount);
        }
    }
    auto ensureMountFromFstab = [&](const QString &dir) {
        if (partman->findByMount(dir)) return;
        for (const auto &mount : rbase.mounts) {
            if (mount.dir == dir) {
                if (PartMan::Device *dev = resolveFsDevice(mount.fsname, &mount)) {
                    setMount(dev, dir, &mount);
                }
                return;
            }
        }
    };
    ensureMountFromFstab(u"/"_s);
    ensureMountFromFstab(u"/boot/efi"_s);
    ensureMountFromFstab(u"/boot"_s);
    if (!partman->findByMount(u"/boot/efi"_s)) {
        for (PartMan::Iterator it(*partman); PartMan::Device *dev = *it; it.next()) {
            if (dev->flags.sysEFI && dev->type == PartMan::Device::PARTITION) {
                setMount(dev, u"/boot/efi"_s, nullptr);
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
        if (cand) setMount(cand, u"/boot"_s, nullptr);
    }

    auto ensureMount = [&](const QString &dir, auto &&predicate) {
        if (partman->findByMount(dir)) return;
        for (PartMan::Iterator it(*partman); PartMan::Device *dev = *it; it.next()) {
            if (predicate(dev)) {
                setMount(dev, dir, nullptr);
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

    // Confirmation and validation
    QTreeWidgetItem *twroot = new QTreeWidgetItem(gui.treeConfirm);
    QString location = rbase.devpath;
    if (!rbase.physdev.isEmpty() && rbase.physdev != rbase.devpath) {
        location += QStringLiteral(" [%1]").arg(rbase.physdev);
    }
    twroot->setText(0, tr("Replace the installation in %1 (%2)").arg(location, rbase.release));
    return partman->validate(true, twroot);
}

bool Replacer::promptPass() noexcept
{
    cryptoPass.fill('#');
    QInputDialog prompt(gui.boxMain);
    prompt.setLabelText(tr("Password for encrypted volumes:"));
    prompt.setTextEchoMode(QLineEdit::Password);
    prompt.setCancelButtonText(tr("Ignore encrypted volumes"));
    cryptoPass.clear();
    if (prompt.exec() == QInputDialog::Accepted) {
        cryptoPass = prompt.textValue().toUtf8();
        return true;
    }
    return false;
}

PartMan::Device *Replacer::resolveDevSource(const QString &source) const noexcept
{
    if (source.isEmpty()) {
        return nullptr;
    } else if (source.startsWith(u"UUID="_s)) {
        const QString &uuid = source.mid(5);
        for (PartMan::Iterator it(*partman); PartMan::Device *dev = *it; it.next()) {
            if (dev->uuid.compare(uuid, Qt::CaseInsensitive) == 0) return dev;
        }
    } else if (source.startsWith(u"PARTUUID="_s)) {
        const QString &partuuid = source.mid(9);
        for (PartMan::Iterator it(*partman); PartMan::Device *dev = *it; it.next()) {
            if (dev->partuuid.compare(partuuid, Qt::CaseInsensitive) == 0) return dev;
        }
    } else if (source.startsWith(u"LABEL="_s)) {
        const QString &label = source.mid(6);
        for (PartMan::Iterator it(*partman); PartMan::Device *dev = *it; it.next()) {
            if (dev->finalLabel().compare(label, Qt::CaseInsensitive) == 0) return dev;
        }
    } else if (source.startsWith(u"PARTLABEL="_s)) {
        const QString partlabel = source.mid(10);
        for (PartMan::Iterator it(*partman); PartMan::Device *dev = *it; it.next()) {
            if (dev->partlabel.compare(partlabel, Qt::CaseInsensitive) == 0) return dev;
        }
    } else {
        QFileInfo info(source);
        if (!source.startsWith(u"/dev/"_s) && info.exists()) {
            const QString canon = info.canonicalFilePath();
            if (!canon.isEmpty()) {
                return partman->findByPath(canon);
            }
        }
    }
    return partman->findByPath(source);
}

bool Replacer::openEncrypted(RootBase &base) noexcept
{
    for (auto &crypt : base.crypts) {
        PartMan::Device *dev = resolveDevSource(crypt.encdev);
        if (dev) {
            if (dev->mapCount == 0 && !crypto.open(dev, cryptoPass)) {
                QMessageBox::critical(gui.boxMain, QString(),
                    tr("Cannot unlock encrypted partition: %1").arg(crypt.encdev));
                return false;
            }
            dev->addToCrypttab = true;
            crypt.device = dev;
        } else {
            QMessageBox::critical(gui.boxMain, QString(),
                tr("Cannot find partition listed in crypttab: %1").arg(crypt.encdev));
            return false;
        }
    }
    return true;
}
void Replacer::closeEncrypted(RootBase &base) noexcept
{
    QStringList failed;
    for (auto &crypt : base.crypts) {
        if (crypt.device) {
            if (crypto.close(crypt.device)) {
                crypt.device = nullptr;
            } else {
                failed.append(crypt.volume);
            }
        }
    }
    if (!failed.isEmpty()) {
        QMessageBox msgbox(gui.boxMain);
        msgbox.setIcon(QMessageBox::Warning);
        msgbox.setText(tr("Could not re-lock encrypted device(s): %1").arg(failed.join(u", "_s)));
        msgbox.exec();
    }
}

Replacer::RootBase::RootBase(MProcess &proc, PartMan::Device *device) noexcept
{
    MProcess::Section sect(proc, nullptr);
    proc.log(__PRETTY_FUNCTION__, MProcess::LOG_MARKER);

    QString mountpoint;
    QString subvolumeBase;
    bool premounted = false;
    devpath = device->mappedDevice();
    physdev = device->path;
    if (devpath.isEmpty() || !QFile::exists(devpath)) return;
    if (proc.exec(u"findmnt"_s, {u"-AfnCoTARGET"_s, u"--source"_s, devpath}, nullptr, true)) {
        const QString output = proc.readOut(true).trimmed();
        if (!output.isEmpty()) {
            const QStringList fields = output.split(' ');
            if (!fields.isEmpty()) {
                mountpoint = fields.at(0).trimmed();
                premounted = !mountpoint.isEmpty();
            }
            for (const QString &field : fields) {
                if (field.startsWith(u"subvol="_s, Qt::CaseInsensitive)) {
                    subvolumeBase = field.mid(field.indexOf('=') + 1).trimmed();
                    if (subvolumeBase.startsWith(u"/"_s)) subvolumeBase.remove(0, 1);
                    break;
                }
            }
        }
    }
    bool mountedHere = false;
    if (!premounted) {
        mountpoint = "/mnt/temp"_L1;
        QStringList mountArgs = {u"-o"_s, u"ro"_s};
        const QString deviceFormat = device->format.isEmpty() ? device->curFormat : device->format;
        const bool btrfsVolume = deviceFormat.compare(u"btrfs"_s, Qt::CaseInsensitive) == 0;
        if (btrfsVolume) {
            if (!subvolumeBase.isEmpty()) {
                mountArgs = {u"-o"_s, QStringLiteral("ro,subvol=%1").arg(subvolumeBase)};
            } else {
                mountArgs = {u"-o"_s, u"ro,subvolid=5"_s};
            }
        }
        if (!proc.exec(u"mount"_s, mountArgs << devpath << u"-m"_s << mountpoint)) return;
        mountedHere = true;
    }
    QString rootBase = mountpoint;
    QString rootSubvolume;
    QHash<QString, QString> subvolById;
    QString defaultSubvolume;
    const QString deviceFormat = device->format.isEmpty() ? device->curFormat : device->format;
    bool btrfsVolume = deviceFormat.compare(u"btrfs"_s, Qt::CaseInsensitive) == 0;

    auto normalizeSubvolPath = [](QString path) -> QString {
        path = path.trimmed();
        if (path.startsWith('/')) path.remove(0, 1);
        return path;
    };

    auto collectBtrfsSubvolumes = [&]() -> bool {
        if (!proc.exec(u"btrfs"_s, {u"subvolume"_s, u"list"_s, mountpoint}, nullptr, true)) {
            proc.readOut(true);
            return false;
        }
        const QStringList lines = proc.readOutLines();
        const QRegularExpression matcher(u"ID\\s+(\\d+)\\s+.*\\spath\\s+(.+)$"_s);
        for (const QString &line : lines) {
            const QRegularExpressionMatch match = matcher.match(line);
            if (!match.hasMatch()) continue;
            const QString id = match.captured(1).trimmed();
            QString path = normalizeSubvolPath(match.captured(2));
            if (!id.isEmpty() && !path.isEmpty()) subvolById.insert(id, path);
        }
        return true;
    };

    auto fetchDefaultSubvolume = [&]() -> bool {
        if (!proc.exec(u"btrfs"_s, {u"subvolume"_s, u"get-default"_s, mountpoint}, nullptr, true)) {
            proc.readOut(true);
            return false;
        }
        const QString output = proc.readOut();
        QRegularExpression pathRe(u"path\\s+(.+)$"_s);
        const QRegularExpressionMatch pathMatch = pathRe.match(output);
        if (pathMatch.hasMatch()) {
            defaultSubvolume = normalizeSubvolPath(pathMatch.captured(1));
            return true;
        }
        QRegularExpression idRe(u"ID\\s+(\\d+)"_s);
        const QRegularExpressionMatch idMatch = idRe.match(output);
        if (idMatch.hasMatch()) {
            const QString mapped = subvolById.value(idMatch.captured(1).trimmed());
            if (!mapped.isEmpty()) {
                defaultSubvolume = mapped;
                return true;
            }
        }
        return false;
    };

    auto trySetRootFrom = [&](const QString &path) -> bool {
        if (path.isEmpty()) return false;
        const QString normalized = normalizeSubvolPath(path);
        const QString testBase = mountpoint + '/' + normalized;
        if (QFileInfo::exists(testBase + "/etc/fstab"_L1)) {
            rootSubvolume = normalized;
            rootBase = testBase;
            return true;
        }
        return false;
    };

    if (collectBtrfsSubvolumes()) {
        btrfsVolume = true;
        fetchDefaultSubvolume();
        if (!trySetRootFrom(defaultSubvolume)) {
            for (auto it = subvolById.cbegin(); it != subvolById.cend(); ++it) {
                if (trySetRootFrom(it.value())) break;
            }
        }
        if (!rootSubvolume.isEmpty()) defaultSubvolume = rootSubvolume;
    }

    // Extract the release from lsb-release.
    MIni lsbrel(rootBase + "/etc/lsb-release"_L1, MIni::OpenMode::ReadOnly);
    release = lsbrel.getString(u"PRETTY_NAME"_s);

    // Parse fstab
    bool hasFstabEntry = false;
    const QString fstname(rootBase + "/etc/fstab"_L1);
    FILE *fstab = setmntent(fstname.toUtf8().constData(), "r");
    if (fstab) {
        for (struct mntent *mntent = getmntent(fstab); mntent != nullptr; mntent = getmntent(fstab)) {
            mounts.emplace_back(mntent);
            if(strcmp(mntent->mnt_dir, "/home") == 0) {
                homeSeparate = true;
            }
            hasFstabEntry = true;
        }
        endmntent(fstab);
    }
    if (hasFstabEntry && btrfsVolume) {
        bool needSubvolList = false;
        bool needDefaultSubvol = false;
        for (const auto &mount : mounts) {
            if (!mount.isBtrfs) continue;
            if (mount.subvol.isEmpty()) {
                if (!mount.subvolId.isEmpty()) needSubvolList = true;
                else if (mount.dir == "/"_L1) needDefaultSubvol = true;
            }
        }

        if (needSubvolList && subvolById.isEmpty()) collectBtrfsSubvolumes();
        if (needDefaultSubvol && defaultSubvolume.isEmpty()) fetchDefaultSubvolume();
        if (defaultSubvolume.isEmpty() && !rootSubvolume.isEmpty()) defaultSubvolume = rootSubvolume;

        for (auto &mount : mounts) {
            if (!mount.isBtrfs) continue;
            if (mount.subvol.isEmpty()) {
                if (!mount.subvolId.isEmpty()) {
                    const QString path = subvolById.value(mount.subvolId);
                    if (!path.isEmpty()) mount.subvol = path;
                } else if (mount.dir == "/"_L1 && !defaultSubvolume.isEmpty()) {
                    mount.subvol = defaultSubvolume;
                }
            }
            if (mount.subvol.startsWith('/')) mount.subvol.remove(0, 1);
        }
    }

    if (hasFstabEntry && release.isEmpty()) {
        release = tr("Unknown release");
    }
    ok = hasFstabEntry;

    if (ok) {
        // Parse crypttab
        QFile crypttab(rootBase + "/etc/crypttab"_L1);
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
        auto doUmount = [&](const QString &point) {
            if (proc.exec(u"umount"_s, {point})) return true;
            return proc.exec(u"umount"_s, {u"-l"_s, point});
        };
        if (mountedHere) {
            doUmount(mountpoint);
        } else if (premounted) {
            doUmount(mountpoint);
        }
    }
}

// struct mntent converted into Qt-friendly form.
Replacer::RootBase::MountEntry::MountEntry(struct mntent *mntent)
    : fsname(mntent->mnt_fsname), dir(mntent->mnt_dir), type(mntent->mnt_type),
    opts(mntent->mnt_opts), freq(mntent->mnt_freq), passno(mntent->mnt_passno)
{
    optionList = opts.split(','_L1, Qt::SkipEmptyParts);
    for (QString &opt : optionList) opt = opt.trimmed();
    if (type.compare(u"btrfs"_s, Qt::CaseInsensitive) == 0) {
        isBtrfs = true;
        auto stripQuotes = [](QString value) {
            if (value.size() >= 2) {
                const QChar first = value.front();
                const QChar last = value.back();
                if ((first == '"' && last == '"') || (first == '\'' && last == '\'')) {
                    value.chop(1);
                    value.remove(0, 1);
                }
            }
            return value;
        };
        for (const QString &opt : std::as_const(optionList)) {
            if (opt.startsWith(u"subvol="_s, Qt::CaseInsensitive)) subvol = stripQuotes(opt.mid(7).trimmed());
            else if (opt.startsWith(u"subvolid="_s, Qt::CaseInsensitive)) subvolId = stripQuotes(opt.mid(9).trimmed());
        }
        if (subvol.startsWith('/')) subvol.remove(0, 1);
    }
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
    qDebug() << "Crypttab" << volume << encdev << keyfile << options;
}

// Slots

void Replacer::pushReplaceScan_clicked(bool) noexcept
{
    QGuiApplication::setOverrideCursor(Qt::WaitCursor);
    gui.boxReplace->setEnabled(false);
    scan(true, true);
    gui.boxReplace->setEnabled(true);
    QGuiApplication::restoreOverrideCursor();
}
