# QTUI - Qt-compatible TUI Library

A portable, self-contained Text User Interface (TUI) library that provides API-compatible alternatives to Qt GUI widgets. Built with Qt 6 Core for signals/slots and ncurses for terminal rendering.

## Features

- **API Compatibility**: Drop-in TUI replacements for Qt widgets (without the 'Q' prefix)
- **Mouse Support**: Click buttons and interact with widgets using the mouse
- **Keyboard Navigation**: Tab to cycle focus, Space/Enter to activate, arrow keys to navigate
- **Qt Signals/Slots**: Full QObject integration for signal/slot connections
- **Portable**: Self-contained library that can be imported into any project

## Currently Implemented Widgets (10 Total - COMPLETE! 🎉)

### MessageBox ✅
Static convenience methods for common dialogs:
- `MessageBox::critical(parent, title, message)` - Critical error dialog
- `MessageBox::warning(parent, title, message)` - Warning dialog  
- `MessageBox::information(parent, title, message)` - Information dialog

Instance methods:
- `setIcon(Icon)` - Set dialog icon (Critical, Warning, Information, Question)
- `setText(QString)` - Set message text
- `setWindowTitle(QString)` - Set dialog title
- `setStandardButtons(StandardButtons)` - Set which buttons to display
- `setDefaultButton(StandardButton)` - Set default focused button
- `setEscapeButton(StandardButton)` - Set button activated by ESC key
- `exec()` - Show dialog and wait for user response

Supported buttons: Ok, Yes, No, Cancel, Save, Discard, Close, Abort, Retry, Ignore

### CheckBox ✅
Boolean toggle widget with visual feedback:
- `setText(QString)` / `text()` - Label text
- `setChecked(bool)` / `isChecked()` - Check state
- `toggle()` - Toggle checked state
- Visual: `[ ]` unchecked, `[X]` checked
- Signals: `toggled(bool)`, `clicked()`, `stateChanged(int)`

### RadioButton ✅
Exclusive selection widget with ButtonGroup support:
- `setText(QString)` / `text()` - Label text
- `setChecked(bool)` / `isChecked()` - Selection state
- `setButtonGroup(ButtonGroup*)` - Assign to exclusive group
- `toggle()` - Select this button (unchecks others in group)
- Visual: `( )` unchecked, `(•)` checked
- Signals: `toggled(bool)`, `clicked()`

**ButtonGroup** helper class:
- `addButton(RadioButton*)` - Add button to group
- `checkedButton()` - Get currently selected button
- `setExclusive(bool)` - Enable/disable exclusive mode

### PushButton ✅
Action button widget with focus/default highlighting:
- `setText(QString)` / `text()` - Button label
- `setDefault(bool)` - Mark as default button (highlighted)
- `click()` - Programmatically trigger click
- Visual: `[ Button Text ]`
- Signals: `clicked()`, `pressed()`, `released()`

### Label ✅
Static text display widget:
- `setText(QString)` / `text()` - Label content
- `setAlignment(int)` - Left, Center, or Right alignment
- `setWordWrap(bool)` - Enable word wrapping
- `setWidth(int)` - Maximum display width
- Visual: Plain text with alignment

### LineEdit ✅
Single-line text input widget with cursor:
- `setText(QString)` / `text()` - Content management
- `setEchoMode(EchoMode)` - Normal or Password display
- `setPlaceholderText(QString)` - Hint when empty
- `setMaxLength(int)` - Character limit
- `clear()` - Reset content
- Visual: `[text input here_]` or `[***********_]` (password)
- Navigation: Arrow keys, Home, End, Backspace, Delete
- Mouse: Click to position cursor
- Signals: `textChanged(QString)`, `textEdited(QString)`, `returnPressed()`

### ComboBox ✅
Dropdown selection widget:
- `addItem(QString, QVariant)` / `addItems(QStringList)` - Add options
- `setCurrentIndex(int)` / `currentIndex()` - Selection management
- `currentText()` / `currentData()` - Get selected item
- `count()` - Number of items
- Visual: `[Selected Item ▼]` with dropdown popup
- Navigation: Arrow keys to change, Space/Enter to open popup
- Mouse: Click to toggle popup, click items to select
- Signals: `currentIndexChanged(int)`, `currentTextChanged(QString)`, `activated(int)`

