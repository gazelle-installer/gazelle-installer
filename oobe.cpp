/***************************************************************************
 * Out-of-Box Experience - GUI and related functions of the installer.
 *
 *   Copyright (C) 2022-2023 by AK-47, along with transplanted code:
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
 ***************************************************************************/

#include <utility>
#include <sys/stat.h>
#include <QDebug>
#include <QMessageBox>
#include <QFile>
#include <QFileInfo>
#include <QLocale>
#include <QTimeZone>
#include "mprocess.h"
#include "core.h"
#include "msettings.h"
#include "oobe.h"

using namespace Qt::Literals::StringLiterals;

Oobe::Oobe(MProcess &mproc, Core &mcore, Ui::MeInstall &ui, MIni &appConf, bool oem, bool modeOOBE, QObject *parent)
    : QObject(parent), proc(mproc), core(mcore), gui(ui), oem(oem), online(modeOOBE),
    passUser(ui.textUserPass, ui.textUserPass2, 0, this), passRoot(ui.textRootPass, ui.textRootPass2, 0, this)
{
    appConf.setSection(u"OOBE"_s);

    gui.textComputerName->setText(appConf.getString(u"DefaultHostName"_s));
    gui.boxRootAccount->setChecked(appConf.getBoolean(u"RootAccountDefault"_s));

    //hide save desktop changes checkbox, for pesky desktop environments
    if (!appConf.getBoolean(u"CanSaveDesktopChanges"_s, true)){
        gui.checkSaveDesktop->hide();
    }

    appConf.setSection();

    // User accounts
    connect(&passUser, &PassEdit::validationChanged, this, &Oobe::userPassValidationChanged);
    connect(&passRoot, &PassEdit::validationChanged, this, &Oobe::userPassValidationChanged);
    connect(gui.textUserName, &QLineEdit::textChanged, this, &Oobe::userPassValidationChanged);
    connect(gui.boxRootAccount, &QGroupBox::toggled, this, &Oobe::userPassValidationChanged);
    // Old home
    connect(gui.radioOldHomeUse, &QRadioButton::toggled, this, &Oobe::oldHomeToggled);
    connect(gui.radioOldHomeSave, &QRadioButton::toggled, this, &Oobe::oldHomeToggled);
    connect(gui.radioOldHomeDelete, &QRadioButton::toggled, this, &Oobe::oldHomeToggled);

    // timezone lists
    connect(gui.comboTimeArea, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &Oobe::timeAreaIndexChanged);

    //search top level zoneinfo folder rather than posix subfolder
    //posix subfolder doesn't existing in trixie/sid
    proc.exec(u"find"_s, {u"-L"_s, u"/usr/share/zoneinfo/"_s,
                u"-mindepth"_s, u"2"_s, u"-type"_s, u"f"_s,
                u"-not"_s, u"-path"_s, u"/usr/share/zoneinfo/posix/*"_s,
                u"-not"_s, u"-path"_s, u"/usr/share/zoneinfo/right/*"_s,
                u"-printf"_s, u"%P\\n"_s}, nullptr, true);

    timeZones = proc.readOutLines();

    gui.comboTimeArea->clear();

    for (const QString &zone : std::as_const(timeZones)) {
        const QString &area = zone.section('/', 0, 0);
        if (gui.comboTimeArea->findData(QVariant(area)) < 0) {
            QString text(area);
            if (area == "Indian"_L1 || area == "Pacific"_L1
                || area == "Atlantic"_L1 || area == "Arctic"_L1) {
                text.append(" Ocean");
            }
            gui.comboTimeArea->addItem(text, area);
        }
    }
    gui.comboTimeArea->model()->sort(0);
    // Guess if hardware clock is UTC or local time
    proc.shell(u"guess-hwclock"_s, nullptr, true);
    if (proc.readOut() == "localtime"_L1) gui.checkLocalClock->setChecked(true);

    // locale list
    connect(gui.comboLocale, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &Oobe::localeIndexChanged);
    gui.comboLocale->clear();
    proc.shell(u"locale -a | grep -Ev '^(C|POSIX)\\.?' | grep -E 'utf8|UTF-8'"_s, nullptr, true);
    QStringList loclist = proc.readOutLines();
    for (QString &strloc : loclist) {
        strloc.replace("utf8"_L1, "UTF-8"_L1, Qt::CaseInsensitive);
        QLocale loc(strloc);
        gui.comboLocale->addItem(loc.nativeTerritoryName() + " - "_L1 + loc.nativeLanguageName(), QVariant(strloc));
    }
    gui.comboLocale->model()->sort(0);
    // default locale selection
    int ixLocale = gui.comboLocale->findData(QVariant(QLocale::system().name() + ".UTF-8"_L1));
    if (gui.comboLocale->currentIndex() != ixLocale) gui.comboLocale->setCurrentIndex(ixLocale);
    else timeAreaIndexChanged(ixLocale);

    if (online) {
        gui.checkSaveDesktop->hide();
    } else {
        // Detect snapshot-backup account(s)
        // test if there's another user other than demo in /home,
        // indicating a possible snapshot or complicated live-usb
        haveSnapshotUserAccounts = proc.shell(u"ls -1 /home"
            " | grep -Ev '^(lost\\+found|demo|snapshot)$' | grep -q '[a-zA-Z0-9]'"_s);
        qDebug() << "check for possible snapshot:" << haveSnapshotUserAccounts;
    }

    //check for samba
    const bool hasInitSmb = QFileInfo(u"/etc/init.d/smbd"_s).exists();
    const bool hasSystemdSmb = QFileInfo(u"/usr/lib/systemd/system/smb.service"_s).exists()
        || QFileInfo(u"/usr/lib/systemd/system/smbd.service"_s).exists()
        || QFileInfo(u"/lib/systemd/system/smb.service"_s).exists();
    if (!hasInitSmb && !hasSystemdSmb) {
        gui.labelComputerGroup->setEnabled(false);
        gui.textComputerGroup->setEnabled(false);
        gui.textComputerGroup->clear();
        gui.checkSamba->setChecked(false);
        gui.checkSamba->setEnabled(false);
    }
    // check for the Samba server
    QString val;
    if (QFileInfo::exists(u"/usr/bin/dpkg"_s)) {
        proc.shell(u"dpkg -s samba | grep '^Status.*ok.*' | sed -e 's/.*ok //'"_s, nullptr, true);
        val = proc.readOut();
    } else if (QFileInfo::exists(u"/usr/bin/pacman"_s)) {
        proc.shell(u"pacman -Qi samba >/dev/null 2>&1 && echo installed || echo missing"_s, nullptr, true);
        val = proc.readOut();
    } else if (hasInitSmb || hasSystemdSmb) {
        val = u"installed"_s; // fall back to services present on non-Debian systems
    }
    haveSamba = (val.compare("installed"_L1, Qt::CaseInsensitive) == 0);

    buildServiceList(appConf);

    // Current user details
    curUser = getlogin();
    // Fall back to "demo" if running directly as root (TUI mode) or if getlogin() returns empty,
    // since using "root" as the template user would corrupt /etc/passwd and other system files
    // in the installed system by replacing all occurrences of "root" with the new username.
    if (curUser.isEmpty() || curUser == "root"_L1) {
        curUser = "demo"_L1;
    }
    // Using $HOME results in the wrong directory when run as root (with su).
    MProcess::Section sect(proc, nullptr);
    proc.shell("echo ~"_L1 + curUser, nullptr, true);
    curHome = proc.readOut(true);
    if (curHome.isEmpty()) {
        curHome = "/home/"_L1 + curUser;
    }
    qDebug() << "Current user:" << curUser << curHome;
}

