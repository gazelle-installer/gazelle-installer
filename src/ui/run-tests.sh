#!/bin/bash

echo "========================================="
echo "UI Wrapper Library Test"
echo "========================================="
echo ""

if [ ! -f build/test-wrapper ]; then
    echo "Error: test-wrapper not built!"
    echo "Run: cmake -B build && cmake --build build"
    exit 1
fi

echo "Choose test mode:"
echo "  1) GUI mode (requires X display)"
echo "  2) TUI mode (terminal only)"
echo "  3) Auto-detect"
echo ""
read -p "Enter choice (1-3): " choice

case $choice in
    1)
        echo ""
        echo "=== Running in GUI mode ==="
        ./build/test-wrapper --gui
        ;;
    2)
        echo ""
        echo "=== Running in TUI mode ==="
        ./build/test-wrapper --tui
        ;;
    3)
        echo ""
        echo "=== Auto-detecting mode ==="
        ./build/test-wrapper
        ;;
    *)
        echo "Invalid choice"
        exit 1
        ;;
esac
