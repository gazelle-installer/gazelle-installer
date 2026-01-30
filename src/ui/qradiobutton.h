#pragma once

#include "context.h"
#include <QtWidgets/QRadioButton>
#include <QtWidgets/QButtonGroup>
#include <qtui/tradiobutton.h>
#include <QtCore/QObject>

namespace ui {

class QRadioButton : public QObject
{
    Q_OBJECT

public:
    explicit QRadioButton(QWidget* parent = nullptr);
    ~QRadioButton() override;
    
    void setText(const QString& text);
    QString text() const;
    
    void setChecked(bool checked);
    bool isChecked() const;
    
    void setEnabled(bool enabled);
    bool isEnabled() const;
    
    void setPosition(int row, int col);
    void setFocus(bool focused);

    void setButtonGroup(::QButtonGroup* guiGroup, qtui::TButtonGroup* tuiGroup);
    
    QWidget* widget() const;
    qtui::Widget* tuiWidget() const;
    
    void show();
    void hide();

signals:
    void toggled(bool checked);

private slots:
    void onGuiToggled(bool checked);
    void onTuiToggled(bool checked);

private:
    ::QRadioButton* m_guiRadioButton;
    qtui::TRadioButton* m_tuiRadioButton;
};

} // namespace ui
