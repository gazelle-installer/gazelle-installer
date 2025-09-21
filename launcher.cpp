/****************************************************************************
 * Gazelle Installer Self-Update Launcher
 * Copyright (C) 2025 MX Linux
 * Licensed under the Apache License, Version 2.0
 ***************************************************************************/

#include "launcher.h"
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QMessageBox>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcess>
#include <QProgressDialog>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QTimer>
#include <QUrl>
#include <QtDebug>
#include <QTemporaryFile>

InstallerLauncher::InstallerLauncher(int argc, char *argv[])
    : QObject(nullptr), app(nullptr), networkManager(nullptr), progressDialog(nullptr), currentEventLoop(nullptr)
{
    // Set Qt platform to minimize display issues
    if (!qgetenv("DISPLAY").isEmpty() || !qgetenv("WAYLAND_DISPLAY").isEmpty()) {
        // GUI environment available
        app = new QApplication(argc, argv);
    } else {
        // Headless/server environment - use offscreen platform
        qputenv("QT_QPA_PLATFORM", "offscreen");
        app = new QApplication(argc, argv);
    }

    app->setApplicationName("Gazelle Installer");

    // Store command line arguments to pass through
    for (int i = 1; i < argc; ++i) {
        cmdArgs << QString(argv[i]);
    }

    // Check if running as root (unless in pretend mode)
    bool pretendMode = cmdArgs.contains("--pretend");
    if (!pretendMode && getuid() != 0) {
        qDebug() << "This operation requires root access.";
        if (!qgetenv("DISPLAY").isEmpty() || !qgetenv("WAYLAND_DISPLAY").isEmpty()) {
            QMessageBox msgbox;
            msgbox.setIcon(QMessageBox::Critical);
            msgbox.setText("This operation requires root access.");
            msgbox.exec();
        }
        exit(EXIT_FAILURE);
    }

    // Initialize network manager after QApplication is created
    networkManager = new QNetworkAccessManager(this);
}

int InstallerLauncher::run()
{
    qDebug() << "Gazelle Installer Launcher starting...";

    // Check for updates first
    if (checkForUpdates()) {
        qDebug() << "Update check completed";
    } else {
        qDebug() << "Update check failed or no internet, proceeding with existing version";
    }

    // Launch the real installer
    return launchRealInstaller();
}

bool InstallerLauncher::checkForUpdates()
{
    QString arch = getCurrentArchitecture();
    QString packagesUrl = QString("https://mxrepo.com/mx/repo/dists/trixie/main/binary-%1/Packages").arg(arch);
    qDebug() << "Checking for updates in Packages file:" << packagesUrl;

    QNetworkRequest request{QUrl(packagesUrl)};
    request.setHeader(QNetworkRequest::UserAgentHeader, "Gazelle-Installer-Launcher/1.0");
    request.setRawHeader("Accept", "text/plain");
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);

    networkSuccess = false;
    QNetworkReply *reply = networkManager->get(request);

    if (!reply) {
        qDebug() << "Failed to create network request";
        return false;
    }

    connect(reply, &QNetworkReply::finished, this, &InstallerLauncher::onNetworkReplyFinished);

    // Create local event loop for this network request
    QEventLoop eventLoop;
    QEventLoop *previousEventLoop = currentEventLoop;
    currentEventLoop = &eventLoop;

    // Set a timeout (5 seconds for faster fallback)
    QTimer::singleShot(5000, [this, reply]() {
        if (reply && !reply->isFinished()) {
            qDebug() << "Network timeout - assuming no internet connection";
            reply->abort();
            networkSuccess = false;
            if (currentEventLoop) currentEventLoop->quit();
        }
    });

    // Process events until network request completes
    eventLoop.exec();
    currentEventLoop = previousEventLoop;

    return networkSuccess;
}

