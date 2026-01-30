#include "qtui/tgroupbox.h"
#include "qtui/application.h"
#include <ncurses.h>

namespace qtui {

TGroupBox::TGroupBox(Widget *parent) noexcept
    : Widget(parent)
{
}

TGroupBox::~TGroupBox() = default;

void TGroupBox::addWidget(Widget *widget) noexcept
{
    if (widget && !children.contains(widget)) {
        children.append(widget);
    }
}

void TGroupBox::removeWidget(Widget *widget) noexcept
{
    children.removeAll(widget);
}

void TGroupBox::render() noexcept
{
    if (!visible) return;

    if (!enabled) {
        attron(A_DIM);
    }

    // Draw box borders
    // Top border with title
    mvaddch(row, col, ACS_ULCORNER);
    
    int titleLen = titleText.length();
    
    // Draw top line with title
    for (int i = 1; i < width - 1; ++i) {
        int pos = col + i;
        if (!titleText.isEmpty() && i >= 2 && i < 2 + titleLen + 2) {
            // Title area
            if (i == 2) {
                mvaddch(row, pos, ' ');
            } else if (i > 2 && i < 2 + titleLen + 1) {
                mvaddch(row, pos, titleText[i - 3].toLatin1());
            } else if (i == 2 + titleLen + 1) {
                mvaddch(row, pos, ' ');
            }
        } else {
            mvaddch(row, pos, ACS_HLINE);
        }
    }
    
    mvaddch(row, col + width - 1, ACS_URCORNER);

    // Left and right borders
    for (int i = 1; i < height - 1; ++i) {
        mvaddch(row + i, col, ACS_VLINE);
        mvaddch(row + i, col + width - 1, ACS_VLINE);
    }

    // Bottom border
    mvaddch(row + height - 1, col, ACS_LLCORNER);
    for (int i = 1; i < width - 1; ++i) {
        mvaddch(row + height - 1, col + i, ACS_HLINE);
    }
    mvaddch(row + height - 1, col + width - 1, ACS_LRCORNER);

    // Render child widgets
    for (Widget *child : children) {
        if (child) {
            child->render();
        }
    }

    if (!enabled) {
        attroff(A_DIM);
    }
}

} // namespace qtui
