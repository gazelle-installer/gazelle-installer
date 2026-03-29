// Example: Extracting business logic from GUI code
// This shows how to separate UI from installation logic

// === BEFORE: Everything mixed together ===

class OldInstaller : public QWidget {
    Q_OBJECT
    
private slots:
    void onInstallClicked() {
        // UI logic mixed with business logic
        if (encryptCheckBox->isChecked()) {
            // Setup encryption
            system("cryptsetup luksFormat /dev/sda1");
        }
        
        // Format partition
        QString fs = fsComboBox->currentText();
        system(QString("mkfs.%1 /dev/sda1").arg(fs).toUtf8());
        
        // Show progress
        progressBar->setValue(50);
        
        // Install packages
        system("pacstrap /mnt base linux");
        
        progressBar->setValue(100);
        QMessageBox::information(this, "Done", "Installation complete");
    }
    
private:
    QCheckBox *encryptCheckBox;
    QComboBox *fsComboBox;
    QProgressBar *progressBar;
};


// === AFTER: Separated into Core + UI ===

// === core/installer-core.h ===
class InstallerCore : public QObject {
    Q_OBJECT
    
public:
    // Configuration
    void setTargetDevice(const QString &device);
    void setFileSystem(const QString &fs);
    void setEncryption(bool enabled);
    void setEncryptionPassword(const QString &password);
    
    // Installation
    bool install();
    
signals:
    void progressChanged(int percent, const QString &message);
    void installationComplete(bool success, const QString &message);
    void errorOccurred(const QString &error);
    
private:
    QString targetDevice;
    QString fileSystem;
    bool encryption = false;
    QString encryptionPassword;
    
    bool formatPartition();
    bool setupEncryption();
    bool installPackages();
};

// === core/installer-core.cpp ===
bool InstallerCore::install() {
    emit progressChanged(10, "Starting installation...");
    
    if (encryption) {
        emit progressChanged(20, "Setting up encryption...");
        if (!setupEncryption()) {
            emit errorOccurred("Failed to setup encryption");
            return false;
        }
    }
    
    emit progressChanged(40, "Formatting partition...");
    if (!formatPartition()) {
        emit errorOccurred("Failed to format partition");
        return false;
    }
    
    emit progressChanged(60, "Installing packages...");
    if (!installPackages()) {
        emit errorOccurred("Failed to install packages");
        return false;
    }
    
    emit progressChanged(100, "Installation complete!");
    emit installationComplete(true, "System installed successfully");
    return true;
}


// === GUI VERSION ===
// gui/installer-gui.h
class GUIInstaller : public QWidget {
    Q_OBJECT
    
public:
    GUIInstaller(QWidget *parent = nullptr);
    
private slots:
    void onInstallClicked();
    void onProgressChanged(int percent, const QString &message);
    void onComplete(bool success, const QString &message);
    
private:
    InstallerCore *core;  // Shared core logic
    
    // GUI widgets
    QCheckBox *encryptCheckBox;
    QComboBox *fsComboBox;
    QProgressBar *progressBar;
    QLabel *statusLabel;
    QPushButton *installButton;
};

// gui/installer-gui.cpp
GUIInstaller::GUIInstaller(QWidget *parent) : QWidget(parent) {
    core = new InstallerCore(this);
    
    // Connect core signals to GUI
    connect(core, &InstallerCore::progressChanged,
            this, &GUIInstaller::onProgressChanged);
    connect(core, &InstallerCore::installationComplete,
            this, &GUIInstaller::onComplete);
    
    // Setup GUI widgets...
}

void GUIInstaller::onInstallClicked() {
    // Configure core from UI
    core->setEncryption(encryptCheckBox->isChecked());
    core->setFileSystem(fsComboBox->currentText());
    core->setTargetDevice("/dev/sda1");
    
    // Start installation
    installButton->setEnabled(false);
    core->install();
}

void GUIInstaller::onProgressChanged(int percent, const QString &message) {
    progressBar->setValue(percent);
    statusLabel->setText(message);
}

void GUIInstaller::onComplete(bool success, const QString &message) {
    installButton->setEnabled(true);
    QMessageBox::information(this, "Installation", message);
}


// === TUI VERSION ===
// tui/installer-tui.h
class TUIInstaller {
public:
    TUIInstaller();
    int exec();
    
private:
    InstallerCore *core;  // Same shared core logic!
    
    void onProgressChanged(int percent, const QString &message);
    void onComplete(bool success, const QString &message);
    
    // TUI widgets
    qtui::TCheckBox *encryptCheckBox;
    qtui::TComboBox *fsComboBox;
    qtui::TProgressBar *progressBar;
    qtui::TLabel *statusLabel;
    qtui::TPushButton *installButton;
};

// tui/installer-tui.cpp
TUIInstaller::TUIInstaller() {
    core = new InstallerCore();
    
    // Connect core signals to TUI
    QObject::connect(core, &InstallerCore::progressChanged,
                     [this](int percent, const QString &msg) {
        onProgressChanged(percent, msg);
    });
    
    QObject::connect(core, &InstallerCore::installationComplete,
                     [this](bool success, const QString &msg) {
        onComplete(success, msg);
    });
    
    // Setup TUI widgets...
    encryptCheckBox = new qtui::TCheckBox();
    encryptCheckBox->setText("Enable encryption");
    encryptCheckBox->setPosition(5, 10);
    
    fsComboBox = new qtui::TComboBox();
    fsComboBox->addItems(QStringList() << "ext4" << "btrfs" << "xfs");
    fsComboBox->setPosition(7, 10);
    
    progressBar = new qtui::TProgressBar();
    progressBar->setPosition(12, 10);
    
    installButton = new qtui::TPushButton();
    installButton->setText("Install");
    installButton->setPosition(15, 10);
    
    QObject::connect(installButton, &qtui::TPushButton::clicked, [this]() {
        // Configure core from TUI (same as GUI!)
        core->setEncryption(encryptCheckBox->isChecked());
        core->setFileSystem(fsComboBox->currentText());
        core->setTargetDevice("/dev/sda1");
        
        // Start installation
        installButton->setEnabled(false);
        core->install();
    });
}

int TUIInstaller::exec() {
    auto *tuiApp = qtui::Application::instance();
    tuiApp->enableMouse();
    
    bool running = true;
    
    while (running) {
        clear();
        
        // Render all widgets
        encryptCheckBox->render();
        fsComboBox->render();
        progressBar->render();
        statusLabel->render();
        installButton->render();
        
        refresh();
        
        // Handle input
        int ch = getch();
        
        if (ch == 27) { // ESC
            running = false;
        } else if (ch == KEY_MOUSE) {
            MEVENT event;
            if (getmouse(&event) == OK && (event.bstate & BUTTON1_CLICKED)) {
                encryptCheckBox->handleMouse(event.y, event.x);
                fsComboBox->handleMouse(event.y, event.x);
                if (installButton->handleMouse(event.y, event.x)) {
                    installButton->click();
                }
            }
        }
    }
    
    return 0;
}

void TUIInstaller::onProgressChanged(int percent, const QString &message) {
    progressBar->setValue(percent);
    statusLabel->setText(message);
}

void TUIInstaller::onComplete(bool success, const QString &message) {
    installButton->setEnabled(true);
    qtui::MessageBox::information(nullptr, "Installation", message);
}