QString InstallerLauncher::parseLatestVersion(const QString &packagesContent)
{
    QString currentArch = getCurrentArchitecture();

    // Parse Debian Packages file format to find ALL mx-installer packages
    QStringList lines = packagesContent.split('\n');
    QString currentPackage;
    QString packageVersion;
    QString packageArch;
    QString packageFilename;

    QStringList foundVersions;
    QString latestVersion;

    for (const QString &line : lines) {
        if (line.startsWith("Package: ")) {
            currentPackage = line.mid(9).trimmed();
        } else if (line.startsWith("Version: ") && currentPackage == "mx-installer") {
            packageVersion = line.mid(9).trimmed();
        } else if (line.startsWith("Architecture: ") && currentPackage == "mx-installer") {
            packageArch = line.mid(14).trimmed();
        } else if (line.startsWith("Filename: ") && currentPackage == "mx-installer") {
            packageFilename = line.mid(10).trimmed();
        } else if (line.isEmpty()) {
            // End of package entry - process if it's mx-installer for our architecture
            if (currentPackage == "mx-installer" && packageArch == currentArch && !packageVersion.isEmpty()) {
                qDebug() << "Found mx-installer package:";
                qDebug() << "  Version:" << packageVersion;
                qDebug() << "  Architecture:" << packageArch;
                qDebug() << "  Filename:" << packageFilename;

                foundVersions << packageVersion;

                // Compare with current latest using our existing compareVersions function
                if (latestVersion.isEmpty() || compareVersions(packageVersion, latestVersion) > 0) {
                    latestVersion = packageVersion;
                    qDebug() << "  -> New highest version:" << latestVersion;
                }
            }

            // Reset for next package
            currentPackage.clear();
            packageVersion.clear();
            packageArch.clear();
            packageFilename.clear();
        }
    }

    return latestVersion;
}

bool InstallerLauncher::isNewerVersion(const QString &remoteVersion)
{
    QString currentVersion = getCurrentVersion();
    qDebug() << "Comparing versions - Current:" << currentVersion << "Remote:" << remoteVersion;

    return compareVersions(remoteVersion, currentVersion) > 0;
}

QString InstallerLauncher::getCurrentVersion()
{
    // Try to get version from dpkg
    QProcess process;
    process.start("dpkg-query", QStringList() << "-W" << "-f" << "${Version}" << "mx-installer");
    process.waitForFinished(5000);

    if (process.exitCode() == 0) {
        QString version = process.readAllStandardOutput().trimmed();
        qDebug() << "Current installed version:" << version;
        return version;
    }

    // Fallback to application version
    return app->applicationVersion();
}

QString InstallerLauncher::getCurrentArchitecture()
{
    // Try to get architecture from dpkg
    QProcess process;
    process.start("dpkg", QStringList() << "--print-architecture");
    process.waitForFinished(5000);

    if (process.exitCode() == 0) {
        QString arch = process.readAllStandardOutput().trimmed();
        return arch;
    }

    // Fallback: try uname -m and map to Debian architectures
    process.start("uname", QStringList() << "-m");
    process.waitForFinished(5000);

    if (process.exitCode() == 0) {
        QString uname_arch = process.readAllStandardOutput().trimmed();

        // Map common uname outputs to Debian architecture names
        if (uname_arch == "x86_64") return "amd64";
        if (uname_arch == "i686" || uname_arch == "i386") return "i386";
        if (uname_arch == "aarch64") return "arm64";
        if (uname_arch.startsWith("arm")) return "armhf";

        // Return as-is if no mapping found
        return uname_arch;
    }

    // Ultimate fallback
    qDebug() << "Failed to detect architecture, defaulting to amd64";
    return "amd64";
}

int InstallerLauncher::compareVersions(const QString &v1, const QString &v2)
{
    // Use dpkg --compare-versions for proper Debian version comparison
    QProcess process;
    process.start("dpkg", QStringList() << "--compare-versions" << v1 << "gt" << v2);
    process.waitForFinished(5000);

    if (process.exitCode() == 0) {
        return 1; // v1 > v2
    }

    process.start("dpkg", QStringList() << "--compare-versions" << v1 << "lt" << v2);
    process.waitForFinished(5000);

    if (process.exitCode() == 0) {
        return -1; // v1 < v2
    }

    return 0; // v1 == v2
}

