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

#include <QCommandLineParser>
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
    MInstall(const QCommandLineParser &args, const QString &cfgfile);
    ~MInstall();

    // helpers
    bool replaceStringInFile(const QString &oldtext, const QString &newtext, const QString &filepath);
    static QString sliderSizeString(long long size);

    bool isInsideVB();

    bool installLoader();
    void enableOOBE();
    bool processOOBE();
    bool validateUserInfo();
    bool validateComputerName();
    bool setComputerName();
    bool setUserInfo();
    void buildBootLists();
    void buildServiceList();
    void disablehiberanteinitramfs();
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
    void on_splitter_splitterMoved(int, int);
    void on_mainTabs_currentChanged(int index);

    void on_abortInstallButton_clicked();
    void on_backButton_clicked();
    void on_buttonSetKeyboard_clicked();
    void on_closeButton_clicked();
    void on_nextButton_clicked();
    void on_passwordCheckBox_stateChanged(int);
    void on_buttonPartReload_clicked();
    void on_buttonRunParted_clicked();
    void on_viewServicesButton_clicked();

    void on_sliderPart_valueChanged(int value);
    void on_checkBoxEncryptAuto_toggled(bool checked);
    void on_customPartButton_clicked(bool checked);

    void on_buttonBenchmarkFDE_clicked();
    void on_buttonAdvancedFDE_clicked();
    void on_buttonAdvancedFDECust_clicked();
    void on_comboFDEcipher_currentIndexChanged(const QString &arg1);
    void on_comboFDEchain_currentIndexChanged(const QString &arg1);
    void on_spinFDEkeysize_valueChanged(int i);
    void on_comboFDEivgen_currentIndexChanged(const QString &arg1);

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
    bool brave, pretend, automatic, nocopy, sync;
    bool oem, oobe;
    // configuration management
    MSettings *config = nullptr;
    enum ConfigAction { ConfigSave, ConfigLoadA, ConfigLoadB };

    bool uefi = false;
    bool mactest = false;
    bool containsSystemD = false;
    bool isRemasteredDemoPresent = false;

    // source medium
    QString rootSources;
    QString bootSource;

    // auto-mount setup
    QString listMaskedMounts;
    bool autoMountEnabled = true;

    QWidget *nextFocus = nullptr;
    PartMan partman;
    BlockDeviceList listBlkDevs;
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
    void diskPassValidationChanged(bool valid);
    void userPassValidationChanged();
    // private functions
    void updateCursor(const Qt::CursorShape shape = Qt::ArrowCursor);
    void updatePartitionWidgets(bool all);
    void updatePartitionCombos(QComboBox *changed);
    void setupPartitionSlider();
    void setupAutoMount(bool enabled);
    bool pretendToInstall(int start, int stop);
    bool saveHomeBasic();
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
