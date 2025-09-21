/****************************************************************************
 * Gazelle Installer Self-Update Launcher Header
 * Copyright (C) 2025 MX Linux
 * Licensed under the Apache License, Version 2.0
 ***************************************************************************/

#pragma once

#include <QApplication>
#include <QEventLoop>
#include <QNetworkAccessManager>
#include <QObject>
#include <QStringList>

class QProgressDialog;

class InstallerLauncher : public QObject
{
    Q_OBJECT

public:
    InstallerLauncher(int argc, char *argv[]);
    int run();

private slots:
    void onNetworkReplyFinished();
    void onDownloadFinished();
    void onDownloadProgress(qint64 bytesReceived, qint64 bytesTotal);

private:
    bool checkForUpdates();
    QString parseLatestVersion(const QString &htmlContent);
    bool isNewerVersion(const QString &remoteVersion);
    QString getCurrentVersion();
    QString getCurrentArchitecture();
    int compareVersions(const QString &v1, const QString &v2);
    void downloadAndInstallUpdate(const QString &version);
    void installPackage(const QString &packagePath);
    int launchRealInstaller();

    QApplication *app;
    QNetworkAccessManager *networkManager;
    QStringList cmdArgs;
    bool networkSuccess = false;
    QProgressDialog *progressDialog;
    QEventLoop *currentEventLoop;
};
