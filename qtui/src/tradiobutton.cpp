#include "qtui/tradiobutton.h"
#include "qtui/application.h"
#include <ncurses.h>

namespace qtui {

// ButtonGroup implementation

TButtonGroup::TButtonGroup(QObject *parent) noexcept
    : QObject(parent)
{
}

TButtonGroup::~TButtonGroup()
{
    for (TRadioButton *button : buttonList) {
        if (button) {
            button->group = nullptr;
        }
    }
}

void TButtonGroup::addButton(TRadioButton *button) noexcept
{
    if (!button || buttonList.contains(button)) return;
    
    if (button->group) {
        button->group->removeButton(button);
    }
    
    buttonList.append(button);
    button->group = this;
}

void TButtonGroup::removeButton(TRadioButton *button) noexcept
{
    if (!button) return;
    buttonList.removeAll(button);
    if (button->group == this) {
        button->group = nullptr;
    }
}

TRadioButton *TButtonGroup::checkedButton() const noexcept
{
    for (TRadioButton *button : buttonList) {
        if (button && button->isChecked()) {
            return button;
        }
    }
    return nullptr;
}

void TButtonGroup::uncheckOthers(TRadioButton *except) noexcept
{
    if (!exclusive) return;
    
    for (TRadioButton *button : buttonList) {
        if (button && button != except && button->isChecked()) {
            button->checked = false;
            emit button->toggled(false);
        }
    }
}

// TRadioButton implementation

TRadioButton::TRadioButton(const QString &text, Widget *parent) noexcept
    : Widget(parent), labelText(text)
{
}

TRadioButton::~TRadioButton()
{
    if (group) {
        group->removeButton(this);
    }
}

void TRadioButton::setChecked(bool checked) noexcept
{
    if (this->checked == checked) return;
    
    if (checked && group) {
        group->uncheckOthers(this);
    }
    
    this->checked = checked;
    emit toggled(checked);
}

void TRadioButton::toggle() noexcept
{
    if (!checked) {
        setChecked(true);
        emit clicked();
    }
}

void TRadioButton::setButtonGroup(TButtonGroup *group) noexcept
{
    if (this->group == group) return;
    
    if (this->group) {
        this->group->removeButton(this);
    }
    
    if (group) {
        group->addButton(this);
    }
}

void TRadioButton::render() noexcept
{
    if (!visible) return;

    if (focused) {
        attron(A_UNDERLINE);
    }

    // Use ASCII-safe characters to avoid encoding issues
    const char* checkMark = checked ? "(*)" : "( )";
    mvprintw(row, col, "%s %s", checkMark,
             labelText.toUtf8().constData());

    if (focused) {
        attroff(A_UNDERLINE);
    }
}

bool TRadioButton::handleMouse(int mouseY, int mouseX) noexcept
{
    if (!enabled || !visible) return false;
    
    int textLen = 4 + labelText.length();
    if (mouseY == row && mouseX >= col && mouseX < col + textLen) {
        toggle();
        return true;
    }
    return false;
}

bool TRadioButton::handleKey(int key) noexcept
{
    if (!enabled || !visible || !focused) return false;
    
    if (key == ' ' || key == '\n' || key == KEY_ENTER) {
        toggle();
        return true;
    }
    return false;
}

} // namespace qtui
