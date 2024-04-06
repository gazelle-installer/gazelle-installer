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
#include "msettings.h"
#include "oobe.h"

Oobe::Oobe(MProcess &mproc, Ui::MeInstall &ui, QWidget *parent, QSettings &appConf, bool oem, bool modeOOBE)
    : QObject(parent), proc(mproc), gui(ui), master(parent), oem(oem), online(modeOOBE),
    passUser(ui.textUserPass, ui.textUserPass2, 0, this), passRoot(ui.textRootPass, ui.textRootPass2, 0, this)
{
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
    proc.exec("find", {"-L", "/usr/share/zoneinfo/posix",
            "-mindepth", "2", "-type", "f", "-printf", "%P\\n"}, nullptr, true);
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
        gui.comboLocale->addItem(loc.nativeCountryName() + " - " + loc.nativeLanguageName(), QVariant(strloc));
    }
    gui.comboLocale->model()->sort(0);
    // default locale selection
    int ixLocale = gui.comboLocale->findData(QVariant(QLocale::system().name() + ".UTF-8"));
    if (gui.comboLocale->currentIndex() != ixLocale) gui.comboLocale->setCurrentIndex(ixLocale);
    else timeAreaIndexChanged(ixLocale);

    if (online) {
        containsSystemD = QFileInfo("/usr/usr/bin/systemctl").isExecutable();
        if (QFile::exists("/etc/service") && QFile::exists("/lib/runit/runit-init")) {
            containsRunit = true;
        }
        gui.checkSaveDesktop->hide();
    } else {
        containsSystemD = QFileInfo("/live/aufs/usr/bin/systemctl").isExecutable();
        if (QFile::exists("/live/aufs/etc/service") && QFile::exists("/live/aufs/sbin/runit")) {
            containsRunit = true;
        }
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
}

void Oobe::manageConfig(MSettings &config, bool save) noexcept
{
    // Services page
    config.startGroup("Services", gui.pageServices);
    QTreeWidgetItemIterator it(gui.treeServices);
    while (*it) {
        if ((*it)->parent() != nullptr) {
            const QString &itext = (*it)->text(0);
            const QVariant checkval((*it)->checkState(0) == Qt::Checked);
            if (save) config.setValue(itext, checkval);
            else {
                const bool val = config.value(itext, checkval).toBool();
                (*it)->setCheckState(0, val ? Qt::Checked : Qt::Unchecked);
            }
        }
        ++it;
    }
    config.endGroup();

    // Network page
    config.startGroup("Network", gui.pageNetwork);
    config.manageLineEdit("ComputerName", gui.textComputerName);
    config.manageLineEdit("Domain", gui.textComputerDomain);
    config.manageLineEdit("Workgroup", gui.textComputerGroup);
    config.manageCheckBox("Samba", gui.checkSamba);
    config.endGroup();

    // Localization page
    config.startGroup("Localization", gui.pageLocalization);
    config.manageComboBox("Locale", gui.comboLocale, true);
    config.manageCheckBox("LocalClock", gui.checkLocalClock);
    static constexpr const char *clockChoices[] = {"24", "12"};
    QRadioButton *const clockRadios[] = {gui.radioClock24, gui.radioClock12};
    config.manageRadios("ClockHours", 2, clockChoices, clockRadios);
    if (save) {
        config.setValue("Timezone", gui.comboTimeZone->currentData().toString());
    } else {
        QVariant def = QString(QTimeZone::systemTimeZoneId());
        const int rc = selectTimeZone(config.value("Timezone", def).toString());
        if (rc == 1) config.markBadWidget(gui.comboTimeArea);
        else if (rc == 2) config.markBadWidget(gui.comboTimeZone);
    }
    config.manageComboBox("Timezone", gui.comboTimeZone, true);
    config.endGroup();

    // User Accounts page
    config.startGroup("User", gui.pageUserAccounts);
    config.manageLineEdit("Username", gui.textUserName);
    config.manageCheckBox("Autologin", gui.checkAutoLogin);
    config.manageCheckBox("SaveDesktop", gui.checkSaveDesktop);
    if (online) gui.checkSaveDesktop->setCheckState(Qt::Unchecked);
    static constexpr const char *oldHomeActions[] = {"Use", "Save", "Delete"};
    QRadioButton *const oldHomeRadios[] = {gui.radioOldHomeUse, gui.radioOldHomeSave, gui.radioOldHomeDelete};
    config.manageRadios("OldHomeAction", 3, oldHomeActions, oldHomeRadios);
    config.manageGroupCheckBox("EnableRoot", gui.boxRootAccount);
    if (!save || oem) { // Transfer default passwords under OEM mode.
        const QString &upass = config.value("UserPass").toString();
        gui.textUserPass->setText(upass);
        gui.textUserPass2->setText(upass);
        const QString &rpass = config.value("RootPass").toString();
        gui.textRootPass->setText(rpass);
        gui.textRootPass2->setText(rpass);
    }
    config.endGroup();
}

