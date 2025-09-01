/***************************************************************************\
 * Media integrity check. This should be done at boot time instead of here.
 *
 *   Copyright (C) 2023 by AK-47
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
\***************************************************************************/
#ifndef CHECKMD5_H
#define CHECKMD5_H

#include <QObject>
#include <QCoreApplication>
#include <QFutureWatcher>
#include <QLabel>
#include "mprocess.h"

class CheckMD5 : public QObject
{
    Q_OBJECT
public:
    CheckMD5(MProcess &mproc, QLabel *splash) noexcept;
    void wait();
    void halt(bool silent = false) noexcept;

private:
    enum CheckState {
        STARTED,
        GOOD,
        BAD,
        ERROR,
        MISSING
    };
    struct CheckResult {
        QString path;
        CheckState state;
        CheckResult(const QString &path, CheckState state) noexcept : path(path), state(state) {}
    };
    QFutureWatcher<CheckResult> watcher;
    MProcess &proc;
    QLabel *labelSplash;
    QString oldSplashText;
    class QListWidgetItem *logEntry = nullptr;
    MProcess::Status status = MProcess::STATUS_OK;
    void check(class QPromise<CheckResult> &promise) const noexcept;

    void watcher_progressValueChanged(int progress) noexcept;
    void watcher_resultReadyAt(int index) noexcept;
    void watcher_finished() noexcept;
};

#endif // CHECKMD5_H