void Oobe::manageConfig(MSettings &config) const noexcept
{
    const bool save = config.isSave();
    // Services page
    config.setSection(u"Services"_s, gui.boxServices);
    QTreeWidgetItemIterator it(gui.treeServices);
    while (*it) {
        if ((*it)->parent() != nullptr) {
            const QString &itext = (*it)->text(0);
            const bool checkval = ((*it)->checkState(0) == Qt::Checked);
            if (save) config.setBoolean(itext, checkval);
            else {
                const bool val = config.getBoolean(itext, checkval);
                (*it)->setCheckState(0, val ? Qt::Checked : Qt::Unchecked);
            }
        }
        ++it;
    }

    // Network page
    config.setSection(u"Network"_s, gui.pageNetwork);
    config.manageLineEdit(u"ComputerName"_s, gui.textComputerName);
    config.manageLineEdit(u"Domain"_s, gui.textComputerDomain);
    config.manageLineEdit(u"Workgroup"_s, gui.textComputerGroup);
    config.manageCheckBox(u"Samba"_s, gui.checkSamba);

    // Localization page
    config.setSection(u"Localization"_s, gui.pageLocalization);
    config.manageComboBox(u"Locale"_s, gui.comboLocale, true);
    config.manageCheckBox(u"LocalClock"_s, gui.checkLocalClock);
    static constexpr const char *clockChoices[] = {"24", "12"};
    QRadioButton *const clockRadios[] = {gui.radioClock24, gui.radioClock12};
    config.manageRadios(u"ClockHours"_s, 2, clockChoices, clockRadios);
    if (save) {
        config.setString(u"Timezone"_s, gui.comboTimeZone->currentData().toString());
    } else {
        const int rc = selectTimeZone(config.getString(u"Timezone"_s, QTimeZone::systemTimeZoneId()));
        if (rc == 1) config.markBadWidget(gui.comboTimeArea);
        else if (rc == 2) config.markBadWidget(gui.comboTimeZone);
    }
    config.manageComboBox(u"Timezone"_s, gui.comboTimeZone, true);

    // User Accounts page
    if (!haveSnapshotUserAccounts) {
        config.setSection(u"User"_s, gui.pageUserAccounts);
        config.manageLineEdit(u"Username"_s, gui.textUserName);
        config.manageCheckBox(u"Autologin"_s, gui.checkAutoLogin);
        config.manageCheckBox(u"SaveDesktop"_s, gui.checkSaveDesktop);
        if (online) {
            gui.checkSaveDesktop->setCheckState(Qt::Unchecked);
        }
        static constexpr const char *oldHomeActions[] = {"Use", "Save", "Delete"};
        QRadioButton *const oldHomeRadios[] = {gui.radioOldHomeUse, gui.radioOldHomeSave, gui.radioOldHomeDelete};
        config.manageRadios(u"OldHomeAction"_s, 3, oldHomeActions, oldHomeRadios);
        config.manageGroupCheckBox(u"EnableRoot"_s, gui.boxRootAccount);
        config.addFilter(u"UserPass"_s);
        config.addFilter(u"RootPass"_s);
        if (!save) {
            const QString &upass = config.getString(u"UserPass"_s);
            gui.textUserPass->setText(upass);
            gui.textUserPass2->setText(upass);
            const QString &rpass = config.getString(u"RootPass"_s);
            gui.textRootPass->setText(rpass);
            gui.textRootPass2->setText(rpass);
        }
    }
}

