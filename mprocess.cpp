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

#include <QApplication>
#include <QDebug>
#include <QEventLoop>
#include <QTimer>
#include <QDir>

#include "mprocess.h"

MProcess::MProcess(QObject *parent)
    : QProcess(parent)
{
}

void MProcess::setupUI(QListWidget *listLog, QProgressBar *progressBar)
{
    logView = listLog;
    progBar = progressBar;
    QPalette pal = listLog->palette();
    pal.setColor(QPalette::Base, Qt::black);
    pal.setColor(QPalette::Text, Qt::white);
    listLog->setPalette(pal);
}

bool MProcess::exec(const QString &cmd, const bool rawexec, const QByteArray *input, bool needRead)
{
    if (halting) return false;
    ++execount;
    qDebug().nospace() << "Exec #" << execount << ": " << cmd;
    QListWidgetItem *logEntry = log(cmd, Exec);
    QEventLoop eloop;
    connect(this, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), &eloop, &QEventLoop::quit);
    if (rawexec) start(cmd);
    else start("/bin/bash", QStringList() << "-c" << cmd);
    if (!debugUnusedOutput) {
        if (!needRead) closeReadChannel(QProcess::StandardOutput);
        closeReadChannel(QProcess::StandardError);
    }
    if (input && !(input->isEmpty())) write(*input);
    closeWriteChannel();
    eloop.exec();
    disconnect(this, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), nullptr, nullptr);
    if (debugUnusedOutput) {
        bool hasOut = false;
        if (!needRead) {
            const QByteArray &StdOut = readAllStandardOutput();
            if (!StdOut.isEmpty()) {
                qDebug().nospace() << "SOut #" << execount << ": " << StdOut;
                hasOut = 1;
            }
        }
        const QByteArray &StdErr = readAllStandardError();
        if (!StdErr.isEmpty()) {
            qDebug().nospace() << "SErr #" << execount << ": " << StdErr;
            hasOut = 1;
        }
        if (hasOut) {
            QFont logFont = logEntry->font();
            logFont.setItalic(true);
            logEntry->setFont(logFont);
        }
    }
    qDebug().nospace() << "Exit #" << execount << ": " << exitCode() << " " << exitStatus();
    int status = 1;
    if (exitStatus() != QProcess::NormalExit) status = -1;
    else if (exitCode() != 0) status = 0;
    log(logEntry, status);
    return (status > 0);
}

QString MProcess::execOut(const QString &cmd, bool everything)
{
    exec(cmd, false, nullptr, true);
    QString strout(readAllStandardOutput().trimmed());
    if (everything) return strout;
    return strout.section("\n", 0, 0);
}

QStringList MProcess::execOutLines(const QString &cmd, const bool rawexec)
{
    exec(cmd, rawexec, nullptr, true);
    return QString(readAllStandardOutput().trimmed()).split('\n', QString::SkipEmptyParts);
}

void MProcess::halt()
{
    halting = true;
    terminate();
    QTimer::singleShot(5000, this, &QProcess::kill);
}

void MProcess::unhalt()
{
    QEventLoop eloop;
    connect(this, static_cast<void(QProcess::*)(int, QProcess::ExitStatus)>(&QProcess::finished), &eloop, &QEventLoop::quit);
    if (state() != QProcess::NotRunning) {
        closeReadChannel(QProcess::StandardOutput);
        closeReadChannel(QProcess::StandardError);
        closeWriteChannel();
        eloop.exec();
    }
    disconnect(this, static_cast<void(QProcess::*)(int, QProcess::ExitStatus)>(&QProcess::finished), nullptr, nullptr);
    halting = false;
}

QListWidgetItem *MProcess::log(const QString &text, const LogType type)
{
    if (type == Standard) qDebug().noquote() << text;
    if (type == Section) qDebug().noquote() << "+++" << text << "+++";
    else if (type == Status) qDebug().noquote() << "-" << text << "-";
    if (!logView) return nullptr;
    QListWidgetItem *entry = new QListWidgetItem(text, logView);
    logView->addItem(entry);
    if (type == Exec) entry->setForeground(Qt::cyan);
    else if (type != Standard) {
        QFont font(entry->font());
        if (type == Section) font.setBold(true);
        else if (type == Status) font.setItalic(true);
        entry->setFont(font);
    }
    logView->scrollToBottom();
    return entry;
}
void MProcess::log(QListWidgetItem *entry, const int status)
{
    if (!entry) return;
    if (status > 0) entry->setForeground(Qt::green);
    else if (status < 0) entry->setForeground(Qt::red);
    else entry->setForeground(Qt::yellow);
}

void MProcess::status(const QString &text, long progress)
{
    if (!progBar) log(text, Status);
    else {
        QString fmt = "%p% - " + text;
        if (progBar->format() != fmt) {
            log(text, Status);
            progBar->setFormat(fmt);
        }
        status(progress);
    }
}
void MProcess::status(long progress)
{
    if (progress < 0) ++progSlicePos;
    else progSlicePos = progress;
    int slice = progSliceSpace;
    if (progSliceSteps > 0) {
        if (progSlicePos > progSliceSteps) progSlicePos = progSliceSteps;
        slice = (progSlicePos * progSliceSpace) / progSliceSteps;
    }
    if (progBar) progBar->setValue(progSliceStart + slice);
    qApp->processEvents();
}
void MProcess::advance(int space, long steps)
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

void MProcess::sleep(const int msec, const bool silent)
{
    if (halting) return;
    QListWidgetItem *logEntry = nullptr;
    if (!silent) {
        ++sleepcount;
        logEntry = log(QString("SLEEP: %1ms").arg(msec), Exec);
        qDebug().nospace() << "Sleep #" << sleepcount << ": " << msec << "ms";
    }
    QTimer cstimer(this);
    QEventLoop eloop(this);
    connect(&cstimer, &QTimer::timeout, &eloop, &QEventLoop::quit);
    cstimer.start(msec);
    const int rc = eloop.exec();
    if (!silent) {
        qDebug().nospace() << "Sleep #" << sleepcount << ": exit " << rc;
        log(logEntry, rc==0?1:-1);
    }
}
bool MProcess::mkpath(const QString &path)
{
    QListWidgetItem *logEntry = log("MKPATH: "+path, Exec);
    const bool rc = QDir().mkpath(path);
    qDebug() << (rc?"MkPath(SUCCESS):":"MkPath(FAILURE):") << path;
    log(logEntry, rc?1:-1);
    return rc;
}
