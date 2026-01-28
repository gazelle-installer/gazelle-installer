#pragma once

#include "widget.h"
#include <QString>
#include <QList>

namespace qtui {

class GroupBox : public Widget
{
    Q_OBJECT

public:
    explicit GroupBox(Widget *parent = nullptr) noexcept;
    ~GroupBox() override;

    void setTitle(const QString &title) noexcept { this->titleText = title; }
    QString title() const noexcept { return titleText; }

    void setEnabled(bool enabled) noexcept { this->enabled = enabled; }
    bool isEnabled() const noexcept { return enabled; }

    void render() noexcept override;

    void setPosition(int row, int col) noexcept { this->row = row; this->col = col; }
    void setSize(int height, int width) noexcept { this->height = height; this->width = width; }

    void addWidget(Widget *widget) noexcept;
    void removeWidget(Widget *widget) noexcept;

private:
    QString titleText;
    int row = 0;
    int col = 0;
    int height = 10;
    int width = 40;
    bool enabled = true;

    QList<Widget*> children;
};

} // namespace qtui