void Oobe::enable() const
{
    MProcess::Section sect(proc);
    sect.setRoot("/mnt/antiX");
    QTreeWidgetItemIterator it(gui.treeServices);
    //comment for now, don't disable services
    //for (; *it; ++it) {
        //if ((*it)->parent()) setService((*it)->text(0), false); // Speed up the OOBE boot.
    //}
    //setService(u"smbd"_s, false);
    //setService(u"nmbd"_s, false);
    //setService(u"samba-ad-dc"_s, false);
    const bool haveUpdateRc = QFileInfo(u"/usr/bin/update-rc.d"_s).isExecutable()
        || QFileInfo(u"/usr/sbin/update-rc.d"_s).isExecutable()
        || QFileInfo(u"/sbin/update-rc.d"_s).isExecutable();
    if (haveUpdateRc) {
        proc.exec(u"update-rc.d"_s, {u"-f"_s, u"oobe"_s, u"defaults"_s});
    }
}

void Oobe::process() const
{
    if (!oem) {
        MProcess::Section sect(proc);
        if (!online) sect.setRoot("/mnt/antiX");

        QTreeWidgetItemIterator it(gui.treeServices);
        for (; *it; ++it) {
            if ((*it)->parent()) {
                core.setService((*it)->text(0), (*it)->checkState(0) == Qt::Checked);
            }
        }
        if (haveSamba) {
            const bool enable = gui.checkSamba->isChecked();
            core.setService(u"smbd"_s, enable);
            core.setService(u"nmbd"_s, enable);
            core.setService(u"samba-ad-dc"_s, enable);
        }
        sect.setRoot(nullptr);
        setComputerName();
        setLocale();
    }
    if (haveSnapshotUserAccounts) {
        // Copy the whole /home/ directory for snapshot accounts.
        proc.exec(u"rsync"_s, {u"-a"_s, u"/home/"_s, u"/mnt/antiX/home/"_s,
            u"--exclude"_s, u".cache"_s, u"--exclude"_s, u".gvfs"_s, u"--exclude"_s, u".dbus"_s,
            u"--exclude"_s, u".Xauthority"_s, u"--exclude"_s, u".ICEauthority"_s,
            u"--exclude"_s, u".config/session"_s});
    } else if (!oem) {
        setUserInfo();
    }
    if (online) {
        proc.shell(u"sed -i 's/nosplash\\b/splash/g' /boot/grub/grub.cfg"_s);
        const bool haveUpdateRc = QFileInfo(u"/usr/bin/update-rc.d"_s).isExecutable()
            || QFileInfo(u"/usr/sbin/update-rc.d"_s).isExecutable()
            || QFileInfo(u"/sbin/update-rc.d"_s).isExecutable();
        if (haveUpdateRc) {
            proc.exec(u"update-rc.d"_s, {u"oobe"_s, u"disable"_s});
        }
    }
}

/* Services */

void Oobe::buildServiceList(MIni &appconf) noexcept
{
    MIni services_desc(u"/usr/share/gazelle-installer-data/services.list"_s, MIni::ReadOnly);

    appconf.setSection(u"Services"_s);
    for (const QStringList &keys = appconf.getKeys(); const QString &service : keys) {
        const QString &lang = QLocale::system().bcp47Name().toLower();
        services_desc.setSection(lang);
        if (!services_desc.contains(service)) {
            services_desc.setSection(QString()); // Use English definition
        }
        QStringList list = services_desc.getString(service).split(',');
        if (list.size() != 2) continue;
        const QString &category = list.at(0).trimmed();
        const QString &description = list.at(1).trimmed();

        const bool hasService = QFile::exists("/etc/init.d/"_L1 + service)
            || QFile::exists("/etc/sv/"_L1 + service)
            || QFile::exists("/usr/lib/systemd/system/"_L1 + service + ".service"_L1)
            || QFile::exists("/lib/systemd/system/"_L1 + service + ".service"_L1)
            || QFile::exists("/etc/systemd/system/"_L1 + service + ".service"_L1);
        if (hasService) {
            QList<QTreeWidgetItem *> found_items = gui.treeServices->findItems(category, Qt::MatchExactly, 0);
            QTreeWidgetItem *parent;
            if (found_items.size() == 0) { // add top item if no top items found
                parent = new QTreeWidgetItem(gui.treeServices);
                parent->setFirstColumnSpanned(true);
                parent->setText(0, category);
            } else {
                parent = found_items.last();
            }
            QTreeWidgetItem *item = new QTreeWidgetItem(parent);
            item->setText(0, service);
            item->setText(1, description);
            item->setCheckState(0, appconf.getBoolean(service) ? Qt::Checked : Qt::Unchecked);
        }
    }
    appconf.setSection();

    gui.treeServices->expandAll();
    gui.treeServices->resizeColumnToContents(0);
}

void Oobe::stashServices(bool save) const noexcept
{
    QTreeWidgetItemIterator it(gui.treeServices);
    while (*it) {
        if ((*it)->parent() != nullptr) {
            // Use an invisible column as a service check state cache.
            (*it)->setCheckState(save?2:0, (*it)->checkState(save?0:2));
        }
        ++it;
    }
}

