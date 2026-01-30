#pragma once

#include "context.h"
#include <QtWidgets/QLineEdit>
#include <qtui/tlineedit.h>
#include <QtCore/QObject>

namespace ui {

class QLineEdit : public QObject
{
    Q_OBJECT

public:
    explicit QLineEdit(QWidget* parent = nullptr);
    ~QLineEdit() override;
    
    void setText(const QString& text);
    QString text() const;
    
    void setPlaceholderText(const QString& text);
    QString placeholderText() const;
    
    void setEchoMode(::QLineEdit::EchoMode mode);
    ::QLineEdit::EchoMode echoMode() const;
    
    void setMaxLength(int maxLength);
    int maxLength() const;
    
    void setEnabled(bool enabled);
    bool isEnabled() const;
    
    void setPosition(int row, int col);
    void setWidth(int width);
    
    QWidget* widget() const;
    qtui::Widget* tuiWidget() const;
    
    void show();
    void hide();
    void setFocus();

signals:
    void textChanged(const QString& text);
    void textEdited(const QString& text);
    void returnPressed();

private slots:
    void onGuiTextChanged(const QString& text);
    void onGuiTextEdited(const QString& text);
    void onGuiReturnPressed();
    void onTuiTextChanged(const QString& text);
    void onTuiTextEdited(const QString& text);
    void onTuiReturnPressed();

private:
    ::QLineEdit* m_guiLineEdit;
    qtui::TLineEdit* m_tuiLineEdit;
};

} // namespace ui
