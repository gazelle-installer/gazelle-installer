#include "qtui/application.h"
#include "qtui/tmessagebox.h"
#include "qtui/tcheckbox.h"
#include "qtui/tradiobutton.h"
#include "qtui/tpushbutton.h"
#include "qtui/tlabel.h"
#include "qtui/tlineedit.h"
#include "qtui/tcombobox.h"
#include "qtui/tprogressbar.h"
#include "qtui/tgroupbox.h"
#include "qtui/tdialog.h"
#include <QCoreApplication>
#include <QStringList>
#include <iostream>
#include <ncurses.h>

void printUsage(const char *prog)
{
    std::cout << "Usage: " << prog << " <widget>::<method> [args...]\n\n";
    std::cout << "Examples:\n";
    std::cout << "  " << prog << " MessageBox::critical nullptr \"Error\" \"Something went wrong\"\n";
    std::cout << "  " << prog << " MessageBox::yesno nullptr \"Confirm\" \"Continue?\"\n";
    std::cout << "  " << prog << " CheckBox \"Enable encryption\"\n";
    std::cout << "  " << prog << " CheckBox \"Enable encryption\" true\n";
    std::cout << "  " << prog << " RadioButton \"Option1,Option2,Option3\" 1\n";
    std::cout << "  " << prog << " PushButton \"Click Me\"\n";
    std::cout << "  " << prog << " Label \"Hello World\" left\n";
    std::cout << "  " << prog << " Label \"Centered Text\" center 40\n";
    std::cout << "  " << prog << " LineEdit \"Initial text\"\n";
    std::cout << "  " << prog << " LineEdit \"\" password\n";
    std::cout << "  " << prog << " ComboBox \"Option1,Option2,Option3\" 1\n";
    std::cout << "  " << prog << " ProgressBar 75\n";
    std::cout << "  " << prog << " GroupBox \"My Group\"\n";
    std::cout << "  " << prog << " Dialog \"My Dialog\"\n";
    std::cout << "\nAvailable widgets and methods:\n";
    std::cout << "  MessageBox::critical <parent> <title> <message>\n";
    std::cout << "  MessageBox::warning <parent> <title> <message>\n";
    std::cout << "  MessageBox::information <parent> <title> <message>\n";
    std::cout << "  MessageBox::yesno <parent> <title> <message>\n";
    std::cout << "  CheckBox <text> [checked]\n";
    std::cout << "  RadioButton <option1,option2,...> [selected_index]\n";
    std::cout << "  PushButton <text>\n";
    std::cout << "  Label <text> [alignment] [width]\n";
    std::cout << "  LineEdit <initial_text> [password]\n";
    std::cout << "  ComboBox <item1,item2,...> [selected_index]\n";
    std::cout << "  ProgressBar <value>\n";
    std::cout << "  GroupBox <title>\n";
    std::cout << "  Dialog <title>\n";
}