QWidget *Oobe::validateComputerName() const noexcept
{
    QMessageBox msgbox(gui.boxMain);
    msgbox.setIcon(QMessageBox::Critical);

    static const QRegularExpression nametest(u"[^0-9a-zA-Z-.]|^[.-]|[.-]$|\\.\\."_s);
    // see if name is reasonable
    if (gui.textComputerName->text().isEmpty()) {
        msgbox.setText(tr("Please enter a computer name."));
        msgbox.exec();
        return gui.textComputerName;
    } else if (gui.textComputerName->text().contains(nametest)) {
        msgbox.setText(tr("The computer name contains invalid characters."));
        msgbox.setInformativeText(tr("Please choose a different name before proceeding."));
        msgbox.exec();
        return gui.textComputerName;
    }
    // see if name is reasonable
    if (gui.textComputerDomain->text().isEmpty()) {
        msgbox.setText(tr("Please enter a domain name."));
        msgbox.exec();
        return gui.textComputerDomain;
    } else if (gui.textComputerDomain->text().contains(nametest)) {
        msgbox.setText(tr("The computer domain contains invalid characters."));
        msgbox.setInformativeText(tr("Please choose a different name before proceeding."));
        msgbox.exec();
        return gui.textComputerDomain;
    }

    if (haveSamba) {
        // see if name is reasonable
        if (gui.textComputerGroup->text().isEmpty()) {
            msgbox.setText(tr("Please enter a workgroup."));
            msgbox.exec();
            return gui.textComputerGroup;
        }
    } else {
        gui.textComputerGroup->clear();
    }

    return nullptr;
}

// set the computer name, can not be rerun
void Oobe::setComputerName() const
{
    QString etcpath = online ? u"/etc"_s : u"/mnt/antiX/etc"_s;
    const auto isArchTarget = [etcpath]() -> bool {
        QFile file(etcpath + "/os-release"_L1);
        if (!file.open(QFile::ReadOnly | QFile::Text)) {
            return false;
        }
        while (!file.atEnd()) {
            QString line = QString::fromUtf8(file.readLine()).trimmed();
            if (!line.startsWith("ID="_L1) && !line.startsWith("ID_LIKE="_L1)) {
                continue;
            }
            QString value = line.section('=', 1).trimmed();
            value.remove('"');
            if (value == "arch"_L1 || value.contains("arch"_L1)) {
                return true;
            }
        }
        return false;
    };
    const bool archTarget = isArchTarget();
    if (haveSamba) {
        //replaceStringInFile(PROJECTSHORTNAME + "1", textComputerName->text(), "/mnt/antiX/etc/samba/smb.conf");
        replaceStringInFile(u"WORKGROUP"_s, gui.textComputerGroup->text(), etcpath + "/samba/smb.conf"_L1);
    }
    //replaceStringInFile(PROJECTSHORTNAME + "1", textComputerName->text(), "/mnt/antiX/etc/hosts");
    const QString &compname = gui.textComputerName->text();
    const QString &domainname = gui.textComputerDomain->text();
    const QString hostsPath = etcpath + "/hosts"_L1;
    const QString hostEntry = domainname.isEmpty()
        ? compname
        : compname + "."_L1 + domainname + " "_L1 + compname;
    if (QFileInfo::exists(hostsPath)) {
        proc.shell("sed -E -i 's/^127\\.0\\.1\\.1\\s+.*/127.0.1.1    "_L1
            + hostEntry + "/' "_L1 + hostsPath);
        proc.shell("grep -qE '^127\\.0\\.1\\.1\\s' "_L1 + hostsPath
            + " || echo \"127.0.1.1    "_L1 + hostEntry + "\" >> "_L1 + hostsPath);
    }
    proc.shell("echo \""_L1 + compname + "\" | cat > "_L1 + etcpath + "/hostname"_L1);
    if (!archTarget) {
        proc.shell("echo \""_L1 + compname + "\" | cat > "_L1 + etcpath + "/mailname"_L1);
    }
    const QString dhclientConf = etcpath + "/dhcp/dhclient.conf"_L1;
    if (QFileInfo::exists(dhclientConf)) {
        proc.shell("sed -i 's/.*send host-name.*/send host-name \""_L1
            + compname + "\";/g' "_L1 + dhclientConf);
    } else {
        qDebug() << "Skip dhclient.conf update (missing):" << dhclientConf;
    }
    if (!archTarget) {
        proc.shell("echo \""_L1 + domainname + "\" | cat > "_L1 + etcpath + "/defaultdomain"_L1);
    }
}

// return 0 = success, 1 = bad area, 2 = bad zone
int Oobe::selectTimeZone(const QString &zone) const noexcept
{
    int index = gui.comboTimeArea->findData(QVariant(zone.section('/', 0, 0)));
    if (index < 0) return 1;
    gui.comboTimeArea->setCurrentIndex(index);
    qApp->processEvents();
    index = gui.comboTimeZone->findData(QVariant(zone));
    if (index < 0) return 2;
    gui.comboTimeZone->setCurrentIndex(index);
    return 0;
}

