//
//   Copyright (C) 2003-2009 by Warren Woodford
//   Heavily edited, with permision, by anticapitalista for antiX 2011-2014.
//   Heavily revised by dolphin oracle, adrian, and anticaptialista 2018.
//   Licensed under the Apache License, Version 2.0 (the "License");
//   you may not use this file except in compliance with the License.
//   You may obtain a copy of the License at
//
//       http://www.apache.org/licenses/LICENSE-2.0
//
//   Unless required by applicable law or agreed to in writing, software
//   distributed under the License is distributed on an "AS IS" BASIS,
//   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//   See the License for the specific language governing permissions and
//   limitations under the License.
//

#include <QFile>
#include <QMessageBox>
#include <QProcess>
#include <QSettings>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>

#include "msettings.h"
#include "ui_meinstall.h"

class SafeCache : public QByteArray {
public:
    SafeCache();
    ~SafeCache();
    bool load(const char *filename, int length);
    bool save(const char *filename, mode_t mode = 0400);
    void erase();
};

struct BlockDeviceInfo {
    QString name;
    QString fs;
    QString label;
    QString model;
    qint64 size;
    bool isFuture = false;
    bool isNasty = false;
    bool isDisk = false;
    bool isGPT = false;
    bool isBoot = false;
    bool isESP = false;
    bool isNative = false;
    bool isSwap = false;
    void addToCombo(QComboBox *combo, bool warnNasty = false) const;
};

class BlockDeviceList : public QList<BlockDeviceInfo> {
public:
    int findDevice(const QString &devname) const;
};

class MInstall : public QDialog, public Ui::MeInstall {
    Q_OBJECT
protected:
    void changeEvent(QEvent *event);
    void resizeEvent(QResizeEvent *);
    void closeEvent(QCloseEvent * event);
    void reject();

public:
    MInstall(const QStringList &args, const QString &cfgfile = "/etc/minstall.conf");
    ~MInstall();

    // helpers
    bool replaceStringInFile(const QString &oldtext, const QString &newtext, const QString &filepath);
    void csleep(int msec);
    QStringList getCmdOuts(const QString &cmd);

    bool isInsideVB();
    bool isGpt(const QString &drv);

    bool checkDisk();
    bool installLoader();
    bool makeLinuxPartition(const QString &dev, const QString &type, bool bad, const QString &label);
    bool makeLuksPartition(const QString &dev, const QByteArray &password);
    bool openLuksPartition(const QString &dev, const QString &fs_name, const QByteArray &password, const QString &options = QString(), const bool failHard = true);
    bool mountPartition(const QString dev, const QString point, const QString mntops);
    bool validateUserInfo();
    bool validateComputerName();
    bool setComputerName();
    bool setUserInfo();
    void buildBootLists();
    void buildServiceList();
    void disablehiberanteinitramfs();
    void makeFstab();
    bool processNextPhase();
    void setLocale();
    void setServices();
    void writeKeyFile();

    bool INSTALL_FROM_ROOT_DEVICE;
    bool POPULATE_MEDIA_MOUNTPOINTS;

    qlonglong MIN_BOOT_DEVICE_SIZE;
    qlonglong MIN_ROOT_DEVICE_SIZE;
    QString DEFAULT_HOSTNAME;
    QString MIN_INSTALL_SIZE;
    QString PREFERRED_MIN_INSTALL_SIZE;
    QString PROJECTFORUM;
    QString PROJECTNAME;
    QString PROJECTSHORTNAME;
    QString PROJECTURL;
    QString PROJECTVERSION;
    QStringList ENABLE_SERVICES;
    bool REMOVE_NOSPLASH;

    // global for now until boot combo box is sorted out
    QString bootDevice;
    QString swapDevice;
    QString rootDevice;
    QString homeDevice;
    QString espDevice;

    int showPage(int curr, int next);
    void gotoPage(int next);
    void pageDisplayed(int next);
    void setupkeyboardbutton();
    bool abort(bool onclose);
    void cleanup(bool endclean = true);

private slots:
    void on_abortInstallButton_clicked();
    void on_backButton_clicked();
    void on_buttonSetKeyboard_clicked();
    void on_closeButton_clicked();
    void on_nextButton_clicked();
    void on_passwordCheckBox_stateChanged(int);
    void on_qtpartedButton_clicked();
    void on_viewServicesButton_clicked();

    void on_userPasswordEdit2_textChanged(const QString &arg1);
    void on_rootPasswordEdit2_textChanged(const QString &arg1);
    void on_userPasswordEdit_textChanged();
    void on_rootPasswordEdit_textChanged();

