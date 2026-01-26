/***************************************************************************\
 * Drive encryption.
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
\***************************************************************************/
#include <QFileInfo>
#include <QList>
#include "mprocess.h"
#include "msettings.h"
#include "partman.h"
#include "crypto.h"

using namespace Qt::Literals::StringLiterals;

Crypto::Crypto(MProcess &mproc, Ui::MeInstall &ui, QObject *parent)
    : QObject(parent), proc(mproc), gui(ui), pass(ui.textCryptoPass, ui.textCryptoPass2, 1, this)
{
    connect(&pass, &PassEdit::validationChanged, gui.pushNext, &QPushButton::setEnabled);

    QFileInfo cryptsetup(u"/usr/bin/cryptsetup"_s);
    if (!cryptsetup.isExecutable()) {
        cryptsetup.setFile(u"/usr/sbin/cryptsetup"_s);
    }
    if (!cryptsetup.exists() || !cryptsetup.isExecutable()) {
        cryptsetup.setFile(u"/usr/bin/cryptsetup"_s);
    }
    QFileInfo crypsetupinitramfs(u"/usr/share/initramfs-tools/conf-hooks.d/cryptsetup"_s);
    cryptsupport = (cryptsetup.exists() && cryptsetup.isExecutable() && crypsetupinitramfs.exists());
}

bool Crypto::manageConfig(MSettings &config) noexcept
{
    config.setSection(u"Encryption"_s, gui.pageEncryption);
    if(!config.isSave()) {
        const QString &epass = config.getString(u"Pass"_s);
        gui.textCryptoPass->setText(epass);
        gui.textCryptoPass2->setText(epass);
    }
    config.addFilter(u"Pass"_s);
    return true;
}

bool Crypto::valid() const noexcept
{
    return pass.valid();
}

void Crypto::formatAll(const PartMan &partman)
{
    MProcess::Section sect(proc, QT_TR_NOOP("Failed to format LUKS container."));
    const QByteArray &encPass = gui.textCryptoPass->text().toUtf8();
    for (PartMan::Iterator it(partman); PartMan::Device *part = *it; it.next()) {
        if (!part->encrypt || !part->willFormat()) continue;
        proc.status(tr("Creating encrypted volume: %1").arg(part->name));
        proc.exec(u"cryptsetup"_s, {u"--batch-mode"_s, u"--key-size=512"_s,
            u"--hash=sha512"_s, u"luksFormat"_s, part->path}, &encPass);
        proc.status();
        if (!open(part, encPass)) {
            throw QT_TR_NOOP("Failed to open LUKS container.");
        }
    }
}
bool Crypto::open(PartMan::Device *part, const QByteArray &password, bool readonly) noexcept
{
    MProcess::Section sect(proc, nullptr);

    if (part->map.isEmpty()) {
        if (!proc.exec(u"cryptsetup"_s, {u"luksUUID"_s, part->path}, nullptr, true)) {
            return false;
        }
        part->map = "luks-" + proc.readAll().trimmed();
    }

    QStringList cargs({u"open"_s, part->path, part->map, u"--type"_s, u"luks"_s});
    if (readonly) {
        cargs.prepend(u"--readonly"_s);
    } else if (part->discgran) {
        cargs.prepend(u"--allow-discards"_s);
    }
    if (!proc.exec(u"cryptsetup"_s, cargs, &password)) return false;
    part->mapCount++;
    return true;
}
bool Crypto::close(PartMan::Device *volume) const noexcept
{
    bool ok = false;
    PartMan::Device *origin = volume->origin;
    if (origin) {
        // Volume is a LUKS container belonging to a partition.
        ok = proc.exec(u"cryptsetup"_s, {u"close"_s, volume->path});
        if (ok) {
            if (origin->mapCount > 0) {
                origin->mapCount--;
            }
            origin->map.clear();
            origin->addToCrypttab = false;
        }
        volume->origin = nullptr;
    } else if (volume->mapCount == 1) {
        // Volume is a partition containing one LUKS container.
        ok = proc.exec(u"cryptsetup"_s, {u"close"_s, volume->map});
        if (ok) {
            volume->mapCount--;
            volume->map.clear();
            volume->addToCrypttab = false;
        }
    }
    return ok;
}

// write out crypttab if encrypting for auto-opening
bool Crypto::makeCrypttab(const PartMan &partman) noexcept
{
    QList<const PartMan::Device *> cryptDevices;
    for (PartMan::Iterator it(partman); PartMan::Device *device = *it; it.next()) {
        if (device->type != PartMan::Device::PARTITION) continue;
        const bool required = device->addToCrypttab || device->encrypt || !device->map.isEmpty();
        if (!required) continue;
        QString mapName = device->map;
        QString luksUuid;
        if (mapName.startsWith(u"luks-"_s, Qt::CaseInsensitive)) {
            luksUuid = mapName.mid(5);
        }
        if (mapName.isEmpty() && (device->encrypt || device->addToCrypttab)) {
            if (proc.exec(u"cryptsetup"_s, {u"luksUUID"_s, device->path}, nullptr, true)) {
                luksUuid = proc.readOut(true).trimmed();
                if (!luksUuid.isEmpty()) mapName = u"luks-"_s + luksUuid;
            }
        }
        if (mapName.isEmpty()) continue;
        device->map = mapName;
        if (device->uuid.isEmpty() && !luksUuid.isEmpty()) device->uuid = luksUuid;
        cryptDevices.append(device);
    }
    if (cryptDevices.isEmpty()) {
        QFile file(u"/mnt/antiX/etc/crypttab"_s);
        file.open(QIODevice::WriteOnly | QIODevice::Truncate);
        return true;
    }
    const int cryptcount = cryptDevices.size();
    // Add devices to crypttab.
    QFile file(u"/mnt/antiX/etc/crypttab"_s);
    if (!file.open(QIODevice::WriteOnly)) return false;
    QTextStream out(&file);
    for (const PartMan::Device *device : std::as_const(cryptDevices)) {
        QString entryUuid = device->uuid;
        if (entryUuid.isEmpty() && device->map.startsWith(u"luks-"_s, Qt::CaseInsensitive)) {
            entryUuid = device->map.mid(5);
        }
        out << device->map << " UUID=" << entryUuid << " none luks";
        if (cryptcount > 1) out << ",keyscript=decrypt_keyctl";
        if (!device->flags.rotational) out << ",no-read-workqueue,no-write-workqueue";
        if (device->discgran) out << ",discard";
        out << '\n';
    }
    return true;
}
