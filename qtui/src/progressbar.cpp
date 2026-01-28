#include "qtui/progressbar.h"
#include "qtui/application.h"
#include <ncurses.h>

namespace qtui {

ProgressBar::ProgressBar(Widget *parent) noexcept
    : Widget(parent)
{
}

ProgressBar::~ProgressBar() = default;

void ProgressBar::setValue(int value) noexcept
{
    int newValue = qBound(minValue, value, maxValue);
    
    if (currentValue != newValue) {
        currentValue = newValue;
        emit valueChanged(currentValue);
    }
}

void ProgressBar::setMinimum(int min) noexcept
{
    minValue = min;
    if (currentValue < minValue) {
        setValue(minValue);
    }
}

void ProgressBar::setMaximum(int max) noexcept
{
    maxValue = max;
    if (currentValue > maxValue) {
        setValue(maxValue);
    }
}

void ProgressBar::setRange(int min, int max) noexcept
{
    minValue = min;
    maxValue = max;
    if (currentValue < minValue) {
        setValue(minValue);
    } else if (currentValue > maxValue) {
        setValue(maxValue);
    }
}

void ProgressBar::reset() noexcept
{
    setValue(minValue);
}

int ProgressBar::percentage() const noexcept
{
    if (maxValue == minValue) {
        return 0;
    }
    
    return ((currentValue - minValue) * 100) / (maxValue - minValue);
}

QString ProgressBar::formattedText() const noexcept
{
    QString text = formatStr;
    
    int pct = percentage();
    text.replace("%p", QString::number(pct));
    text.replace("%v", QString::number(currentValue));
    text.replace("%m", QString::number(maxValue));
    
    return text;
}

void ProgressBar::render() noexcept
{
    if (!visible) return;

    if (!enabled) {
        attron(A_DIM);
    }

    mvaddch(row, col, '[');
    
    int fillWidth = barWidth - 2;
    int pct = percentage();
    int filledChars = (fillWidth * pct) / 100;
    
    for (int i = 0; i < fillWidth; ++i) {
        char ch = ' ';
        
        if (i < filledChars) {
            ch = '=';
        } else if (i == filledChars && pct > 0) {
            ch = '>';
        }
        
        mvaddch(row, col + 1 + i, ch);
    }
    
    mvaddch(row, col + barWidth - 1, ']');

    if (textVisible) {
        QString text = formattedText();
        int textCol = col + barWidth + 1;
        mvprintw(row, textCol, " %s", text.toUtf8().constData());
    }

    if (!enabled) {
        attroff(A_DIM);
    }
}

} // namespace qtui