    void on_checkBoxEncryptAuto_toggled(bool checked);
    void on_existing_partitionsButton_clicked(bool checked);

    void on_FDEpassword_textChanged();
    void on_FDEpassword2_textChanged(const QString &arg1);
    void on_FDEpassCust_textChanged();
    void on_FDEpassCust2_textChanged(const QString &arg1);
    void on_buttonBenchmarkFDE_clicked();
    void on_buttonAdvancedFDE_clicked();
    void on_buttonAdvancedFDECust_clicked();
    void on_comboFDEcipher_currentIndexChanged(const QString &arg1);
    void on_comboFDEchain_currentIndexChanged(const QString &arg1);
    void on_spinFDEkeysize_valueChanged(int i);
    void on_comboFDEivgen_currentIndexChanged(const QString &arg1);

    void on_checkBoxEncryptRoot_toggled(bool checked);
    void on_checkBoxEncryptHome_toggled(bool checked);
    void on_checkBoxEncryptSwap_toggled(bool checked);

    void on_rootTypeCombo_activated(QString item = "");
    void on_rootCombo_currentIndexChanged(const QString &text);
    void on_homeCombo_currentIndexChanged(const QString &text);
    void on_swapCombo_currentIndexChanged(const QString &text);
    void on_bootCombo_currentIndexChanged(int);

    void on_grubCheckBox_toggled(bool checked);
    void on_grubMbrButton_toggled();
    void on_grubPbrButton_toggled();
    void on_grubEspButton_toggled();

    void on_progressBar_valueChanged(int value);

private:
    QProcess *proc;
    int phase = 0;

    // command line options
    bool brave, pretend, automatic, nocopy, sync;
    // configuration management
    MSettings *config = nullptr;
    enum ConfigAction { ConfigSave, ConfigLoadA, ConfigLoadB };

    QString auto_mount;
    bool isHomeEncrypted = false;
    bool isRootEncrypted = false;
    bool isSwapEncrypted = false;
    bool uefi = false;

    // if these variables are non-zero then the installer formats the partition
    // if they are negative the installer formats an existing partition
    qint64 rootFormatSize = 0;
    qint64 homeFormatSize = 0;
    qint64 swapFormatSize = 0;
    qint64 bootFormatSize = 0;
    qint64 espFormatSize = 0;

    QWidget *nextFocus = nullptr;
    BlockDeviceList listBlkDevs;
    QStringList listToUnmount;
    QString home_mntops = "defaults";
    QString root_mntops = "defaults";
    QStringList listHomes;
    SafeCache key;

    // for the tips display
    int ixTip = 0;
    int ixTipStart = -1;
    int iLastProgress = -1;

    // info needed for Phase 2 of the process
    bool canMBR, canPBR, canESP;
    bool haveSamba = false;
    bool haveSnapshotUserAccounts = false;
    enum OldHomeAction {
        OldHomeNothing, OldHomeUse, OldHomeSave, OldHomeDelete
    } oldHomeAction = OldHomeNothing;

    // Advanced Encryption Settings page
    int ixPageRefAdvancedFDE = 0;
    int indexFDEcipher;
    int indexFDEchain;
    int indexFDEivgen;
    int indexFDEivhash;
    int iFDEkeysize;
    int indexFDEhash;
    int indexFDErandom;
    int iFDEroundtime;

    // slots
    void startup();
    // helpers
    bool execute(const QString &cmd, const bool rawexec = false, const QByteArray *input = nullptr, bool needRead = false);
    QString getCmdOut(const QString &cmd, bool everything = false);
    bool checkPassword(QLineEdit *passEdit);
    // private functions
    void updateStatus(const QString &msg, int val = -1);
    void updateCursor(const Qt::CursorShape shape = Qt::ArrowCursor);
    void updatePartitionWidgets();
    void updatePartitionCombos(QComboBox *changed);
    QStringList splitDevice(const QString &device) const;
    void buildBlockDevList();
    bool pretendToInstall(int start, int stop);
    bool saveHomeBasic();
    bool validateChosenPartitions();
    bool calculateDefaultPartitions();
    bool makePartitions();
    bool formatPartitions();
    bool installLinux(const int progend);
    bool copyLinux(const int progend);
    void failUI(const QString &msg);
    void manageConfig(enum ConfigAction mode);
    void stashServices(bool save);
    void stashAdvancedFDE(bool save);
};
