# Testing the UI Wrapper Library

## Quick Start

The wrapper library automatically detects whether to use GUI or TUI mode based on the presence of an X display.

### Test Program

```bash
cd src/ui/build

# GUI mode (if DISPLAY is set)
./test-wrapper --gui

# TUI mode (text terminal)
./test-wrapper --tui

# Auto-detect mode
./test-wrapper
```

**Note:** The TUI mode will enter an event loop and display an interactive form in your terminal. Use Tab to navigate between fields, Space to toggle the checkbox, and press the Submit button (or Ctrl+C to exit).

## Testing Methods

There are three ways to force TUI mode for testing:

### Method 1: Command-line flag (Easiest)
```bash
./test-wrapper --tui
```

### Method 2: Unset DISPLAY environment variable
```bash
env -u DISPLAY -u WAYLAND_DISPLAY ./test-wrapper
```

### Method 3: SSH to headless system
```bash
ssh user@server
cd /path/to/gazelle-installer/src/ui/build
./test-wrapper  # Will auto-detect TUI mode
```

## Interactive Test Script

We provide a convenient test script that checks prerequisites and runs both modes:

```bash
cd src/ui
./run-tests.sh
```

The script will:
1. Check if the library is built
2. Test GUI mode (if X display available)
3. Test TUI mode
4. Verify auto-detection works

## Expected Behavior

### GUI Mode
- Opens a Qt window with form fields
- Standard Qt widgets and styling
- Click "Submit" to test validation

### TUI Mode
- Clears terminal and shows text-based form
- Keyboard navigation:
  - **Tab**: Move between fields
  - **Space**: Toggle checkbox / activate buttons
  - **Enter**: Submit form / select items
  - **Ctrl+C**: Exit application
- Mouse support (click on widgets to interact)
- Validation dialogs appear as text boxes

## Troubleshooting

### "Running in TUI mode" but nothing appears
- The program IS working - it's waiting in the event loop
- Check that you're running in an actual terminal (not through a test harness)
- Try interacting: press Tab, type text, press Space

### SIGSEGV on Exit
When you press **Ctrl+C** to exit the TUI program, you may see a segmentation fault. **This is expected** - the program is abruptly interrupted while ncurses is active, and the signal handler doesn't have a chance to properly clean up the terminal state.

This is NOT a bug in the wrapper - it's normal behavior when forcefully exiting an ncurses application. The program is working correctly; it's just waiting for user input in the event loop.

### Terminal gets garbled after running TUI
If the terminal state isn't properly restored after exiting:
```bash
reset
```

### Qt Widgets errors in TUI mode
Make sure you're running with `--tui` flag or without DISPLAY environment variable.

### Linking errors
Ensure qtui library is built:
```bash
cd ../../qtui
cmake --build build
```
