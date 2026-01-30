#include "qmessagebox.h"

namespace ui {

QMessageBox::StandardButton QMessageBox::critical(QWidget* parent, const QString& title, const QString& text,
                                                  StandardButtons buttons, StandardButton defaultButton)
{
    return showMessageBox(parent, Critical, title, text, buttons, defaultButton);
}

QMessageBox::StandardButton QMessageBox::warning(QWidget* parent, const QString& title, const QString& text,
                                                 StandardButtons buttons, StandardButton defaultButton)
{
    return showMessageBox(parent, Warning, title, text, buttons, defaultButton);
}

QMessageBox::StandardButton QMessageBox::information(QWidget* parent, const QString& title, const QString& text,
                                                     StandardButtons buttons, StandardButton defaultButton)
{
    return showMessageBox(parent, Information, title, text, buttons, defaultButton);
}

QMessageBox::StandardButton QMessageBox::question(QWidget* parent, const QString& title, const QString& text,
                                                  StandardButtons buttons, StandardButton defaultButton)
{
    return showMessageBox(parent, Question, title, text, buttons, defaultButton);
}

QMessageBox::StandardButton QMessageBox::showMessageBox(QWidget* parent, Icon icon, const QString& title,
                                                        const QString& text, StandardButtons buttons,
                                                        StandardButton defaultButton)
{
    if (Context::isGUI()) {
        ::QMessageBox::Icon qtIcon = ::QMessageBox::NoIcon;
        switch (icon) {
            case Information: qtIcon = ::QMessageBox::Information; break;
            case Warning: qtIcon = ::QMessageBox::Warning; break;
            case Critical: qtIcon = ::QMessageBox::Critical; break;
            case Question: qtIcon = ::QMessageBox::Question; break;
            default: break;
        }
        
        ::QMessageBox::StandardButton result = static_cast<::QMessageBox::StandardButton>(
            ::QMessageBox::question(parent, title, text, toQtButtons(buttons), toQtButton(defaultButton))
        );
        
        if (qtIcon != ::QMessageBox::Question) {
            // Use appropriate static method for icon type
            if (qtIcon == ::QMessageBox::Critical) {
                result = static_cast<::QMessageBox::StandardButton>(
                    ::QMessageBox::critical(parent, title, text, toQtButtons(buttons), toQtButton(defaultButton))
                );
            } else if (qtIcon == ::QMessageBox::Warning) {
                result = static_cast<::QMessageBox::StandardButton>(
                    ::QMessageBox::warning(parent, title, text, toQtButtons(buttons), toQtButton(defaultButton))
                );
            } else if (qtIcon == ::QMessageBox::Information) {
                result = static_cast<::QMessageBox::StandardButton>(
                    ::QMessageBox::information(parent, title, text, toQtButtons(buttons), toQtButton(defaultButton))
                );
            }
        }
        
        return fromQtButton(result);
    } else {
        qtui::TMessageBox::Icon tuiIcon = qtui::TMessageBox::NoIcon;
        switch (icon) {
            case Information: tuiIcon = qtui::TMessageBox::Information; break;
            case Warning: tuiIcon = qtui::TMessageBox::Warning; break;
            case Critical: tuiIcon = qtui::TMessageBox::Critical; break;
            case Question: tuiIcon = qtui::TMessageBox::Question; break;
            default: break;
        }
        
        qtui::TMessageBox::StandardButton result;
        if (tuiIcon == qtui::TMessageBox::Critical) {
            result = qtui::TMessageBox::critical(nullptr, title, text, toTuiButtons(buttons), toTuiButton(defaultButton));
        } else if (tuiIcon == qtui::TMessageBox::Warning) {
            result = qtui::TMessageBox::warning(nullptr, title, text, toTuiButtons(buttons), toTuiButton(defaultButton));
        } else {
            // Both Information and Question use information
            result = qtui::TMessageBox::information(nullptr, title, text, toTuiButtons(buttons), toTuiButton(defaultButton));
        }
        
        return fromTuiButton(result);
    }
}

::QMessageBox::StandardButton QMessageBox::toQtButton(StandardButton button)
{
    switch (button) {
        case Ok: return ::QMessageBox::Ok;
        case Cancel: return ::QMessageBox::Cancel;
        case Yes: return ::QMessageBox::Yes;
        case No: return ::QMessageBox::No;
        default: return ::QMessageBox::NoButton;
    }
}

::QMessageBox::StandardButtons QMessageBox::toQtButtons(StandardButtons buttons)
{
    ::QMessageBox::StandardButtons result;
    if (buttons & Ok) result |= ::QMessageBox::Ok;
    if (buttons & Cancel) result |= ::QMessageBox::Cancel;
    if (buttons & Yes) result |= ::QMessageBox::Yes;
    if (buttons & No) result |= ::QMessageBox::No;
    return result;
}

QMessageBox::StandardButton QMessageBox::fromQtButton(::QMessageBox::StandardButton button)
{
    switch (button) {
        case ::QMessageBox::Ok: return Ok;
        case ::QMessageBox::Cancel: return Cancel;
        case ::QMessageBox::Yes: return Yes;
        case ::QMessageBox::No: return No;
        default: return NoButton;
    }
}

qtui::TMessageBox::StandardButton QMessageBox::toTuiButton(StandardButton button)
{
    switch (button) {
        case Ok: return qtui::TMessageBox::Ok;
        case Cancel: return qtui::TMessageBox::Cancel;
        case Yes: return qtui::TMessageBox::Yes;
        case No: return qtui::TMessageBox::No;
        default: return qtui::TMessageBox::NoButton;
    }
}

qtui::TMessageBox::StandardButtons QMessageBox::toTuiButtons(StandardButtons buttons)
{
    qtui::TMessageBox::StandardButtons result;
    if (buttons & Ok) result |= qtui::TMessageBox::Ok;
    if (buttons & Cancel) result |= qtui::TMessageBox::Cancel;
    if (buttons & Yes) result |= qtui::TMessageBox::Yes;
    if (buttons & No) result |= qtui::TMessageBox::No;
    return result;
}

QMessageBox::StandardButton QMessageBox::fromTuiButton(qtui::TMessageBox::StandardButton button)
{
    switch (button) {
        case qtui::TMessageBox::Ok: return Ok;
        case qtui::TMessageBox::Cancel: return Cancel;
        case qtui::TMessageBox::Yes: return Yes;
        case qtui::TMessageBox::No: return No;
        default: return NoButton;
    }
}

} // namespace ui