int main(int argc, char *argv[])
{
    QCoreApplication coreApp(argc, argv);
    
    if (argc < 2) {
        printUsage(argv[0]);
        return 1;
    }
    
    QString command = argv[1];
    
    // Check if it's a simple widget name or Widget::method format
    if (!command.contains("::")) {
        // Simple widget test
        qtui::Application app(argc, argv);
        app.enableMouse();
        
        if (command == "CheckBox") {
            QString text = (argc > 2) ? QString::fromUtf8(argv[2]) : "CheckBox";
            bool checked = (argc > 3) ? (QString::fromUtf8(argv[3]).toLower() == "true") : false;
            
            qtui::TCheckBox checkbox(text);
            checkbox.setChecked(checked);
            checkbox.setPosition(5, 5);
            checkbox.setFocus(true);
            checkbox.show();
            
            clear();
            mvprintw(1, 1, "CheckBox Test - Use Space to toggle, 'q' to quit");
            mvprintw(2, 1, "Mouse: Click the checkbox to toggle");
            mvprintw(3, 1, "Keyboard: Press Space to toggle");
            
            bool running = true;
            while (running) {
                checkbox.render();
                refresh();
                
                int ch = getch();
                if (ch == 'q' || ch == 'Q' || ch == 27) {
                    running = false;
                } else if (ch == KEY_MOUSE) {
                    MEVENT event;
                    if (getmouse(&event) == OK) {
                        if (event.bstate & BUTTON1_CLICKED) {
                            checkbox.handleMouse(event.y, event.x);
                        }
                    }
                } else {
                    checkbox.handleKey(ch);
                }
            }
            
            std::cout << "Final state: " << (checkbox.isChecked() ? "Checked" : "Unchecked") << "\n";
            return 0;
            
        } else if (command == "RadioButton") {
            QString options = (argc > 2) ? QString::fromUtf8(argv[2]) : "Option 1,Option 2,Option 3";
            int selected = (argc > 3) ? QString::fromUtf8(argv[3]).toInt() : 0;
            
            QStringList optionList = options.split(',');
            qtui::TButtonGroup group;
            QList<qtui::TRadioButton *> buttons;
            
            int startRow = 5;
            for (int i = 0; i < optionList.size(); ++i) {
                qtui::TRadioButton *radio = new qtui::TRadioButton(optionList[i].trimmed());
                radio->setButtonGroup(&group);
                radio->setPosition(startRow + i, 5);
                radio->show();
                buttons.append(radio);
                
                if (i == selected) {
                    radio->setChecked(true);
                }
            }
            
            if (!buttons.isEmpty()) {
                buttons[0]->setFocus(true);
            }
            
            int focusedIndex = 0;
            
            clear();
            mvprintw(1, 1, "RadioButton Test - Use Space to select, Up/Down to navigate, 'q' to quit");
            mvprintw(2, 1, "Mouse: Click a radio button to select");
            mvprintw(3, 1, "Keyboard: Arrow keys to move, Space to select");
            
            bool running = true;
            while (running) {
                for (auto *btn : buttons) {
                    btn->render();
                }
                refresh();
                
                int ch = getch();
                if (ch == 'q' || ch == 'Q' || ch == 27) {
                    running = false;
                } else if (ch == KEY_MOUSE) {
                    MEVENT event;
                    if (getmouse(&event) == OK) {
                        if (event.bstate & BUTTON1_CLICKED) {
                            for (int i = 0; i < buttons.size(); ++i) {
                                if (buttons[i]->handleMouse(event.y, event.x)) {
                                    buttons[focusedIndex]->setFocus(false);
                                    focusedIndex = i;
                                    buttons[focusedIndex]->setFocus(true);
                                    break;
                                }
                            }
                        }
                    }
                } else if (ch == KEY_UP) {
                    buttons[focusedIndex]->setFocus(false);
                    focusedIndex = (focusedIndex - 1 + buttons.size()) % buttons.size();
                    buttons[focusedIndex]->setFocus(true);
                } else if (ch == KEY_DOWN) {
                    buttons[focusedIndex]->setFocus(false);
                    focusedIndex = (focusedIndex + 1) % buttons.size();
                    buttons[focusedIndex]->setFocus(true);
                } else {
                    buttons[focusedIndex]->handleKey(ch);
                }
            }
            
            qtui::TRadioButton *checked = group.checkedButton();
            std::cout << "Selected: " << (checked ? checked->text().toStdString() : "None") << "\n";
            
            qDeleteAll(buttons);
            return 0;
            
        } else if (command == "PushButton") {
            QString text = (argc > 2) ? QString::fromUtf8(argv[2]) : "Click Me";
            
            qtui::TPushButton button(text);
            button.setPosition(5, 5);
            button.setFocus(true);
            button.show();
            
            int clickCount = 0;
            QObject::connect(&button, &qtui::TPushButton::clicked, [&clickCount]() {
                clickCount++;
            });
            
            clear();
            mvprintw(1, 1, "PushButton Test - Press Enter/Space to click, 'q' to quit");
            mvprintw(2, 1, "Mouse: Click the button");
            mvprintw(3, 1, "Keyboard: Press Enter or Space");
            
            bool running = true;
            while (running) {
                button.render();
                mvprintw(7, 5, "Click count: %d", clickCount);
                refresh();
                
                int ch = getch();
                if (ch == 'q' || ch == 'Q' || ch == 27) {
                    running = false;
                } else if (ch == KEY_MOUSE) {
                    MEVENT event;
                    if (getmouse(&event) == OK) {
                        if (event.bstate & BUTTON1_CLICKED) {
                            button.handleMouse(event.y, event.x);
                        }
                    }
                } else {
                    button.handleKey(ch);
                }
            }
            
            std::cout << "Total clicks: " << clickCount << "\n";
            return 0;
            
        } else if (command == "Label") {
            QString text = (argc > 2) ? QString::fromUtf8(argv[2]) : "Label Text";
            QString alignStr = (argc > 3) ? QString::fromUtf8(argv[3]).toLower() : "left";
            int width = (argc > 4) ? QString::fromUtf8(argv[4]).toInt() : 0;
            
            qtui::TLabel label(text);
            label.setPosition(5, 5);
            label.show();
            
            if (alignStr == "center") {
                label.setAlignment(qtui::TLabel::AlignHCenter);
            } else if (alignStr == "right") {
                label.setAlignment(qtui::TLabel::AlignRight);
            }
            
            if (width > 0) {
                label.setWidth(width);
                label.setWordWrap(true);
            }
            
            clear();
            mvprintw(1, 1, "Label Test - Press 'q' to quit");
            mvprintw(2, 1, "Alignment: %s", alignStr.toUtf8().constData());
            if (width > 0) {
                mvprintw(3, 1, "Width: %d (word wrap enabled)", width);
            }
            
            bool running = true;
            while (running) {
                label.render();
                refresh();
                
                int ch = getch();
                if (ch == 'q' || ch == 'Q' || ch == 27) {
                    running = false;
                }
            }
            
            return 0;
            
        } else if (command == "LineEdit") {
            QString initialText = (argc > 2) ? QString::fromUtf8(argv[2]) : "";
            QString modeStr = (argc > 3) ? QString::fromUtf8(argv[3]).toLower() : "normal";
            
            qtui::TLineEdit lineEdit(initialText);
            lineEdit.setPosition(5, 5);
            lineEdit.setWidth(30);
            lineEdit.setFocus(true);
            lineEdit.show();
            
            if (modeStr == "password") {
                lineEdit.setEchoMode(qtui::TLineEdit::Password);
            }
            
            if (initialText.isEmpty()) {
                lineEdit.setPlaceholderText("Enter text here...");
            }
            
            QObject::connect(&lineEdit, &qtui::TLineEdit::returnPressed, [&]() {
                // User pressed Enter - we'll exit
            });
            
            clear();
            mvprintw(1, 1, "LineEdit Test - Type to edit, Enter to finish, 'Esc' to quit");
            mvprintw(2, 1, "Mode: %s", modeStr.toUtf8().constData());
            mvprintw(3, 1, "Use arrow keys, Home, End, Backspace, Delete");
            
            bool running = true;
            while (running) {
                lineEdit.render();
                mvprintw(7, 5, "Current text: %s", lineEdit.text().toUtf8().constData());
                refresh();
                
                int ch = getch();
                if (ch == 27) { // ESC
                    running = false;
                } else if (ch == '\n' || ch == KEY_ENTER) {
                    if (lineEdit.handleKey(ch)) {
                        running = false;
                    }
                } else if (ch == KEY_MOUSE) {
                    MEVENT event;
                    if (getmouse(&event) == OK) {
                        if (event.bstate & BUTTON1_CLICKED) {
                            lineEdit.handleMouse(event.y, event.x);
                        }
                    }
                } else {
                    lineEdit.handleKey(ch);
                }
            }
            
            std::cout << "Final text: " << lineEdit.text().toStdString() << "\n";
            return 0;
            
        } else if (command == "ComboBox") {
            QString options = (argc > 2) ? QString::fromUtf8(argv[2]) : "Option 1,Option 2,Option 3";
            int selected = (argc > 3) ? QString::fromUtf8(argv[3]).toInt() : 0;
            
            QStringList optionList = options.split(',');
            qtui::TComboBox combo;
            for (const QString &opt : optionList) {
                combo.addItem(opt.trimmed());
            }
            
            if (selected >= 0 && selected < combo.count()) {
                combo.setCurrentIndex(selected);
            }
            
            combo.setPosition(5, 5);
            combo.setWidth(30);
            combo.setFocus(true);
            combo.show();
            
            clear();
            mvprintw(1, 1, "ComboBox Test - Space/Enter to open, Up/Down to navigate, 'q' to quit");
            mvprintw(2, 1, "Mouse: Click to toggle dropdown, click items to select");
            
            bool running = true;
            while (running) {
                combo.render();
                mvprintw(7, 5, "Current selection: %s (index: %d)", 
                        combo.currentText().toUtf8().constData(), combo.currentIndex());
                refresh();
                
                int ch = getch();
                if (ch == 'q' || ch == 'Q') {
                    if (!combo.isPopupVisible()) {
                        running = false;
                    } else {
                        combo.handleKey(27); // ESC to close popup
                    }
                } else if (ch == 27) {
                    if (combo.isPopupVisible()) {
                        combo.handleKey(ch);
                    } else {
                        running = false;
                    }
                } else if (ch == KEY_MOUSE) {
                    MEVENT event;
                    if (getmouse(&event) == OK) {
                        if (event.bstate & BUTTON1_CLICKED) {
                            combo.handleMouse(event.y, event.x);
                        }
                    }
                } else {
                    combo.handleKey(ch);
                }
            }
            
            std::cout << "Selected: " << combo.currentText().toStdString() 
                     << " (index: " << combo.currentIndex() << ")\n";
            return 0;
            
        } else if (command == "ProgressBar") {
            int initialValue = (argc > 2) ? QString::fromUtf8(argv[2]).toInt() : 0;
            
            qtui::TProgressBar progress;
            progress.setPosition(5, 5);
            progress.setWidth(40);
            progress.setValue(initialValue);
            progress.show();
            
            clear();
            mvprintw(1, 1, "ProgressBar Test - '+' to increase, '-' to decrease, 'q' to quit");
            mvprintw(2, 1, "Press 's' to simulate progress, 'r' to reset");
            
            bool running = true;
            bool simulating = false;
            
            while (running) {
                progress.render();
                mvprintw(7, 5, "Current value: %d%%", progress.value());
                refresh();
                
                timeout(simulating ? 100 : -1);
                int ch = getch();
                
                if (ch == 'q' || ch == 'Q' || ch == 27) {
                    running = false;
                } else if (ch == '+' || ch == '=') {
                    progress.setValue(progress.value() + 5);
                } else if (ch == '-' || ch == '_') {
                    progress.setValue(progress.value() - 5);
                } else if (ch == 's' || ch == 'S') {
                    simulating = !simulating;
                } else if (ch == 'r' || ch == 'R') {
                    progress.reset();
                } else if (ch == ERR && simulating) {
                    if (progress.value() < 100) {
                        progress.setValue(progress.value() + 1);
                    } else {
                        simulating = false;
                    }
                }
            }
            
            std::cout << "Final value: " << progress.value() << "%\n";
            return 0;
            
        } else if (command == "GroupBox") {
            QString title = (argc > 2) ? QString::fromUtf8(argv[2]) : "Group Title";
            
            qtui::TGroupBox groupBox;
            groupBox.setTitle(title);
            groupBox.setPosition(3, 5);
            groupBox.setSize(12, 50);
            groupBox.show();
            
            // Add some child widgets
            qtui::TLabel label1;
            label1.setText("This is inside the group box");
            label1.setPosition(5, 7);
            label1.show();
            groupBox.addWidget(&label1);
            
            qtui::TCheckBox checkBox;
            checkBox.setText("Enable option");
            checkBox.setPosition(7, 7);
            checkBox.show();
            groupBox.addWidget(&checkBox);
            
            qtui::TPushButton button;
            button.setText("Action");
            button.setPosition(9, 7);
            button.show();
            groupBox.addWidget(&button);
            
            clear();
            mvprintw(1, 1, "GroupBox Test - Space to toggle checkbox, 'e' to toggle enabled, 'q' to quit");
            
            bool running = true;
            bool enabled = true;
            
            while (running) {
                groupBox.render();
                mvprintw(16, 5, "GroupBox enabled: %s", enabled ? "Yes" : "No");
                mvprintw(17, 5, "Checkbox state: %s", checkBox.isChecked() ? "Checked" : "Unchecked");
                refresh();
                
                int ch = getch();
                if (ch == 'q' || ch == 'Q' || ch == 27) {
                    running = false;
                } else if (ch == 'e' || ch == 'E') {
                    enabled = !enabled;
                    groupBox.setEnabled(enabled);
                } else if (ch == ' ') {
                    checkBox.toggle();
                }
            }
            
            return 0;
            
        } else if (command == "Dialog") {
            QString title = (argc > 2) ? QString::fromUtf8(argv[2]) : "Dialog Title";
            
            qtui::TDialog dialog;
            dialog.setWindowTitle(title);
            dialog.show();
            
            int result = dialog.exec();
            
            std::cout << "Dialog result: " << (result == qtui::TDialog::Accepted ? "Accepted" : "Rejected") << "\n";
            return 0;
            
        } else {
            std::cerr << "Error: Unknown widget '" << command.toStdString() << "'\n";
            printUsage(argv[0]);
            return 1;
        }
    }
    
    // Widget::method format
    QStringList parts = command.split("::");
    
    if (parts.size() != 2) {
        std::cerr << "Error: Invalid command format. Expected <Widget>::<method> or <Widget>\n";
        printUsage(argv[0]);
        return 1;
    }
    
    QString widget = parts[0];
    QString method = parts[1];
    
    qtui::Application app(argc, argv);
    qtui::TMessageBox::StandardButton result = qtui::TMessageBox::NoButton;
    
    if (widget == "MessageBox") {
        QString title = (argc > 3) ? QString::fromUtf8(argv[3]) : QString();
        QString message = (argc > 4) ? QString::fromUtf8(argv[4]) : QString();
        
        if (method == "critical") {
            result = qtui::TMessageBox::critical(nullptr, title, message);
        } else if (method == "warning") {
            result = qtui::TMessageBox::warning(nullptr, title, message);
        } else if (method == "information") {
            result = qtui::TMessageBox::information(nullptr, title, message);
        } else if (method == "yesno") {
            result = qtui::TMessageBox::warning(nullptr, title, message,
                                              qtui::TMessageBox::Yes | qtui::TMessageBox::No,
                                              qtui::TMessageBox::No);
        } else {
            std::cerr << "Error: Unknown method '" << method.toStdString() << "' for MessageBox\n";
            printUsage(argv[0]);
            return 1;
        }
        
        std::cout << "Result: ";
        switch (result) {
            case qtui::TMessageBox::Ok: std::cout << "Ok"; break;
            case qtui::TMessageBox::Yes: std::cout << "Yes"; break;
            case qtui::TMessageBox::No: std::cout << "No"; break;
            case qtui::TMessageBox::Cancel: std::cout << "Cancel"; break;
            default: std::cout << "NoButton"; break;
        }
        std::cout << " (" << static_cast<int>(result) << ")\n";
        
    } else {
        std::cerr << "Error: Unknown widget '" << widget.toStdString() << "'\n";
        printUsage(argv[0]);
        return 1;
    }
    
    return 0;
}
