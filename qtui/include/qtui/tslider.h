#pragma once

#include "widget.h"
#include <QString>

namespace qtui {

class TSlider : public Widget
{
    Q_OBJECT

public:
    explicit TSlider(Widget *parent = nullptr) noexcept;
    ~TSlider() override;

    void setValue(int value) noexcept;
    int value() const noexcept { return currentValue; }

    void setMinimum(int min) noexcept;
    int minimum() const noexcept { return minValue; }

    void setMaximum(int max) noexcept;
    int maximum() const noexcept { return maxValue; }

    void setRange(int min, int max) noexcept;

    void setSingleStep(int step) noexcept { this->stepSize = step; }
    int singleStep() const noexcept { return stepSize; }

    void setPageStep(int step) noexcept { this->pageStepSize = step; }
    int pageStep() const noexcept { return pageStepSize; }

    void setLeftLabel(const QString &label) noexcept { this->leftLabelText = label; }
    QString leftLabel() const noexcept { return leftLabelText; }

    void setRightLabel(const QString &label) noexcept { this->rightLabelText = label; }
    QString rightLabel() const noexcept { return rightLabelText; }

    void render() noexcept override;
    void handleKey(int key) noexcept;
    bool handleMouse(int mouseY, int mouseX) noexcept;

    void setPosition(int row, int col) noexcept { this->row = row; this->col = col; }
    void setWidth(int width) noexcept { this->barWidth = width; }

    void setFocus(bool focused) noexcept { this->hasFocus = focused; }
    bool isFocused() const noexcept { return hasFocus; }

signals:
    void valueChanged(int value);
    void sliderMoved(int position);

private:
    int currentValue = 50;
    int minValue = 0;
    int maxValue = 100;
    int stepSize = 1;
    int pageStepSize = 10;
    int row = 0;
    int col = 0;
    int barWidth = 40;
    bool hasFocus = false;
    QString leftLabelText;
    QString rightLabelText;
};

} // namespace qtui
