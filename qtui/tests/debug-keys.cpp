#include <ncurses.h>
#include <iostream>

int main() {
    initscr();
    raw();
    keypad(stdscr, TRUE);
    noecho();
    
    mvprintw(0, 0, "Press keys to see their codes. Press 'q' to quit.");
    refresh();
    
    while (true) {
        int ch = getch();
        mvprintw(2, 0, "Key code: %d (0x%x) '%c'    ", ch, ch, (ch >= 32 && ch < 127) ? ch : '?');
        
        if (ch == '\n') {
            mvprintw(3, 0, "This is newline (\\n)");
        } else if (ch == KEY_ENTER) {
            mvprintw(3, 0, "This is KEY_ENTER     ");
        } else if (ch == 10) {
            mvprintw(3, 0, "This is ASCII 10      ");
        } else if (ch == 13) {
            mvprintw(3, 0, "This is ASCII 13 (CR) ");
        } else {
            mvprintw(3, 0, "                      ");
        }
        
        refresh();
        
        if (ch == 'q' || ch == 'Q') {
            break;
        }
    }
    
    endwin();
    return 0;
}
