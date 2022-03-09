/***************************************************************************
 * MProcess class - Installer-specific extensions to QProcess.
 ***************************************************************************
 *
 *   Copyright (C) 2019 by AK-47
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
#include <QListWidget>
#include <QProgressBar>

class MProcess : public QProcess
{
    int execount = 0;
    int sleepcount = 0;
    bool halting = false;
    bool debugUnusedOutput = true;
    QListWidget *logView = nullptr;
    QProgressBar *progBar = nullptr;
    int progSliceStart = 0, progSliceSpace = 0;
    long progSlicePos = 0, progSliceSteps = 0;
    // System detection results
    QString testArch;
    int testEFI = -1;
    int testMac = -1;
    // Common execution core
    bool exec(const QString &program, const QStringList &arguments,
        const QByteArray *input, bool needRead, QListWidgetItem *logEntry);
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
    void setupUI(QListWidget *listLog, QProgressBar *progInstall);
    void setChRoot(const QString &newroot = QString());
    bool exec(const QString &program, const QStringList &arguments = {},
        const QByteArray *input = nullptr, bool needRead = false);
    bool shell(const QString &cmd, const QByteArray *input = nullptr, bool needRead = false);
    QString readOut(bool everything = false);
    QStringList readOutLines();
    // Miscellaneous
    void halt();
    void unhalt();
    bool halted() const { return halting; }
    static QString joinCommand(const QString &program, const QStringList &arguments);
    QListWidgetItem *log(const QString &text, const enum LogType type = Section);
    void log(QListWidgetItem *entry, const int status = 1);
    void status(const QString &text, long progress = -1);
    void status(long progress = -1);
    void advance(int space, long steps);
    // Common functions that are traditionally carried out by processes.
    void sleep(const int msec, const bool silent = false);
    bool mkpath(const QString &path);
    // Operating system
    const QString &detectArch();
    int detectEFI(bool noTest = false);
    bool detectMac();
};

#endif // MPROCESS_H
