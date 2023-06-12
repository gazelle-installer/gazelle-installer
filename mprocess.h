/***************************************************************************
 * MProcess class - Installer-specific extensions to QProcess.
 *
 *   Copyright (C) 2019-2023 by AK-47
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
#ifndef MPROCESS_H
#define MPROCESS_H

#include <QObject>
#include <QProcess>

class MProcess : public QProcess
{
    Q_OBJECT
    int execount = 0;
    int sleepcount = 0;
    bool halting = false;
    bool debugUnusedOutput = true;
    class QListWidget *logView = nullptr;
    class QProgressBar *progBar = nullptr;
    int progSliceStart = 0, progSliceSpace = 0;
    long progSlicePos = 0, progSliceSteps = 0;
    const char *exceptionInfo = nullptr;
    // System detection results
    QString testArch;
    int testEFI = -1;
    int testMac = -1;
    // Common execution core
    bool exec(const QString &program, const QStringList &arguments,
        const QByteArray *input, bool needRead, class QListWidgetItem *logEntry) noexcept(false);
protected:
    void setupChildProcess() override;
public:
    enum LogType {
        Standard,
        Section,
        Status,
        Fail,
        Exec
    };
    QString chRoot;
    MProcess(QObject *parent = Q_NULLPTR);
    void setupUI(class QListWidget *listLog, class QProgressBar *progInstall) noexcept;
    void setChRoot(const QString &newroot = QString()) noexcept;
    bool exec(const QString &program, const QStringList &arguments = {},
        const QByteArray *input = nullptr, bool needRead = false) noexcept(false);
    bool shell(const QString &cmd, const QByteArray *input = nullptr, bool needRead = false) noexcept(false);
    QString readOut(bool everything = false) noexcept;
    QStringList readOutLines() noexcept;
    // Miscellaneous
    void halt() noexcept;
    void unhalt() noexcept;
    bool halted() const noexcept { return halting; }
    // User interface
    static QString joinCommand(const QString &program, const QStringList &arguments) noexcept;
    class QListWidgetItem *log(const QString &text, const enum LogType type = Standard) noexcept;
    void log(class QListWidgetItem *entry, const int status = 1) noexcept;
    void status(const QString &text, long progress = -1) noexcept;
    void status(long progress = -1) noexcept;
    void advance(int space, long steps) noexcept;
    void setExceptionMode(const char *failInfo) noexcept;
    // Common functions that are traditionally carried out by processes.
    void sleep(const int msec, const bool silent = false) noexcept;
    bool mkpath(const QString &path) noexcept(false);
    // Operating system
    const QString &detectArch();
    int detectEFI(bool noTest = false);
    bool detectMac();
};

#endif // MPROCESS_H
