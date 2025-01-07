/*
*  Dolphin for Mario Party Netplay
*  Copyright (C) 2025 Tabitha Hanegan <tabithahanegan.com>
*/

#include <QObject>
#include <QString>

class DownloadWorker : public QObject
{
    Q_OBJECT

public:
    DownloadWorker(const QString& url, const QString& filename);
    
public slots:
    void startDownload();

signals:
    void progressUpdated(qint64 size, qint64 total);
    void finished();
    void errorOccurred(const QString& errorMsg);

private:
    QString url;
    QString filename;
};