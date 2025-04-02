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

#include <QApplication>
#include <QProcessEnvironment>
#include <QDebug>
#include <QTimer>
#include <QListWidget>
#include <QProgressBar>
#include <QScrollBar>

#include "mprocess.h"

using namespace Qt::Literals::StringLiterals;

MProcess::MProcess(QObject *parent)
    : QProcess(parent)
{
    // Stop user-selected locale from interfering with command output parsing.
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert(u"LC_ALL"_s, u"C.UTF-8"_s);
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

bool MProcess::checkHalt()
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
    return exec(u"/usr/bin/bash"_s, {u"-c"_s, cmd}, input, needRead, log(cmd, LOG_EXEC, false));
}

QString MProcess::readOut(bool everything) noexcept
{
    QString strout(readAllStandardOutput().trimmed());
    if (everything) return strout;
    return strout.section('\n', 0, 0).trimmed();
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
        arg.replace('\"', "\\\""_L1);
        if (!wspace) text += ' ' + arg;
        else text += " \""_L1 + arg + '\"';
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
        QString fmt = "%p% - "_L1 + text;
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
        proc.log(u"Revert root: "_s + oldroot, LOG_LOG);
    }
    proc.section = oldsection;
    proc.syncRoot();
}

// Note: newroot must be valid and unchanged for the life of the chroot.
void MProcess::Section::setRoot(const char *newroot) noexcept
{
    if (qstrcmp(newroot, rootdir)) {
        proc.log(u"Change root: "_s + newroot, LOG_LOG);
    }
    rootdir = newroot; // Might be different pointers to the same text.
    proc.syncRoot();
}
