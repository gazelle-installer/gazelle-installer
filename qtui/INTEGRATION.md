# QTUI Library - Integration Guide

This guide shows how to integrate the QTUI library into your own Qt projects.

## Installation

### Option 1: System-Wide Installation

```bash
cd qtui
mkdir build && cd build
cmake ..
make
sudo make install
```

This installs:
- Headers to `/usr/local/include/qtui/`
- Library to `/usr/local/lib/libqtui.so`

### Option 2: Manual Integration

Copy the `qtui` directory into your project and add as a subdirectory:

```cmake
add_subdirectory(qtui)
target_link_libraries(your_app qtui)
```

## CMake Configuration

### Using Installed Library

```cmake
cmake_minimum_required(VERSION 3.21)
project(MyTUIApp)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Core)
find_package(Curses REQUIRED)

add_executable(myapp main.cpp)

target_link_libraries(myapp
    qtui
    Qt6::Core
    ${CURSES_LIBRARIES}
)
```

### Using as Subdirectory

```cmake
cmake_minimum_required(VERSION 3.21)
project(MyTUIApp)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Core)

add_subdirectory(qtui)

add_executable(myapp main.cpp)
target_link_libraries(myapp qtui Qt6::Core)
```

## Basic Application Template

```cpp
#include <QCoreApplication>
#include "qtui/application.h"
#include "qtui/messagebox.h"
#include <ncurses.h>

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    
    // Initialize QTUI (handles ncurses initialization)
    auto *tuiApp = qtui::Application::instance();
    tuiApp->enableMouse();
    
    // Show a simple message box
    qtui::MessageBox::information(nullptr, "Hello", "Welcome to QTUI!");
    
    return 0;
}
```

## Converting Qt GUI Code to QTUI

### MessageBox

**Qt GUI:**
```cpp
QMessageBox::critical(this, "Error", "Failed to save file");
```

**QTUI:**
```cpp
qtui::MessageBox::critical(nullptr, "Error", "Failed to save file");
```

### CheckBox

**Qt GUI:**
```cpp
QCheckBox *checkbox = new QCheckBox("Enable option", this);
connect(checkbox, &QCheckBox::toggled, this, &MyClass::onToggled);
```

**QTUI:**
```cpp
qtui::CheckBox checkbox;
checkbox.setText("Enable option");
checkbox.setPosition(5, 10);
checkbox.show();
QObject::connect(&checkbox, &qtui::CheckBox::toggled, 
                 this, &MyClass::onToggled);
```

### LineEdit

**Qt GUI:**
```cpp
QLineEdit *lineEdit = new QLineEdit(this);
lineEdit->setPlaceholderText("Enter text");
lineEdit->setEchoMode(QLineEdit::Password);
```

**QTUI:**
```cpp
qtui::LineEdit lineEdit;
lineEdit.setPlaceholderText("Enter text");
lineEdit.setEchoMode(qtui::LineEdit::Password);
lineEdit.setPosition(5, 10);
lineEdit.setWidth(30);
lineEdit.show();
```

### ComboBox

**Qt GUI:**
```cpp
QComboBox *combo = new QComboBox(this);
combo->addItems({"Option 1", "Option 2", "Option 3"});
connect(combo, QOverload<int>::of(&QComboBox::currentIndexChanged),
        this, &MyClass::onSelectionChanged);
```

**QTUI:**
```cpp
qtui::ComboBox combo;
combo.addItems(QStringList() << "Option 1" << "Option 2" << "Option 3");
combo.setPosition(5, 10);
combo.setWidth(25);
combo.show();
QObject::connect(&combo, &qtui::ComboBox::currentIndexChanged,
                 this, &MyClass::onSelectionChanged);
```

## Event Loop Pattern

QTUI requires manual event handling. Use this pattern:

```cpp
int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    auto *tuiApp = qtui::Application::instance();
    tuiApp->enableMouse();
    
    // Create widgets
    qtui::PushButton okButton;
    okButton.setText("OK");
    okButton.setPosition(10, 20);
    okButton.show();
    
    // Event handler
    bool running = true;
    QObject::connect(&okButton, &qtui::PushButton::clicked, [&]() {
        running = false;
    });
    
    // Main loop
    while (running) {
        clear();  // Clear screen
        
        // Render all widgets
        okButton.render();
        
        refresh();  // Update display
        
        // Handle input
        int ch = getch();
        
        if (ch == 27) {  // ESC
            running = false;
        } else if (ch == KEY_MOUSE) {
            MEVENT event;
            if (getmouse(&event) == OK) {
                if (event.bstate & BUTTON1_CLICKED) {
                    if (okButton.handleMouse(event.y, event.x)) {
                        okButton.click();
                    }
                }
            }
        } else if (ch == '\n' || ch == KEY_ENTER) {
            okButton.click();
        }
    }
    
    return 0;
}
```

## Widget Positioning

QTUI uses manual positioning (row, column):

```cpp
widget.setPosition(row, col);  // row=vertical, col=horizontal
widget.setSize(height, width); // For containers like GroupBox
widget.setWidth(width);        // For input widgets
```

Screen coordinates start at (0, 0) in the top-left corner.

## Focus Management

Manage focus manually for keyboard navigation:

```cpp
qtui::PushButton button1, button2;
button1.setFocus(true);   // Give focus
button2.setFocus(false);  // Remove focus

// In event loop, handle Tab key:
if (ch == '\t') {
    button1.setFocus(false);
    button2.setFocus(true);
}
```

## Signals and Slots

QTUI widgets use Qt's signal/slot mechanism:

```cpp
// Lambda connection
QObject::connect(&widget, &qtui::Widget::signalName, [&]() {
    // Handle signal
});

// Member function connection
QObject::connect(&widget, &qtui::Widget::signalName,
                 this, &MyClass::slotFunction);

// With parameters
QObject::connect(&lineEdit, &qtui::LineEdit::textChanged,
                 [](const QString &text) {
    qDebug() << "New text:" << text;
});
```

## Common Patterns

### Form with Multiple Inputs

```cpp
std::vector<qtui::LineEdit*> inputs;
int focusIndex = 0;

// Handle Tab navigation
if (ch == '\t') {
    inputs[focusIndex]->setFocus(false);
    focusIndex = (focusIndex + 1) % inputs.size();
    inputs[focusIndex]->setFocus(true);
}

// Route input to focused widget
else if (inputs[focusIndex]->hasFocus()) {
    inputs[focusIndex]->handleKey(ch);
}
```

### Progress Updates

```cpp
qtui::ProgressBar progress;
progress.setRange(0, 100);

// In your work loop:
for (int i = 0; i <= 100; ++i) {
    progress.setValue(i);
    clear();
    progress.render();
    refresh();
    
    // Do work...
    usleep(50000);  // 50ms delay
}
```

### Container Layouts

```cpp
qtui::GroupBox group;
group.setTitle("Settings");
group.setPosition(5, 10);
group.setSize(15, 50);

// Child widgets positioned relative to screen (not group)
qtui::CheckBox option1;
option1.setPosition(7, 12);  // Inside group visually
group.addWidget(&option1);   // Render with group

// Disable entire group
group.setEnabled(false);  // Dims all children
```

## Dependencies

- Qt 6 Core (for QObject, signals/slots, QString)
- ncurses (for terminal UI)
- C++23 compiler

## Troubleshooting

### Problem: Widgets not rendering
**Solution:** Ensure you call `widget.show()` and `refresh()` after rendering.

### Problem: Mouse not working
**Solution:** Call `tuiApp->enableMouse()` after creating Application instance.

### Problem: Signals not emitted
**Solution:** Ensure CMAKE_AUTOMOC is enabled and headers are listed in CMakeLists.txt.

### Problem: Screen flickers
**Solution:** Call `clear()` once at start of render loop, not for each widget.

### Problem: Text input not working
**Solution:** Ensure widget has focus with `setFocus(true)` and route keys with `handleKey()`.

## Further Reading

- See `examples/` directory for complete working examples
- Check `tests/test-tui.cpp` for widget usage patterns
- Read widget headers in `include/qtui/` for complete API reference

## License

QTUI is released under the GPL-3.0 license, matching the gazelle-installer project.
