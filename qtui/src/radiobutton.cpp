#include "qtui/radiobutton.h"
#include "qtui/application.h"
#include <ncurses.h>

namespace qtui {

// ButtonGroup implementation

ButtonGroup::ButtonGroup(QObject *parent) noexcept
    : QObject(parent)
{
}

ButtonGroup::~ButtonGroup()
{
    for (RadioButton *button : buttonList) {
        if (button) {
            button->group = nullptr;
        }
    }
}

void ButtonGroup::addButton(RadioButton *button) noexcept
{
    if (!button || buttonList.contains(button)) return;
    
    if (button->group) {
        button->group->removeButton(button);
    }
    
    buttonList.append(button);
    button->group = this;
}

void ButtonGroup::removeButton(RadioButton *button) noexcept
{
    if (!button) return;
    buttonList.removeAll(button);
    if (button->group == this) {
        button->group = nullptr;
    }
}

RadioButton *ButtonGroup::checkedButton() const noexcept
{
    for (RadioButton *button : buttonList) {
        if (button && button->isChecked()) {
            return button;
        }
    }
    return nullptr;
}

void ButtonGroup::uncheckOthers(RadioButton *except) noexcept
{
    if (!exclusive) return;
    
    for (RadioButton *button : buttonList) {
        if (button && button != except && button->isChecked()) {
            button->checked = false;
            emit button->toggled(false);
        }
    }
}

// RadioButton implementation

RadioButton::RadioButton(const QString &text, Widget *parent) noexcept
    : Widget(parent), labelText(text)
{
}

RadioButton::~RadioButton()
{
    if (group) {
        group->removeButton(this);
    }
}

void RadioButton::setChecked(bool checked) noexcept
{
    if (this->checked == checked) return;
    
    if (checked && group) {
        group->uncheckOthers(this);
    }
    
    this->checked = checked;
    emit toggled(checked);
}

void RadioButton::toggle() noexcept
{
    if (!checked) {
        setChecked(true);
        emit clicked();
    }
}

void RadioButton::setButtonGroup(ButtonGroup *group) noexcept
{
    if (this->group == group) return;
    
    if (this->group) {
        this->group->removeButton(this);
    }
    
    if (group) {
        group->addButton(this);
    }
}

void RadioButton::render() noexcept
{
    if (!visible) return;
    
    if (focused) {
        attron(A_UNDERLINE);
    }
    
    QString checkMark = checked ? "(•)" : "( )";
    mvprintw(row, col, "%s %s", checkMark.toUtf8().constData(), 
             labelText.toUtf8().constData());
    
    if (focused) {
        attroff(A_UNDERLINE);
    }
}

bool RadioButton::handleMouse(int mouseY, int mouseX) noexcept
{
    if (!enabled || !visible) return false;
    
    int textLen = 4 + labelText.length();
    if (mouseY == row && mouseX >= col && mouseX < col + textLen) {
        toggle();
        return true;
    }
    return false;
}

bool RadioButton::handleKey(int key) noexcept
{
    if (!enabled || !visible || !focused) return false;
    
    if (key == ' ' || key == '\n' || key == KEY_ENTER) {
        toggle();
        return true;
    }
    return false;
}

} // namespace qtui