void InstallerLauncher::downloadAndInstallUpdate(const QString &version)
{
    // Get current system architecture
    QString arch = getCurrentArchitecture();
    QString packageUrl = QString("https://mxrepo.com/mx/repo/pool/main/g/gazelle-installer/mx-installer_%1_%2.deb").arg(version, arch);
    qDebug() << "Downloading update from:" << packageUrl;

    // Show progress dialog only if we have a GUI environment
    if (!qgetenv("DISPLAY").isEmpty() || !qgetenv("WAYLAND_DISPLAY").isEmpty()) {
        progressDialog = new QProgressDialog("Installer is upgrading itself. Please wait...",
                                           QString(), 0, 100, nullptr);
        progressDialog->setWindowTitle("Gazelle Installer Update");
        progressDialog->setModal(true);
        progressDialog->setCancelButton(nullptr);
        progressDialog->show();
    } else {
        qDebug() << "Downloading update in headless mode...";
    }

    QNetworkRequest request{QUrl(packageUrl)};
    request.setHeader(QNetworkRequest::UserAgentHeader, "Gazelle-Installer-Launcher/1.0");
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);

    QNetworkReply *reply = networkManager->get(request);
    connect(reply, &QNetworkReply::finished, this, &InstallerLauncher::onDownloadFinished);
    connect(reply, &QNetworkReply::downloadProgress, this, &InstallerLauncher::onDownloadProgress);

    QEventLoop eventLoop;
    QEventLoop *previousEventLoop = currentEventLoop;
    currentEventLoop = &eventLoop;
    eventLoop.exec();
    currentEventLoop = previousEventLoop;
}

void InstallerLauncher::installPackage(const QString &packagePath)
{
    qDebug() << "Installing package:" << packagePath;

    // Show installation progress dialog with pulsating progress bar
    QProgressDialog *installDialog = nullptr;
    if (!qgetenv("DISPLAY").isEmpty() || !qgetenv("WAYLAND_DISPLAY").isEmpty()) {
        installDialog = new QProgressDialog("Installing update. Please wait...", QString(), 0, 0, nullptr);
        installDialog->setWindowTitle("Gazelle Installer Update");
        installDialog->setModal(true);
        installDialog->setCancelButton(nullptr);
        installDialog->setMinimumDuration(0);
        installDialog->show();

        // Process events to show the dialog
        QApplication::processEvents();
    }

    QProcess process;

    // Use dpkg first, then apt-get to fix dependencies if needed
    qDebug() << "Running dpkg -i" << packagePath;
    process.start("dpkg", QStringList() << "-i" << packagePath);

    // Keep the progress dialog pulsating while dpkg runs
    while (!process.waitForFinished(500)) {
        if (installDialog) {
            QApplication::processEvents();
        }
    }

    if (process.exitCode() != 0) {
        qDebug() << "dpkg failed, trying apt-get to fix dependencies";

        // Update dialog message for dependency fixing
        if (installDialog) {
            installDialog->setLabelText("Fixing dependencies. Please wait...");
            QApplication::processEvents();
        }

        QProcess fixProcess;
        fixProcess.start("apt-get", QStringList() << "install" << "-f" << "-y");

        // Keep the progress dialog pulsating while apt-get runs
        while (!fixProcess.waitForFinished(500)) {
            if (installDialog) {
                QApplication::processEvents();
            }
        }

        if (fixProcess.exitCode() != 0) {
            qDebug() << "Failed to install update. Please install manually.";

            // Close installation dialog
            if (installDialog) {
                installDialog->close();
                delete installDialog;
                installDialog = nullptr;
            }

            // Close any existing download progress dialog
            if (progressDialog) {
                progressDialog->close();
                delete progressDialog;
                progressDialog = nullptr;
            }

            // Don't show message box in headless mode
            if (!qgetenv("DISPLAY").isEmpty() || !qgetenv("WAYLAND_DISPLAY").isEmpty()) {
                QMessageBox::warning(nullptr, "Update Failed",
                    "Failed to install update. Please install manually.");
            }
            return;
        }
    }

    // Close installation dialog
    if (installDialog) {
        installDialog->close();
        delete installDialog;
        installDialog = nullptr;
    }

    qDebug() << "Package installed successfully";
    networkSuccess = true;

    // Clean up
    QFile::remove(packagePath);
}

