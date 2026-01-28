#pragma once

#include "widget.h"
#include <QString>

namespace qtui {

class LineEdit : public Widget
{
    Q_OBJECT

public:
    enum EchoMode {
        Normal,
        NoEcho,
        Password,
        PasswordEchoOnEdit
    };

    explicit LineEdit(const QString &text = QString(), Widget *parent = nullptr) noexcept;
    ~LineEdit() override;

    void setText(const QString &text) noexcept;
    QString text() const noexcept { return editText; }

    void setEchoMode(EchoMode mode) noexcept { this->echoMode = mode; }
    EchoMode getEchoMode() const noexcept { return echoMode; }

    void setMaxLength(int maxLen) noexcept { this->maxLen = maxLen; }
    int maxLength() const noexcept { return maxLen; }

    void setPlaceholderText(const QString &text) noexcept { this->placeholder = text; }
    QString placeholderText() const noexcept { return placeholder; }

    void render() noexcept override;
    
    void setPosition(int row, int col) noexcept { this->row = row; this->col = col; }
    void setWidth(int width) noexcept { this->displayWidth = width; }
    void setFocus(bool focused) noexcept { this->focused = focused; }
    bool hasFocus() const noexcept { return focused; }
    
    void clear() noexcept;
    
    bool handleMouse(int mouseY, int mouseX) noexcept;
    bool handleKey(int key) noexcept;

signals:
    void textChanged(const QString &text);
    void textEdited(const QString &text);
    void returnPressed();
    void editingFinished();

private:
    QString editText;
    QString placeholder;
    EchoMode echoMode = Normal;
    int maxLen = 32767;
    int cursorPos = 0;
    int displayOffset = 0;
    int row = 0;
    int col = 0;
    int displayWidth = 20;
    bool focused = false;

    void moveCursorLeft() noexcept;
    void moveCursorRight() noexcept;
    void moveCursorHome() noexcept;
    void moveCursorEnd() noexcept;
    void insertChar(QChar ch) noexcept;
    void deleteChar() noexcept;
    void backspace() noexcept;
    QString getDisplayText() const noexcept;
    void adjustOffset() noexcept;
};

} // namespace qtui
