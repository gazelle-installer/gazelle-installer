#include "qtui/application.h"
#include "qtui/messagebox.h"
#include "qtui/checkbox.h"
#include "qtui/lineedit.h"
#include "qtui/pushbutton.h"
#include <QCoreApplication>
#include <ncurses.h>

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    
    // Initialize TUI application
    auto *tuiApp = qtui::Application::instance();
    tuiApp->enableMouse();
    
    // Create widgets
    qtui::LineEdit nameEdit;
    nameEdit.setPlaceholderText("Enter your name");
    nameEdit.setPosition(5, 10);
    nameEdit.setWidth(30);
    nameEdit.show();
    
    qtui::LineEdit passwordEdit;
    passwordEdit.setPlaceholderText("Enter password");
    passwordEdit.setEchoMode(qtui::LineEdit::Password);
    passwordEdit.setPosition(7, 10);
    passwordEdit.setWidth(30);
    passwordEdit.show();
    
    qtui::CheckBox rememberMe;
    rememberMe.setText("Remember me");
    rememberMe.setPosition(9, 10);
    rememberMe.show();
    
    qtui::PushButton loginButton;
    loginButton.setText("Login");
    loginButton.setPosition(11, 10);
    loginButton.show();
    
    qtui::PushButton cancelButton;
    cancelButton.setText("Cancel");
    cancelButton.setPosition(11, 20);
    cancelButton.show();
    
    // Connect signals
    bool running = true;
    
    QObject::connect(&loginButton, &qtui::PushButton::clicked, [&]() {
        if (nameEdit.text().isEmpty() || passwordEdit.text().isEmpty()) {
            qtui::MessageBox::warning(nullptr, "Validation Error", 
                                     "Please enter both name and password");
        } else {
            QString msg = QString("Welcome, %1!").arg(nameEdit.text());
            if (rememberMe.isChecked()) {
                msg += "\n(Session will be remembered)";
            }
            qtui::MessageBox::information(nullptr, "Login Successful", msg);
            running = false;
        }
    });
    
    QObject::connect(&cancelButton, &qtui::PushButton::clicked, [&]() {
        running = false;
    });
    
    // Focus management
    int focusIndex = 0;
    nameEdit.setFocus(true);
    
    // Main event loop
    clear();
    mvprintw(1, 10, "=== Login Form Demo ===");
    mvprintw(3, 10, "Name:");
    mvprintw(5, 10, "Password:");
    mvprintw(15, 10, "Tab: next field | Space: toggle checkbox | Enter: activate button");
    
    while (running) {
        nameEdit.render();
        passwordEdit.render();
        rememberMe.render();
        loginButton.render();
        cancelButton.render();
        refresh();
        
        int ch = getch();
        
        if (ch == 27) { // ESC
            running = false;
        } else if (ch == '\t') { // Tab
            nameEdit.setFocus(false);
            passwordEdit.setFocus(false);
            rememberMe.setFocus(false);
            loginButton.setFocus(false);
            cancelButton.setFocus(false);
            
            focusIndex = (focusIndex + 1) % 5;
            
            switch (focusIndex) {
                case 0: nameEdit.setFocus(true); break;
                case 1: passwordEdit.setFocus(true); break;
                case 2: rememberMe.setFocus(true); break;
                case 3: loginButton.setFocus(true); break;
                case 4: cancelButton.setFocus(true); break;
            }
        } else if (ch == ' ') {
            if (focusIndex == 2) rememberMe.toggle();
            if (focusIndex == 3) loginButton.click();
            if (focusIndex == 4) cancelButton.click();
        } else if (ch == '\n' || ch == KEY_ENTER) {
            if (focusIndex == 0) passwordEdit.setFocus(true), nameEdit.setFocus(false), focusIndex = 1;
            else if (focusIndex == 1) rememberMe.setFocus(true), passwordEdit.setFocus(false), focusIndex = 2;
            else if (focusIndex == 3) loginButton.click();
            else if (focusIndex == 4) cancelButton.click();
        } else if (ch == KEY_MOUSE) {
            MEVENT event;
            if (getmouse(&event) == OK && (event.bstate & BUTTON1_CLICKED)) {
                nameEdit.handleMouse(event.y, event.x);
                passwordEdit.handleMouse(event.y, event.x);
                if (rememberMe.handleMouse(event.y, event.x)) {
                    // Checkbox was clicked
                }
                if (loginButton.handleMouse(event.y, event.x)) {
                    loginButton.click();
                }
                if (cancelButton.handleMouse(event.y, event.x)) {
                    cancelButton.click();
                }
            }
        } else {
            if (focusIndex == 0) nameEdit.handleKey(ch);
            else if (focusIndex == 1) passwordEdit.handleKey(ch);
        }
    }
    
    return 0;
}
