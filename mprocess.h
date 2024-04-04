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
public:
    enum LogType {
        LOG_LOG,
        LOG_MARKER,
        LOG_STATUS,
        LOG_FAIL,
        LOG_EXEC
    };
    enum Status {
        STATUS_OK,
        STATUS_ERROR,
        STATUS_CRITICAL,
        STATUS_INFORMATIVE
    };
    class Section;

    MProcess(QObject *parent = Q_NULLPTR);
    void setupUI(class QListWidget *listLog, class QProgressBar *progInstall) noexcept;
    bool exec(const QString &program, const QStringList &arguments = {},
        const QByteArray *input = nullptr, bool needRead = false);
    bool shell(const QString &cmd, const QByteArray *input = nullptr, bool needRead = false);
    QString readOut(bool everything = false) noexcept;
    QStringList readOutLines() noexcept;
    // Killer functions
    void halt(bool exception = false) noexcept;
    void unhalt() noexcept;
    bool halted() const noexcept { return halting!=NO_HALT; }
    // User interface
    static QString joinCommand(const QString &program, const QStringList &arguments) noexcept;
    class QListWidgetItem *log(const QString &text, const enum LogType type = LOG_LOG, bool save = true) noexcept;
    void log(class QListWidgetItem *entry, enum Status status = STATUS_OK) noexcept;
    void status(const QString &text, long progress = -1) noexcept;
    void status(long progress = -1) noexcept;
    void advance(int space, long steps) noexcept;
    // Common functions that are traditionally carried out by processes.
    void sleep(const int msec, const bool silent = false) noexcept;
    bool mkpath(const QString &path, mode_t mode = 0, bool force = false);
    // Operating system
    const QString &detectArch();
    int detectEFI(bool noTest = false);
#if QT_VERSION < QT_VERSION_CHECK(6,0,0)
protected:
    void setupChildProcess() noexcept override;
#endif

private:
    friend class ExceptionInfo;
    class Section *section = nullptr;
    int execount = 0;
    int sleepcount = 0;
    enum HaltMode { NO_HALT, THROW_HALT, HALTED } halting = NO_HALT;
    bool debugUnusedOutput = true;
    class QListWidget *logView = nullptr;
    class QProgressBar *progBar = nullptr;
    int progSliceStart = 0, progSliceSpace = 0;
    long progSlicePos = 0, progSliceSteps = 0;
    int prevScrollMax = 0;
    // System detection results
    QString testArch;
    int testEFI = -1;
    // Common execution core
    bool exec(const QString &program, const QStringList &arguments,
        const QByteArray *input, bool needRead, class QListWidgetItem *logEntry);
    bool checkHalt();
#if QT_VERSION >= QT_VERSION_CHECK(6,0,0)
    void syncRoot() noexcept;
#endif
};

// A section with specific error handling or chroot requirements.
// When destroyed, the original process properties are restored.
// This is useful where exception handlers have cleanup routines.
class MProcess::Section {
    friend class MProcess;
    class MProcess &proc;
    class Section *oldsection;
    const char *failmsg = nullptr;
    const char *rootdir = nullptr;
    bool strictfail = true;
public:
    Section(class MProcess &mproc) noexcept;
    inline Section(class MProcess &mproc, const char *failmessage) noexcept
        : Section(mproc) { failmsg = failmessage; }
    ~Section() noexcept;
    inline const char *failMessage() noexcept { return failmsg; }
    inline bool strict() noexcept { return strictfail; }
    inline void setExceptionMode(const char *message) noexcept { failmsg = message; }
    inline void setExceptionMode(bool strict) noexcept { strictfail = strict; }
    inline void setExceptionMode(const char *message, bool strict) noexcept
    {
        failmsg = message;
        strictfail = strict;
    }
    void setRoot(const char *newroot) noexcept;
    inline const char *root() noexcept { return rootdir; }
    Section(const Section &) = delete;
    Section &operator=(const Section &) = delete;
};

#endif // MPROCESS_H
