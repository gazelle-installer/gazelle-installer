#include "qtui/widget.h"

namespace qtui {

Widget::Widget(Widget *parent) noexcept
    : QObject(parent), parentWidget(parent)
{
}

Widget::~Widget() = default;

void Widget::show() noexcept
{
    visible = true;
}

void Widget::hide() noexcept
{
    visible = false;
}

} // namespace qtui