void Oobe::setLocale() const
{
    proc.log(__PRETTY_FUNCTION__, MProcess::LOG_MARKER);

    //locale
    qDebug() << "Update locale";
    const QString langSetting = gui.comboLocale->currentData().toString();
    if (!langSetting.isEmpty()) {
        QString updateLocale = u"/usr/bin/update-locale"_s;
        if (!QFileInfo(updateLocale).isExecutable()) {
            updateLocale = u"/usr/sbin/update-locale"_s;
        }
        QString cmd;
        if (!online) cmd = "chroot /mnt/antiX "_L1;
        if (QFileInfo(updateLocale).isExecutable()) {
            cmd += QStringLiteral("%1 \"LANG=%2\"").arg(updateLocale, langSetting);
        } else {
            // Arch: fall back to locale.gen update + locale-gen
            const QString localeGen = (online ? QStringLiteral("locale-gen") : QStringLiteral("chroot /mnt/antiX locale-gen"));
            const QString localeGenFile = (online ? QStringLiteral("/etc/locale.gen") : QStringLiteral("/mnt/antiX/etc/locale.gen"));
            proc.shell(QStringLiteral("sed -i 's/^#*\\(%1\\)/\\1/' %2").arg(langSetting, localeGenFile));
            cmd = localeGen;
        }
        proc.shell(cmd);
    }

    //set paper size based on locale
    QString papersize;
    QStringList letterpapersizedefualt = {u"en_US"_s, u"es_BO"_s, u"es_CO"_s,
        u"es_MX"_s, u"es_NI"_s, u"es_PA"_s, u"es_US"_s, u"es_VE"_s, u"fr_CA"_s, u"en_CA"_s };
    if (containsAnySubstring(gui.comboLocale->currentData().toString(), letterpapersizedefualt)) {
        papersize = "letter"_L1;
    } else {
        papersize = "a4"_L1;
    }

    if (!online) {
        proc.shell("echo "_L1 + papersize + " >/mnt/antiX/etc/papersize"_L1);
    } else {
        proc.shell("echo "_L1 + papersize + " >/etc/papersize"_L1);
    }

    const QString &selTimeZone = gui.comboTimeZone->currentData().toString();

    // /etc/localtime is either a file or a symlink to a file in /usr/share/zoneinfo. Use the one selected by the user.
    //replace with link
    if (!online) {
        proc.exec(u"ln"_s, {u"-nfs"_s, "/usr/share/zoneinfo/"_L1 + selTimeZone, u"/mnt/antiX/etc/localtime"_s});
    }
    proc.exec(u"ln"_s, {u"-nfs"_s, "/usr/share/zoneinfo/"_L1 + selTimeZone, u"/etc/localtime"_s});
    // /etc/timezone is text file with the timezone written in it. Write the user-selected timezone in it now.
    if (!online) {
        proc.shell("echo "_L1 + selTimeZone + " > /mnt/antiX/etc/timezone"_L1);
    }
    proc.shell("echo "_L1 + selTimeZone + " > /etc/timezone"_L1);

    // Set clock to use LOCAL
    if (gui.checkLocalClock->isChecked()) {
        proc.shell(u"echo '0.0 0 0.0\n0\nLOCAL' > /etc/adjtime"_s);
    } else {
        proc.shell(u"echo '0.0 0 0.0\n0\nUTC' > /etc/adjtime"_s);
    }
    proc.exec(u"hwclock"_s, {u"--hctosys"_s});
    if (!online) {
        proc.exec(u"cp"_s, {u"-f"_s, u"/etc/adjtime"_s, u"/mnt/antiX/etc/"_s});
        if (QFileInfo::exists(u"/etc/default/rcS"_s)) {
            proc.exec(u"cp"_s, {u"-f"_s, u"/etc/default/rcS"_s, u"/mnt/antiX/etc/default"_s});
        }
    }

    // Set clock format
    setUserClockFormat(online ? u"/etc/skel"_s : u"/mnt/antiX/etc/skel"_s);

    // localize repo
    //qDebug() << "Localize repo";
    //if (online) proc.exec(u"localize-repo"_s, {u"default"_s});
    //else proc.exec(u"chroot"_s, {u"/mnt/antiX"_s, u"localize-repo"_s, u"default"_s});

    //machine id
    if (online){
        // create a /etc/machine-id file and /var/lib/dbus/machine-id file
        proc.exec(u"rm"_s, {u"/var/lib/dbus/machine-id"_s, u"/etc/machine-id"_s});
        proc.exec(u"dbus-uuidgen"_s, {u"--ensure=/etc/machine-id"_s});
        proc.exec(u"dbus-uuidgen"_s, {u"--ensure"_s});
    }
}
void Oobe::setUserClockFormat(const QString &skelpath) const noexcept
{
    MProcess::Section sect(proc, nullptr);
    auto sedIfExists = [this](const QString &cmd, const QString &path) {
        if (QFileInfo::exists(path)) proc.shell(cmd + path);
    };

    if (gui.radioClock12->isChecked()) {
        //mx systems
        sedIfExists("sed -i '/data0=/c\\data0=%l:%M' "_L1, skelpath + "/.config/xfce4/panel/xfce4-orageclock-plugin-1.rc"_L1);
        sedIfExists("sed -i '/time_format=/c\\time_format=%l:%M' "_L1, skelpath + "/.config/xfce4/panel/datetime-1.rc"_L1);
        sedIfExists("sed -i 's/%H/%l/' "_L1, skelpath + "/.config/xfce4/xfconf/xfce-perchannel-xml/xfce4-panel.xml"_L1);

        //mx kde
        sedIfExists("sed -i '/use24hFormat=/c\\use24hFormat=0' "_L1, skelpath + "/.config/plasma-org.kde.plasma.desktop-appletsrc"_L1);

        //mx fluxbox
        sedIfExists("sed -i '/time1_format/c\\time1_format=%l:%M' "_L1, skelpath + "/.config/tint2/tint2rc"_L1);

        //antix systems
        sedIfExists("sed -i 's/%H:%M/%l:%M/g' "_L1, skelpath + "/.icewm/preferences"_L1);
        sedIfExists("sed -i 's/%k:%M/%l:%M/g' "_L1, skelpath + "/.fluxbox/init"_L1);
        sedIfExists("sed -i 's/%H:%M/%l:%M/g' "_L1, skelpath + "/.jwm/tray"_L1);
    } else {
        //mx systems
        sedIfExists("sed -i '/data0=/c\\data0=%H:%M' "_L1, skelpath + "/.config/xfce4/panel/xfce4-orageclock-plugin-1.rc"_L1);
        sedIfExists("sed -i '/time_format=/c\\time_format=%H:%M' "_L1, skelpath + "/.config/xfce4/panel/datetime-1.rc"_L1);
        sedIfExists("sed -i 's/%l/%H/' "_L1, skelpath + "/.config/xfce4/xfconf/xfce-perchannel-xml/xfce4-panel.xml"_L1);

        //mx kde
        sedIfExists("sed -i '/use24hFormat=/c\\use24hFormat=2' "_L1, skelpath + "/.config/plasma-org.kde.plasma.desktop-appletsrc"_L1);

        //mx fluxbox
        sedIfExists("sed -i '/time1_format/c\\time1_format=%H:%M' "_L1, skelpath + "/.config/tint2/tint2rc"_L1);

        //antix systems
        sedIfExists("sed -i 's/%H:%M/%H:%M/g' "_L1, skelpath + "/.icewm/preferences"_L1);
        sedIfExists("sed -i 's/%k:%M/%k:%M/g' "_L1, skelpath + "/.fluxbox/init"_L1);
        sedIfExists("sed -i 's/%H:%M/%k:%M/g' "_L1, skelpath + "/.jwm/tray"_L1);
    }
}

