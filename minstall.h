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
#include "core.h"
#include "partman.h"
#include <QVector>

// Forward declare UI wrapper classes for TUI support
namespace ui { class QCheckBox; }
namespace ui { class QLabel; }
namespace ui { class QRadioButton; }
namespace ui { class QLineEdit; }
namespace ui { class QComboBox; }
namespace ui { class QSlider; }
namespace qtui { class TButtonGroup; }
namespace qtui { class TPushButton; }
namespace qtui { class TLineEdit; }
class QTreeWidgetItem;

class MInstall : public QDialog {
    Q_OBJECT
protected:
    void changeEvent(QEvent *event) noexcept;
    void closeEvent(QCloseEvent * event) noexcept;
    void reject() noexcept;

public:
    MInstall(class MIni &acfg, const class QCommandLineParser &args, const QString &cfgfile, QWidget *parent = nullptr) noexcept;
    ~MInstall();

    static constexpr int TUI_KEY_ALT_LEFT = 10001;

    QString PROJECTFORUM;
    QString PROJECTNAME;
    QString PROJECTSHORTNAME;
    QString PROJECTURL;
    QString PROJECTVERSION;

    int showPage(int curr, int next) noexcept;
    void gotoPage(int next) noexcept;
    void pageDisplayed(int next) noexcept;

    // TUI-specific methods
    void renderCurrentPage() noexcept;
    void handleInput(int key) noexcept;  // Handle keyboard input for current page
    void handleMouse(int mouseY, int mouseX, int mouseState) noexcept;
    bool processDeferredActions() noexcept;
    bool shouldExit() const noexcept { return tui_exitRequested; }
    int getCurrentPage() const noexcept { return currentPageIndex; }
    bool tuiWantsEsc() const noexcept;
    bool canGoBack() const noexcept;  // Check if backward navigation is allowed
    bool canGoNext() const noexcept;  // Check if forward navigation is allowed

private:
    void gotoAfterPartitionsTui() noexcept;
    Ui::MeInstall gui;
    MProcess proc;
    Core core;
    class MIni &appConf;
    const class QCommandLineParser &appArgs;
    int currentPageIndex = 0;  // Track current page for TUI rendering
    int tui_focusInstallation = 0;
    int tui_focusInstallationField = 0;
    int tui_focusEncryption = 0;
    QString tui_encryptionError;
    QString tui_networkError;
    QString tui_userError;
    QString tui_oldHomeError;
    bool tui_confirmEmptyUserPass = false;
    bool tui_confirmEmptyRootPass = false;
    int tui_focusBoot = 0;
    int tui_focusBootField = 0;
    int tui_focusSwap = 0;
    int tui_focusLocalization = 0;
    int tui_focusNetwork = 0;
    int tui_focusUserAccounts = 0;
    int tui_focusServices = 0;
    int tui_focusOldHome = 0;
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
    bool advanced = false; // Always enabled with custom partitions.
    // configuration management
    QString configFile;

    // auto-mount setup
    QString listMaskedMounts;
    bool autoMountEnabled = true;

    QWidget *nextFocus = nullptr;
    class CheckMD5 *checkmd5 = nullptr;
    class PartMan *partman = nullptr;
    class AutoPart *autopart = nullptr;
    class Replacer *replacer = nullptr;
    class Crypto *crypto = nullptr;
    class Base *base = nullptr;
    class Oobe *oobe = nullptr;
    class BootMan *bootman = nullptr;
    class SwapMan *swapman = nullptr;

    // TUI wrapper widgets for pageEnd
    class ui::QCheckBox *tui_checkExitReboot = nullptr;
    class ui::QLabel *tui_textReminders = nullptr;
    
    // TUI wrapper widgets for pageTerms
    class ui::QLabel *tui_textCopyright = nullptr;
    class ui::QLabel *tui_keyboardInfo = nullptr;
    
    // TUI wrapper widgets for pageSplash
    class ui::QLabel *tui_labelSplash = nullptr;

