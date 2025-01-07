/*
*  Dolphin for Mario Party Netplay
*  Copyright (C) 2025 Tabitha Hanegan <tabithahanegan.com>
*/

#include "DownloadWorker.h"
#include "Common/HttpRequest.h"
#include <QFile>
#include <QMessageBox>

DownloadWorker::DownloadWorker(const QString& url, const QString& filename)
    : url(url), filename(filename) {}

void DownloadWorker::startDownload()
{
    Common::HttpRequest httpRequest;

    // Set the progress callback
    httpRequest.SetProgressCallback([this](s64 dltotal, s64 dlnow, s64 ultotal, s64 ulnow) {
        emit progressUpdated(dlnow, dltotal); // Emit progress signal
        return true; // Continue the download
    });

    httpRequest.FollowRedirects();
    
    Common::HttpRequest::Headers headers;
    headers["User-Agent"] = "Dolphin-MPN/1.0";

    // Perform the GET request
    auto response = httpRequest.Get(url.toStdString(), headers);

    // Check the response
    if (response)
    {
        QFile file(filename);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        {
            emit errorOccurred(QStringLiteral("Failed to open file for writing."));
            return;
        }
        QByteArray byteArray(reinterpret_cast<const char*>(response->data()), response->size());
        file.write(byteArray);
        file.close();
        emit finished(); // Emit finished signal
    }
    else
    {
        emit errorOccurred(QStringLiteral("Failed to download update file.")); // This should also work
    }
}
