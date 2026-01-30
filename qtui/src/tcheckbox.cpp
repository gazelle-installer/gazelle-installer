#include "qtui/tcheckbox.h"
#include "qtui/application.h"
#include <ncurses.h>

namespace qtui {

TCheckBox::TCheckBox(const QString &text, Widget *parent) noexcept
    : Widget(parent), labelText(text)
{
}

TCheckBox::~TCheckBox() = default;

void TCheckBox::setChecked(bool checked) noexcept
{
    if (this->checked != checked) {
        this->checked = checked;
        emit toggled(checked);
        emit stateChanged(checked ? 2 : 0);
    }
}

void TCheckBox::toggle() noexcept
{
    setChecked(!checked);
    emit clicked();
}

void TCheckBox::render() noexcept
{
    if (!visible) return;
    
    if (focused) {
        attron(A_UNDERLINE);
    }
    
    QString checkMark = checked ? "[X]" : "[ ]";
    mvprintw(row, col, "%s %s", checkMark.toUtf8().constData(), 
             labelText.toUtf8().constData());
    
    if (focused) {
        attroff(A_UNDERLINE);
    }
}

bool TCheckBox::handleMouse(int mouseY, int mouseX) noexcept
{
    if (!enabled || !visible) return false;
    
    int textLen = 4 + labelText.length();
    if (mouseY == row && mouseX >= col && mouseX < col + textLen) {
        toggle();
        return true;
    }
    return false;
}

bool TCheckBox::handleKey(int key) noexcept
{
    if (!enabled || !visible || !focused) return false;
    
    if (key == ' ' || key == '\n' || key == KEY_ENTER) {
        toggle();
        return true;
    }
    return false;
}

} // namespace qtui
