# QTUI Library Examples

This directory contains example applications demonstrating how to use the QTUI library.

## Building Examples

```bash
cd qtui/examples
mkdir build && cd build
cmake ..
make
```

## Available Examples

### 1. login-form
Demonstrates:
- LineEdit widgets (text and password)
- CheckBox
- PushButton
- Signal/slot connections
- Focus management
- Form validation

Run: `./login-form`

### 2. installer-settings
Demonstrates:
- GroupBox containers
- RadioButton with ButtonGroup
- ComboBox dropdown
- Multiple grouped sections
- Complex layout

Run: `./installer-settings`

### 3. progress-demo
Demonstrates:
- ProgressBar widget
- QTimer integration
- Multi-phase progress tracking
- Dynamic label updates
- Cancellable operations

Run: `./progress-demo`

## Integration Notes

All examples follow this pattern:

```cpp
#include <QCoreApplication>
#include "qtui/application.h"
#include "qtui/widget.h"

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    
    // Initialize TUI
    auto *tuiApp = qtui::Application::instance();
    tuiApp->enableMouse();  // Enable mouse support
    
    // Create widgets
    qtui::Widget myWidget;
    myWidget.setPosition(row, col);
    myWidget.show();
    
    // Event loop
    while (running) {
        myWidget.render();
        refresh();
        
        int ch = getch();
        // Handle input...
    }
    
    return 0;
}
```

## CMakeLists.txt Template

```cmake
cmake_minimum_required(VERSION 3.21)
project(MyTUIApp)

set(CMAKE_CXX_STANDARD 23)
find_package(Qt6 REQUIRED COMPONENTS Core)
find_package(qtui REQUIRED)

add_executable(myapp main.cpp)
target_link_libraries(myapp qtui Qt6::Core)
```
