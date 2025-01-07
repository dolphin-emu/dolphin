/*
*  Dolphin for Mario Party Netplay
*  Copyright (C) 2025 Tabitha Hanegan <tabithahanegan.com>
*/

#include "InstallUpdateDialog.h"
#include <QCoreApplication>
#include <QProcess>
#include <QDir>
#include <QTextStream>
#include <QVBoxLayout>
#include <QLabel>
#include <QProgressBar>
#include <QMessageBox>
#include <mz_compat.h>

// Constructor implementation
InstallUpdateDialog::InstallUpdateDialog(QWidget *parent, QString installationDirectory, QString temporaryDirectory, QString filename)
    : DownloadUpdateDialog(parent, QUrl(), filename),
      installationDirectory(installationDirectory),
      temporaryDirectory(temporaryDirectory)
{
    // Create UI components
    QVBoxLayout* layout = new QVBoxLayout(this);
    QLabel* label = new QLabel(QStringLiteral("Installing %1...").arg(this->filename), this);
    QProgressBar* progressBar = new QProgressBar(this);

    // Set up the layout
    layout->addWidget(label);
    layout->addWidget(progressBar);

    setLayout(layout);

    startTimer(100);
}

// Destructor implementation
InstallUpdateDialog::~InstallUpdateDialog(void)
{
}

void InstallUpdateDialog::install(void)
{
    QString fullFilePath = this->temporaryDirectory + QStringLiteral("/") + this->filename;   
    QString appPath = QCoreApplication::applicationDirPath();
    QString appPid  = QString::number(QCoreApplication::applicationPid());

    // Convert paths to use the right path separator
    this->temporaryDirectory = QDir::toNativeSeparators(this->temporaryDirectory);
    fullFilePath             = QDir::toNativeSeparators(fullFilePath);
    appPath                  = QDir::toNativeSeparators(appPath);

    if (this->filename.endsWith(QStringLiteral(".exe")))
    {
        this->label->setText(QStringLiteral("Executing %1...").arg(this->filename));
        QStringList scriptLines =
        {
            QStringLiteral("@echo off"),
            QStringLiteral("("),
            QStringLiteral("   echo == Attempting to kill PID ") + appPid,
            QStringLiteral("   taskkill /F /PID:") + appPid,
            QStringLiteral("   echo == Attempting to start '") + fullFilePath + QStringLiteral("'"),
            QStringLiteral("   \"") + fullFilePath + QStringLiteral("\" /CLOSEAPPLICATIONS /NOCANCEL /MERGETASKS=\"!desktopicon\" /SILENT /DIR=\"") + appPath + QStringLiteral("\""),
            QStringLiteral(")"),
            QStringLiteral("IF NOT ERRORLEVEL 0 ("),
            QStringLiteral("   start \"\" cmd /c \"echo Update failed, check the log for more information && pause\""),
            QStringLiteral(")"),
            // Remove temporary directory at last
            QStringLiteral("rmdir /S /Q \"") + this->temporaryDirectory + QStringLiteral("\""),
        };
        this->writeAndRunScript(scriptLines);
        this->accept();
        return;
    }

    this->label->setText(QStringLiteral("Extracting %1...").arg(this->filename));
    this->progressBar->setValue(50);

    QDir dir(this->temporaryDirectory);
    if (!dir.mkdir(QStringLiteral("extract")))
    {
        QMessageBox::critical(this, QStringLiteral("Error"), QStringLiteral("Failed to create extract directory."));
        this->reject();
        return;
    }

    QString extractDirectory = this->temporaryDirectory + QStringLiteral("/extract");

    if (!unzipFile(fullFilePath.toStdString(), extractDirectory.toStdString()))
    {
        QMessageBox::critical(this, QStringLiteral("Error"), QStringLiteral("Unzip failed: Unable to extract files."));
        this->reject();
        return;
    }

    this->label->setText(QStringLiteral("Executing update script..."));
    this->progressBar->setValue(100);

    extractDirectory = QDir::toNativeSeparators(extractDirectory);

    QStringList scriptLines = 
    {
        QStringLiteral("@echo off"),
        QStringLiteral("("),
        QStringLiteral("   echo == Attempting to remove '") + fullFilePath + QStringLiteral("'"),
        QStringLiteral("   del /F /Q \"") + fullFilePath + QStringLiteral("\""),
        QStringLiteral("   echo == Attempting to kill PID ") + appPid,
        QStringLiteral("   taskkill /F /PID:") + appPid,
        QStringLiteral("   echo == Attempting to copy '") + extractDirectory + QStringLiteral("' to '") + appPath + QStringLiteral("'"),
        QStringLiteral("   xcopy /S /Y /I \"") + extractDirectory + QStringLiteral("\\*\" \"") + appPath + QStringLiteral("\""),
        QStringLiteral("   echo == Attempting to start '") + appPath + QStringLiteral("\\Dolphin-MPN.exe'"),
        QStringLiteral("   start \"\" \"") + appPath + QStringLiteral("\\Dolphin-MPN.exe\""),
        QStringLiteral(")"),
        QStringLiteral("IF NOT ERRORLEVEL 0 ("),
        QStringLiteral("   start \"\" cmd /c \"echo Update failed && pause\""),
        QStringLiteral(")"),
        // Remove temporary directory at last
        QStringLiteral("rmdir /S /Q \"") + this->temporaryDirectory + QStringLiteral("\""),
    };
    this->writeAndRunScript(scriptLines);
}

