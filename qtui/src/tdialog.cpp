#include "qtui/tdialog.h"
#include "qtui/application.h"
#include <ncurses.h>

namespace qtui {

TDialog::TDialog(Widget *parent) noexcept
    : Widget(parent)
{
}

TDialog::~TDialog() = default;

void TDialog::accept() noexcept
{
    done(Accepted);
}

void TDialog::reject() noexcept
{
    done(Rejected);
}

void TDialog::done(int result) noexcept
{
    dialogResult = result;
    dialogActive = false;
    
    if (result == Accepted) {
        emit accepted();
    } else {
        emit rejected();
    }
    emit finished(result);
}

int TDialog::exec() noexcept
{
    if (!isModal) {
        return Rejected;
    }

    dialogActive = true;
    dialogResult = Rejected;
    
    Application::instance()->enableMouse();
    curs_set(0); // Hide cursor

    while (dialogActive) {
        clear();
        render();
        refresh();

        int ch = getch();
        
        if (ch == 27) { // ESC
            reject();
        } else if (ch == '\n' || ch == KEY_ENTER) {
            accept();
        } else if (ch == KEY_MOUSE) {
            MEVENT event;
            if (getmouse(&event) == OK) {
                if (event.bstate & BUTTON1_CLICKED) {
                    // Handle mouse clicks - subclasses can override
                }
            }
        }
    }

    curs_set(1); // Show cursor
    return dialogResult;
}

void TDialog::render() noexcept
{
    if (!visible) return;

    // Draw title bar
    attron(COLOR_PAIR(1) | A_BOLD);
    for (int i = 0; i < width; ++i) {
        mvaddch(row, col + i, ' ');
    }
    
    QString displayTitle = titleText;
    if (displayTitle.length() > width - 4) {
        displayTitle = displayTitle.left(width - 7) + "...";
    }
    mvprintw(row, col + 2, "%s", displayTitle.toUtf8().constData());
    attroff(COLOR_PAIR(1) | A_BOLD);

    // Draw box borders
    mvaddch(row + 1, col, ACS_ULCORNER);
    for (int i = 1; i < width - 1; ++i) {
        mvaddch(row + 1, col + i, ACS_HLINE);
    }
    mvaddch(row + 1, col + width - 1, ACS_URCORNER);

    // Left and right borders
    for (int i = 2; i < height - 1; ++i) {
        mvaddch(row + i, col, ACS_VLINE);
        mvaddch(row + i, col + width - 1, ACS_VLINE);
    }

    // Bottom border
    mvaddch(row + height - 1, col, ACS_LLCORNER);
    for (int i = 1; i < width - 1; ++i) {
        mvaddch(row + height - 1, col + i, ACS_HLINE);
    }
    mvaddch(row + height - 1, col + width - 1, ACS_LRCORNER);

    // Render content (override in subclasses)
    renderContent();
}

void TDialog::renderContent() noexcept
{
    // Default: show help text
    mvprintw(row + 3, col + 2, "Press Enter to accept, ESC to cancel");
}

} // namespace qtui
