// MProcess class - Installer-specific extensions to QProcess.
//
//   Copyright (C) 2019 by AK-47
//   Licensed under the Apache License, Version 2.0 (the "License");
//   you may not use this file except in compliance with the License.
//   You may obtain a copy of the License at
//
//       http://www.apache.org/licenses/LICENSE-2.0
//
//   Unless required by applicable law or agreed to in writing, software
//   distributed under the License is distributed on an "AS IS" BASIS,
//   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//   See the License for the specific language governing permissions and
//   limitations under the License.
//
// This file is part of the gazelle-installer.

#ifndef MPROCESS_H
#define MPROCESS_H

#include <QObject>
#include <QProcess>

class MProcess : public QProcess
{
    int execount = 0;
    bool halting = false;
    bool debugUnusedOutput = true;
public:
    MProcess(QObject *parent = Q_NULLPTR);
    bool exec(const QString &cmd, const bool rawexec = false, const QByteArray *input = nullptr, bool needRead = false);
    QString execOut(const QString &cmd, bool everything = false);
    QStringList execOutLines(const QString &cmd, const bool rawexec = false);
    void halt();
    void unhalt();
};

#endif // MPROCESS_H