bool InstallUpdateDialog::unzipFile(const std::string& zipFilePath, const std::string& destDir)
{
    unzFile zipFile = unzOpen(zipFilePath.c_str());
    if (!zipFile)
    {
        return false; // Failed to open zip file
    }

    if (unzGoToFirstFile(zipFile) != UNZ_OK)
    {
        unzClose(zipFile);
        return false; // No files in zip
    }

    do
    {
        char filename[256];
        unz_file_info fileInfo;
        if (unzGetCurrentFileInfo(zipFile, &fileInfo, filename, sizeof(filename), nullptr, 0, nullptr, 0) != UNZ_OK)
        {
            unzClose(zipFile);
            return false; // Failed to get file info
        }

        // Create full path for the extracted file using std::string concatenation
        std::string fullPath = destDir + "/" + std::string(filename);

        // Create directories if needed
        QString qFullPath = QString::fromStdString(fullPath);
        QDir().mkpath(QFileInfo(qFullPath).path());

        // Open the file in the zip
        if (unzOpenCurrentFile(zipFile) != UNZ_OK)
        {
            unzClose(zipFile);
            return false; // Failed to open file in zip
        }

        // Write the file to disk
        QFile outFile(qFullPath);
        if (!outFile.open(QIODevice::WriteOnly))
        {
            unzCloseCurrentFile(zipFile);
            unzClose(zipFile);
            return false; // Failed to create output file
        }

        char buffer[8192];
        int bytesRead;
        while ((bytesRead = unzReadCurrentFile(zipFile, buffer, sizeof(buffer))) > 0)
        {
            outFile.write(buffer, bytesRead);
        }

        outFile.close();
        unzCloseCurrentFile(zipFile);
    } while (unzGoToNextFile(zipFile) == UNZ_OK);

    unzClose(zipFile);
    return true; // Successfully unzipped all files
}

void InstallUpdateDialog::writeAndRunScript(QStringList stringList)
{
    QString scriptPath = this->temporaryDirectory + QStringLiteral("/update.bat");
    
    QFile scriptFile(scriptPath);
    if (!scriptFile.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        QMessageBox::critical(this, tr("Error"), tr("Failed to open file for writing: %1").arg(filename));
        return;
    }

    QTextStream textStream(&scriptFile);

    // Write script to file
    for (const QString& str : stringList)
    {
        textStream << str << "\n";
    }

    scriptFile.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner);
    scriptFile.close();

    this->launchProcess(scriptPath, {});
}

void InstallUpdateDialog::launchProcess(QString file, QStringList arguments)
{
    QProcess process;
    process.setProgram(file);
    process.setArguments(arguments);
    process.startDetached();
}

void InstallUpdateDialog::timerEvent(QTimerEvent *event)
{
    this->killTimer(event->timerId());
    this->install();
}
