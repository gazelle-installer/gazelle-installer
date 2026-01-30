#pragma once

#include "context.h"
#include <QtWidgets/QMessageBox>
#include <qtui/tmessagebox.h>
#include <QtCore/QObject>

namespace ui {

class QMessageBox
{
public:
    enum StandardButton {
        NoButton   = 0x00000000,
        Ok         = 0x00000400,
        Cancel     = 0x00400000,
        Yes        = 0x00004000,
        No         = 0x00010000
    };
    Q_DECLARE_FLAGS(StandardButtons, StandardButton)
    
    enum Icon {
        NoIcon = 0,
        Information = 1,
        Warning = 2,
        Critical = 3,
        Question = 4
    };
    
    static StandardButton critical(QWidget* parent, const QString& title, const QString& text,
                                   StandardButtons buttons = Ok, StandardButton defaultButton = NoButton);
    
    static StandardButton warning(QWidget* parent, const QString& title, const QString& text,
                                  StandardButtons buttons = Ok, StandardButton defaultButton = NoButton);
    
    static StandardButton information(QWidget* parent, const QString& title, const QString& text,
                                      StandardButtons buttons = Ok, StandardButton defaultButton = NoButton);
    
    static StandardButton question(QWidget* parent, const QString& title, const QString& text,
                                   StandardButtons buttons = StandardButtons(Yes | No), 
                                   StandardButton defaultButton = NoButton);

private:
    static StandardButton showMessageBox(QWidget* parent, Icon icon, const QString& title, 
                                        const QString& text, StandardButtons buttons, 
                                        StandardButton defaultButton);
    
    static ::QMessageBox::StandardButton toQtButton(StandardButton button);
    static ::QMessageBox::StandardButtons toQtButtons(StandardButtons buttons);
    static StandardButton fromQtButton(::QMessageBox::StandardButton button);
    
    static qtui::TMessageBox::StandardButton toTuiButton(StandardButton button);
    static qtui::TMessageBox::StandardButtons toTuiButtons(StandardButtons buttons);
    static StandardButton fromTuiButton(qtui::TMessageBox::StandardButton button);
};

Q_DECLARE_OPERATORS_FOR_FLAGS(QMessageBox::StandardButtons)

} // namespace ui
