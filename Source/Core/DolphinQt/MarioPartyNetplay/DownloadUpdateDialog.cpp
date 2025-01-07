/*
*  Dolphin for Mario Party Netplay
*  Copyright (C) 2025 Tabitha Hanegan <tabithahanegan.com>
*/

#include "DownloadUpdateDialog.h"
#include <QMessageBox>
#include <QFile>
#include <QVBoxLayout>
#include <QLabel>
#include <curl/curl.h>
#include <iostream>

// Callback function for libcurl to write data
size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp)
{
    ((std::string*)userp)->append((char*)contents, size * nmemb);
    return size * nmemb;
}

// Constructor implementation
DownloadUpdateDialog::DownloadUpdateDialog(QWidget* parent, const QUrl& url, const QString& filename)
    : QDialog(parent), filename(filename)
{
    // Create UI components
    QVBoxLayout* layout = new QVBoxLayout(this);
    QLabel* label = new QLabel(QStringLiteral("Downloading %1...").arg(this->filename), this);
    progressBar = new QProgressBar(this);

    // Set up the layout
    layout->addWidget(label);
    layout->addWidget(progressBar);

    // Set the layout for the dialog
    setLayout(layout);

    // Start the download
    startDownload(url);
}

// Destructor implementation
DownloadUpdateDialog::~DownloadUpdateDialog()
{
}

// Returns the temporary directory
QString DownloadUpdateDialog::GetTempDirectory() const
{
    return temporaryDirectory;
}

// Returns the filename
QString DownloadUpdateDialog::GetFileName() const
{
    return filename;
}

// Update the progress bar
void DownloadUpdateDialog::updateProgress(qint64 size, qint64 total)
{
    progressBar->setMaximum(total);
    progressBar->setMinimum(0);
    progressBar->setValue(size);
}

// Starts the download process
void DownloadUpdateDialog::startDownload(const QUrl& url)
{
    CURL* curl;
    CURLcode res;
    std::string readBuffer;

    curl = curl_easy_init();
    if (curl)
    {
        // Convert QUrl to std::string properly
        std::string urlStr = url.toString().toStdString();
        curl_easy_setopt(curl, CURLOPT_URL, urlStr.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);
        
        // Set the progress function
        curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
        curl_easy_setopt(curl, CURLOPT_PROGRESSFUNCTION, [](void* clientp, double totalToDownload, double nowDownloaded, double totalToUpload, double nowUploaded) {
            DownloadUpdateDialog* dialog = static_cast<DownloadUpdateDialog*>(clientp);
            dialog->updateProgress(static_cast<qint64>(nowDownloaded), static_cast<qint64>(totalToDownload));
            return 0; // Return 0 to continue the download
        });
        curl_easy_setopt(curl, CURLOPT_PROGRESSDATA, this);

        res = curl_easy_perform(curl);
        if (res != CURLE_OK)
        {
            // Use QString::fromStdString() for proper string conversion
            QString errorMsg = QString::fromStdString(std::string(curl_easy_strerror(res)));
            QMessageBox::critical(this, tr("Error"), tr("Failed to download update file: %1").arg(errorMsg));
            reject();
        }
        else
        {
            QFile file(filename);
            if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
            {
                QMessageBox::critical(this, tr("Error"), tr("Failed to open file for writing."));
                reject();
            }
            else
            {
                file.write(readBuffer.c_str(), readBuffer.size());
                file.close();
                accept();
            }
        }
        curl_easy_cleanup(curl);
    }
}