void Oobe::enable()
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

void Oobe::process()
{
    if (!oem) {
        MProcess::Section sect(proc);
        if (!online) sect.setRoot("/mnt/antiX");

        QTreeWidgetItemIterator it(gui.treeServices);
        for (; *it; ++it) {
            if ((*it)->parent()) setService((*it)->text(0), (*it)->checkState(0) == Qt::Checked);
        }
        if (haveSamba) {
            const bool enable = gui.checkSamba->isChecked();
            setService("smbd", enable);
            setService("nmbd", enable);
            setService("samba-ad-dc", enable);
        }
        sect.setRoot(nullptr);
        setComputerName();
        setLocale();
    }
    if (haveSnapshotUserAccounts || oem) { // skip user account creation
        proc.exec("rsync", {"-a", "/home/", "/mnt/antiX/home/",
            "--exclude", ".cache", "--exclude", ".gvfs", "--exclude", ".dbus", "--exclude", ".Xauthority",
            "--exclude", ".ICEauthority", "--exclude", ".config/session"});
    } else {
        setUserInfo();
    }
    if (online) {
        proc.shell("sed -i 's/nosplash\\b/splash/g' /boot/grub/grub.cfg");
        proc.exec("update-rc.d", {"oobe", "disable"});
    }
}

/* Services */

void Oobe::buildServiceList(QSettings &appconf) noexcept
{
    //setup treeServices
    gui.treeServices->header()->setMinimumSectionSize(150);
    gui.treeServices->header()->resizeSection(0,150);

    QSettings services_desc("/usr/share/gazelle-installer-data/services.list", QSettings::NativeFormat);
#if QT_VERSION < QT_VERSION_CHECK(6,0,0)
    services_desc.setIniCodec("UTF-8");
#endif

    gui.textComputerName->setText(appconf.value("DEFAULT_HOSTNAME").toString());
    appconf.beginGroup("SERVICES");
    for (const QString &service : appconf.allKeys()) {
        const QString &lang = QLocale::system().bcp47Name().toLower();
        QStringList list = services_desc.value(lang + '/' + service).toStringList();
        if (list.size() != 2) {
            list = services_desc.value(service).toStringList(); // Use English definition
            if (list.size() != 2) continue;
        }
        QString category, description;
        category = list.at(0);
        description = list.at(1);

        if (QFile::exists("/etc/init.d/"+service) || QFile::exists("/etc/sv/"+service)) {
            QList<QTreeWidgetItem *> found_items = gui.treeServices->findItems(category, Qt::MatchExactly, 0);
            QTreeWidgetItem *top_item;
            QTreeWidgetItem *item;
            QTreeWidgetItem *parent;
            if (found_items.size() == 0) { // add top item if no top items found
                top_item = new QTreeWidgetItem(gui.treeServices);
                top_item->setText(0, category);
                parent = top_item;
            } else {
                parent = found_items.last();
            }
            item = new QTreeWidgetItem(parent);
            item->setText(0, service);
            item->setText(1, description);
            item->setCheckState(0, appconf.value(service).toBool() ? Qt::Checked : Qt::Unchecked);
        }
    }
    appconf.endGroup();

    gui.treeServices->expandAll();
    gui.treeServices->resizeColumnToContents(0);
    gui.treeServices->resizeColumnToContents(1);
}

void Oobe::stashServices(bool save) noexcept
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

