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

#include <QDebug>
#include <QEventLoop>
#include <QTimer>

#include "mprocess.h"

MProcess::MProcess(QObject *parent)
    : QProcess(parent)
{
}

bool MProcess::exec(const QString &cmd, const bool rawexec, const QByteArray *input, bool needRead)
{
    if (halting) return false;
    ++execount;
    qDebug().nospace() << "Exec #" << execount << ": " << cmd;
    QListWidgetItem *logEntry = log(cmd, false);
    QEventLoop eloop;
    connect(this, static_cast<void (QProcess::*)(int)>(&QProcess::finished), &eloop, &QEventLoop::quit);
    if (rawexec) start(cmd);
    else start("/bin/bash", QStringList() << "-c" << cmd);
    if (!debugUnusedOutput) {
        if (!needRead) closeReadChannel(QProcess::StandardOutput);
        closeReadChannel(QProcess::StandardError);
    }
    if (input && !(input->isEmpty())) write(*input);
    closeWriteChannel();
    eloop.exec();
    disconnect(this, SIGNAL(finished(int, QProcess::ExitStatus)), nullptr, nullptr);
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
        if(hasOut) {
            QFont logFont = logEntry->font();
            logFont.setItalic(true);
            logEntry->setFont(logFont);
        }
    }
    qDebug().nospace() << "Exit #" << execount << ": " << exitCode() << " " << exitStatus();
    int status = 1;
    if(exitStatus() != QProcess::NormalExit) status = -1;
    else if(exitCode() != 0) status = 0;
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

void MProcess::sleep(const int msec, const bool silent)
{
    if(!silent) {
        ++sleepcount;
        qDebug().nospace() << "Sleep #" << sleepcount << ": " << msec << "ms";
    }
    QTimer cstimer(this);
    QEventLoop eloop(this);
    connect(&cstimer, &QTimer::timeout, &eloop, &QEventLoop::quit);
    cstimer.start(msec);
    int rc = eloop.exec();
    if(!silent) qDebug().nospace() << "Sleep #" << sleepcount << ": exit " << rc;
}

void MProcess::halt()
{
    halting = true;
    terminate();
    QTimer::singleShot(5000, this, SLOT(kill()));
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

QListWidgetItem *MProcess::log(const QString &text, const bool section)
{
    if(section) qDebug().noquote() << "+++" << text << "+++";
    if(!logView) return nullptr;
    QListWidgetItem *entry = new QListWidgetItem(text, logView);
    logView->addItem(entry);
    if(!section) entry->setTextColor(Qt::cyan);
    else {
        QFont font(entry->font());
        font.setItalic(true);
        entry->setFont(font);
    }
    logView->scrollToBottom();
    return entry;
}
void MProcess::log(QListWidgetItem *entry, const int status)
{
    if(!entry) return;
    if(status > 0) entry->setTextColor(Qt::green);
    else if(status < 0) entry->setTextColor(Qt::red);
    else entry->setTextColor(Qt::yellow);
}