QWidget *Oobe::validateUserInfo(bool automatic) const noexcept
{
    QMessageBox msgbox(gui.boxMain);
    msgbox.setIcon(QMessageBox::Critical);
    msgbox.setInformativeText(tr("Please choose a different name before proceeding."));
    const QString &userName = gui.textUserName->text();
    // see if username is reasonable length
    static const QRegularExpression usertest(u"^[a-zA-Z_][a-zA-Z0-9_-]*[$]?$"_s);
    if (!userName.contains(usertest)) {
        msgbox.setText(tr("The user name cannot contain special characters or spaces."));
        msgbox.exec();
        return gui.textUserName;
    }
    // check that user name is not already used
    QFile file(u"/etc/passwd"_s);
    if (file.open(QFile::ReadOnly | QFile::Text)) {
        const QByteArray &match = QStringLiteral("%1:").arg(userName).toUtf8();
        while (!file.atEnd()) {
            if (file.readLine().startsWith(match)) {
                msgbox.setText(tr("The chosen user name is in use."));
                msgbox.exec();
                return gui.textUserName;
            }
        }
    }

    if (!automatic) {
        msgbox.setIcon(QMessageBox::Warning);
        msgbox.setInformativeText(tr("Are you sure you want to continue?"));
        msgbox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
        msgbox.setDefaultButton(QMessageBox::No);
        if (passUser.length()==0) {
            // Confirm that an empty user password is not accidental.
            msgbox.setText(tr("You did not provide a passphrase for %1.").arg(gui.textUserName->text()));
            if (msgbox.exec() != QMessageBox::Yes) return gui.textUserPass;
        }
        if (gui.boxRootAccount->isChecked() && passRoot.length()==0) {
            // Confirm that an empty root password is not accidental.
            msgbox.setText(tr("You did not provide a password for the root account."));
            if (msgbox.exec() != QMessageBox::Yes) return gui.textRootPass;
        }
    }

    return nullptr;
}

