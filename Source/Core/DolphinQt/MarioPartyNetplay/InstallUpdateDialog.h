/*
*  Dolphin for Mario Party Netplay
*  Copyright (C) 2025 Tabitha Hanegan <tabithahanegan.com>
*/

#include <QDialog>
#include <QString>
#include <QLabel>
#include <QProgressBar>

// Forward declarations
QT_BEGIN_NAMESPACE
class QString;
class QLabel;
class QProgressBar;
QT_END_NAMESPACE

class InstallUpdateDialog : public QDialog 
{
    Q_OBJECT

public:
    InstallUpdateDialog(QWidget *parent, QString installationDirectory, QString temporaryDirectory, QString filename);
    ~InstallUpdateDialog();

    void install(void);

private:
    QString installationDirectory;
    QString temporaryDirectory;    
    QString filename;              
    QLabel* label;                
    QProgressBar* progressBar;     

    void writeAndRunScript(QStringList stringList);
    void launchProcess(QString file, QStringList arguments);
    void timerEvent(QTimerEvent* event);
    bool unzipFile(const std::string& zipFilePath, const std::string& destDir);
};
