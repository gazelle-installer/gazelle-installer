#include "qtui/tcombobox.h"
#include "qtui/application.h"
#include <ncurses.h>

namespace qtui {

TComboBox::TComboBox(Widget *parent) noexcept
    : Widget(parent)
{
}

TComboBox::~TComboBox() = default;

void TComboBox::addItem(const QString &text, const QVariant &userData) noexcept
{
    Item item;
    item.text = text;
    item.data = userData;
    items.append(item);
    
    if (currentIdx < 0 && !items.isEmpty()) {
        currentIdx = 0;
    }
}

void TComboBox::addItems(const QStringList &texts) noexcept
{
    for (const QString &text : texts) {
        addItem(text);
    }
}

void TComboBox::insertItem(int index, const QString &text, const QVariant &userData) noexcept
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

void TComboBox::removeItem(int index) noexcept
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

void TComboBox::clear() noexcept
{
    items.clear();
    currentIdx = -1;
    highlightIdx = 0;
    emit currentIndexChanged(-1);
}

void TComboBox::setCurrentIndex(int index) noexcept
{
    if (index < 0 || index >= items.size()) return;
    
    if (currentIdx != index) {
        currentIdx = index;
        highlightIdx = index;
        emit currentIndexChanged(index);
        emit currentTextChanged(items[index].text);
    }
}

QString TComboBox::currentText() const noexcept
{
    if (currentIdx >= 0 && currentIdx < items.size()) {
        return items[currentIdx].text;
    }
    return QString();
}

QVariant TComboBox::currentData() const noexcept
{
    if (currentIdx >= 0 && currentIdx < items.size()) {
        return items[currentIdx].data;
    }
    return QVariant();
}

QString TComboBox::itemText(int index) const noexcept
{
    if (index >= 0 && index < items.size()) {
        return items[index].text;
    }
    return QString();
}

QVariant TComboBox::itemData(int index) const noexcept
{
    if (index >= 0 && index < items.size()) {
        return items[index].data;
    }
    return QVariant();
}

void TComboBox::selectItem(int index) noexcept
{
    if (index < 0 || index >= items.size()) return;
    
    setCurrentIndex(index);
    emit activated(index);
    hidePopup();
}

void TComboBox::render() noexcept
{
    if (!visible) return;

    if (!enabled) {
        attron(A_DIM);
    }

    QString displayText = currentText();
    int maxTextWidth = displayWidth - 5;  // Account for "[ " and " v]"
    if (displayText.length() > maxTextWidth) {
        displayText = displayText.left(maxTextWidth - 3) + "...";
    }

    mvaddch(row, col, '[');
    move(row, col + 1);
    addstr(displayText.toUtf8().constData());

    // Pad with spaces and add dropdown indicator
    int curX, curY;
    getyx(stdscr, curY, curX);
    (void)curY;
    int indicatorCol = col + displayWidth - 2;
    while (curX < indicatorCol) {
        addch(' ');
        curX++;
    }

    if (focused) {
        attron(A_BOLD);
    }
    printw("%c]", popupVisible ? '^' : 'v');
    if (focused) {
        attroff(A_BOLD);
    }

    if (!enabled) {
        attroff(A_DIM);
    }
}

void TComboBox::renderPopup() noexcept
{
    if (!visible || !popupVisible || items.isEmpty()) return;

    int popupRow = row + 1;
    int popupWidth = displayWidth;

    // Calculate scroll offset for lists with more than 8 items
    int maxItems = qMin(8, items.size());
    int scrollOffset = 0;
    if (items.size() > 8) {
        // Keep highlighted item visible, center it when possible
        scrollOffset = qMax(0, qMin(highlightIdx - 3, items.size() - 8));
    }

    // Draw top border
    mvaddch(popupRow, col, ACS_ULCORNER);
    for (int i = 1; i < popupWidth - 1; ++i) {
        mvaddch(popupRow, col + i, ACS_HLINE);
    }
    mvaddch(popupRow, col + popupWidth - 1, ACS_URCORNER);

    // Draw items
    for (int i = 0; i < maxItems; ++i) {
        int itemIdx = i + scrollOffset;
        int itemRow = popupRow + 1 + i;
        mvaddch(itemRow, col, ACS_VLINE);

        QString itemText = items[itemIdx].text;
        // Truncate based on display width, not byte length
        int maxTextWidth = popupWidth - 4;
        if (itemText.length() > maxTextWidth) {
            itemText = itemText.left(maxTextWidth - 3) + "...";
        }

        if (itemIdx == highlightIdx) {
            attron(COLOR_PAIR(2) | A_BOLD);
        }

        // Use move + addstr for proper UTF-8 handling
        move(itemRow, col + 1);
        addch(' ');
        addstr(itemText.toUtf8().constData());
        // Pad with spaces to fill the column width
        int curX, curY;
        getyx(stdscr, curY, curX);
        (void)curY;
        int endCol = col + popupWidth - 2;
        while (curX < endCol) {
            addch(' ');
            curX++;
        }

        if (itemIdx == highlightIdx) {
            attroff(COLOR_PAIR(2) | A_BOLD);
        }

        mvaddch(itemRow, col + popupWidth - 1, ACS_VLINE);
    }

    // Draw bottom border with scroll indicator if needed
    mvaddch(popupRow + maxItems + 1, col, ACS_LLCORNER);
    for (int i = 1; i < popupWidth - 1; ++i) {
        mvaddch(popupRow + maxItems + 1, col + i, ACS_HLINE);
    }
    mvaddch(popupRow + maxItems + 1, col + popupWidth - 1, ACS_LRCORNER);

    // Show scroll indicators
    if (items.size() > 8) {
        if (scrollOffset > 0) {
            mvaddch(popupRow, col + popupWidth / 2, ACS_UARROW);
        }
        if (scrollOffset + 8 < items.size()) {
            mvaddch(popupRow + maxItems + 1, col + popupWidth / 2, ACS_DARROW);
        }
    }
}

bool TComboBox::handleMouse(int mouseY, int mouseX) noexcept
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

bool TComboBox::handleKey(int key) noexcept
{
    if (!enabled || !visible || !focused) return false;

    if (key == ' ' || key == '\n' || key == KEY_ENTER) {
        if (popupVisible) {
            selectItem(highlightIdx);
        } else {
            highlightIdx = currentIdx >= 0 ? currentIdx : 0;
            showPopup();
        }
        return true;
    }

    if (popupVisible) {
        if (key == KEY_UP) {
            if (highlightIdx > 0) {
                highlightIdx--;
            }
            return true;
        } else if (key == KEY_DOWN) {
            if (highlightIdx < items.size() - 1) {
                highlightIdx++;
            }
            return true;
        } else if (key == KEY_PPAGE) {  // Page Up
            highlightIdx = qMax(0, highlightIdx - 8);
            return true;
        } else if (key == KEY_NPAGE) {  // Page Down
            highlightIdx = qMin(items.size() - 1, highlightIdx + 8);
            return true;
        } else if (key == KEY_HOME) {
            highlightIdx = 0;
            return true;
        } else if (key == KEY_END) {
            highlightIdx = items.size() - 1;
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