### ProgressBar ✅
Progress indication widget:
- `setValue(int)` / `value()` - Set/get current value
- `setMinimum(int)` / `setMaximum(int)` / `setRange(int, int)` - Range control
- `setFormat(QString)` - Custom text format (%p for percent, %v for value, %m for max)
- `setTextVisible(bool)` - Show/hide percentage text
- `reset()` - Reset to minimum
- Visual: `[=================>          ] 45%`
- Signals: `valueChanged(int)`

### GroupBox ✅
Container widget with titled border for grouping controls:
- `setTitle(QString)` / `title()` - Group title displayed in border
- `setEnabled(bool)` / `isEnabled()` - Enable/disable entire group (dims children)
- `setPosition(int, int)` - Set position on screen
- `setSize(int, int)` - Set height and width
- `addWidget(Widget*)` - Add child widget to group
- `removeWidget(Widget*)` - Remove child widget
- Visual: `┌─ Title ─────┐` box with title
- Usage: Group related controls (encryption settings, partition options, etc.)

### Dialog ✅
Modal dialog base class for custom dialogs:
- `setWindowTitle(QString)` / `windowTitle()` - Dialog title
- `setModal(bool)` / `modal()` - Modal mode (blocks until closed)
- `exec()` - Show modal dialog and wait for result
- `accept()` - Close dialog with Accepted result
- `reject()` - Close dialog with Rejected result
- `done(int)` - Close with custom result code
- `result()` - Get dialog result code
- Visual: Bordered dialog with title bar
- Signals: `accepted()`, `rejected()`, `finished(int)`
- Override `renderContent()` to customize dialog content
- Usage: Create custom dialogs by inheriting from Dialog class

### MessageBox ✅
Static convenience methods for common dialogs:
- `MessageBox::critical(parent, title, message)` - Critical error dialog
- `MessageBox::warning(parent, title, message)` - Warning dialog  
- `MessageBox::information(parent, title, message)` - Information dialog

Instance methods:
- `setIcon(Icon)` - Set dialog icon (Critical, Warning, Information, Question)
- `setText(QString)` - Set message text
- `setWindowTitle(QString)` - Set dialog title
- `setStandardButtons(StandardButtons)` - Set which buttons to display
- `setDefaultButton(StandardButton)` - Set default focused button
- `setEscapeButton(StandardButton)` - Set button activated by ESC key
- `exec()` - Show dialog and wait for user response

Supported buttons: Ok, Yes, No, Cancel, Save, Discard, Close, Abort, Retry, Ignore

### CheckBox ✅
Boolean toggle widget with visual feedback:
- `setText(QString)` / `text()` - Label text
- `setChecked(bool)` / `isChecked()` - Check state
- `toggle()` - Toggle checked state
- Visual: `[ ]` unchecked, `[X]` checked
- Signals: `toggled(bool)`, `clicked()`, `stateChanged(int)`

### RadioButton ✅
Exclusive selection widget with ButtonGroup support:
- `setText(QString)` / `text()` - Label text
- `setChecked(bool)` / `isChecked()` - Selection state
- `setButtonGroup(ButtonGroup*)` - Assign to exclusive group
- `toggle()` - Select this button (unchecks others in group)
- Visual: `( )` unchecked, `(•)` checked
- Signals: `toggled(bool)`, `clicked()`

**ButtonGroup** helper class:
- `addButton(RadioButton*)` - Add button to group
- `checkedButton()` - Get currently selected button
- `setExclusive(bool)` - Enable/disable exclusive mode

### PushButton ✅
Action button widget with focus/default highlighting:
- `setText(QString)` / `text()` - Button label
- `setDefault(bool)` - Mark as default button (highlighted)
- `click()` - Programmatically trigger click
- Visual: `[ Button Text ]`
- Signals: `clicked()`, `pressed()`, `released()`

