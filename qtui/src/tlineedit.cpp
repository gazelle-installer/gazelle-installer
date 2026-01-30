#include "qtui/tlineedit.h"
#include "qtui/application.h"
#include <ncurses.h>

namespace qtui {

TLineEdit::TLineEdit(const QString &text, Widget *parent) noexcept
    : Widget(parent), editText(text), cursorPos(text.length())
{
}

TLineEdit::~TLineEdit() = default;

void TLineEdit::setText(const QString &text) noexcept
{
    if (editText != text) {
        editText = text;
        cursorPos = text.length();
        displayOffset = 0;
        adjustOffset();
        emit textChanged(editText);
    }
}

void TLineEdit::clear() noexcept
{
    setText(QString());
}

QString TLineEdit::getDisplayText() const noexcept
{
    if (echoMode == NoEcho) {
        return QString();
    } else if (echoMode == Password || echoMode == PasswordEchoOnEdit) {
        return QString(editText.length(), '*');
    }
    return editText;
}

void TLineEdit::adjustOffset() noexcept
{
    if (cursorPos < displayOffset) {
        displayOffset = cursorPos;
    } else if (cursorPos >= displayOffset + displayWidth) {
        displayOffset = cursorPos - displayWidth + 1;
    }
}

void TLineEdit::moveCursorLeft() noexcept
{
    if (cursorPos > 0) {
        cursorPos--;
        adjustOffset();
    }
}

void TLineEdit::moveCursorRight() noexcept
{
    if (cursorPos < editText.length()) {
        cursorPos++;
        adjustOffset();
    }
}

void TLineEdit::moveCursorHome() noexcept
{
    cursorPos = 0;
    adjustOffset();
}

void TLineEdit::moveCursorEnd() noexcept
{
    cursorPos = editText.length();
    adjustOffset();
}

void TLineEdit::insertChar(QChar ch) noexcept
{
    if (editText.length() >= maxLen) return;
    
    editText.insert(cursorPos, ch);
    cursorPos++;
    adjustOffset();
    emit textChanged(editText);
    emit textEdited(editText);
}

void TLineEdit::deleteChar() noexcept
{
    if (cursorPos < editText.length()) {
        editText.remove(cursorPos, 1);
        emit textChanged(editText);
        emit textEdited(editText);
    }
}

void TLineEdit::backspace() noexcept
{
    if (cursorPos > 0) {
        editText.remove(cursorPos - 1, 1);
        cursorPos--;
        adjustOffset();
        emit textChanged(editText);
        emit textEdited(editText);
    }
}

void TLineEdit::render() noexcept
{
    if (!visible) return;

    if (!enabled) {
        attron(A_DIM);
    }

    mvaddch(row, col, '[');
    
    QString displayText = getDisplayText();
    if (displayText.isEmpty() && !focused) {
        displayText = placeholder;
        attron(A_DIM);
    }

    QString visibleText = displayText.mid(displayOffset, displayWidth);
    
    for (int i = 0; i < displayWidth; ++i) {
        if (i < visibleText.length()) {
            mvaddch(row, col + 1 + i, visibleText[i].toLatin1());
        } else {
            mvaddch(row, col + 1 + i, ' ');
        }
    }

    mvaddch(row, col + displayWidth + 1, ']');

    if (displayText.isEmpty() && !focused) {
        attroff(A_DIM);
    }

    if (focused && enabled) {
        int visualCursor = cursorPos - displayOffset;
        move(row, col + 1 + visualCursor);
        curs_set(1);
    } else {
        curs_set(0);
    }

    if (!enabled) {
        attroff(A_DIM);
    }
}

bool TLineEdit::handleMouse(int mouseY, int mouseX) noexcept
{
    if (!enabled || !visible) return false;
    
    if (mouseY == row && mouseX >= col + 1 && mouseX < col + displayWidth + 1) {
        int clickOffset = mouseX - (col + 1);
        cursorPos = displayOffset + clickOffset;
        if (cursorPos > editText.length()) {
            cursorPos = editText.length();
        }
        return true;
    }
    return false;
}

bool TLineEdit::handleKey(int key) noexcept
{
    if (!enabled || !visible || !focused) return false;

    switch (key) {
        case KEY_LEFT:
            moveCursorLeft();
            return true;
            
        case KEY_RIGHT:
            moveCursorRight();
            return true;
            
        case KEY_HOME:
            moveCursorHome();
            return true;
            
        case KEY_END:
            moveCursorEnd();
            return true;
            
        case KEY_BACKSPACE:
        case 127:
        case 8:
            backspace();
            return true;
            
        case KEY_DC:
            deleteChar();
            return true;
            
        case '\n':
        case KEY_ENTER:
            emit returnPressed();
            emit editingFinished();
            return true;
            
        default:
            if (key >= 32 && key < 127) {
                insertChar(QChar(key));
                return true;
            }
            break;
    }
    
    return false;
}

void TLineEdit::showCursor() noexcept
{
    if (focused && enabled && visible) {
        int visualCursor = cursorPos - displayOffset;
        move(row, col + 1 + visualCursor);
        curs_set(1);
    }
}

} // namespace qtui