    // TUI wrapper widgets for pageInstallation
    class ui::QRadioButton *tui_radioEntireDrive = nullptr;
    class ui::QRadioButton *tui_radioCustomPart = nullptr;
    class ui::QRadioButton *tui_radioReplace = nullptr;
    class ui::QLabel *tui_labelInstallType = nullptr;
    qtui::TButtonGroup *tui_installButtonGroup = nullptr;
    class ui::QCheckBox *tui_checkDualDrive = nullptr;
    class ui::QLabel *tui_labelDriveSystem = nullptr;
    class ui::QComboBox *tui_comboDriveSystem = nullptr;
    class ui::QLabel *tui_labelDriveHome = nullptr;
    class ui::QComboBox *tui_comboDriveHome = nullptr;
    class ui::QSlider *tui_sliderPart = nullptr;
    class ui::QCheckBox *tui_checkEncryptAuto = nullptr;

    // TUI wrapper widgets for pageConfirm
    class ui::QLabel *tui_labelConfirmTitle = nullptr;
    class ui::QLabel *tui_labelConfirmInfo = nullptr;

    // TUI wrapper widgets for pageUserAccounts
    class ui::QLabel *tui_labelUserName = nullptr;
    class ui::QLabel *tui_labelUserPass = nullptr;
    class ui::QLabel *tui_labelUserPass2 = nullptr;
    class ui::QLineEdit *tui_textUserName = nullptr;
    class ui::QLineEdit *tui_textUserPass = nullptr;
    class ui::QLineEdit *tui_textUserPass2 = nullptr;
    class ui::QCheckBox *tui_checkAutoLogin = nullptr;
    class ui::QCheckBox *tui_checkRootAccount = nullptr;
    class ui::QLabel *tui_labelRootPass = nullptr;
    class ui::QLineEdit *tui_textRootPass = nullptr;
    class ui::QLabel *tui_labelRootPass2 = nullptr;
    class ui::QLineEdit *tui_textRootPass2 = nullptr;
    class ui::QCheckBox *tui_checkSaveDesktop = nullptr;

    // TUI wrapper widgets for pageOldHome
    class ui::QLabel *tui_labelOldHomeTitle = nullptr;
    class ui::QLabel *tui_labelOldHomeInfo = nullptr;
    class ui::QRadioButton *tui_radioOldHomeUse = nullptr;
    class ui::QRadioButton *tui_radioOldHomeSave = nullptr;
    class ui::QRadioButton *tui_radioOldHomeDelete = nullptr;

    // TUI wrapper widgets for pageTips
    class ui::QLabel *tui_labelTipsTitle = nullptr;
    class ui::QLabel *tui_labelTipsInfo = nullptr;
    class ui::QCheckBox *tui_checkTipsReboot = nullptr;

    // TUI wrapper widgets for pageNetwork
    class ui::QLabel *tui_labelComputerName = nullptr;
    class ui::QLabel *tui_labelComputerDomain = nullptr;
    class ui::QLineEdit *tui_textComputerName = nullptr;
    class ui::QLineEdit *tui_textComputerDomain = nullptr;
    class ui::QLabel *tui_labelComputerGroup = nullptr;
    class ui::QLineEdit *tui_textComputerGroup = nullptr;
    class ui::QCheckBox *tui_checkSamba = nullptr;

    // TUI wrapper widgets for pageBoot
    class ui::QLabel *tui_labelBootTitle = nullptr;
    class ui::QLabel *tui_labelBootLocation = nullptr;
    class ui::QComboBox *tui_comboBoot = nullptr;
    class ui::QRadioButton *tui_radioBootMBR = nullptr;
    class ui::QRadioButton *tui_radioBootPBR = nullptr;
    class ui::QRadioButton *tui_radioBootESP = nullptr;
    class ui::QCheckBox *tui_checkBootInstall = nullptr;
    class ui::QCheckBox *tui_checkBootHostSpecific = nullptr;
    qtui::TButtonGroup *tui_bootButtonGroup = nullptr;

