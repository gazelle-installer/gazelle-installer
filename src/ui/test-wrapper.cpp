#include "ui/context.h"
#include "ui/qcheckbox.h"
#include "ui/qpushbutton.h"
#include "ui/qlabel.h"
#include "ui/qlineedit.h"
#include "ui/qmessagebox.h"

#include <QCoreApplication>
#include <QApplication>
#include <QVBoxLayout>
#include <QWidget>
#include <qtui/application.h>
#include <qtui/tcheckbox.h>
#include <qtui/tpushbutton.h>
#include <qtui/tlineedit.h>
#include <qtui/tlabel.h>
#include <ncurses.h>
#include <iostream>

int main(int argc, char *argv[])
{
    // Check for command-line flag to force mode
    bool forceTUI = false;
    bool forceGUI = false;
    
    for (int i = 1; i < argc; i++) {
        QString arg(argv[i]);
        if (arg == "--tui" || arg == "-t") {
            forceTUI = true;
        } else if (arg == "--gui" || arg == "-g") {
            forceGUI = true;
        } else if (arg == "--help" || arg == "-h") {
            std::cout << "Usage: " << argv[0] << " [OPTIONS]\n"
                      << "Options:\n"
                      << "  --tui, -t    Force TUI mode\n"
                      << "  --gui, -g    Force GUI mode\n"
                      << "  --help, -h   Show this help\n";
            return 0;
        }
    }
    
    // Detect mode
    bool hasDisplay = ui::Context::detectXDisplay();
    
    // Override with command-line flags
    if (forceTUI) {
        hasDisplay = false;
    } else if (forceGUI) {
        hasDisplay = true;
    }
    
    if (hasDisplay) {
        QApplication app(argc, argv);
        ui::Context::setMode(ui::Context::GUI);
        
        std::cout << "Running in GUI mode" << std::endl;
        
        QWidget window;
        QVBoxLayout *layout = new QVBoxLayout(&window);
        
        // Create widgets using ui:: wrappers
        ui::QLabel *label = new ui::QLabel("Welcome to Wrapper Test!");
        ui::QLineEdit *nameEdit = new ui::QLineEdit();
        nameEdit->setPlaceholderText("Enter your name");
        ui::QCheckBox *agreeBox = new ui::QCheckBox();
        agreeBox->setText("I agree to the terms");
        ui::QPushButton *submitBtn = new ui::QPushButton("Submit");
        
        // Add to layout (GUI only)
        layout->addWidget(label->widget());
        layout->addWidget(nameEdit->widget());
        layout->addWidget(agreeBox->widget());
        layout->addWidget(submitBtn->widget());
        
        // Connect signals
        QObject::connect(submitBtn, &ui::QPushButton::clicked, [&]() {
            if (!agreeBox->isChecked()) {
                ui::QMessageBox::warning(nullptr, "Error", "You must agree to the terms!");
                return;
            }
            
            QString name = nameEdit->text();
            if (name.isEmpty()) {
                ui::QMessageBox::critical(nullptr, "Error", "Name cannot be empty!");
                return;
            }
            
            ui::QMessageBox::information(nullptr, "Success", "Welcome, " + name + "!");
        });
        
        window.setWindowTitle("UI Wrapper Test (GUI)");
        window.resize(400, 200);
        window.show();
        
        return app.exec();
        
    } else {
        QCoreApplication app(argc, argv);
        ui::Context::setMode(ui::Context::TUI);
        
        std::cout << "Running in TUI mode" << std::endl;
        
        // IMPORTANT: Create Application object (not just use instance())
        qtui::Application tuiApp(argc, argv);
        tuiApp.enableMouse();
        
        // Create widgets using ui:: wrappers
        ui::QLabel *label = new ui::QLabel("Welcome to Wrapper Test!");
        ui::QLineEdit *nameEdit = new ui::QLineEdit();
        nameEdit->setPlaceholderText("Enter your name");
        ui::QCheckBox *agreeBox = new ui::QCheckBox();
        agreeBox->setText("I agree to the terms");
        ui::QPushButton *submitBtn = new ui::QPushButton("Submit");
        
        // Position for TUI
        label->setPosition(2, 5);
        nameEdit->setPosition(4, 5);
        nameEdit->setWidth(30);
        agreeBox->setPosition(6, 5);
        submitBtn->setPosition(8, 5);
        
        // Connect signals (same as GUI!)
        QObject::connect(submitBtn, &ui::QPushButton::clicked, [&]() {
            if (!agreeBox->isChecked()) {
                ui::QMessageBox::warning(nullptr, "Error", "You must agree to the terms!");
                return;
            }
            
            QString name = nameEdit->text();
            if (name.isEmpty()) {
                ui::QMessageBox::critical(nullptr, "Error", "Name cannot be empty!");
                return;
            }
            
            ui::QMessageBox::information(nullptr, "Success", "Welcome, " + name + "!");
        });
        
        // Show widgets
        label->show();
        nameEdit->show();
        agreeBox->show();
        submitBtn->show();
        
        // Get TUI widget pointers
        auto *tuiLabel = dynamic_cast<qtui::TLabel*>(label->tuiWidget());
        auto *tuiNameEdit = dynamic_cast<qtui::TLineEdit*>(nameEdit->tuiWidget());
        auto *tuiAgreeBox = dynamic_cast<qtui::TCheckBox*>(agreeBox->tuiWidget());
        auto *tuiSubmitBtn = dynamic_cast<qtui::TPushButton*>(submitBtn->tuiWidget());
        
        // TUI event loop - must manually render widgets
        clear();
        mvprintw(0, 5, "=== UI Wrapper Test (TUI Mode) ===");
        mvprintw(14, 5, "Tab: next field | Space: toggle/click | Enter: activate | Esc: exit");
        
        int focusIndex = 0;
        tuiNameEdit->setFocus(true);
        bool running = true;
        
        while (running) {
            // Render all widgets
            tuiLabel->render();
            tuiNameEdit->render();
            tuiAgreeBox->render();
            tuiSubmitBtn->render();
            
            // Position and show cursor for focused input field
            // This must be called AFTER all widgets render to ensure cursor stays visible
            if (focusIndex == 0) {
                tuiNameEdit->showCursor();
            } else {
                curs_set(0);  // Hide cursor when not on input field
            }
            
            refresh();
            
            int ch = getch();
            
            if (ch == 27) { // ESC
                running = false;
            } else if (ch == '\t') { // Tab
                tuiNameEdit->setFocus(false);
                tuiAgreeBox->setFocus(false);
                tuiSubmitBtn->setFocus(false);
                
                focusIndex = (focusIndex + 1) % 3;
                
                switch (focusIndex) {
                    case 0: tuiNameEdit->setFocus(true); break;
                    case 1: tuiAgreeBox->setFocus(true); break;
                    case 2: tuiSubmitBtn->setFocus(true); break;
                }
            } else if (ch == ' ') {
                if (focusIndex == 1) tuiAgreeBox->toggle();
                if (focusIndex == 2) emit submitBtn->clicked();
            } else if (ch == '\n' || ch == KEY_ENTER) {
                if (focusIndex == 2) emit submitBtn->clicked();
            } else if (ch == KEY_MOUSE) {
                MEVENT event;
                if (getmouse(&event) == OK && (event.bstate & BUTTON1_CLICKED)) {
                    tuiNameEdit->handleMouse(event.y, event.x);
                    if (tuiAgreeBox->handleMouse(event.y, event.x)) {
                        tuiAgreeBox->toggle();
                    }
                    if (tuiSubmitBtn->handleMouse(event.y, event.x)) {
                        emit submitBtn->clicked();
                    }
                }
            } else {
                if (focusIndex == 0) tuiNameEdit->handleKey(ch);
            }
        }
        
        return 0;
    }
}
