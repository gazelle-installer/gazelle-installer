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

class MInstall : public QDialog, public Ui::MeInstall {
    Q_OBJECT
protected:
    void changeEvent(QEvent *event) noexcept;
    void closeEvent(QCloseEvent * event) noexcept;
    void reject() noexcept;

public:
    MInstall(class QSettings &acfg, const class QCommandLineParser &args, const QString &cfgfile) noexcept;
    ~MInstall();

    // helpers
    bool processNextPhase() noexcept;

    QString PROJECTFORUM;
    QString PROJECTNAME;
    QString PROJECTSHORTNAME;
    QString PROJECTURL;
    QString PROJECTVERSION;

    int showPage(int curr, int next) noexcept;
    void gotoPage(int next) noexcept;
    void pageDisplayed(int next) noexcept;
    void setupkeyboardbutton() noexcept;
    bool abortUI() noexcept;
    void cleanup(bool endclean = true);

private slots:
    void on_pushAbort_clicked() noexcept;
    void on_pushBack_clicked() noexcept;
    void on_pushSetKeyboard_clicked() noexcept;
    void on_pushNext_clicked() noexcept;
    void on_pushServices_clicked() noexcept;

    void on_radioEntireDisk_toggled(bool checked) noexcept;

    void on_progInstall_valueChanged(int value) noexcept;

private:
    MProcess proc;
    class QSettings &appConf;
    const class QCommandLineParser &appArgs;
    int phase = 0;

    // command line options
    bool pretend, automatic;
    bool oem, modeOOBE, mountkeep;
    // configuration management
    class MSettings *config = nullptr;
    enum ConfigAction { ConfigSave, ConfigLoadA, ConfigLoadB };

    // auto-mount setup
    QString listMaskedMounts;
    bool autoMountEnabled = true;

    QWidget *nextFocus = nullptr;
    class PartMan *partman = nullptr;
    class AutoPart *autopart = nullptr;
    class Base *base = nullptr;
    class Oobe *oobe = nullptr;
    class BootMan *bootman = nullptr;
    class SwapMan *swapman = nullptr;
    QStringList listHomes;

    QPixmap helpBackdrop;
    // Splash screen
    int throbPos = 0;
    QTimer *throbber = nullptr;
    // for the tips display
    int ixTip = 0;
    int ixTipStart = -1;
    int iLastProgress = -1;

    // info needed for Phase 2 of the process
    bool haveOldHome = false;

    void startup() noexcept;
    void splashSetThrobber(bool active) noexcept;
    void splashThrob() noexcept;
    // private functions
    void updateCursor(const Qt::CursorShape shape = Qt::ArrowCursor) noexcept;
    void setupAutoMount(bool enabled);
    bool pretendToInstall(int space, long steps) noexcept;
    bool saveHomeBasic();
    void manageConfig(enum ConfigAction mode) noexcept;
    bool eventFilter(QObject *watched, QEvent *event) noexcept;
};
