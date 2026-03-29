#pragma once

#include "context.h"
#include <QtWidgets/QCheckBox>
#include <qtui/tcheckbox.h>
#include <QtCore/QObject>

namespace ui {

class QCheckBox : public QObject
{
    Q_OBJECT

public:
    explicit QCheckBox(QWidget* parent = nullptr);
    ~QCheckBox() override;
    
    void setText(const QString& text);
    QString text() const;
    
    void setChecked(bool checked);
    bool isChecked() const;
    
    void setEnabled(bool enabled);
    bool isEnabled() const;
    
    void setPosition(int row, int col);
    
    void setFocus(bool focus);
    bool hasFocus() const;
    
    QWidget* widget() const;
    qtui::Widget* tuiWidget() const;
    
    void show();
    void hide();

signals:
    void toggled(bool checked);
    void clicked();

private slots:
    void onGuiToggled(bool checked);
    void onGuiClicked();
    void onTuiToggled(bool checked);
    void onTuiClicked();

private:
    ::QCheckBox* m_guiCheckBox;
    qtui::TCheckBox* m_tuiCheckBox;
};

} // namespace ui
