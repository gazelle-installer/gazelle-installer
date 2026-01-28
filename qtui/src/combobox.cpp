#include "qtui/combobox.h"
#include "qtui/application.h"
#include <ncurses.h>

namespace qtui {

ComboBox::ComboBox(Widget *parent) noexcept
    : Widget(parent)
{
}

ComboBox::~ComboBox() = default;

void ComboBox::addItem(const QString &text, const QVariant &userData) noexcept
{
    Item item;
    item.text = text;
    item.data = userData;
    items.append(item);
    
    if (currentIdx < 0 && !items.isEmpty()) {
        currentIdx = 0;
    }
}

void ComboBox::addItems(const QStringList &texts) noexcept
{
    for (const QString &text : texts) {
        addItem(text);
    }
}

void ComboBox::insertItem(int index, const QString &text, const QVariant &userData) noexcept
{
    if (index < 0 || index > items.size()) return;
    
    Item item;
    item.text = text;
    item.data = userData;
    items.insert(index, item);
    
    if (currentIdx >= index) {
        currentIdx++;
    }
}

void ComboBox::removeItem(int index) noexcept
{
    if (index < 0 || index >= items.size()) return;
    
    items.removeAt(index);
    
    if (currentIdx >= items.size()) {
        currentIdx = items.size() - 1;
    }
    
    if (currentIdx >= 0) {
        emit currentIndexChanged(currentIdx);
    }
}

void ComboBox::clear() noexcept
{
    items.clear();
    currentIdx = -1;
    highlightIdx = 0;
    emit currentIndexChanged(-1);
}

void ComboBox::setCurrentIndex(int index) noexcept
{
    if (index < 0 || index >= items.size()) return;
    
    if (currentIdx != index) {
        currentIdx = index;
        highlightIdx = index;
        emit currentIndexChanged(index);
        emit currentTextChanged(items[index].text);
    }
}

QString ComboBox::currentText() const noexcept
{
    if (currentIdx >= 0 && currentIdx < items.size()) {
        return items[currentIdx].text;
    }
    return QString();
}

QVariant ComboBox::currentData() const noexcept
{
    if (currentIdx >= 0 && currentIdx < items.size()) {
        return items[currentIdx].data;
    }
    return QVariant();
}

QString ComboBox::itemText(int index) const noexcept
{
    if (index >= 0 && index < items.size()) {
        return items[index].text;
    }
    return QString();
}

QVariant ComboBox::itemData(int index) const noexcept
{
    if (index >= 0 && index < items.size()) {
        return items[index].data;
    }
    return QVariant();
}

void ComboBox::selectItem(int index) noexcept
{
    if (index < 0 || index >= items.size()) return;
    
    setCurrentIndex(index);
    emit activated(index);
    hidePopup();
}

void ComboBox::render() noexcept
{
    if (!visible) return;

    if (!enabled) {
        attron(A_DIM);
    }

    QString displayText = currentText();
    if (displayText.length() > displayWidth - 3) {
        displayText = displayText.left(displayWidth - 6) + "...";
    }
    
    mvaddch(row, col, '[');
    
    for (int i = 0; i < displayWidth - 3; ++i) {
        if (i < displayText.length()) {
            mvaddch(row, col + 1 + i, displayText[i].toLatin1());
        } else {
            mvaddch(row, col + 1 + i, ' ');
        }
    }
    
    if (focused) {
        attron(A_BOLD);
    }
    mvprintw(row, col + displayWidth - 2, " %c]", popupVisible ? '^' : 'v');
    if (focused) {
        attroff(A_BOLD);
    }

    // Clear popup area when hidden
    if (!popupVisible) {
        int popupRow = row + 1;
        int maxItems = qMin(8, items.size());
        int popupHeight = maxItems + 2; // +2 for top and bottom borders
        
        for (int i = 0; i < popupHeight; ++i) {
            mvhline(popupRow + i, col, ' ', displayWidth);
        }
    }

    if (popupVisible && !items.isEmpty()) {
        int popupRow = row + 1;
        int popupWidth = displayWidth;
        
        int maxItems = qMin(8, items.size());
        
        mvaddch(popupRow, col, ACS_ULCORNER);
        for (int i = 1; i < popupWidth - 1; ++i) {
            mvaddch(popupRow, col + i, ACS_HLINE);
        }
        mvaddch(popupRow, col + popupWidth - 1, ACS_URCORNER);
        
        for (int i = 0; i < maxItems; ++i) {
            int itemRow = popupRow + 1 + i;
            mvaddch(itemRow, col, ACS_VLINE);
            
            QString itemText = items[i].text;
            if (itemText.length() > popupWidth - 4) {
                itemText = itemText.left(popupWidth - 7) + "...";
            }
            
            if (i == highlightIdx) {
                attron(COLOR_PAIR(2) | A_BOLD);
            }
            
            mvprintw(itemRow, col + 1, " %-*s ", popupWidth - 4, 
                     itemText.toUtf8().constData());
            
            if (i == highlightIdx) {
                attroff(COLOR_PAIR(2) | A_BOLD);
            }
            
            mvaddch(itemRow, col + popupWidth - 1, ACS_VLINE);
        }
        
        mvaddch(popupRow + maxItems + 1, col, ACS_LLCORNER);
        for (int i = 1; i < popupWidth - 1; ++i) {
            mvaddch(popupRow + maxItems + 1, col + i, ACS_HLINE);
        }
        mvaddch(popupRow + maxItems + 1, col + popupWidth - 1, ACS_LRCORNER);
    }

    if (!enabled) {
        attroff(A_DIM);
    }
}

bool ComboBox::handleMouse(int mouseY, int mouseX) noexcept
{
    if (!enabled || !visible) return false;
    
    if (mouseY == row && mouseX >= col && mouseX < col + displayWidth) {
        if (popupVisible) {
            hidePopup();
        } else {
            showPopup();
        }
        return true;
    }
    
    if (popupVisible) {
        int popupRow = row + 1;
        int maxItems = qMin(8, items.size());
        
        for (int i = 0; i < maxItems; ++i) {
            int itemRow = popupRow + 1 + i;
            if (mouseY == itemRow && mouseX >= col && mouseX < col + displayWidth) {
                selectItem(i);
                return true;
            }
        }
    }
    
    return false;
}

bool ComboBox::handleKey(int key) noexcept
{
    if (!enabled || !visible || !focused) return false;

    if (key == ' ' || key == '\n' || key == KEY_ENTER) {
        if (popupVisible) {
            selectItem(highlightIdx);
        } else {
            showPopup();
        }
        return true;
    }
    
    if (popupVisible) {
        if (key == KEY_UP) {
            highlightIdx = (highlightIdx - 1 + items.size()) % items.size();
            return true;
        } else if (key == KEY_DOWN) {
            highlightIdx = (highlightIdx + 1) % items.size();
            return true;
        } else if (key == 27) {
            hidePopup();
            return true;
        }
    } else {
        if (key == KEY_UP) {
            if (currentIdx > 0) {
                setCurrentIndex(currentIdx - 1);
            }
            return true;
        } else if (key == KEY_DOWN) {
            if (currentIdx < items.size() - 1) {
                setCurrentIndex(currentIdx + 1);
            }
            return true;
        }
    }
    
    return false;
}

} // namespace qtui