void Oobe::setService(const QString &service, bool enabled)
{
    qDebug() << "Set service:" << service << enabled;
    MProcess::Section sect(proc);
    const QString chroot(sect.root());
    if (enabled) {
        proc.exec("update-rc.d", {"-f", service, "defaults"});
        if (containsSystemD) {
            proc.exec("systemctl", {"unmask", service});
            proc.exec("systemctl", {"enable", service});
        }
        if (containsRunit) {
            QFile::remove(chroot + "/etc/sv/" + service + "/down");
            if (!QFile::exists(chroot + "/etc/sv/" + service)) {
                proc.mkpath(chroot + "/etc/sv/" + service);
                proc.exec("ln", {"-fs", "/etc/sv/" + service, "/etc/service/"});
            }
        }
    } else {
        proc.exec("update-rc.d", {"-f", service, "remove"});
        if (containsSystemD) {
            proc.exec("systemctl", {"disable", service});
            proc.exec("systemctl", {"mask", service});
        }
        if (containsRunit) {
            if (!QFile::exists(chroot + "/etc/sv/" + service)) {
                proc.mkpath(chroot + "/etc/sv/" + service);
                proc.exec("ln", {"-fs", "/etc/sv/" + service, "/etc/service/"});
            }
            proc.exec("touch", {"/etc/sv/" + service + "/down"});
        }
    }
}

QWidget *Oobe::validateComputerName() noexcept
{
    // see if name is reasonable
    if (gui.textComputerName->text().isEmpty()) {
        QMessageBox::critical(master, master->windowTitle(), tr("Please enter a computer name."));
        return gui.textComputerName;
    } else if (gui.textComputerName->text().contains(QRegularExpression("[^0-9a-zA-Z-.]|^[.-]|[.-]$|\\.\\."))) {
        QMessageBox::critical(master, master->windowTitle(),
            tr("Sorry, your computer name contains invalid characters.\nYou'll have to select a different\nname before proceeding."));
        return gui.textComputerName;
    }
    // see if name is reasonable
    if (gui.textComputerDomain->text().isEmpty()) {
        QMessageBox::critical(master, master->windowTitle(), tr("Please enter a domain name."));
        return gui.textComputerDomain;
    } else if (gui.textComputerDomain->text().contains(QRegularExpression("[^0-9a-zA-Z-.]|^[.-]|[.-]$|\\.\\."))) {
        QMessageBox::critical(master, master->windowTitle(),
                              tr("Sorry, your computer domain contains invalid characters.\nYou'll have to select a different\nname before proceeding."));
        return gui.textComputerDomain;
    }

    if (haveSamba) {
        // see if name is reasonable
        if (gui.textComputerGroup->text().isEmpty()) {
            QMessageBox::critical(master, master->windowTitle(), tr("Please enter a workgroup."));
            return gui.textComputerGroup;
        }
    } else {
        gui.textComputerGroup->clear();
    }

    return nullptr;
}

// set the computer name, can not be rerun
void Oobe::setComputerName()
{
    QString etcpath = online ? "/etc" : "/mnt/antiX/etc";
    if (haveSamba) {
        //replaceStringInFile(PROJECTSHORTNAME + "1", textComputerName->text(), "/mnt/antiX/etc/samba/smb.conf");
        replaceStringInFile("WORKGROUP", gui.textComputerGroup->text(), etcpath + "/samba/smb.conf");
    }
    //replaceStringInFile(PROJECTSHORTNAME + "1", textComputerName->text(), "/mnt/antiX/etc/hosts");
    const QString &compname = gui.textComputerName->text();
    QString cmd("sed -E -i '/localhost/! s/^(127\\.0\\.0\\.1|127\\.0\\.1\\.1).*/\\1    " + compname + "/'");
    proc.shell(cmd + " " + etcpath + "/hosts");
    proc.shell("echo \"" + compname + "\" | cat > " + etcpath + "/hostname");
    proc.shell("echo \"" + compname + "\" | cat > " + etcpath + "/mailname");
    proc.shell("sed -i 's/.*send host-name.*/send host-name \""
        + compname + "\";/g' " + etcpath + "/dhcp/dhclient.conf");
    proc.shell("echo \"" + compname + "\" | cat > " + etcpath + "/defaultdomain");
}

// return 0 = success, 1 = bad area, 2 = bad zone
int Oobe::selectTimeZone(const QString &zone) noexcept
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

