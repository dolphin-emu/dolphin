/*
*  Dolphin for Mario Party Netplay
*  Copyright (C) 2025 Tabitha Hanegan <tabithahanegan.com>
*/

#pragma once

#include <QDialog>
#include <QUrl>
#include <QString>
#include <QProgressBar>

class DownloadUpdateDialog : public QDialog
{
    Q_OBJECT

public:
    DownloadUpdateDialog(QWidget* parent, const QUrl& url, const QString& filename);
    ~DownloadUpdateDialog();

    QString GetTempDirectory() const;
    QString GetFileName() const;

private:
    QString filename;              // Member variable for filename
    QString temporaryDirectory;    // Member variable for temporary directory
    QProgressBar* progressBar;    // Declare progressBar as a member variable

    void startDownload(const QUrl& url);
    void updateProgress(qint64 size, qint64 total); // Method to update progress
};
