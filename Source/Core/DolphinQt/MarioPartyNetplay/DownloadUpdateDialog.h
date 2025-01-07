/*
*  Dolphin for Mario Party Netplay
*  Copyright (C) 2025 Tabitha Hanegan <tabithahanegan.com>
*/

#pragma once

#include <QDialog>
#include <QString>
#include <QProgressBar>

class DownloadUpdateDialog : public QDialog
{
    Q_OBJECT

public:
    explicit DownloadUpdateDialog(QWidget* parent, const QString& url, const QString& filename);
    ~DownloadUpdateDialog();

public slots:
    void updateProgress(qint64 size, qint64 total);
    void handleError(const QString& errorMsg); // Ensure this is declared
    void onDownloadFinished();

private:
    QString filename; // Ensure this is declared as a private member
    QProgressBar* progressBar;
    QString installationDirectory;
    QString temporaryDirectory;
};
