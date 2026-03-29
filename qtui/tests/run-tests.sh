#!/bin/bash

echo "Testing TUI Library MessageBox"
echo "==============================="
echo ""
echo "Test 1: MessageBox::critical"
echo "Press Enter to activate the OK button (or Tab to navigate, Space/Enter to select)"
echo "Press ESC to cancel"
echo ""
read -p "Press Enter to start test..." 

cd "$(dirname "$0")/../build/tests"
./test-tui MessageBox::critical nullptr "Error" "Something went wrong!"

echo ""
echo "Test 2: MessageBox::warning"
echo ""
read -p "Press Enter to start test..."

./test-tui MessageBox::warning nullptr "Warning" "Are you sure you want to continue?"

echo ""
echo "Test 3: MessageBox::information"
echo ""
read -p "Press Enter to start test..."

./test-tui MessageBox::information nullptr "Success" "Operation completed successfully!"

echo ""
echo "All tests completed!"
