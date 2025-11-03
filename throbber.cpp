/***************************************************************************
 * Throbber busy wait splash indicator.
 *
 *   Copyright (C) 2022-2025 by AK-47
 *   Licensed under the Apache License, Version 2.0 (the "License");
 *   you may not use this file except in compliance with the License.
 *   You may obtain a copy of the License at
 *
 *       http://www.apache.org/licenses/LICENSE-2.0
 *
 *   Unless required by applicable law or agreed to in writing, software
 *   distributed under the License is distributed on an "AS IS" BASIS,
 *   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *   See the License for the specific language governing permissions and
 *   limitations under the License.
 *
 * This file is part of the gazelle-installer.
 ***************************************************************************/

#include <QEvent>
#include <QPainter>
#include <QConicalGradient>
#include "throbber.h"

Throbber::Throbber(QWidget *widget, QObject *parent) noexcept
    : QObject{parent}, timer(this), widget(widget)
{
    widget->installEventFilter(this);
    connect(&timer, &QTimer::timeout, this, &Throbber::timer_timeout);
    timer.start(120);
    widget->update();
}
Throbber::~Throbber() noexcept
{
    timer.stop();
    widget->removeEventFilter(this);
    widget->update();
}

bool Throbber::eventFilter(QObject *watched, QEvent *event) noexcept
{
    if (watched == widget && event->type() == QEvent::Paint) {
        // Draw the load indicator on the splash screen.
        QPainter painter(widget);
        painter.setRenderHints(QPainter::Antialiasing);
        const int lW = widget->width(), lH = widget->height();
        painter.translate(lW / 2, lH / 2);
        painter.scale(lW / 200.0, lH / 200.0);

        static constexpr int blades = 12;
        static constexpr qreal angle = 360.0 / blades;
        static constexpr float alphastep = 0.18 / blades;
        const float huestep = std::min(position, 360) / (360.0 * blades);

        float hue = 1.0, alpha = 0.18;
        QPen pen;
        pen.setWidth(3);
        pen.setJoinStyle(Qt::MiterJoin);

        painter.rotate(angle * position); // Angle of current position.
        for (unsigned int ixi=blades; ixi>0; --ixi) {
            static constexpr QPoint blade[] = {{-15, -6}, {15, -75}, {0, -93}, {-15, -75}};
            static constexpr qreal bladeangle = 23.5; // atan(30/(75-6)) in degrees.

            QConicalGradient gradient(-15, -6, 90-bladeangle);
            const QColor &color = QColor::fromHsvF(hue, 1.0, 1.0, alpha);
            gradient.setStops({{0, color}, {bladeangle/360.0, color.darker()}});
            hue -= huestep, alpha += alphastep;

            painter.setBrush(gradient);
            pen.setColor(color.darker());
            painter.setPen(pen);

            painter.drawConvexPolygon(blade, 4);
            painter.rotate(angle);
        }
    }
    return QObject::eventFilter(watched, event);
}

// Slots

void Throbber::timer_timeout() noexcept
{
    ++position;
    widget->update();
}
