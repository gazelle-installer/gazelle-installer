#pragma once

#include "widget.h"
#include <QString>

namespace qtui {

class TMessageBox : public Widget
{
    Q_OBJECT

public:
    enum Icon {
        NoIcon,
        Information,
        Warning,
        Critical,
        Question
    };

    enum StandardButton {
        NoButton   = 0x00000000,
        Ok         = 0x00000400,
        Save       = 0x00000800,
        SaveAll    = 0x00001000,
        Open       = 0x00002000,
        Yes        = 0x00004000,
        YesToAll   = 0x00008000,
        No         = 0x00010000,
        NoToAll    = 0x00020000,
        Abort      = 0x00040000,
        Retry      = 0x00080000,
        Ignore     = 0x00100000,
        Close      = 0x00200000,
        Cancel     = 0x00400000,
        Discard    = 0x00800000,
        Help       = 0x01000000,
        Apply      = 0x02000000,
        Reset      = 0x04000000
    };
    Q_DECLARE_FLAGS(StandardButtons, StandardButton)

    explicit TMessageBox(Widget *parent = nullptr) noexcept;
    ~TMessageBox() override;

    void setIcon(Icon icon) noexcept { this->iconType = icon; }
    Icon icon() const noexcept { return iconType; }

    void setText(const QString &text) noexcept { this->messageText = text; }
    QString text() const noexcept { return messageText; }

    void setWindowTitle(const QString &title) noexcept { this->titleText = title; }
    QString windowTitle() const noexcept { return titleText; }

    void setStandardButtons(StandardButtons buttons) noexcept { this->buttons = buttons; }
    StandardButtons standardButtons() const noexcept { return buttons; }

    void setDefaultButton(StandardButton button) noexcept { this->defaultBtn = button; }
    StandardButton defaultButton() const noexcept { return defaultBtn; }

    void setEscapeButton(StandardButton button) noexcept { this->escapeBtn = button; }
    StandardButton escapeButton() const noexcept { return escapeBtn; }

    StandardButton exec() noexcept;
    void render() noexcept override;

    // Static convenience methods
    static StandardButton critical(Widget *parent, const QString &title, 
                                   const QString &text,
                                   StandardButtons buttons = Ok,
                                   StandardButton defaultButton = NoButton) noexcept;

    static StandardButton warning(Widget *parent, const QString &title,
                                 const QString &text,
                                 StandardButtons buttons = Ok,
                                 StandardButton defaultButton = NoButton) noexcept;

    static StandardButton information(Widget *parent, const QString &title,
                                     const QString &text,
                                     StandardButtons buttons = Ok,
                                     StandardButton defaultButton = NoButton) noexcept;

private:
    Icon iconType = NoIcon;
    QString messageText;
    QString titleText;
    StandardButtons buttons = Ok;
    StandardButton defaultBtn = NoButton;
    StandardButton escapeBtn = NoButton;
    StandardButton result = NoButton;

    int selectedButton = 0;
    bool needsRedraw = true;

    void drawBox(int y, int x, int height, int width) noexcept;
    QString getIconText() const noexcept;
    QList<StandardButton> getButtonList() const noexcept;
    QString getButtonText(StandardButton button) const noexcept;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(qtui::TMessageBox::StandardButtons)

} // namespace qtui
