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
#include <QTimer>
#include <QProgressDialog>
#include <QSettings>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>

#include "ui_meinstall.h"
#include "cmd.h"

class SafeCache : public QByteArray {
public:
    SafeCache();
    ~SafeCache();
    bool load(const char *filename, int length);
    bool save(const char *filename, mode_t mode = 0400);
    void erase();
};

class MInstall : public QWidget, public Ui::MeInstall {
    Q_OBJECT
protected:
    QProcess *proc;
    QTimer *timer;
    QDialog *mmn;
    bool eventFilter(QObject *obj, QEvent *event);

public:
    /** constructor */
    MInstall(QWidget* parent=0, QStringList args = QStringList());
    /** destructor */
    ~MInstall();

    QStringList args;

    void checkUefi();
    void goBack(const QString &msg);
    void unmountGoBack(const QString &msg);

    // helpers
    bool replaceStringInFile(const QString &oldtext, const QString &newtext, const QString &filepath);
    int runCmd(const QString &cmd);
    bool runProc(const QString &cmd, const QByteArray &input = QByteArray());
    void csleep(int msec);
    QString getCmdOut(const QString &cmd);
    QStringList getCmdOuts(const QString &cmd);
    static int command(const QString &string);
    int getPartitionNumber();

    bool isInsideVB();
    bool isGpt(const QString &drv);

    bool checkDisk();
    bool checkPassword(const QString &pass);
    bool installLoader();
    bool validateChosenPartitions();
    bool makeChosenPartitions(QString &rootType, QString &homeType, bool &formatBoot, bool &formatSwap);
    bool makeDefaultPartitions(bool &formatBoot);
    bool makeEsp(const QString &drv, int size);
    bool formatPartitions(const QByteArray &encPass, const QString &rootType, const QString &homeType, bool formatBoot, bool formatSwap);
    bool makeLinuxPartition(const QString &dev, const QString &type, bool bad, const QString &label);
    bool makeLuksPartition(const QString &dev, const QByteArray &password);
    bool openLuksPartition(const QString &dev, const QString &fs_name, const QByteArray &password, const QString &options = QString());
    bool mountPartition(const QString dev, const QString point, const QString mntops);
    bool validateUserInfo();
    bool validateComputerName();
    bool setComputerName();
    bool setPasswords();
    bool setUserInfo();
    bool setUserName();
    void addItemCombo(QComboBox *cb, const QString *part);
    void buildBootLists();
    void buildServiceList();
    bool copyLinux();
    void disablehiberanteinitramfs();
    bool installLinux();
    void makeFstab();
    bool processNextPhase();
    void removeItemCombo(QComboBox *cb, const QString *part);
    void saveConfig();
    void setLocale();
    void setServices();
    void updatePartCombo(QString *prevItem, const QString &part);
    void updatePartitionWidgets();
    void loadAdvancedFDE();
    void saveAdvancedFDE();
    void writeKeyFile();

    bool INSTALL_FROM_ROOT_DEVICE;
    bool POPULATE_MEDIA_MOUNTPOINTS;

    QString DEFAULT_HOSTNAME;
    QString MIN_BOOT_DEVICE_SIZE;
    QString MIN_INSTALL_SIZE;
    QString MIN_ROOT_DEVICE_SIZE;
    QString PREFERRED_MIN_INSTALL_SIZE;
    QString PROJECTFORUM;
    QString PROJECTNAME;
    QString PROJECTSHORTNAME;
    QString PROJECTURL;
    QString PROJECTVERSION;
    QString getPartType(const QString &dev);
    QStringList ENABLE_SERVICES;
    bool REMOVE_NOSPLASH;

    // global for now until boot combo box is sorted out
    QString bootdev;
    QString swapDevicePreserve;
    QString rootDevicePreserve;
    QString homeDevicePreserve;

    int showPage(int curr, int next);
    void firstRefresh(QDialog *main);
    void gotoPage(int next);
    void pageDisplayed(int next);
    void updateDiskInfo();
    void setupkeyboardbutton();
    bool abort(bool onclose);

public slots:
    void cleanup();
    void copyTime();

private slots:
    void on_abortInstallButton_clicked();
    void on_backButton_clicked();
    void on_buttonSetKeyboard_clicked();
    void on_closeButton_clicked();
    void on_nextButton_clicked();
    void on_passwordCheckBox_stateChanged(int);
    void on_qtpartedButton_clicked();
    void on_viewServicesButton_clicked();


    void on_homeCombo_currentIndexChanged(const QString &arg1);
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

    void updatePartInfo();
    void on_rootTypeCombo_activated(QString item = "");
    void on_rootCombo_activated(const QString &arg1 = "");
    void on_homeCombo_activated(const QString &arg1);
    void on_swapCombo_activated(const QString &arg1);
    void on_bootCombo_activated(const QString &arg1);

    void on_grubCheckBox_toggled(bool checked);
    void on_grubMbrButton_toggled();
    void on_grubPbrButton_toggled();
    void on_grubEspButton_toggled();

    void on_progressBar_valueChanged(int value);

private:
    int phase = 0;
    bool isHomeEncrypted = false;
    bool isRootEncrypted = false;
    bool isSwapEncrypted = false;
    bool isRootFormatted = false;
    bool isHomeFormatted = false;
    bool uefi = false;
    bool haveSysConfig = false;

    QWidget *nextFocus = NULL;
    Cmd shell;
    QString home_mntops = "defaults";
    QString root_mntops = "defaults";
    QStringList listHomes;
    SafeCache key;

    // for file copy progress updates
    fsfilcnt_t iTargetInodes = 0;
    int iCopyBarStart;

    // for the tips display
    int ixTip = 0;
    int ixTipStart = -1;
    int iLastProgress = -1;

    // for partition combo updates
    QHash<QString, int> removedRoot; // remember items removed from combo box: item, index
    QHash<QString, int> removedHome;
    QHash<QString, int> removedSwap;
    QHash<QString, int> removedBoot;
    QSettings *config;
    QString auto_mount;
    QString prevItemRoot; // remember previously selected item in combo box
    QString prevItemHome;
    QString prevItemSwap;
    QString prevItemBoot;
    int indexPartInfoDisk = -1;

    // info needed for Phase 2 of the process
    QStringList listBootDrives;
    QStringList listBootESP;
    QStringList listBootPart;
    bool haveSamba = false;
    bool haveSnapshotUserAccounts = false;
    enum OldHomeAction {
        OldHomeUse, OldHomeSave, OldHomeDelete
    } oldHomeAction;

    // Advanced Encryption Settings page
    int ixPageRefAdvancedFDE = 0;
    int indexFDEcipher = -1;
    int indexFDEchain = -1;
    int indexFDEivgen = -1;
    int indexFDEivhash = -1;
    int iFDEkeysize = -1;
    int indexFDEhash = -1;
    int indexFDErandom = -1;
    long iFDEroundtime = -1;

    // private functions
    void updateStatus(const QString &msg, int val = -1);
    bool pretendToInstall(int start, int stop, int sleep);
    void prepareToInstall(const bool pretend);
};