void Oobe::setLocale()
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
    QString skelpath = online ? "/etc/skel" : "/mnt/antiX/etc/skel";
    if (gui.radioClock12->isChecked()) {
        //mx systems
        proc.shell("sed -i '/data0=/c\\data0=%l:%M' /home/demo/.config/xfce4/panel/xfce4-orageclock-plugin-1.rc");
        proc.shell("sed -i '/data0=/c\\data0=%l:%M' " + skelpath + "/.config/xfce4/panel/xfce4-orageclock-plugin-1.rc");
        proc.shell("sed -i '/time_format=/c\\time_format=%l:%M' /home/demo/.config/xfce4/panel/datetime-1.rc");
        proc.shell("sed -i '/time_format=/c\\time_format=%l:%M' " + skelpath + "/.config/xfce4/panel/datetime-1.rc");
        proc.shell("sed -i 's/%H/%l/' /home/demo/.config/xfce4/xfconf/xfce-perchannel-xml/xfce4-panel.xml");
        proc.shell("sed -i 's/%H/%l/' " + skelpath + "/.config/xfce4/xfconf/xfce-perchannel-xml/xfce4-panel.xml");

        //mx kde
        proc.shell("sed -i '/use24hFormat=/c\\use24hFormat=0' /home/demo/.config/plasma-org.kde.plasma.desktop-appletsrc");
        proc.shell("sed -i '/use24hFormat=/c\\use24hFormat=0' " + skelpath + "/.config/plasma-org.kde.plasma.desktop-appletsrc");

        //mx fluxbox
        proc.shell("sed -i '/time1_format/c\\time1_format=%l:%M' " + skelpath + "/.config/tint2/tint2rc");
        proc.shell("sed -i '/time1_format/c\\time1_format=%l:%M' /home/demo/.config/tint2/tint2rc");

        //antix systems
        proc.shell("sed -i 's/%H:%M/%l:%M/g' " + skelpath + "/.icewm/preferences");
        proc.shell("sed -i 's/%k:%M/%l:%M/g' " + skelpath + "/.fluxbox/init");
        proc.shell("sed -i 's/%H:%M/%l:%M/g' " + skelpath + "/.jwm/tray");
    } else {
        //mx systems
        proc.shell("sed -i '/data0=/c\\data0=%H:%M' /home/demo/.config/xfce4/panel/xfce4-orageclock-plugin-1.rc");
        proc.shell("sed -i '/data0=/c\\data0=%H:%M' " + skelpath + "/.config/xfce4/panel/xfce4-orageclock-plugin-1.rc");
        proc.shell("sed -i '/time_format=/c\\time_format=%H:%M' /home/demo/.config/xfce4/panel/datetime-1.rc");
        proc.shell("sed -i '/time_format=/c\\time_format=%H:%M' " + skelpath + "/.config/xfce4/panel/datetime-1.rc");
        proc.shell("sed -i 's/%l/%H/' /home/demo/.config/xfce4/xfconf/xfce-perchannel-xml/xfce4-panel.xml");
        proc.shell("sed -i 's/%l/%H/' " + skelpath + "/.config/xfce4/xfconf/xfce-perchannel-xml/xfce4-panel.xml");


        //mx kde
        proc.shell("sed -i '/use24hFormat=/c\\use24hFormat=2' /home/demo/.config/plasma-org.kde.plasma.desktop-appletsrc");
        proc.shell("sed -i '/use24hFormat=/c\\use24hFormat=2' " + skelpath + "/.config/plasma-org.kde.plasma.desktop-appletsrc");

        //mx fluxbox
        proc.shell("sed -i '/time1_format/c\\time1_format=%H:%M' " + skelpath + "/.config/tint2/tint2rc");
        proc.shell("sed -i '/time1_format/c\\time1_format=%H:%M' /home/demo/.config/tint2/tint2rc");

        //antix systems
        proc.shell("sed -i 's/%H:%M/%H:%M/g' " + skelpath + "/.icewm/preferences");
        proc.shell("sed -i 's/%k:%M/%k:%M/g' " + skelpath + "/.fluxbox/init");
        proc.shell("sed -i 's/%H:%M/%k:%M/g' " + skelpath + "/.jwm/tray");
    }

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

