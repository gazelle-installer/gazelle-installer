#pragma once

#include "widget.h"
#include <QString>

namespace qtui {

class ProgressBar : public Widget
{
    Q_OBJECT

public:
    explicit ProgressBar(Widget *parent = nullptr) noexcept;
    ~ProgressBar() override;

    void setValue(int value) noexcept;
    int value() const noexcept { return currentValue; }

    void setMinimum(int min) noexcept;
    int minimum() const noexcept { return minValue; }

    void setMaximum(int max) noexcept;
    int maximum() const noexcept { return maxValue; }

    void setRange(int min, int max) noexcept;

    void setFormat(const QString &format) noexcept { this->formatStr = format; }
    QString format() const noexcept { return formatStr; }

    void setTextVisible(bool visible) noexcept { this->textVisible = visible; }
    bool isTextVisible() const noexcept { return textVisible; }

    void render() noexcept override;
    
    void setPosition(int row, int col) noexcept { this->row = row; this->col = col; }
    void setWidth(int width) noexcept { this->barWidth = width; }

    void reset() noexcept;

signals:
    void valueChanged(int value);

private:
    int currentValue = 0;
    int minValue = 0;
    int maxValue = 100;
    int row = 0;
    int col = 0;
    int barWidth = 40;
    QString formatStr = "%p%";
    bool textVisible = true;

    QString formattedText() const noexcept;
    int percentage() const noexcept;
};

} // namespace qtui