int InstallerLauncher::launchRealInstaller()
{
    qDebug() << "Launching minstall-real with args:" << cmdArgs;

    QString realInstallerPath = "/usr/bin/minstall-real";

    // For development/testing, also check in the same directory as the launcher
    if (!QFile::exists(realInstallerPath)) {
            // Don't show message box in headless mode
            if (!qgetenv("DISPLAY").isEmpty() || !qgetenv("WAYLAND_DISPLAY").isEmpty()) {
                QMessageBox::critical(nullptr, "Error",
                    "Real installer (minstall-real) not found at: " + realInstallerPath);
            }
        return 1;
    }

    // Replace current process with the real installer to preserve environment
    QProcess process;
    process.setProgram(realInstallerPath);
    process.setArguments(cmdArgs);

    // Start the process and wait for it to finish
    if (!process.startDetached()) {
        qDebug() << "Failed to start real installer:" << process.errorString();

        // Don't show message box in headless mode
        if (!qgetenv("DISPLAY").isEmpty() || !qgetenv("WAYLAND_DISPLAY").isEmpty()) {
            QMessageBox::critical(nullptr, "Error",
                "Failed to start real installer: " + process.errorString());
        }
        return 1;
    }

    // Exit the launcher since the real installer is now running
    return 0;
}

void InstallerLauncher::onNetworkReplyFinished()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) {
        networkSuccess = false;
        if (currentEventLoop) currentEventLoop->quit();
        return;
    }

    reply->deleteLater();

    if (reply->error() != QNetworkReply::NoError) {
        qDebug() << "Network error:" << reply->errorString();
        networkSuccess = false;
        if (currentEventLoop) currentEventLoop->quit();
        return;
    }

    QString content = QString::fromUtf8(reply->readAll());
    qDebug() << "Repository response received, size:" << content.length();

    // Parse the directory listing to find the latest version
    QString latestVersion = parseLatestVersion(content);
    if (!latestVersion.isEmpty()) {
        qDebug() << "Latest version found:" << latestVersion;
        if (isNewerVersion(latestVersion)) {
            qDebug() << "Newer version available, starting download...";
            downloadAndInstallUpdate(latestVersion);
            if (currentEventLoop) currentEventLoop->quit();
            return;
        } else {
            qDebug() << "Current version is up to date";
        }
    }

    networkSuccess = true;
    if (currentEventLoop) currentEventLoop->quit();
}

void InstallerLauncher::onDownloadFinished()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) {
        if (currentEventLoop) currentEventLoop->quit();
        return;
    }

    if (progressDialog) {
        progressDialog->close();
        delete progressDialog;
        progressDialog = nullptr;
    }

    reply->deleteLater();

    if (reply->error() != QNetworkReply::NoError) {
        qDebug() << "Download failed:" << reply->errorString();

        // Don't show message box in headless mode
        if (!qgetenv("DISPLAY").isEmpty() || !qgetenv("WAYLAND_DISPLAY").isEmpty()) {
            QMessageBox::warning(nullptr, "Update Failed",
                "Failed to download update: " + reply->errorString());
        }
        if (currentEventLoop) currentEventLoop->quit();
        return;
    }

    // Save the downloaded file
    QByteArray data = reply->readAll();
    QTemporaryFile *tempFile = new QTemporaryFile(QStandardPaths::writableLocation(QStandardPaths::TempLocation) +
                                                 "/mx-installer_update_XXXXXX.deb", this);
    tempFile->setAutoRemove(false); // Keep file for installPackage call
    if (!tempFile->open()) {
        qDebug() << "Failed to create temporary file:" << tempFile->errorString();
        delete tempFile;
        if (currentEventLoop) currentEventLoop->quit();
        return;
    }
    QString tempFilePath = tempFile->fileName();

    tempFile->write(data);
    tempFile->close();

    // Install the update
    installPackage(tempFilePath);
    if (currentEventLoop) currentEventLoop->quit();
}

void InstallerLauncher::onDownloadProgress(qint64 bytesReceived, qint64 bytesTotal)
{
    if (progressDialog && bytesTotal > 0) {
        int percentage = (bytesReceived * 100) / bytesTotal;
        progressDialog->setValue(percentage);
    }

    // Also log progress for headless mode
    if (bytesTotal > 0) {
        int percentage = (bytesReceived * 100) / bytesTotal;
        static int lastPercentage = -1;
        if (percentage != lastPercentage && percentage % 10 == 0) {
            qDebug() << "Download progress:" << percentage << "%";
            lastPercentage = percentage;
        }
    }
}

int main(int argc, char *argv[])
{
    InstallerLauncher launcher(argc, argv);
    return launcher.run();
}
