/****************************************************************************
 *  Copyright (C) 2003-2010 by Warren Woodford
 *  Heavily edited, with permision, by anticapitalista for antiX 2011-2014.
 *  Heavily revised by dolphin oracle, adrian, and anticaptialista 2018.
 *  Major GUI update and user experience improvements by AK-47 2019.
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
 ****************************************************************************/
#include "ui_meinstall.h"
#include "mprocess.h"

class MInstall : public QDialog {
    Q_OBJECT
protected:
    void changeEvent(QEvent *event) noexcept;
    void closeEvent(QCloseEvent * event) noexcept;
    void reject() noexcept;

public:
    MInstall(class QSettings &acfg, const class QCommandLineParser &args, const QString &cfgfile) noexcept;
    ~MInstall();

    QString PROJECTFORUM;
    QString PROJECTNAME;
    QString PROJECTSHORTNAME;
    QString PROJECTURL;
    QString PROJECTVERSION;

    int showPage(int curr, int next) noexcept;
    void gotoPage(int next) noexcept;
    void pageDisplayed(int next) noexcept;

private:
    Ui::MeInstall gui;
    MProcess proc;
    class QSettings &appConf;
    const class QCommandLineParser &appArgs;
    enum Phase {
        PH_STARTUP = -1, // Must be less than Ready. -1 for log cosmetics.
        PH_READY, // Must be less than all install phases. 0 for log cosmetics.
        PH_PREPARING,
        PH_INSTALLING,
        PH_WAITING_FOR_INFO,
        PH_CONFIGURING,
        PH_OUT_OF_BOX,
        PH_FINISHED = 99 // Must be the largest. 99 for log cosmetics.
    } phase = PH_STARTUP;
    enum Abortion {
        AB_NO_ABORT,
        AB_ABORTING,
        AB_CLOSING,
        AB_ABORTED
    } abortion = AB_NO_ABORT;

    // command line options
    bool pretend, automatic;
    bool oem, modeOOBE, mountkeep;
    // configuration management
    QString configFile;
    enum ConfigAction { CONFIG_SAVE, CONFIG_LOAD1, CONFIG_LOAD2 };

    // auto-mount setup
    QString listMaskedMounts;
    bool autoMountEnabled = true;

    QWidget *nextFocus = nullptr;
    class CheckMD5 *checkmd5 = nullptr;
    class PartMan *partman = nullptr;
    class AutoPart *autopart = nullptr;
    class Base *base = nullptr;
    class Oobe *oobe = nullptr;
    class BootMan *bootman = nullptr;
    class SwapMan *swapman = nullptr;
    class PassEdit *passCrypto = nullptr;

    QPixmap helpBackdrop;
    // Splash screen
    QTimer *throbber = nullptr;
    int throbPos = 0;
    // for the tips display
    int ixTip = -1, ixTipStart = -1;
    int iLastProgress = -1;

    // info needed for Phase 2 of the process
    bool haveOldHome = false;

    void startup();
    void splashSetThrobber(bool active) noexcept;
    void progressUpdate(int value) noexcept;
    void setupkeyboardbutton() noexcept;
    void runKeyboardSetup() noexcept;
    void abortUI(bool manual, bool closing) noexcept;
    void abortEndUI(bool closenow) noexcept;
    void cleanup();
    // private functions
    void setupAutoMount(bool enabled);
    void processNextPhase() noexcept;
    void pretendNextPhase() noexcept;
    void manageConfig(enum ConfigAction mode) noexcept;
    bool eventFilter(QObject *watched, QEvent *event) noexcept;
};
