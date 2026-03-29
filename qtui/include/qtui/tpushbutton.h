#pragma once

#include "widget.h"
#include <QString>

namespace qtui {

class TPushButton : public Widget
{
    Q_OBJECT

public:
    explicit TPushButton(const QString &text = QString(), Widget *parent = nullptr) noexcept;
    ~TPushButton() override;

    void setText(const QString &text) noexcept { this->buttonText = text; }
    QString text() const noexcept { return buttonText; }

    void setDefault(bool isDefault) noexcept { this->isDefaultButton = isDefault; }
    bool isDefault() const noexcept { return isDefaultButton; }

    void render() noexcept override;
    
    void setPosition(int row, int col) noexcept { this->row = row; this->col = col; }
    void setFocus(bool focused) noexcept { this->focused = focused; }
    bool hasFocus() const noexcept { return focused; }
    
    void click() noexcept;
    
    bool handleMouse(int mouseY, int mouseX) noexcept;
    bool handleKey(int key) noexcept;

signals:
    void clicked();
    void pressed();
    void released();

private:
    QString buttonText;
    bool focused = false;
    bool isDefaultButton = false;
    bool isPressed = false;
    int row = 0;
    int col = 0;
};

} // namespace qtui
