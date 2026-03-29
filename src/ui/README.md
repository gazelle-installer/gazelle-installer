# UI Wrapper Library

Unified widget library that works in both GUI (Qt Widgets) and TUI (ncurses) modes.

## Quick Start

### Build
```bash
cd src/ui
cmake -B build && cmake --build build
```

### Test

**Interactive menu:**
```bash
./run-tests.sh
```

**Force TUI mode (recommended for testing CLI):**
```bash
./build/test-wrapper --tui
```

**Force GUI mode:**
```bash
./build/test-wrapper --gui
```

**Auto-detect:**
```bash
./build/test-wrapper
```

**Alternative: Unset DISPLAY**
```bash
env -u DISPLAY ./build/test-wrapper
```

## Command-Line Options

- `--tui` or `-t` - Force TUI mode
- `--gui` or `-g` - Force GUI mode
- `--help` or `-h` - Show help

## Available Wrappers

All in `ui::` namespace:

- `ui::Context` - Mode detection and management
- `ui::QCheckBox` - Checkbox widget
- `ui::QRadioButton` - Radio button widget
- `ui::QPushButton` - Push button widget
- `ui::QLineEdit` - Text input widget
- `ui::QComboBox` - Dropdown/combobox widget
- `ui::QLabel` - Text label widget
- `ui::QProgressBar` - Progress bar widget
- `ui::QMessageBox` - Message dialog (static methods)

## Usage Example

```cpp
#include "ui/context.h"
#include "ui/qcheckbox.h"

// Detect and set mode
ui::Context::setMode(
    ui::Context::detectXDisplay() ? ui::Context::GUI : ui::Context::TUI
);

// Create widget (works in both modes!)
ui::QCheckBox *checkbox = new ui::QCheckBox();
checkbox->setText("Enable encryption");

// Connect signals (same for both!)
connect(checkbox, &ui::QCheckBox::toggled, this, &MyClass::onToggled);

// Layout: only difference between modes
if (ui::Context::isGUI()) {
    layout->addWidget(checkbox->widget());
} else {
    checkbox->setPosition(5, 10);
}
```

## Files

- `context.h/cpp` - Mode detection
- `qcheckbox.h/cpp` - CheckBox wrapper
- `qradiobutton.h/cpp` - RadioButton wrapper
- `qpushbutton.h/cpp` - PushButton wrapper
- `qlineedit.h/cpp` - LineEdit wrapper
- `qcombobox.h/cpp` - ComboBox wrapper
- `qlabel.h/cpp` - Label wrapper
- `qprogressbar.h/cpp` - ProgressBar wrapper
- `qmessagebox.h/cpp` - MessageBox wrapper
- `test-wrapper.cpp` - Test program
- `CMakeLists.txt` - Build configuration

## Dependencies

- Qt 6 Core
- Qt 6 Widgets
- QTUI library (../qtui/)
- ncurses

## See Also

- `TESTING.md` - Testing guide
- `../qtui/README.md` - QTUI library documentation
- `../../WRAPPER_RECOMMENDATION.md` - Architecture overview
- `../../WRAPPER_STATUS.md` - Project status
