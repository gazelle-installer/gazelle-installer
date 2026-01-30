#include "qtui/application.h"
#include "qtui/groupbox.h"
#include "qtui/radiobutton.h"
#include "qtui/combobox.h"
#include "qtui/checkbox.h"
#include "qtui/pushbutton.h"
#include "qtui/messagebox.h"
#include <QCoreApplication>
#include <ncurses.h>

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    
    auto *tuiApp = qtui::Application::instance();
    tuiApp->enableMouse();
    
    // Partition options group
    qtui::TGroupBox partitionGroup;
    partitionGroup.setTitle("Partitioning");
    partitionGroup.setPosition(3, 5);
    partitionGroup.setSize(10, 50);
    partitionGroup.show();
    
    qtui::TRadioButton autoPartition, manualPartition;
    autoPartition.setText("Automatic partitioning");
    autoPartition.setPosition(5, 8);
    autoPartition.setChecked(true);
    autoPartition.show();
    
    manualPartition.setText("Manual partitioning");
    manualPartition.setPosition(7, 8);
    manualPartition.show();
    
    qtui::TButtonGroup partitionButtonGroup;
    partitionButtonGroup.addButton(&autoPartition);
    partitionButtonGroup.addButton(&manualPartition);
    
    partitionGroup.addWidget(&autoPartition);
    partitionGroup.addWidget(&manualPartition);
    
    // File system group
    qtui::TGroupBox fsGroup;
    fsGroup.setTitle("File System");
    fsGroup.setPosition(14, 5);
    fsGroup.setSize(7, 50);
    fsGroup.show();
    
    qtui::TComboBox fsCombo;
    fsCombo.addItems(QStringList() << "ext4" << "btrfs" << "xfs" << "f2fs");
    fsCombo.setCurrentIndex(0);
    fsCombo.setPosition(16, 8);
    fsCombo.setWidth(20);
    fsCombo.show();
    
    fsGroup.addWidget(&fsCombo);
    
    // Encryption group
    qtui::TGroupBox encryptionGroup;
    encryptionGroup.setTitle("Encryption");
    encryptionGroup.setPosition(22, 5);
    encryptionGroup.setSize(6, 50);
    encryptionGroup.show();
    
    qtui::TCheckBox enableEncryption;
    enableEncryption.setText("Enable full disk encryption");
    enableEncryption.setPosition(24, 8);
    enableEncryption.show();
    
    encryptionGroup.addWidget(&enableEncryption);
    
    // Action buttons
    qtui::TPushButton installButton, cancelButton;
    installButton.setText("Install");
    installButton.setPosition(30, 5);
    installButton.show();
    
    cancelButton.setText("Cancel");
    cancelButton.setPosition(30, 20);
    cancelButton.show();
    
    // Event handlers
    bool running = true;
    
    QObject::connect(&installButton, &qtui::TPushButton::clicked, [&]() {
        QString summary = "Installation Summary:\n\n";
        summary += QString("Partitioning: %1\n").arg(
            autoPartition.isChecked() ? "Automatic" : "Manual");
        summary += QString("File System: %1\n").arg(fsCombo.currentText());
        summary += QString("Encryption: %1\n").arg(
            enableEncryption.isChecked() ? "Enabled" : "Disabled");
        
        qtui::MessageBox msgBox;
        msgBox.setIcon(qtui::MessageBox::Question);
        msgBox.setWindowTitle("Confirm Installation");
        msgBox.setText(summary + "\nProceed with installation?");
        msgBox.setStandardButtons(qtui::MessageBox::Yes | qtui::MessageBox::No);
        msgBox.setDefaultButton(qtui::MessageBox::No);
        
        auto result = msgBox.exec();
        if (result == qtui::MessageBox::Yes) {
            qtui::MessageBox::information(nullptr, "Success", 
                                        "Installation started!");
            running = false;
        }
    });
    
    QObject::connect(&cancelButton, &qtui::TPushButton::clicked, [&]() {
        running = false;
    });
    
    // Main loop
    clear();
    mvprintw(1, 5, "=== Installation Settings Demo ===");
    
    while (running) {
        partitionGroup.render();
        fsGroup.render();
        encryptionGroup.render();
        installButton.render();
        cancelButton.render();
        
        mvprintw(33, 5, "Space: toggle | Tab: navigate | Enter: select | 'q': quit");
        refresh();
        
        int ch = getch();
        
        if (ch == 'q' || ch == 'Q' || ch == 27) {
            running = false;
        } else if (ch == ' ') {
            if (autoPartition.hasFocus()) autoPartition.toggle();
            if (manualPartition.hasFocus()) manualPartition.toggle();
            if (enableEncryption.hasFocus()) enableEncryption.toggle();
            if (installButton.hasFocus()) installButton.click();
            if (cancelButton.hasFocus()) cancelButton.click();
            if (fsCombo.hasFocus()) fsCombo.handleKey(ch);
        } else if (ch == KEY_MOUSE) {
            MEVENT event;
            if (getmouse(&event) == OK && (event.bstate & BUTTON1_CLICKED)) {
                autoPartition.handleMouse(event.y, event.x);
                manualPartition.handleMouse(event.y, event.x);
                fsCombo.handleMouse(event.y, event.x);
                enableEncryption.handleMouse(event.y, event.x);
                if (installButton.handleMouse(event.y, event.x)) {
                    installButton.click();
                }
                if (cancelButton.handleMouse(event.y, event.x)) {
                    cancelButton.click();
                }
            }
        } else {
            fsCombo.handleKey(ch);
        }
    }
    
    return 0;
}
