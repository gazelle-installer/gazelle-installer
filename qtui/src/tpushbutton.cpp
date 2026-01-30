#include "qtui/tpushbutton.h"
#include "qtui/application.h"
#include <ncurses.h>

namespace qtui {

TPushButton::TPushButton(const QString &text, Widget *parent) noexcept
    : Widget(parent), buttonText(text)
{
}

TPushButton::~TPushButton() = default;

void TPushButton::click() noexcept
{
    if (!enabled) return;
    
    emit pressed();
    emit clicked();
    emit released();
}

void TPushButton::render() noexcept
{
    if (!visible) return;
    
    QString displayText = "[ " + buttonText + " ]";
    
    if (focused || isDefaultButton) {
        attron(COLOR_PAIR(2) | A_BOLD);
    }
    
    if (!enabled) {
        attron(A_DIM);
    }
    
    mvprintw(row, col, "%s", displayText.toUtf8().constData());
    
    if (!enabled) {
        attroff(A_DIM);
    }
    
    if (focused || isDefaultButton) {
        attroff(COLOR_PAIR(2) | A_BOLD);
    }
}

bool TPushButton::handleMouse(int mouseY, int mouseX) noexcept
{
    if (!enabled || !visible) return false;
    
    QString displayText = "[ " + buttonText + " ]";
    int textLen = displayText.length();
    
    if (mouseY == row && mouseX >= col && mouseX < col + textLen) {
        click();
        return true;
    }
    return false;
}

bool TPushButton::handleKey(int key) noexcept
{
    if (!enabled || !visible || !focused) return false;
    
    if (key == '\n' || key == KEY_ENTER || key == ' ') {
        click();
        return true;
    }
    return false;
}

} // namespace qtui
