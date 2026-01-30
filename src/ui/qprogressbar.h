#pragma once

#include "context.h"
#include <QtWidgets/QProgressBar>
#include <qtui/tprogressbar.h>
#include <QtCore/QObject>

namespace ui {

class QProgressBar : public QObject
{
    Q_OBJECT

public:
    explicit QProgressBar(QWidget* parent = nullptr);
    ~QProgressBar() override;
    
    void setValue(int value);
    int value() const;
    
    void setMinimum(int minimum);
    int minimum() const;
    
    void setMaximum(int maximum);
    int maximum() const;
    
    void setRange(int minimum, int maximum);
    
    void setFormat(const QString& format);
    QString format() const;
    
    void setEnabled(bool enabled);
    bool isEnabled() const;
    
    void setPosition(int row, int col);
    void setWidth(int width);
    
    QWidget* widget() const;
    qtui::Widget* tuiWidget() const;
    
    void show();
    void hide();

signals:
    void valueChanged(int value);

private slots:
    void onGuiValueChanged(int value);
    void onTuiValueChanged(int value);

private:
    ::QProgressBar* m_guiProgressBar;
    qtui::TProgressBar* m_tuiProgressBar;
};

} // namespace ui
