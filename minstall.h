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

#include "mprocess.h"
#include "msettings.h"
#include "partman.h"
#include "blockdev.h"
#include "safecache.h"

#include "ui_meinstall.h"

class MInstall : public QDialog, public Ui::MeInstall {
    Q_OBJECT
protected:
    void changeEvent(QEvent *event);
    void resizeEvent(QResizeEvent *);
    void closeEvent(QCloseEvent * event);
    void reject();

public:
    MInstall(const QStringList &args, const QString &cfgfile);
    ~MInstall();

    // helpers
    bool replaceStringInFile(const QString &oldtext, const QString &newtext, const QString &filepath);

    bool isInsideVB();

    bool checkTargetDrivesOK();
    bool installLoader();
    bool makeLinuxPartition(const QString &dev, const QString &type, bool chkBadBlocks, const QString &label);
    bool makeLuksPartition(const QString &dev, const QByteArray &password);
    bool openLuksPartition(const QString &dev, const QString &fs_name, const QByteArray &password, const QString &options = QString(), const bool failHard = true);
    bool mountPartition(const QString dev, const QString point, const QString mntops);
    void enableOOBE();
    bool processOOBE();
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
    QString SQFILE_FULL;


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

    void on_localeCombo_currentIndexChanged(int index);
    void on_cmbTimeArea_currentIndexChanged(int index);

    void on_radioOldHomeUse_toggled(bool);
    void on_radioOldHomeSave_toggled(bool);
    void on_radioOldHomeDelete_toggled(bool);

    void on_progressBar_valueChanged(int value);


private:
    MProcess proc;
    int phase = 0;

    // command line options
    bool brave, pretend, automatic, nocopy, sync, gptoverride;
    bool oem, oobe;
    // configuration management
    MSettings *config = nullptr;
    enum ConfigAction { ConfigSave, ConfigLoadA, ConfigLoadB };

    bool isHomeEncrypted = false;
    bool isRootEncrypted = false;
    bool isSwapEncrypted = false;
    bool uefi = false;
    bool mactest = false;
    bool containsSystemD = false;
    bool isRemasteredDemoPresent = false;

    // source medium
    QString rootSources;
    QString bootSource;
    long long rootSpaceNeeded = 0;
    long long bootSpaceNeeded = 0;

    // auto-mount setup
    QString listMaskedMounts;
    bool autoMountEnabled = true;

    // if these variables are non-zero then the installer formats the partition
    // if they are negative the installer formats an existing partition
    long long rootFormatSize = 0;
    long long homeFormatSize = 0;
    long long swapFormatSize = 0;
    long long bootFormatSize = 0;
    long long espFormatSize = 0;

    QString bootDevice;
    QString swapDevice;
    QString rootDevice;
    QString homeDevice;
    QString espDevice;
    QString biosGrubDevice;

    QWidget *nextFocus = nullptr;
    PartMan partman;
    BlockDeviceList listBlkDevs;
    QStringList listToUnmount;
    QString home_mntops = "defaults";
    QString root_mntops = "defaults";
    QString boot_mntops = "defaults";
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
    bool haveOldHome = false;

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

    // cached time zone list
    QStringList listTimeZones;

    // slots
    void startup();
    // helpers
    bool checkPassword(QLineEdit *passEdit);
    // private functions
    void updateStatus(const QString &msg, int val = -1);
    void updateCursor(const Qt::CursorShape shape = Qt::ArrowCursor);
    void updatePartitionWidgets();
    void updatePartitionCombos(QComboBox *changed);
    void setupAutoMount(bool enabled);
    bool pretendToInstall(int start, int stop);
    bool saveHomeBasic();
    bool calculateDefaultPartitions();
    bool makePartitions();
    bool formatPartitions();
    bool installLinux(const int progend);
    bool copyLinux(const int progend);
    void failUI(const QString &msg);
    void manageConfig(enum ConfigAction mode);
    void stashServices(bool save);
    void stashAdvancedFDE(bool save);
    int selectTimeZone(const QString &zone);
    void clearpartitiontables(const QString &dev);
    bool checkForSnapshot();
    bool checkForRemaster();
    void rsynchomefolder(const QString dpath);
    void changeRemasterdemoToNewUser(const QString dpath);
    void resetBlueman();
};
