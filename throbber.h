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

#ifndef THROBBER_H
#define THROBBER_H

#include <QObject>
#include <QWidget>
#include <QTimer>

class Throbber : public QObject
{
    Q_OBJECT
public:
    explicit Throbber(QWidget *widget, QObject *parent = nullptr) noexcept;
    ~Throbber() noexcept;

private:
    QTimer timer;
    QWidget *widget;
    int position = 0;
    bool eventFilter(QObject *watched, QEvent *event) noexcept;
    // Slots
    void timer_timeout() noexcept;
};

#endif // THROBBER_H
