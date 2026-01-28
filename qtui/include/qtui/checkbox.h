#pragma once

#include "widget.h"
#include <QString>

namespace qtui {

class CheckBox : public Widget
{
    Q_OBJECT

public:
    explicit CheckBox(const QString &text = QString(), Widget *parent = nullptr) noexcept;
    ~CheckBox() override;

    void setText(const QString &text) noexcept { this->labelText = text; }
    QString text() const noexcept { return labelText; }

    void setChecked(bool checked) noexcept;
    bool isChecked() const noexcept { return checked; }

    void toggle() noexcept;

    void render() noexcept override;
    
    void setPosition(int row, int col) noexcept { this->row = row; this->col = col; }
    void setFocus(bool focused) noexcept { this->focused = focused; }
    bool hasFocus() const noexcept { return focused; }
    
    bool handleMouse(int mouseY, int mouseX) noexcept;
    bool handleKey(int key) noexcept;

signals:
    void toggled(bool checked);
    void clicked();
    void stateChanged(int state);

private:
    QString labelText;
    bool checked = false;
    bool focused = false;
    int row = 0;
    int col = 0;
};

} // namespace qtui
