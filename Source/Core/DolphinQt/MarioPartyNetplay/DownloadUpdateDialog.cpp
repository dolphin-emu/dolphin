/*
*  Dolphin for Mario Party Netplay
*  Copyright (C) 2025 Tabitha Hanegan <tabithahanegan.com>
*/

#include "DownloadUpdateDialog.h"
#include "InstallUpdateDialog.h"
#include <QMessageBox>
#include <QFile>
#include <QVBoxLayout>
#include <QCoreApplication>
#include <QDir>
#include <QString>
#include <QLabel>
#include <QThread>
#include "DownloadWorker.h"

// Constructor implementation
DownloadUpdateDialog::DownloadUpdateDialog(QWidget* parent, const QString& url, const QString& filename)
    : QDialog(parent), filename(filename)
{
    // Create UI components
    setWindowTitle(QStringLiteral("Downloading %1").arg(this->filename));
    QVBoxLayout* layout = new QVBoxLayout(this);
    
    // Create and add label
    QLabel* label = new QLabel(QStringLiteral("Downloading %1...").arg(this->filename), this);
    layout->addWidget(label);
    
    // Create and add progress bar
    progressBar = new QProgressBar(this);
    layout->addWidget(progressBar);
    
    // Set the layout for the dialog
    setLayout(layout);
    
    // Set a minimum size for the dialog
    setMinimumSize(300, 100); // Adjust size as needed

    // Create the worker and thread
    DownloadWorker* worker = new DownloadWorker(url, filename);
    QThread* thread = new QThread;

    // Move the worker to the thread
    worker->moveToThread(thread);

    // Connect signals and slots
    connect(thread, &QThread::started, worker, &DownloadWorker::startDownload, Qt::UniqueConnection);
    connect(worker, &DownloadWorker::progressUpdated, this, &DownloadUpdateDialog::updateProgress, Qt::UniqueConnection);
    connect(worker, &DownloadWorker::finished, thread, &QThread::quit, Qt::UniqueConnection);
    connect(worker, &DownloadWorker::finished, worker, &DownloadWorker::deleteLater, Qt::UniqueConnection);
    connect(worker, &DownloadWorker::finished, this, &DownloadUpdateDialog::onDownloadFinished, Qt::UniqueConnection);
    connect(worker, &DownloadWorker::finished, this, &DownloadUpdateDialog::accept, Qt::UniqueConnection);
    connect(worker, &DownloadWorker::errorOccurred, this, &DownloadUpdateDialog::handleError, Qt::UniqueConnection);
    connect(thread, &QThread::finished, thread, &QThread::deleteLater, Qt::UniqueConnection);

    // Start the thread
    thread->start();

    this->exec(); // Show the dialog
}

// Destructor implementation
DownloadUpdateDialog::~DownloadUpdateDialog()
{
}

// Handle errors
void DownloadUpdateDialog::handleError(const QString& errorMsg)
{
    QMessageBox::critical(this, tr("Error"), errorMsg);
    reject();
}

void DownloadUpdateDialog::onDownloadFinished()
{
    // Use QCoreApplication to get the application's directory path
    #ifdef _WIN32
    installationDirectory = QCoreApplication::applicationDirPath(); // Set the installation directory
    #endif
    #ifdef __APPLE__
    installationDirectory = QCoreApplication::applicationDirPath() + QStringLiteral("/../../")
    #endif

    // Use QStandardPaths to get the system's temporary directory
    temporaryDirectory = QDir::tempPath();

    InstallUpdateDialog* installDialog =
        new InstallUpdateDialog(this, installationDirectory, temporaryDirectory, this->filename);
    installDialog->exec(); // Show the installation dialog modally

    accept();
}

void DownloadUpdateDialog::updateProgress(qint64 size, qint64 total)
{
    progressBar->setMaximum(total);
    progressBar->setMinimum(0);
    progressBar->setValue(size);
}
