#pragma once

#include "context.h"
#include <QtWidgets/QSlider>
#include <qtui/tslider.h>
#include <QtCore/QObject>

namespace ui {

class QSlider : public QObject
{
    Q_OBJECT

public:
    explicit QSlider(QWidget* parent = nullptr);
    ~QSlider() override;

    void setValue(int value);
    int value() const;

    void setMinimum(int minimum);
    int minimum() const;

    void setMaximum(int maximum);
    int maximum() const;

    void setRange(int minimum, int maximum);

    void setSingleStep(int step);
    int singleStep() const;

    void setPageStep(int step);
    int pageStep() const;

    void setLeftLabel(const QString& label);
    void setRightLabel(const QString& label);

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
    void sliderMoved(int position);

private slots:
    void onGuiValueChanged(int value);
    void onGuiSliderMoved(int position);
    void onTuiValueChanged(int value);
    void onTuiSliderMoved(int position);

private:
    ::QSlider* m_guiSlider;
    qtui::TSlider* m_tuiSlider;
};

} // namespace ui