// setup the user, cannot be rerun
void Oobe::setUserInfo() const
{
    // set the user passwords first
    bool ok = true;
    MProcess::Section sect(proc, QT_TR_NOOP("Failed to set user account passwords."));
    if (!online) sect.setRoot("/mnt/antiX");
    const QString &userPass = gui.textUserPass->text();
    QByteArray userinfo;
    if (!gui.boxRootAccount->isChecked()) {
        proc.exec(u"passwd"_s, {u"-l"_s, u"root"_s});
    } else {
        const QString &rootPass = gui.textRootPass->text();
        if (rootPass.isEmpty()) proc.exec(u"passwd"_s, {u"-d"_s, u"root"_s});
        else userinfo.append(QString("root:"_L1 + rootPass).toUtf8());
    }
    if (userPass.isEmpty()) {
        proc.exec(u"passwd"_s, {u"-d"_s, curUser});
    } else {
        if (!userinfo.isEmpty()) userinfo.append('\n');
        userinfo.append(QString(curUser + ':' + userPass).toUtf8());
    }
    if (!userinfo.isEmpty()) {
        proc.exec(u"chpasswd"_s, {}, &userinfo);
    }
    sect.setRoot(nullptr);

    QString rootpath;
    if (!online) rootpath = "/mnt/antiX"_L1;
    QString skelpath = rootpath + "/etc/skel"_L1;
    const QString &username = gui.textUserName->text();
    QString dpath = rootpath + "/home/"_L1 + username;
    //ensure home exists
    proc.exec(u"mkdir"_s,{u"-p"_s, rootpath + "/home/"_L1});

    if (QFileInfo::exists(dpath)) {
        if (gui.radioOldHomeSave->isChecked()) {
            sect.setExceptionMode(QT_TR_NOOP("Failed to save old home directory."));
            for (int ixi = 1; ; ++ixi) {
                const QString &dest(dpath + ".00"_L1 + QString::number(ixi));
                if (!QFileInfo::exists(dest)) {
                    proc.exec(u"mv"_s, {u"-f"_s, dpath, dest});
                    break;
                }
            }
        } else if (gui.radioOldHomeDelete->isChecked()) {
            sect.setExceptionMode(QT_TR_NOOP("Failed to delete old home directory."));
            proc.exec(u"rm"_s, {u"-rf"_s, dpath});
        }
        proc.exec(u"sync"_s); // The sync(2) system call will block the GUI.
    }

    // check the linuxfs squashfs for a home/demo folder, which indicates a remaster perserving /home.
    const bool remasteredUserHome = QFileInfo("/live/linux"_L1 + curHome).isDir();
    qDebug() << "check for remastered home folder:" << remasteredUserHome;

    if (QFileInfo::exists(dpath.toUtf8())) { // Still exists.
        sect.setExceptionMode(nullptr);
        proc.exec(u"cp"_s, {u"-n"_s, skelpath + "/.bash_profile"_L1, dpath});
        proc.exec(u"cp"_s, {u"-n"_s, skelpath + "/.bashrc"_L1, dpath});
        proc.exec(u"cp"_s, {u"-n"_s, skelpath + "/.gtkrc"_L1, dpath});
        proc.exec(u"cp"_s, {u"-n"_s, skelpath + "/.gtkrc-2.0"_L1, dpath});
        proc.exec(u"cp"_s, {u"-Rn"_s, skelpath + "/.config"_L1, dpath});
        proc.exec(u"cp"_s, {u"-Rn"_s, skelpath + "/.local"_L1, dpath});
    } else { // dir does not exist, must create it
        // Copy skel to demo, unless demo folder exists in remastered linuxfs.
        if (!remasteredUserHome) {
            sect.setExceptionMode(QT_TR_NOOP("Sorry, failed to create user directory."));
            proc.exec(u"cp"_s, {u"-a"_s, skelpath, dpath});
        } else { // still rename the demo directory even if remastered demo home folder is detected
            sect.setExceptionMode(QT_TR_NOOP("Sorry, failed to name user directory."));
            proc.exec(u"mv"_s, {u"-f"_s, rootpath + curHome, dpath});
        }
    }
    setUserClockFormat(dpath);

    sect.setExceptionMode(nullptr);

    // saving Desktop changes
    if (gui.checkSaveDesktop->isChecked()) {
        resetBlueman();
        // rsync home folder
        QString cmd = ("rsync -a --info=name1 %1/ %2"
            " --exclude '.cache' --exclude '.gvfs' --exclude '.dbus' --exclude '.Xauthority' --exclude '.ICEauthority'"
            " --exclude '.mozilla' --exclude 'Installer.desktop' --exclude 'minstall.desktop' --exclude 'Desktop/antixsources.desktop'"
            " --exclude '.idesktop/gazelle.lnk' --exclude '.jwm/menu' --exclude '.icewm/menu' --exclude '.fluxbox/menu'"
            " --exclude '.config/rox.sourceforge.net/ROX-Filer/pb_antiX-fluxbox'"
            " --exclude '.config/rox.sourceforge.net/ROX-Filer/pb_antiX-icewm'"
            " --exclude '.config/rox.sourceforge.net/ROX-Filer/pb_antiX-jwm'"
            " --exclude '.config/session' | xargs -I '$' sed -i 's|%1|/home/"_L1
            + username + "|g' %2/$"_L1).arg(curHome, dpath);
        proc.shell(cmd);
    }

    // check if remaster exists and checkSaveDesktop not checked, modify the remastered demo folder
    if (remasteredUserHome && !gui.checkSaveDesktop->isChecked())
    {
        resetBlueman();
        // Change remaster "demo" to new user.
        QString cmd = ("find "_L1 + dpath + " -maxdepth 1 -type f -name '.*' -print0 | xargs -0 sed -i 's|"_L1 + curHome + "|/home/"_L1 + username + "|g'"_L1).arg(dpath);
        proc.shell(cmd);
        cmd = ("find "_L1 + dpath + "/.config -type f -print0 | xargs -0 sed -i 's|"_L1 + curHome + "|/home/"_L1 + username + "|g'"_L1).arg(dpath);
        proc.shell(cmd);
        cmd = ("find "_L1 + dpath + "/.local -type f  -print0 | xargs -0 sed -i 's|"_L1 + curHome + "|/home/"_L1 + username + "|g'"_L1).arg(dpath);
        proc.shell(cmd);
    }

    // fix the ownership, demo=newuser
    sect.setExceptionMode(QT_TR_NOOP("Failed to set ownership or permissions of user directory."));
    proc.exec(u"chown"_s, {u"-R"_s, curUser + ':' + curUser, dpath});
    // Set permissions according to /etc/adduser.conf
    const MIni addusercfg(u"/etc/adduser.conf"_s, MIni::ReadOnly);
    mode_t mode = addusercfg.getString(u"DIR_MODE"_s, u"0700"_s).toUInt(&ok, 8);
    if (chmod(dpath.toUtf8().constData(), mode) != 0) throw sect.failMessage();

    // change in files
    replaceStringInFile(curUser, username, rootpath + "/etc/group"_L1);
    replaceStringInFile(curUser, username, rootpath + "/etc/gshadow"_L1);
    replaceStringInFile(curUser, username, rootpath + "/etc/passwd"_L1);
    replaceStringInFile(curUser, username, rootpath + "/etc/shadow"_L1);
    replaceStringInFile(curUser, username, rootpath + "/etc/subuid"_L1);
    replaceStringInFile(curUser, username, rootpath + "/etc/subgid"_L1);
    replaceStringInFile(curUser, username, rootpath + "/etc/slim.conf"_L1);
    replaceStringInFile(curUser, username, rootpath + "/etc/slimski.local.conf"_L1);
    replaceStringInFile(curUser, username, rootpath + "/etc/lightdm/lightdm.conf"_L1);
    replaceStringInFile(curUser, username, rootpath + "/etc/plasmalogin.conf.d/autologin.conf"_L1);
    replaceStringInFile(curUser, username, rootpath + "/home/*/.gtkrc-2.0"_L1);
    replaceStringInFile(curUser, username, rootpath + "/root/.gtkrc-2.0"_L1);
    if (gui.checkAutoLogin->isChecked()) {
        replaceStringInFile(u"#auto_login"_s, u"auto_login"_s, rootpath + "/etc/slim.conf"_L1);
        replaceStringInFile(u"#default_user "_s, u"default_user "_s, rootpath + "/etc/slim.conf"_L1);
        replaceStringInFile(u"#autologin_enabled"_s, u"autologin_enabled"_s, rootpath + "/etc/slimski.local.conf"_L1);
        replaceStringInFile(u"#default_user "_s, u"default_user "_s, rootpath + "/etc/slimski.local.conf"_L1);
        replaceStringInFile(u"User="_s, "User="_L1 + username, rootpath + "/etc/sddm.conf"_L1);
        replaceStringInFile(u"User="_s, "User="_L1 + username, rootpath + "/etc/plasmalogin.conf.d/autologin.conf"_L1);
    }
    else {
        replaceStringInFile(u"auto_login"_s, u"#auto_login"_s, rootpath + "/etc/slim.conf"_L1);
        replaceStringInFile(u"default_user "_s, u"#default_user "_s, rootpath + "/etc/slim.conf"_L1);
        replaceStringInFile(u"autologin_enabled"_s, u"#autologin_enabled"_s, rootpath + "/etc/slimski.local.conf"_L1);
        replaceStringInFile(u"default_user "_s, u"#default_user "_s, rootpath + "/etc/slimski.local.conf"_L1);
        replaceStringInFile(u"autologin-user="_s, u"#autologin-user="_s, rootpath + "/etc/lightdm/lightdm.conf"_L1);
        replaceStringInFile(u"User=.*"_s, u"User="_s, rootpath + "/etc/sddm.conf"_L1);
        replaceStringInFile(u"User=.*"_s, u"User="_s, rootpath + "/etc/plasmalogin.conf.d/autologin.conf"_L1);
    }
    proc.exec(u"touch"_s, {rootpath + "/var/mail/"_L1 + username});
}

