#pragma once

#include "context.h"
#include <QtWidgets/QPushButton>
#include <qtui/tpushbutton.h>
#include <QtCore/QObject>

namespace ui {

class QPushButton : public QObject
{
    Q_OBJECT

public:
    explicit QPushButton(const QString& text = QString(), QWidget* parent = nullptr);
    ~QPushButton() override;
    
    void setText(const QString& text);
    QString text() const;
    
    void setEnabled(bool enabled);
    bool isEnabled() const;
    
    void setPosition(int row, int col);
    
    QWidget* widget() const;
    qtui::Widget* tuiWidget() const;
    
    void show();
    void hide();

signals:
    void clicked();

private slots:
    void onGuiClicked();
    void onTuiClicked();

private:
    ::QPushButton* m_guiButton;
    qtui::TPushButton* m_tuiButton;
};

} // namespace ui