### Label ✅
Static text display widget:
- `setText(QString)` / `text()` - Label content
- `setAlignment(int)` - Left, Center, or Right alignment
- `setWordWrap(bool)` - Enable word wrapping
- `setWidth(int)` - Maximum display width
- Visual: Plain text with alignment

### LineEdit ✅
Single-line text input widget with cursor:
- `setText(QString)` / `text()` - Content management
- `setEchoMode(EchoMode)` - Normal or Password display
- `setPlaceholderText(QString)` - Hint when empty
- `setMaxLength(int)` - Character limit
- `clear()` - Reset content
- Visual: `[text input here_]` or `[***********_]` (password)
- Navigation: Arrow keys, Home, End, Backspace, Delete
- Mouse: Click to position cursor
- Signals: `textChanged(QString)`, `textEdited(QString)`, `returnPressed()`

## Building

### Requirements
- Qt 6 Core
- ncurses
- CMake 3.21+
- C++23 compiler

### Build Instructions

```bash
mkdir build && cd build
cmake ..
cmake --build .
```

### Installation

```bash
cmake --build . --target install
```

## Usage Example

```cpp
#include <qtui/application.h>
#include <qtui/messagebox.h>

int main(int argc, char *argv[])
{
    qtui::Application app(argc, argv);
    
    auto result = qtui::MessageBox::critical(
        nullptr,
        "Error",
        "Something went wrong!"
    );
    
    if (result == qtui::MessageBox::Ok) {
        // Handle OK button click
    }
    
    return 0;
}
```

### Integration with CMake

```cmake
find_package(qtui REQUIRED)
target_link_libraries(your_app PRIVATE qtui)
```

## Testing

Run the test harness to see individual widgets:

```bash
# MessageBox dialogs
./build/tests/test-tui MessageBox::critical nullptr "Error" "Failed!"
./build/tests/test-tui MessageBox::yesno nullptr "Confirm" "Proceed?"

# Input widgets
./build/tests/test-tui CheckBox "Enable encryption"
./build/tests/test-tui RadioButton "Option A,Option B,Option C" 1
./build/tests/test-tui PushButton "Submit"

# Text widgets
./build/tests/test-tui Label "Centered Text" center 40
./build/tests/test-tui LineEdit "Hello World"
./build/tests/test-tui LineEdit "" password

# Selection and progress
./build/tests/test-tui ComboBox "Boot MBR,Boot PBR,Boot ESP" 0
./build/tests/test-tui ProgressBar 75

# Container widgets
./build/tests/test-tui GroupBox "Encryption Settings"
./build/tests/test-tui Dialog "Confirm Action"
```

Or run the interactive test script:

```bash
./tests/run-tests.sh
```

## Interaction

- **Mouse**: Click buttons directly
- **Keyboard**:
  - `Tab` / `Shift+Tab`: Cycle through buttons
  - `Left/Right Arrow`: Navigate between buttons
  - `Enter` / `Space`: Activate selected button
  - `ESC`: Cancel (activates escape button or last button)

## Roadmap

See [plan.md](../.copilot/session-state/bc18ff09-7288-4eb3-b508-ab2053750fbf/plan.md) for the full implementation plan.

### Upcoming Features (Optional Enhancements)
- Layout managers - VBox, HBox, Grid (manual positioning currently works well)
- Advanced widgets - Spinner, Menu, Table, TextEdit
- Custom themes and color schemes

## Architecture

- **Widget**: Base class for all TUI widgets (inherits QObject)
- **Application**: Manages ncurses initialization and event loop
- **MessageBox**: First complete widget implementation (proof of concept)

Each widget:
1. Inherits from `Widget` (which inherits `QObject`)
2. Implements `render()` for visual display
3. Emits Qt signals on user interaction
4. Supports mouse and keyboard input

## License

Follows the same license as the gazelle-installer project.

## Contributing

This library is being developed alongside the gazelle-installer project. New widgets are implemented based on actual usage in that codebase to ensure real-world applicability.