    // TUI wrapper widgets for pageLocalization
    class ui::QLabel *tui_labelLocaleTitle = nullptr;
    class ui::QLabel *tui_labelLocale = nullptr;
    class ui::QComboBox *tui_comboLocale = nullptr;
    class ui::QLabel *tui_labelTimeArea = nullptr;
    class ui::QComboBox *tui_comboTimeArea = nullptr;
    class ui::QLabel *tui_labelTimeZone = nullptr;
    class ui::QComboBox *tui_comboTimeZone = nullptr;
    class ui::QCheckBox *tui_checkLocalClock = nullptr;
    class ui::QLabel *tui_labelClockFormat = nullptr;
    class ui::QRadioButton *tui_radioClock24 = nullptr;
    class ui::QRadioButton *tui_radioClock12 = nullptr;
    qtui::TButtonGroup *tui_clockButtonGroup = nullptr;

    // TUI wrapper widgets for pageServices
    class ui::QLabel *tui_labelServicesTitle = nullptr;
    class ui::QLabel *tui_labelServicesInfo = nullptr;
    QVector<ui::QLabel*> tui_serviceCategoryLabels;
    struct TuiServiceItem {
        class ui::QCheckBox *checkbox = nullptr;
        QTreeWidgetItem *item = nullptr;
        int row = 0;
    };
    QVector<TuiServiceItem> tui_serviceItems;
    bool tui_servicesBuilt = false;

    // TUI wrapper widgets for pageReplace
    class ui::QLabel *tui_labelReplaceTitle = nullptr;
    class ui::QCheckBox *tui_checkReplacePackages = nullptr;
    int tui_focusReplace = 0;  // Selected installation index
    bool tui_replaceScanning = false;
    bool tui_replaceScanPending = false;
    bool tui_exitRequested = false;
    bool tui_localizationInitialized = false;
    int tui_spinnerTick = 0;
    bool tui_installStarting = false;
    int tui_installStartTicks = 0;
    int tui_deferredPage = -1;

    // TUI wrapper widgets for pagePartitions
    class ui::QLabel *tui_labelPartitionsInfo = nullptr;
    enum PartitionColumn {
        PART_COL_SIZE = 0,
        PART_COL_USEFOR = 1,
        PART_COL_LABEL = 2,
        PART_COL_FORMAT = 3
    };
    enum PartitionSizeUnit {
        SIZE_UNIT_MB = 0,
        SIZE_UNIT_GB = 1
    };
    int tui_partitionRow = 0;        // Currently selected row in partition list
    int tui_partitionCol = 0;        // Currently selected column (for editing)
    int tui_partitionScroll = 0;     // Scroll offset for the partition list
    bool tui_partitionEditing = false; // True when editing a cell
    int tui_partitionEditIndex = 0;  // Index in dropdown when editing
    bool tui_partitionEditIsLabel = false;
    bool tui_partitionEditIsSize = false;
    PartitionSizeUnit tui_partitionSizeUnit = SIZE_UNIT_MB;
    QString tui_partitionSizeOriginal;
    QString tui_partitionLabelOriginal;
    qtui::TLineEdit *tui_partitionLabelEdit = nullptr;
    qtui::TLineEdit *tui_partitionSizeEdit = nullptr;
    bool tui_partitionPopupVisible = false;
    int tui_partitionPopupRow = 0;
    int tui_partitionPopupCol = 0;
    int tui_partitionPopupHeight = 0;
    int tui_partitionPopupScroll = 0;
    qtui::TPushButton *tui_buttonPartitionsApply = nullptr;
    int tui_focusPartitions = 0;     // 0 = table, 1 = apply button
    bool tui_partitionUnlocking = false;
    bool tui_unlockFocusPass = true;
    bool tui_unlockAddCrypttab = true;
    PartMan::Device *tui_unlockDevice = nullptr;
    QString tui_unlockError;
    qtui::TLineEdit *tui_unlockPassEdit = nullptr;

