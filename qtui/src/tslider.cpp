#include "qtui/tslider.h"
#include "qtui/application.h"
#include <ncurses.h>

namespace qtui {

TSlider::TSlider(Widget *parent) noexcept
    : Widget(parent)
{
}

TSlider::~TSlider() = default;

void TSlider::setValue(int value) noexcept
{
    int newValue = qBound(minValue, value, maxValue);

    if (currentValue != newValue) {
        currentValue = newValue;
        emit valueChanged(currentValue);
    }
}

void TSlider::setMinimum(int min) noexcept
{
    minValue = min;
    if (currentValue < minValue) {
        setValue(minValue);
    }
}

void TSlider::setMaximum(int max) noexcept
{
    maxValue = max;
    if (currentValue > maxValue) {
        setValue(maxValue);
    }
}

void TSlider::setRange(int min, int max) noexcept
{
    minValue = min;
    maxValue = max;
    if (currentValue < minValue) {
        setValue(minValue);
    } else if (currentValue > maxValue) {
        setValue(maxValue);
    }
}

void TSlider::handleKey(int key) noexcept
{
    int oldValue = currentValue;

    switch (key) {
        case KEY_LEFT:
            setValue(currentValue - stepSize);
            break;
        case KEY_RIGHT:
            setValue(currentValue + stepSize);
            break;
        case KEY_PPAGE: // Page Up
        case KEY_HOME:
            setValue(currentValue - pageStepSize);
            break;
        case KEY_NPAGE: // Page Down
        case KEY_END:
            setValue(currentValue + pageStepSize);
            break;
        default:
            break;
    }

    if (currentValue != oldValue) {
        emit sliderMoved(currentValue);
    }
}

void TSlider::render() noexcept
{
    if (!visible) return;

    if (!enabled) {
        attron(A_DIM);
    }

    // Calculate positions - use absolute percentage (0-100) for visual representation
    int innerWidth = barWidth - 2; // Exclude brackets
    // Position based on absolute value (currentValue is a percentage 0-100)
    int sliderPos = (currentValue * innerWidth) / 100;
    sliderPos = qBound(0, sliderPos, innerWidth - 1);
    // Minimum position marker
    int minPos = (minValue * innerWidth) / 100;

    // Render left label with percentage
    QString leftText = leftLabelText;
    if (!leftText.isEmpty()) {
        leftText += QString(" %1%").arg(currentValue);
        mvprintw(row, col, "%s", leftText.toUtf8().constData());
    }

    // Calculate slider bar position
    int barCol = col;
    if (!leftLabelText.isEmpty()) {
        barCol = col + 12; // Leave space for label
    }

    // Draw the slider bar
    if (hasFocus) {
        attron(A_BOLD);
    }

    mvaddch(row, barCol, '[');

    for (int i = 0; i < innerWidth; ++i) {
        char ch;
        if (i < minPos) {
            // Below minimum - show as blocked/unavailable
            ch = 'x';
        } else if (i < sliderPos) {
            ch = '=';
        } else if (i == sliderPos) {
            if (hasFocus) {
                attron(COLOR_PAIR(2));
                ch = ' ';
            } else {
                ch = '|';
            }
        } else {
            ch = '-';
        }
        mvaddch(row, barCol + 1 + i, ch);
        if (i == sliderPos && hasFocus) {
            attroff(COLOR_PAIR(2));
        }
    }

    mvaddch(row, barCol + barWidth - 1, ']');

    if (hasFocus) {
        attroff(A_BOLD);
    }

    // Render right label with percentage
    QString rightText = rightLabelText;
    if (!rightText.isEmpty()) {
        int rightPercent = 100 - currentValue;
        rightText += QString(" %1%").arg(rightPercent);
        int rightCol = barCol + barWidth + 1;
        mvprintw(row, rightCol, "%s", rightText.toUtf8().constData());
    }

    if (!enabled) {
        attroff(A_DIM);
    }
}

bool TSlider::handleMouse(int mouseY, int mouseX) noexcept
{
    if (!visible || !enabled) return false;

    // Check if click is on this row
    if (mouseY != row) return false;

    // Calculate slider bar position
    int barCol = col;
    if (!leftLabelText.isEmpty()) {
        barCol = col + 12;
    }

    // Check if click is within the slider bar area
    if (mouseX >= barCol && mouseX < barCol + barWidth) {
        // Convert click position to absolute percentage (0-100)
        int innerWidth = barWidth - 2;
        int clickPos = mouseX - barCol - 1; // Position within inner area
        clickPos = qBound(0, clickPos, innerWidth - 1);

        // Convert to percentage (0-100), setValue will clamp to min/max
        int newValue = (clickPos * 100) / (innerWidth - 1);
        setValue(newValue);
        emit sliderMoved(currentValue);
        return true;
    }

    return false;
}

} // namespace qtui
