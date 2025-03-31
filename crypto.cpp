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
#include "mprocess.h"
#include "msettings.h"
#include "partman.h"
#include "crypto.h"

Crypto::Crypto(MProcess &mproc, Ui::MeInstall &ui, QObject *parent)
    : QObject(parent), proc(mproc), gui(ui), pass(ui.textCryptoPass, ui.textCryptoPass2, 1, this)
{
    connect(&pass, &PassEdit::validationChanged, gui.pushNext, &QPushButton::setEnabled);

    QFileInfo cryptsetup("/usr/sbin/cryptsetup");
    QFileInfo crypsetupinitramfs("/usr/share/initramfs-tools/conf-hooks.d/cryptsetup");
    cryptsupport = (cryptsetup.exists() && cryptsetup.isExecutable() && crypsetupinitramfs.exists());
}

bool Crypto::manageConfig(MSettings &config) noexcept
{
    config.setSection("Encryption", gui.pageEncryption);
    if(!config.isSave()) {
        const QString &epass = config.getString("Pass");
        gui.textCryptoPass->setText(epass);
        gui.textCryptoPass2->setText(epass);
    }
    config.addFilter("Pass");
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
        proc.exec("cryptsetup", {"--batch-mode", "--key-size=512",
                                 "--hash=sha512", "luksFormat", part->path}, &encPass);
        proc.status();
        open(part, encPass);
    }
}
void Crypto::open(PartMan::Device *part, const QByteArray &password)
{
    MProcess::Section sect(proc, QT_TR_NOOP("Failed to open LUKS container."));
    if (part->map.isEmpty()) {
        proc.exec("cryptsetup", {"luksUUID", part->path}, nullptr, true);
        part->map = "luks-" + proc.readAll().trimmed();
    }
    QStringList cargs({"open", part->path, part->map, "--type", "luks"});
    if (part->discgran) cargs.prepend("--allow-discards");
    proc.exec("cryptsetup", cargs, &password);
}

// write out crypttab if encrypting for auto-opening
bool Crypto::makeCrypttab(const PartMan &partman) noexcept
{
    // Count the number of crypttab entries.
    int cryptcount = 0;
    for (PartMan::Iterator it(partman); PartMan::Device *device = *it; it.next()) {
        if (device->addToCrypttab) ++cryptcount;
    }
    // Add devices to crypttab.
    QFile file("/mnt/antiX/etc/crypttab");
    if (!file.open(QIODevice::WriteOnly)) return false;
    QTextStream out(&file);
    for (PartMan::Iterator it(partman); PartMan::Device *device = *it; it.next()) {
        if (!device->addToCrypttab) continue;
        out << device->map << " UUID=" << device->uuid << " none luks";
        if (cryptcount > 1) out << ",keyscript=decrypt_keyctl";
        if (!device->flags.rotational) out << ",no-read-workqueue,no-write-workqueue";
        if (device->discgran) out << ",discard";
        out << '\n';
    }
    return true;
}
