/***************************************************************************
 * MProcess class - Installer-specific extensions to QProcess.
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

#include <unistd.h>
#include <sys/stat.h>

#include <QApplication>
#include <QProcessEnvironment>
#include <QDebug>
#include <QEventLoop>
#include <QTimer>
#include <QDir>
#include <QListWidget>
#include <QProgressBar>
#include <QScrollBar>

#include "mprocess.h"

MProcess::MProcess(QObject *parent)
    : QProcess(parent)
{
    // Stop user-selected locale from interfering with command output parsing.
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert("LC_ALL", "C.UTF-8");
    setProcessEnvironment(env);
}

void MProcess::setupUI(QListWidget *listLog, QProgressBar *progInstall) noexcept
{
    logView = listLog;
    progBar = progInstall;
    QPalette pal = listLog->palette();
    pal.setColor(QPalette::Base, Qt::black);
    pal.setColor(QPalette::Text, Qt::white);
    listLog->setPalette(pal);
    QScrollBar *lvscroll = logView->verticalScrollBar();
    connect(lvscroll, &QScrollBar::rangeChanged, lvscroll, [this, lvscroll](int, int max){
        if (lvscroll->value() >= prevScrollMax) logView->scrollToBottom();
        prevScrollMax = max;
    });
}

#if QT_VERSION >= QT_VERSION_CHECK(6,0,0)
void MProcess::syncRoot() noexcept
{
    if (section && section->rootdir) {
        setChildProcessModifier([this]() {
            if (section && section->rootdir) {
                if (chroot(section->rootdir) != 0) exit(255);
                if (chdir("/") != 0) exit(255);
            }
        });
    } else {
        setChildProcessModifier(nullptr);
    }
}
#else
void MProcess::setupChildProcess() noexcept
{
    if (section && section->rootdir) {
        if (chroot(section->rootdir) != 0) exit(255);
        if (chdir("/") != 0) exit(255);
    }
}
#endif

inline bool MProcess::checkHalt()
{
    if (halting == THROW_HALT) throw("");
    else if (halting == HALTED) return true;
    return false;
}

bool MProcess::exec(const QString &program, const QStringList &arguments,
    const QByteArray *input, bool needRead, QListWidgetItem *logEntry)
{
    QEventLoop eloop;
    connect(this, QOverload<QProcess::ProcessError>::of(&QProcess::errorOccurred), &eloop, &QEventLoop::quit);
    connect(this, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), &eloop, &QEventLoop::quit);
    start(program, arguments);
    if (!debugUnusedOutput) {
        if (!needRead) closeReadChannel(QProcess::StandardOutput);
        closeReadChannel(QProcess::StandardError);
    }
    if (input && !(input->isEmpty())) write(*input);
    closeWriteChannel();
    eloop.exec();
    disconnect(&eloop);

    if (debugUnusedOutput) {
        bool hasOut = false;
        if (!needRead) {
            const QByteArray &StdOut = readAllStandardOutput();
            if (!StdOut.isEmpty()) {
                qDebug().nospace() << "SOut #" << execount << ": " << StdOut;
                hasOut = true;
            }
        }
        const QByteArray &StdErr = readAllStandardError();
        if (!StdErr.isEmpty()) {
            qDebug().nospace() << "SErr #" << execount << ": " << StdErr;
            hasOut = true;
        }
        if (hasOut) {
            QFont logFont = logEntry->font();
            logFont.setItalic(true);
            logEntry->setFont(logFont);
        }
    }

    enum Status status = STATUS_OK;
    if (exitStatus() != QProcess::NormalExit) status = STATUS_CRITICAL; // Process crash
    else if (error() != QProcess::UnknownError) status = STATUS_CRITICAL; // Start error
    else if (exitCode() != 0) status = STATUS_ERROR; // Process error

    QDebug dbg(qDebug().nospace() << "Exit #" << execount << ": " << exitCode());
    if (status < 0) dbg << " " << exitStatus() << " " << error();

    log(logEntry, status);
    if (status != STATUS_OK) {
        if (section && section->failmsg && (status==STATUS_CRITICAL || section->strictfail)) {
            throw section->failmsg;
        }
        return false;
    }
    return true;
}

bool MProcess::exec(const QString &program, const QStringList &arguments, const QByteArray *input, bool needRead)
{
    if (checkHalt()) return false;
    ++execount;
    const QString &cmd = joinCommand(program, arguments);
    qDebug().nospace().noquote() << "Exec #" << execount << ": " << cmd;
    return exec(program, arguments, input, needRead, log(cmd, LOG_EXEC, false));
}
bool MProcess::shell(const QString &cmd,  const QByteArray *input, bool needRead)
{
    if (checkHalt()) return false;
    ++execount;
    qDebug().nospace().noquote() << "Bash #" << execount << ": " << cmd;
    return exec("/usr/bin/bash", {"-c", cmd}, input, needRead, log(cmd, LOG_EXEC, false));
}

QString MProcess::readOut(bool everything) noexcept
{
    QString strout(readAllStandardOutput().trimmed());
    if (everything) return strout;
    return strout.section("\n", 0, 0);
}
QStringList MProcess::readOutLines() noexcept
{
    return QString(readAllStandardOutput().trimmed()).split('\n', Qt::SkipEmptyParts);
}

void MProcess::halt(bool exception) noexcept
{
    log(__PRETTY_FUNCTION__, LOG_MARKER);
    halting = exception ? THROW_HALT : HALTED;
    if(state() != QProcess::NotRunning) {
        QEventLoop eloop;
        connect(this, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), &eloop, &QEventLoop::quit);
        closeReadChannel(QProcess::StandardOutput);
        closeReadChannel(QProcess::StandardError);
        closeWriteChannel();
        this->terminate();
        QTimer::singleShot(5000, this, &QProcess::kill);
        eloop.exec();
    }
}
void MProcess::unhalt() noexcept
{
    if (halting == NO_HALT) return;
    QEventLoop eloop;
    connect(this, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), &eloop, &QEventLoop::quit);
    if (state() != QProcess::NotRunning) {
        closeReadChannel(QProcess::StandardOutput);
        closeReadChannel(QProcess::StandardError);
        closeWriteChannel();
        eloop.exec();
    }
    halting = NO_HALT;
}

QString MProcess::joinCommand(const QString &program, const QStringList &arguments) noexcept
{
    QString text = program;
    for(QString arg : arguments) {
        bool wspace = false;
        for(const auto &ch : arg) {
            if (ch.isSpace()) {
                wspace = true;
                break;
            }
        }
        arg.replace("\"", "\\\"");
        if (!wspace) text += ' ' + arg;
        else text += " \"" + arg + "\"";
    }
    return text;
}

QListWidgetItem *MProcess::log(const QString &text, const LogType type, bool save) noexcept
{
    if (save) {
        if (type == LOG_MARKER) qDebug().noquote() << "<<" << text << ">>";
        else if (type == LOG_STATUS) qDebug().noquote() << "++" << text << "++";
        else if (type == LOG_FAIL) qDebug().noquote() << "--" << text << "--";
        else qDebug().noquote() << text;
    }
    if (!logView || type == LOG_MARKER) return nullptr;
    QListWidgetItem *entry = new QListWidgetItem(text, logView);
    logView->addItem(entry);
    if (type == LOG_EXEC) entry->setForeground(Qt::cyan);
    else if (type == LOG_STATUS || type == LOG_FAIL) {
        QFont font(entry->font());
        font.setBold(true);
        entry->setFont(font);
        if (type == LOG_FAIL) entry->setForeground(Qt::magenta);
    }
    return entry;
}
void MProcess::log(QListWidgetItem *entry, enum Status status) noexcept
{
    if (!entry) return;
    switch (status) {
        case STATUS_CRITICAL: entry->setForeground(Qt::red); break;
        case STATUS_OK: entry->setForeground(Qt::green); break;
        case STATUS_ERROR: entry->setForeground(Qt::yellow); break;
        default: entry->setForeground(Qt::blue); break;
    }
}

void MProcess::status(const QString &text, long progress) noexcept
{
    if (!progBar) log(text, LOG_STATUS);
    else {
        QString fmt = "%p% - " + text;
        if (progBar->format() != fmt) {
            log(text, LOG_STATUS);
            progBar->setFormat(fmt);
        }
        status(progress);
    }
}
void MProcess::status(long progress) noexcept
{
    if (progress < 0) ++progSlicePos;
    else progSlicePos = progress;
    int slice = progSliceSpace;
    if (progSliceSteps > 0) {
        if (progSlicePos > progSliceSteps) progSlicePos = progSliceSteps;
        slice = static_cast<int>((progSlicePos * progSliceSpace) / progSliceSteps);
    }
    if (progBar) progBar->setValue(progSliceStart + slice);
    qApp->processEvents();
}
void MProcess::advance(int space, long steps) noexcept
{
    if (space < 0) steps = progSliceSpace = progSliceStart = space = 0; // Reset progress
    progSliceStart += progSliceSpace;
    progSliceSpace = space;
    progSliceSteps = steps;
    progSlicePos = -1;
    progBar->setValue(progSliceStart);
    qApp->processEvents();
}

// Common functions that are traditionally carried out by processes.

void MProcess::sleep(const int msec, const bool silent) noexcept
{
    QListWidgetItem *logEntry = nullptr;
    if (!silent) {
        ++sleepcount;
        logEntry = log(QString("SLEEP: %1ms").arg(msec), LOG_EXEC, false);
        qDebug().nospace() << "Sleep #" << sleepcount << ": " << msec << "ms";
    }
    QTimer cstimer(this);
    QEventLoop eloop(this);
    connect(&cstimer, &QTimer::timeout, &eloop, &QEventLoop::quit);
    cstimer.start(msec);
    const int rc = eloop.exec();
    if (!silent) {
        qDebug().nospace() << "Sleep #" << sleepcount << ": exit " << rc;
        log(logEntry, rc == 0 ? STATUS_OK : STATUS_CRITICAL);
    }
}
bool MProcess::mkpath(const QString &path, mode_t mode, bool force)
{
    if (checkHalt()) return false;
    if (!force && QFileInfo::exists(path)) return true;

    QListWidgetItem *logEntry = log("MKPATH: "+path, LOG_EXEC, false);
    bool rc = QDir().mkpath(path);
    if (mode && rc) rc = (chmod(path.toUtf8().constData(), mode) == 0);
    qDebug() << (rc ? "MkPath(SUCCESS):" : "MkPath(FAILURE):") << path;
    log(logEntry, rc ? STATUS_OK : STATUS_CRITICAL);
    if(!rc && section && section->failmsg) throw section->failmsg;
    return rc;
}

const QString &MProcess::detectArch()
{
    if (testArch.isEmpty()) {
        if (exec("uname", {"-m"}, nullptr, true)) testArch = readOut();
        qDebug() << "Detect arch:" << testArch;
    }
    return testArch;
}
int MProcess::detectEFI(bool noTest)
{
    if (testEFI < 0) {
        testEFI = 0;
        QFile file("/sys/firmware/efi/fw_platform_size");
        if (file.open(QFile::ReadOnly | QFile::Text)) {
            testEFI = file.readLine().toInt();
            file.close();
        }
        qDebug() << "Detect EFI:" << testEFI;
    }
    if (!noTest && testEFI == 64) {
        // Bad combination: 32-bit OS on 64-bit EFI.
        if (detectArch()=="i686") return 0;
    }
    return testEFI;
}

// Local execution environment

MProcess::Section::Section(class MProcess &mproc) noexcept
    : proc(mproc), oldsection(mproc.section)
{
    if (oldsection) {
        failmsg = oldsection->failmsg;
        rootdir = oldsection->rootdir;
        strictfail = oldsection->strictfail;
    }
    mproc.section = this;
}
MProcess::Section::~Section() noexcept
{
    const char *oldroot = oldsection ? oldsection->rootdir : nullptr;
    if (qstrcmp(oldroot, rootdir)) {
        proc.log(QString("Revert root: ")+oldroot, LOG_LOG);
    }
    proc.section = oldsection;
#if QT_VERSION >= QT_VERSION_CHECK(6,0,0)
    proc.syncRoot();
#endif
}

// Note: newroot must be valid and unchanged for the life of the chroot.
void MProcess::Section::setRoot(const char *newroot) noexcept
{
    if (qstrcmp(newroot, rootdir)) {
        proc.log(QString("Change root: ")+newroot, LOG_LOG);
    }
    rootdir = newroot; // Might be different pointers to the same text.
#if QT_VERSION >= QT_VERSION_CHECK(6,0,0)
    proc.syncRoot();
#endif
}
