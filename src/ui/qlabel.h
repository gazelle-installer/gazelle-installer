#pragma once

#include "context.h"
#include <QtWidgets/QLabel>
#include <qtui/tlabel.h>
#include <QtCore/QObject>

namespace ui {

class QLabel : public QObject
{
    Q_OBJECT

public:
    explicit QLabel(const QString& text = QString(), QWidget* parent = nullptr);
    ~QLabel() override;
    
    void setText(const QString& text);
    QString text() const;
    
    void setAlignment(Qt::Alignment alignment);
    Qt::Alignment alignment() const;
    
    void setEnabled(bool enabled);
    bool isEnabled() const;
    
    void setPosition(int row, int col);
    void setWidth(int width);
    
    QWidget* widget() const;
    qtui::Widget* tuiWidget() const;
    
    void show();
    void hide();

private:
    ::QLabel* m_guiLabel;
    qtui::TLabel* m_tuiLabel;
};

} // namespace ui