void Oobe::resetBlueman() const
{
    proc.exec(u"runuser"_s, {u"-l"_s, curUser, u"-c"_s, u"dconf reset /org/blueman/transfer/shared-path"_s}); //reset blueman path
}

/* Slots */

void Oobe::localeIndexChanged(int index) const noexcept
{
    // riot control
    QLocale locale(gui.comboLocale->itemData(index).toString());
    if (locale.timeFormat().startsWith('h')) gui.radioClock12->setChecked(true);
    else gui.radioClock24->setChecked(true);
}

void Oobe::timeAreaIndexChanged(int index) const noexcept
{
    if (index < 0 || index >= gui.comboTimeArea->count()) return;
    const QString &area = gui.comboTimeArea->itemData(index).toString();
    gui.comboTimeZone->clear();
    for (const QString &zone : std::as_const(timeZones)) {
        if (zone.startsWith(area)) {
            QString text(QString(zone).section('/', 1));
            text.replace('_', ' ');
            gui.comboTimeZone->addItem(text, QVariant(zone));
        }
    }
    gui.comboTimeZone->model()->sort(0);
}

void Oobe::userPassValidationChanged() const noexcept
{
    bool ok = !gui.textUserName->text().isEmpty();
    if (ok) ok = passUser.valid() || gui.textUserName->text().isEmpty();
    if (ok && gui.boxRootAccount->isChecked()) {
        ok = passRoot.valid() || gui.textRootPass->text().isEmpty();
    }
    gui.pushNext->setEnabled(ok);
}

void Oobe::oldHomeToggled() const noexcept
{
    gui.pushNext->setEnabled(gui.radioOldHomeUse->isChecked()
        || gui.radioOldHomeSave->isChecked() || gui.radioOldHomeDelete->isChecked());
}

/* Helpers*/

bool Oobe::replaceStringInFile(const QString &oldtext, const QString &newtext, const QString &filepath) const noexcept
{
    MProcess::Section sect(proc, nullptr);
    // Skip silently if target file does not exist (common across differing DEs/DMs).
    if (!QFileInfo::exists(filepath)) return true;
    return proc.exec(u"sed"_s, {u"-i"_s, QStringLiteral("s/%1/%2/g").arg(oldtext, newtext), filepath});
}

bool Oobe::containsAnySubstring(const QString& mainString, const QStringList& substrings) const noexcept {
    for (const QString& substring : substrings) {
        if (mainString.contains(substring)) {
            return true;
        }
    }
    return false;
}