QWidget *Oobe::validateUserInfo(bool automatic) noexcept
{
    const QString &userName = gui.textUserName->text();
    // see if username is reasonable length
    if (!userName.contains(QRegularExpression("^[a-zA-Z_][a-zA-Z0-9_-]*[$]?$"))) {
        QMessageBox::critical(master, master->windowTitle(),
            tr("The user name cannot contain special characters or spaces.\n"
                "Please choose another name before proceeding."));
        return gui.textUserName;
    }
    // check that user name is not already used
    QFile file("/etc/passwd");
    if (file.open(QFile::ReadOnly | QFile::Text)) {
        const QByteArray &match = QString("%1:").arg(userName).toUtf8();
        while (!file.atEnd()) {
            if (file.readLine().startsWith(match)) {
                QMessageBox::critical(master, master->windowTitle(),
                    tr("Sorry, that name is in use.\nPlease select a different name."));
                return gui.textUserName;
            }
        }
    }

    if (!automatic && passUser.length()==0) {
        // Confirm that an empty user password is not accidental.
        const QMessageBox::StandardButton ans = QMessageBox::warning(master, master->windowTitle(),
            tr("You did not provide a passphrase for %1.").arg(gui.textUserName->text()) + '\n'
            + tr("Are you sure you want to continue?"), QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (ans != QMessageBox::Yes) return gui.textUserPass;
    }
    if (!automatic && gui.boxRootAccount->isChecked() && passRoot.length()==0) {
        // Confirm that an empty root password is not accidental.
        const QMessageBox::StandardButton ans = QMessageBox::warning(master,
            master->windowTitle(), tr("You did not provide a password for the root account."
                " Do you want to continue?"), QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (ans != QMessageBox::Yes) return gui.textRootPass;
    }

    return nullptr;
}

// setup the user, cannot be rerun
void Oobe::setUserInfo()
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
    if (userPass.isEmpty()) proc.exec("passwd", {"-d", "demo"});
    else {
        if (!userinfo.isEmpty()) userinfo.append('\n');
        userinfo.append(QString("demo:" + userPass).toUtf8());
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
    const bool remasteredDemo = QFileInfo("/live/linux/home/demo").isDir();
    qDebug() << "check for remastered home demo folder:" << remasteredDemo;

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
        if (!remasteredDemo) {
            sect.setExceptionMode(QT_TR_NOOP("Sorry, failed to create user directory."));
            proc.exec("cp", {"-a", skelpath, dpath});
        } else { // still rename the demo directory even if remastered demo home folder is detected
            sect.setExceptionMode(QT_TR_NOOP("Sorry, failed to name user directory."));
            proc.exec("mv", {"-f", rootpath + "/home/demo", dpath});
        }
    }

    sect.setExceptionMode(nullptr);

    // saving Desktop changes
    if (gui.checkSaveDesktop->isChecked()) {
        resetBlueman();
        // rsync home folder
        QString cmd = ("rsync -a --info=name1 /home/demo/ %1"
            " --exclude '.cache' --exclude '.gvfs' --exclude '.dbus' --exclude '.Xauthority' --exclude '.ICEauthority'"
            " --exclude '.mozilla' --exclude 'Installer.desktop' --exclude 'minstall.desktop' --exclude 'Desktop/antixsources.desktop'"
            " --exclude '.idesktop/gazelle.lnk' --exclude '.jwm/menu' --exclude '.icewm/menu' --exclude '.fluxbox/menu'"
            " --exclude '.config/rox.sourceforge.net/ROX-Filer/pb_antiX-fluxbox'"
            " --exclude '.config/rox.sourceforge.net/ROX-Filer/pb_antiX-icewm'"
            " --exclude '.config/rox.sourceforge.net/ROX-Filer/pb_antiX-jwm'"
            " --exclude '.config/session' | xargs -I '$' sed -i 's|home/demo|home/" + username + "|g' %1/$").arg(dpath);
        proc.shell(cmd);
    }

    // check if remaster exists and checkSaveDesktop not checked, modify the remastered demo folder
    if (remasteredDemo && !gui.checkSaveDesktop->isChecked())
    {
        resetBlueman();
        // Change remaster "demo" to new user.
        QString cmd = ("find " + dpath + " -maxdepth 1 -type f -name '.*' -print0 | xargs -0 sed -i 's|home/demo|home/" + username + "|g'").arg(dpath);
        proc.shell(cmd);
        cmd = ("find " + dpath + "/.config -type f -print0 | xargs -0 sed -i 's|home/demo|home/" + username + "|g'").arg(dpath);
        proc.shell(cmd);
        cmd = ("find " + dpath + "/.local -type f  -print0 | xargs -0 sed -i 's|home/demo|home/" + username + "|g'").arg(dpath);
        proc.shell(cmd);
    }

    // fix the ownership, demo=newuser
    sect.setExceptionMode(QT_TR_NOOP("Failed to set ownership or permissions of user directory."));
    proc.exec("chown", {"-R", "demo:demo", dpath});
    // Set permissions according to /etc/adduser.conf
    QSettings addusercfg("/etc/adduser.conf", QSettings::NativeFormat);
    mode_t mode = addusercfg.value("DIR_MODE", "0700").toString().toUInt(&ok, 8);
    if (chmod(dpath.toUtf8().constData(), mode) != 0) throw sect.failMessage();

    // change in files
    sect.setExceptionMode(nullptr);
    replaceStringInFile("demo", username, rootpath + "/etc/group");
    replaceStringInFile("demo", username, rootpath + "/etc/gshadow");
    replaceStringInFile("demo", username, rootpath + "/etc/passwd");
    replaceStringInFile("demo", username, rootpath + "/etc/shadow");
    replaceStringInFile("demo", username, rootpath + "/etc/subuid");
    replaceStringInFile("demo", username, rootpath + "/etc/subgid");
    replaceStringInFile("demo", username, rootpath + "/etc/slim.conf");
    replaceStringInFile("demo", username, rootpath + "/etc/lightdm/lightdm.conf");
    replaceStringInFile("demo", username, rootpath + "/home/*/.gtkrc-2.0");
    replaceStringInFile("demo", username, rootpath + "/root/.gtkrc-2.0");
    if (gui.checkAutoLogin->isChecked()) {
        replaceStringInFile("#auto_login", "auto_login", rootpath + "/etc/slim.conf");
        replaceStringInFile("#default_user ", "default_user ", rootpath + "/etc/slim.conf");
        replaceStringInFile("User=", "User=" + username, rootpath + "/etc/sddm.conf");
    }
    else {
        replaceStringInFile("auto_login", "#auto_login", rootpath + "/etc/slim.conf");
        replaceStringInFile("default_user ", "#default_user ", rootpath + "/etc/slim.conf");
        replaceStringInFile("autologin-user=", "#autologin-user=", rootpath + "/etc/lightdm/lightdm.conf");
        replaceStringInFile("User=.*", "User=", rootpath + "/etc/sddm.conf");
    }
    proc.exec("touch", {rootpath + "/var/mail/" + username});
}

void Oobe::resetBlueman()
{
    proc.exec("runuser", {"-l", "demo", "-c", "dconf reset /org/blueman/transfer/shared-path"}); //reset blueman path
}

/* Slots */

void Oobe::localeIndexChanged(int index) noexcept
{
    // riot control
    QLocale locale(gui.comboLocale->itemData(index).toString());
    if (locale.timeFormat().startsWith('h')) gui.radioClock12->setChecked(true);
    else gui.radioClock24->setChecked(true);
}

void Oobe::timeAreaIndexChanged(int index) noexcept
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

void Oobe::userPassValidationChanged() noexcept
{
    bool ok = !gui.textUserName->text().isEmpty();
    if (ok) ok = passUser.valid() || gui.textUserName->text().isEmpty();
    if (ok && gui.boxRootAccount->isChecked()) {
        ok = passRoot.valid() || gui.textRootPass->text().isEmpty();
    }
    gui.pushNext->setEnabled(ok);
}

void Oobe::oldHomeToggled() noexcept
{
    gui.pushNext->setEnabled(gui.radioOldHomeUse->isChecked()
        || gui.radioOldHomeSave->isChecked() || gui.radioOldHomeDelete->isChecked());
}

/* Helpers*/

bool Oobe::replaceStringInFile(const QString &oldtext, const QString &newtext, const QString &filepath)
{
    return proc.exec("sed", {"-i", QString("s/%1/%2/g").arg(oldtext, newtext), filepath});
}

bool Oobe::containsAnySubstring(const QString& mainString, const QStringList& substrings) {
    for (const QString& substring : substrings) {
        if (mainString.contains(substring)) {
            return true;
        }
    }
    return false;
}