    // TUI wrapper widgets for pageSwap
    class ui::QLabel *tui_labelSwapTitle = nullptr;
    class ui::QCheckBox *tui_checkSwapFile = nullptr;
    class ui::QLabel *tui_labelSwapLocation = nullptr;
    class ui::QLineEdit *tui_textSwapFile = nullptr;
    class ui::QLabel *tui_labelSwapSize = nullptr;
    class ui::QLineEdit *tui_textSwapSize = nullptr;
    class ui::QLabel *tui_labelSwapMax = nullptr;
    class ui::QLabel *tui_labelSwapReset = nullptr;
    class ui::QCheckBox *tui_checkHibernation = nullptr;
    class ui::QLabel *tui_labelZramTitle = nullptr;
    class ui::QCheckBox *tui_checkZram = nullptr;
    class ui::QRadioButton *tui_radioZramPercent = nullptr;
    class ui::QLineEdit *tui_textZramPercent = nullptr;
    class ui::QLabel *tui_labelZramRecPercent = nullptr;
    class ui::QRadioButton *tui_radioZramSize = nullptr;
    class ui::QLineEdit *tui_textZramSize = nullptr;
    class ui::QLabel *tui_labelZramRecSize = nullptr;
    qtui::TButtonGroup *tui_zramButtonGroup = nullptr;

    // TUI wrapper widgets for pageEncryption
    class ui::QLabel *tui_labelEncryptionTitle = nullptr;
    class ui::QLabel *tui_labelCryptoPass = nullptr;
    class ui::QLineEdit *tui_textCryptoPass = nullptr;
    class ui::QLabel *tui_labelCryptoPass2 = nullptr;
    class ui::QLineEdit *tui_textCryptoPass2 = nullptr;

    QPixmap helpBackdrop;
    // Splash screen
    class Throbber *throbber = nullptr;
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
    void loadConfig(int stage) noexcept;
    void saveConfig() noexcept;
    void runShutdown(const QString &action) noexcept;
    bool eventFilter(QObject *watched, QEvent *event) noexcept;

    // TUI functions
    void setupPageEndTUI() noexcept;
    void setupPageTermsTUI() noexcept;
    void setupPageSplashTUI() noexcept;
    void setupPageInstallationTUI() noexcept;
    void setupPageConfirmTUI() noexcept;
    void setupPageUserAccountsTUI() noexcept;
    void setupPageOldHomeTUI() noexcept;
    void setupPageTipsTUI() noexcept;
    void setupPageNetworkTUI() noexcept;
    void setupPageBootTUI() noexcept;
    void setupPageLocalizationTUI() noexcept;
    void setupPageServicesTUI() noexcept;
    void setupPageReplaceTUI() noexcept;
    void setupPagePartitionsTUI() noexcept;
    void setupPageSwapTUI() noexcept;
    void setupPageEncryptionTUI() noexcept;
    void syncInstallationTuiFromGui() noexcept;
    void syncSwapTuiFromGui() noexcept;
    void buildServicesTui() noexcept;
    void syncServicesTuiFromGui() noexcept;
    void renderPageEnd() noexcept;
    void renderPageTerms() noexcept;
    void renderPageSplash() noexcept;
    void renderPageInstallation() noexcept;
    void renderPageConfirm() noexcept;
    void renderPageUserAccounts() noexcept;
    void renderPageOldHome() noexcept;
    void renderPageTips() noexcept;
    void renderPageNetwork() noexcept;
    void renderPageBoot() noexcept;
    void renderPageLocalization() noexcept;
    void renderPageServices() noexcept;
    void renderPageReplace() noexcept;
    void renderPagePartitions() noexcept;
    void renderPageSwap() noexcept;
    void renderPageEncryption() noexcept;
};
