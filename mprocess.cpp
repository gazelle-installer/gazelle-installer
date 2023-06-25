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

#include <QApplication>
#include <QProcessEnvironment>
#include <QDebug>
#include <QEventLoop>
#include <QTimer>
#include <QDir>
#include <unistd.h>

#include "mprocess.h"

MProcess::MProcess(QObject *parent)
    : QProcess(parent)
{
    // Stop user-selected locale from interfering with command output parsing.
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert("LC_ALL", "C.UTF-8");
    setProcessEnvironment(env);
}

void MProcess::setupUI(QListWidget *listLog, QProgressBar *progInstall)
{
    logView = listLog;
    progBar = progInstall;
    QPalette pal = listLog->palette();
    pal.setColor(QPalette::Base, Qt::black);
    pal.setColor(QPalette::Text, Qt::white);
    listLog->setPalette(pal);
}

void MProcess::setupChildProcess()
{
    if (chRoot.isEmpty()) return;
    chroot(chRoot.toUtf8().constData());
    chdir("/");
}
void MProcess::setChRoot(const QString &newroot)
{
    if (!newroot.isEmpty()) log("Set chroot: " + newroot, Standard);
    else if (!chRoot.isEmpty() && newroot.isEmpty()) log("End chroot: " + chRoot, Standard);
    chRoot = newroot;
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
    disconnect(this, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), nullptr, nullptr);
    disconnect(this, QOverload<QProcess::ProcessError>::of(&QProcess::errorOccurred), nullptr, nullptr);

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

    int status = 1;
    if (exitStatus() != QProcess::NormalExit) status = -1;
    else if (error() != QProcess::UnknownError) status = -2;
    else if (exitCode() != 0) status = 0;

    QDebug dbg(qDebug().nospace() << "Exit #" << execount << ": " << exitCode());
    if (status < 0) dbg << " " << exitStatus() << " " << error();

    log(logEntry, status);
    return (status > 0);
}

bool MProcess::exec(const QString &program, const QStringList &arguments, const QByteArray *input, bool needRead)
{
    if (halting) return false;
    ++execount;
    const QString &cmd = joinCommand(program, arguments);
    qDebug().nospace().noquote() << "Exec #" << execount << ": " << cmd;
    return exec(program, arguments, input, needRead, log(cmd, Exec));
}
bool MProcess::shell(const QString &cmd,  const QByteArray *input, bool needRead)
{
    if (halting) return false;
    ++execount;
    qDebug().nospace().noquote() << "Bash #" << execount << ": " << cmd;
    return exec("/bin/bash", {"-c", cmd}, input, needRead, log(cmd, Exec));
}

QString MProcess::readOut(bool everything)
{
    QString strout(readAllStandardOutput().trimmed());
    if (everything) return strout;
    return strout.section("\n", 0, 0);
}
QStringList MProcess::readOutLines()
{
    return QString(readAllStandardOutput().trimmed()).split('\n', Qt::SkipEmptyParts);
}

void MProcess::halt()
{
    halting = true;
    terminate();
    QTimer::singleShot(5000, this, &QProcess::kill);
    setChRoot();
}

void MProcess::unhalt()
{
    QEventLoop eloop;
    connect(this, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), &eloop, &QEventLoop::quit);
    if (state() != QProcess::NotRunning) {
        closeReadChannel(QProcess::StandardOutput);
        closeReadChannel(QProcess::StandardError);
        closeWriteChannel();
        eloop.exec();
    }
    disconnect(this, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), nullptr, nullptr);
    halting = false;
}

QString MProcess::joinCommand(const QString &program, const QStringList &arguments)
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

QListWidgetItem *MProcess::log(const QString &text, const LogType type)
{
    if (type == Standard) qDebug().noquote() << text;
    else if (type == Section) qDebug().noquote() << "<<" << text << ">>";
    else if (type == Status) qDebug().noquote() << "++" << text << "++";
    else if (type == Fail) qDebug().noquote() << "--" << text << "--";
    if (!logView || type == Section) return nullptr;
    QListWidgetItem *entry = new QListWidgetItem(text, logView);
    logView->addItem(entry);
    if (type == Exec) entry->setForeground(Qt::cyan);
    else if (type == Status || type == Fail) {
        QFont font(entry->font());
        font.setBold(true);
        entry->setFont(font);
        if (type == Fail) entry->setForeground(Qt::magenta);
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
        slice = static_cast<int>((progSlicePos * progSliceSpace) / progSliceSteps);
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
        log(logEntry, rc == 0 ? 1 : -1);
    }
}
bool MProcess::mkpath(const QString &path)
{
    QListWidgetItem *logEntry = log("MKPATH: "+path, Exec);
    const bool rc = QDir().mkpath(path);
    qDebug() << (rc ? "MkPath(SUCCESS):" : "MkPath(FAILURE):") << path;
    log(logEntry, rc ? 1 : -1);
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
bool MProcess::detectMac()
{
    bool rc = false;
    if (testMac < 0) {
        // if it looks like an apple...
        rc = exec("grub-probe", {"-d", "/dev/sda2"}, nullptr, true);
        if (rc) rc = readOut(true).contains("hfsplus");
        testMac = (rc ? 1 : 0);
        qDebug() << "Detect Mac:" << rc;
    }
    return rc;
}
