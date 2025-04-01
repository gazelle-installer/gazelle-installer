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
#include <QFileInfo>
#include <QLocale>
#include <QTimeZone>
#include "mprocess.h"
#include "core.h"
#include "msettings.h"
#include "oobe.h"

Oobe::Oobe(MProcess &mproc, Core &mcore, Ui::MeInstall &ui, MIni &appConf, bool oem, bool modeOOBE, QObject *parent)
    : QObject(parent), proc(mproc), core(mcore), gui(ui), oem(oem), online(modeOOBE),
    passUser(ui.textUserPass, ui.textUserPass2, 0, this), passRoot(ui.textRootPass, ui.textRootPass2, 0, this)
{
    appConf.setSection("OOBE");

    gui.textComputerName->setText(appConf.getString("DefaultHostName"));
    gui.boxRootAccount->setChecked(appConf.getBoolean("RootAccountDefault"));

    //hide save desktop changes checkbox, for pesky desktop environments
    if (!appConf.getBoolean("CanSaveDesktopChanges", true)){
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
    proc.exec("find", {"-L", "/usr/share/zoneinfo/",
                "-mindepth", "2", "-type", "f",
                "-not", "-path", "/usr/share/zoneinfo/posix/*",
                "-not", "-path", "/usr/share/zoneinfo/right/*",
                "-printf", "%P\\n"}, nullptr, true);

    timeZones = proc.readOutLines();

    gui.comboTimeArea->clear();

    for (const QString &zone : std::as_const(timeZones)) {
        const QString &area = zone.section('/', 0, 0);
        if (gui.comboTimeArea->findData(QVariant(area)) < 0) {
            QString text(area);
            if (area == "Indian" || area == "Pacific"
                || area == "Atlantic" || area == "Arctic") text.append(" Ocean");
            gui.comboTimeArea->addItem(text, area);
        }
    }
    gui.comboTimeArea->model()->sort(0);
    // Guess if hardware clock is UTC or local time
    proc.shell("guess-hwclock", nullptr, true);
    if (proc.readOut() == "localtime") gui.checkLocalClock->setChecked(true);

    // locale list
    connect(gui.comboLocale, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &Oobe::localeIndexChanged);
    gui.comboLocale->clear();
    proc.shell("locale -a | grep -Ev '^(C|POSIX)\\.?' | grep -E 'utf8|UTF-8'", nullptr, true);
    QStringList loclist = proc.readOutLines();
    for (QString &strloc : loclist) {
        strloc.replace("utf8", "UTF-8", Qt::CaseInsensitive);
        QLocale loc(strloc);
        gui.comboLocale->addItem(loc.nativeTerritoryName() + " - " + loc.nativeLanguageName(), QVariant(strloc));
    }
    gui.comboLocale->model()->sort(0);
    // default locale selection
    int ixLocale = gui.comboLocale->findData(QVariant(QLocale::system().name() + ".UTF-8"));
    if (gui.comboLocale->currentIndex() != ixLocale) gui.comboLocale->setCurrentIndex(ixLocale);
    else timeAreaIndexChanged(ixLocale);

    if (online) {
        gui.checkSaveDesktop->hide();
    } else {
        // Detect snapshot-backup account(s)
        // test if there's another user other than demo in /home,
        // indicating a possible snapshot or complicated live-usb
        haveSnapshotUserAccounts = proc.shell("ls -1 /home"
            " | grep -Ev '^(lost\\+found|demo|snapshot)$' | grep -q '[a-zA-Z0-9]'");
        qDebug() << "check for possible snapshot:" << haveSnapshotUserAccounts;
    }

    //check for samba
    QFileInfo info("/etc/init.d/smbd");
    if (!info.exists()) {
        gui.labelComputerGroup->setEnabled(false);
        gui.textComputerGroup->setEnabled(false);
        gui.textComputerGroup->setText("");
        gui.checkSamba->setChecked(false);
        gui.checkSamba->setEnabled(false);
    }
    // check for the Samba server
    proc.shell("dpkg -s samba | grep '^Status.*ok.*' | sed -e 's/.*ok //'", nullptr, true);
    QString val = proc.readOut();
    haveSamba = (val.compare("installed") == 0);

    buildServiceList(appConf);

    // Current user details
    curUser = getlogin();
    if (curUser.isEmpty()) {
        curUser = "demo";
    }
    // Using $HOME results in the wrong directory when run as root (with su).
    MProcess::Section sect(proc, nullptr);
    proc.shell("echo ~" + curUser, nullptr, true);
    curHome = proc.readOut(true);
    if (curHome.isEmpty()) {
        curHome = "/home/" + curUser;
    }
    qDebug() << "Current user:" << curUser << curHome;
}

void Oobe::manageConfig(MSettings &config) const noexcept
{
    const bool save = config.isSave();
    // Services page
    config.setSection("Services", gui.pageServices);
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
    config.setSection("Network", gui.pageNetwork);
    config.manageLineEdit("ComputerName", gui.textComputerName);
    config.manageLineEdit("Domain", gui.textComputerDomain);
    config.manageLineEdit("Workgroup", gui.textComputerGroup);
    config.manageCheckBox("Samba", gui.checkSamba);

    // Localization page
    config.setSection("Localization", gui.pageLocalization);
    config.manageComboBox("Locale", gui.comboLocale, true);
    config.manageCheckBox("LocalClock", gui.checkLocalClock);
    static constexpr const char *clockChoices[] = {"24", "12"};
    QRadioButton *const clockRadios[] = {gui.radioClock24, gui.radioClock12};
    config.manageRadios("ClockHours", 2, clockChoices, clockRadios);
    if (save) {
        config.setString("Timezone", gui.comboTimeZone->currentData().toString());
    } else {
        const int rc = selectTimeZone(config.getString("Timezone", QTimeZone::systemTimeZoneId()));
        if (rc == 1) config.markBadWidget(gui.comboTimeArea);
        else if (rc == 2) config.markBadWidget(gui.comboTimeZone);
    }
    config.manageComboBox("Timezone", gui.comboTimeZone, true);

    // User Accounts page
    if (!haveSnapshotUserAccounts) {
        config.setSection("User", gui.pageUserAccounts);
        config.manageLineEdit("Username", gui.textUserName);
        config.manageCheckBox("Autologin", gui.checkAutoLogin);
        config.manageCheckBox("SaveDesktop", gui.checkSaveDesktop);
        if (online) {
            gui.checkSaveDesktop->setCheckState(Qt::Unchecked);
        }
        static constexpr const char *oldHomeActions[] = {"Use", "Save", "Delete"};
        QRadioButton *const oldHomeRadios[] = {gui.radioOldHomeUse, gui.radioOldHomeSave, gui.radioOldHomeDelete};
        config.manageRadios("OldHomeAction", 3, oldHomeActions, oldHomeRadios);
        config.manageGroupCheckBox("EnableRoot", gui.boxRootAccount);
        config.addFilter("UserPass");
        config.addFilter("RootPass");
        if (!save) {
            const QString &upass = config.getString("UserPass");
            gui.textUserPass->setText(upass);
            gui.textUserPass2->setText(upass);
            const QString &rpass = config.getString("RootPass");
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
    //setService("smbd", false);
    //setService("nmbd", false);
    //setService("samba-ad-dc", false);
    proc.exec("update-rc.d", {"-f", "oobe", "defaults"});
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
            core.setService("smbd", enable);
            core.setService("nmbd", enable);
            core.setService("samba-ad-dc", enable);
        }
        sect.setRoot(nullptr);
        setComputerName();
        setLocale();
    }
    if (haveSnapshotUserAccounts) {
        // Copy the whole /home/ directory for snapshot accounts.
        proc.exec("rsync", {"-a", "/home/", "/mnt/antiX/home/",
            "--exclude", ".cache", "--exclude", ".gvfs", "--exclude", ".dbus", "--exclude", ".Xauthority",
            "--exclude", ".ICEauthority", "--exclude", ".config/session"});
    } else if (!oem) {
        setUserInfo();
    }
    if (online) {
        proc.shell("sed -i 's/nosplash\\b/splash/g' /boot/grub/grub.cfg");
        proc.exec("update-rc.d", {"oobe", "disable"});
    }
}

/* Services */

void Oobe::buildServiceList(MIni &appconf) noexcept
{
    MIni services_desc("/usr/share/gazelle-installer-data/services.list", MIni::ReadOnly);

    appconf.setSection("Services");
    for (const QStringList &keys = appconf.getKeys(); const QString &service : keys) {
        const QString &lang = QLocale::system().bcp47Name().toLower();
        services_desc.setSection(lang);
        if (!services_desc.contains(service)) {
            services_desc.setSection(""); // Use English definition
        }
        QStringList list = services_desc.getString(service).split(',');
        if (list.size() != 2) continue;
        const QString &category = list.at(0).trimmed();
        const QString &description = list.at(1).trimmed();

        if (QFile::exists("/etc/init.d/"+service) || QFile::exists("/etc/sv/"+service)) {
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

    static const QRegularExpression nametest("[^0-9a-zA-Z-.]|^[.-]|[.-]$|\\.\\.");
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
    QString etcpath = online ? "/etc" : "/mnt/antiX/etc";
    if (haveSamba) {
        //replaceStringInFile(PROJECTSHORTNAME + "1", textComputerName->text(), "/mnt/antiX/etc/samba/smb.conf");
        replaceStringInFile("WORKGROUP", gui.textComputerGroup->text(), etcpath + "/samba/smb.conf");
    }
    //replaceStringInFile(PROJECTSHORTNAME + "1", textComputerName->text(), "/mnt/antiX/etc/hosts");
    const QString &compname = gui.textComputerName->text();
    const QString &domainname = gui.textComputerDomain->text();
    QString cmd("sed -E -i '/localhost/! s/^(127\\.0\\.0\\.1|127\\.0\\.1\\.1).*/\\1    " + compname + "/'");
    proc.shell(cmd + " " + etcpath + "/hosts");
    proc.shell("echo \"" + compname + "\" | cat > " + etcpath + "/hostname");
    proc.shell("echo \"" + compname + "\" | cat > " + etcpath + "/mailname");
    proc.shell("sed -i 's/.*send host-name.*/send host-name \""
        + compname + "\";/g' " + etcpath + "/dhcp/dhclient.conf");
    proc.shell("echo \"" + domainname + "\" | cat > " + etcpath + "/defaultdomain");
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
    QString cmd;
    if (!online) cmd = "chroot /mnt/antiX ";
    cmd += QString("/usr/sbin/update-locale \"LANG=%1\"").arg(gui.comboLocale->currentData().toString());
    proc.shell(cmd);

    //set paper size based on locale
    QString papersize;
    QStringList letterpapersizedefualt = {"en_US", "es_BO", "es_CO", "es_MX", "es_NI", "es_PA", "es_US", "es_VE", "fr_CA", "en_CA" };
    if (containsAnySubstring(gui.comboLocale->currentData().toString(), letterpapersizedefualt)) {
        papersize = "letter";
    } else {
        papersize = "a4";
    }

    if (!online) {
        proc.shell("echo " + papersize + " >/mnt/antiX/etc/papersize");
    } else {
        proc.shell("echo " + papersize + " >/etc/papersize");
    }

    const QString &selTimeZone = gui.comboTimeZone->currentData().toString();

    // /etc/localtime is either a file or a symlink to a file in /usr/share/zoneinfo. Use the one selected by the user.
    //replace with link
    if (!online) {
        proc.exec("ln", {"-nfs", "/usr/share/zoneinfo/" + selTimeZone, "/mnt/antiX/etc/localtime"});
    }
    proc.exec("ln", {"-nfs", "/usr/share/zoneinfo/" + selTimeZone, "/etc/localtime"});
    // /etc/timezone is text file with the timezone written in it. Write the user-selected timezone in it now.
    if (!online) proc.shell("echo " + selTimeZone + " > /mnt/antiX/etc/timezone");
    proc.shell("echo " + selTimeZone + " > /etc/timezone");

    // Set clock to use LOCAL
    if (gui.checkLocalClock->isChecked()) {
        proc.shell("echo '0.0 0 0.0\n0\nLOCAL' > /etc/adjtime");
    } else {
        proc.shell("echo '0.0 0 0.0\n0\nUTC' > /etc/adjtime");
    }
    proc.exec("hwclock", {"--hctosys"});
    if (!online) {
        proc.exec("cp", {"-f", "/etc/adjtime", "/mnt/antiX/etc/"});
        proc.exec("cp", {"-f", "/etc/default/rcS", "/mnt/antiX/etc/default"});
    }

    // Set clock format
    setUserClockFormat(online ? "/etc/skel" : "/mnt/antiX/etc/skel");

    // localize repo
    qDebug() << "Localize repo";
    if (online) proc.exec("localize-repo", {"default"});
    else proc.exec("chroot", {"/mnt/antiX", "localize-repo", "default"});

    //machine id
    if (online){
        // create a /etc/machine-id file and /var/lib/dbus/machine-id file
        proc.exec("rm", {"/var/lib/dbus/machine-id", "/etc/machine-id"});
        proc.exec("dbus-uuidgen", {"--ensure=/etc/machine-id"});
        proc.exec("dbus-uuidgen", {"--ensure"});
    }
}
void Oobe::setUserClockFormat(const QString &skelpath) const noexcept
{
    MProcess::Section sect(proc, nullptr);
    if (gui.radioClock12->isChecked()) {
        //mx systems
        proc.shell("sed -i '/data0=/c\\data0=%l:%M' " + skelpath + "/.config/xfce4/panel/xfce4-orageclock-plugin-1.rc");
        proc.shell("sed -i '/time_format=/c\\time_format=%l:%M' " + skelpath + "/.config/xfce4/panel/datetime-1.rc");
        proc.shell("sed -i 's/%H/%l/' " + skelpath + "/.config/xfce4/xfconf/xfce-perchannel-xml/xfce4-panel.xml");

        //mx kde
        proc.shell("sed -i '/use24hFormat=/c\\use24hFormat=0' " + skelpath + "/.config/plasma-org.kde.plasma.desktop-appletsrc");

        //mx fluxbox
        proc.shell("sed -i '/time1_format/c\\time1_format=%l:%M' " + skelpath + "/.config/tint2/tint2rc");

        //antix systems
        proc.shell("sed -i 's/%H:%M/%l:%M/g' " + skelpath + "/.icewm/preferences");
        proc.shell("sed -i 's/%k:%M/%l:%M/g' " + skelpath + "/.fluxbox/init");
        proc.shell("sed -i 's/%H:%M/%l:%M/g' " + skelpath + "/.jwm/tray");
    } else {
        //mx systems
        proc.shell("sed -i '/data0=/c\\data0=%H:%M' " + skelpath + "/.config/xfce4/panel/xfce4-orageclock-plugin-1.rc");
        proc.shell("sed -i '/time_format=/c\\time_format=%H:%M' " + skelpath + "/.config/xfce4/panel/datetime-1.rc");
        proc.shell("sed -i 's/%l/%H/' " + skelpath + "/.config/xfce4/xfconf/xfce-perchannel-xml/xfce4-panel.xml");

        //mx kde
        proc.shell("sed -i '/use24hFormat=/c\\use24hFormat=2' " + skelpath + "/.config/plasma-org.kde.plasma.desktop-appletsrc");

        //mx fluxbox
        proc.shell("sed -i '/time1_format/c\\time1_format=%H:%M' " + skelpath + "/.config/tint2/tint2rc");

        //antix systems
        proc.shell("sed -i 's/%H:%M/%H:%M/g' " + skelpath + "/.icewm/preferences");
        proc.shell("sed -i 's/%k:%M/%k:%M/g' " + skelpath + "/.fluxbox/init");
        proc.shell("sed -i 's/%H:%M/%k:%M/g' " + skelpath + "/.jwm/tray");
    }
}

QWidget *Oobe::validateUserInfo(bool automatic) const noexcept
{
    QMessageBox msgbox(gui.boxMain);
    msgbox.setIcon(QMessageBox::Critical);
    msgbox.setInformativeText(tr("Please choose a different name before proceeding."));
    const QString &userName = gui.textUserName->text();
    // see if username is reasonable length
    static const QRegularExpression usertest("^[a-zA-Z_][a-zA-Z0-9_-]*[$]?$");
    if (!userName.contains(usertest)) {
        msgbox.setText(tr("The user name cannot contain special characters or spaces."));
        msgbox.exec();
        return gui.textUserName;
    }
    // check that user name is not already used
    QFile file("/etc/passwd");
    if (file.open(QFile::ReadOnly | QFile::Text)) {
        const QByteArray &match = QString("%1:").arg(userName).toUtf8();
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
    if (!gui.boxRootAccount->isChecked()) proc.exec("passwd", {"-l", "root"});
    else {
        const QString &rootPass = gui.textRootPass->text();
        if (rootPass.isEmpty()) proc.exec("passwd", {"-d", "root"});
        else userinfo.append(QString("root:" + rootPass).toUtf8());
    }
    if (userPass.isEmpty()) {
        proc.exec("passwd", {"-d", curUser});
    } else {
        if (!userinfo.isEmpty()) userinfo.append('\n');
        userinfo.append(QString(curUser + ':' + userPass).toUtf8());
    }
    if (!userinfo.isEmpty()) proc.exec("chpasswd", {}, &userinfo);
    sect.setRoot(nullptr);

    QString rootpath;
    if (!online) rootpath = "/mnt/antiX";
    QString skelpath = rootpath + "/etc/skel";
    const QString &username = gui.textUserName->text();
    QString dpath = rootpath + "/home/" + username;

    if (QFileInfo::exists(dpath)) {
        if (gui.radioOldHomeSave->isChecked()) {
            sect.setExceptionMode(QT_TR_NOOP("Failed to save old home directory."));
            for (int ixi = 1; ; ++ixi) {
                const QString &dest(dpath + ".00" + QString::number(ixi));
                if (!QFileInfo::exists(dest)) {
                    proc.exec("mv", {"-f", dpath, dest});
                    break;
                }
            }
        } else if (gui.radioOldHomeDelete->isChecked()) {
            sect.setExceptionMode(QT_TR_NOOP("Failed to delete old home directory."));
            proc.exec("rm", {"-rf", dpath});
        }
        proc.exec("sync"); // The sync(2) system call will block the GUI.
    }

    // check the linuxfs squashfs for a home/demo folder, which indicates a remaster perserving /home.
    const bool remasteredUserHome = QFileInfo("/live/linux" + curHome).isDir();
    qDebug() << "check for remastered home folder:" << remasteredUserHome;

    if (QFileInfo::exists(dpath.toUtf8())) { // Still exists.
        sect.setExceptionMode(nullptr);
        proc.exec("cp", {"-n", skelpath + "/.bash_profile", dpath});
        proc.exec("cp", {"-n", skelpath + "/.bashrc", dpath});
        proc.exec("cp", {"-n", skelpath + "/.gtkrc", dpath});
        proc.exec("cp", {"-n", skelpath + "/.gtkrc-2.0", dpath});
        proc.exec("cp", {"-Rn", skelpath + "/.config", dpath});
        proc.exec("cp", {"-Rn", skelpath + "/.local", dpath});
    } else { // dir does not exist, must create it
        // Copy skel to demo, unless demo folder exists in remastered linuxfs.
        if (!remasteredUserHome) {
            sect.setExceptionMode(QT_TR_NOOP("Sorry, failed to create user directory."));
            proc.exec("cp", {"-a", skelpath, dpath});
        } else { // still rename the demo directory even if remastered demo home folder is detected
            sect.setExceptionMode(QT_TR_NOOP("Sorry, failed to name user directory."));
            proc.exec("mv", {"-f", rootpath + curHome, dpath});
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
            " --exclude '.config/session' | xargs -I '$' sed -i 's|%1|/home/"
            + username + "|g' %2/$").arg(curHome, dpath);
        proc.shell(cmd);
    }

    // check if remaster exists and checkSaveDesktop not checked, modify the remastered demo folder
    if (remasteredUserHome && !gui.checkSaveDesktop->isChecked())
    {
        resetBlueman();
        // Change remaster "demo" to new user.
        QString cmd = ("find " + dpath + " -maxdepth 1 -type f -name '.*' -print0 | xargs -0 sed -i 's|" + curHome + "|/home/" + username + "|g'").arg(dpath);
        proc.shell(cmd);
        cmd = ("find " + dpath + "/.config -type f -print0 | xargs -0 sed -i 's|" + curHome + "|/home/" + username + "|g'").arg(dpath);
        proc.shell(cmd);
        cmd = ("find " + dpath + "/.local -type f  -print0 | xargs -0 sed -i 's|" + curHome + "|/home/" + username + "|g'").arg(dpath);
        proc.shell(cmd);
    }

    // fix the ownership, demo=newuser
    sect.setExceptionMode(QT_TR_NOOP("Failed to set ownership or permissions of user directory."));
    proc.exec("chown", {"-R", curUser + ':' + curUser, dpath});
    // Set permissions according to /etc/adduser.conf
    const MIni addusercfg("/etc/adduser.conf", MIni::ReadOnly);
    mode_t mode = addusercfg.getString("DIR_MODE", "0700").toUInt(&ok, 8);
    if (chmod(dpath.toUtf8().constData(), mode) != 0) throw sect.failMessage();

    // change in files
    replaceStringInFile(curUser, username, rootpath + "/etc/group");
    replaceStringInFile(curUser, username, rootpath + "/etc/gshadow");
    replaceStringInFile(curUser, username, rootpath + "/etc/passwd");
    replaceStringInFile(curUser, username, rootpath + "/etc/shadow");
    replaceStringInFile(curUser, username, rootpath + "/etc/subuid");
    replaceStringInFile(curUser, username, rootpath + "/etc/subgid");
    replaceStringInFile(curUser, username, rootpath + "/etc/slim.conf");
    replaceStringInFile(curUser, username, rootpath + "/etc/slimski.local.conf");
    replaceStringInFile(curUser, username, rootpath + "/etc/lightdm/lightdm.conf");
    replaceStringInFile(curUser, username, rootpath + "/home/*/.gtkrc-2.0");
    replaceStringInFile(curUser, username, rootpath + "/root/.gtkrc-2.0");
    if (gui.checkAutoLogin->isChecked()) {
        replaceStringInFile("#auto_login", "auto_login", rootpath + "/etc/slim.conf");
        replaceStringInFile("#default_user ", "default_user ", rootpath + "/etc/slim.conf");
        replaceStringInFile("#autologin_enabled", "autologin_enabled", rootpath + "/etc/slimski.local.conf");
        replaceStringInFile("#default_user ", "default_user ", rootpath + "/etc/slimski.local.conf");
        replaceStringInFile("User=", "User=" + username, rootpath + "/etc/sddm.conf");
    }
    else {
        replaceStringInFile("auto_login", "#auto_login", rootpath + "/etc/slim.conf");
        replaceStringInFile("default_user ", "#default_user ", rootpath + "/etc/slim.conf");
        replaceStringInFile("autologin_enabled", "#autologin_enabled", rootpath + "/etc/slimski.local.conf");
        replaceStringInFile("default_user ", "#default_user ", rootpath + "/etc/slimski.local.conf");
        replaceStringInFile("autologin-user=", "#autologin-user=", rootpath + "/etc/lightdm/lightdm.conf");
        replaceStringInFile("User=.*", "User=", rootpath + "/etc/sddm.conf");
    }
    proc.exec("touch", {rootpath + "/var/mail/" + username});
}

void Oobe::resetBlueman() const
{
    proc.exec("runuser", {"-l", curUser, "-c", "dconf reset /org/blueman/transfer/shared-path"}); //reset blueman path
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
    return proc.exec("sed", {"-i", QString("s/%1/%2/g").arg(oldtext, newtext), filepath});
}

bool Oobe::containsAnySubstring(const QString& mainString, const QStringList& substrings) const noexcept {
    for (const QString& substring : substrings) {
        if (mainString.contains(substring)) {
            return true;
        }
    }
    return false;
}
