/*
*  Dolphin for Mario Party Netplay
*  Copyright (C) 2025 Tabitha Hanegan <tabithahanegan.com>
*/

#include "UpdateDialog.h"
#include "DownloadUpdateDialog.h"

#include <QFileInfo>
#include <QPushButton>
#include <QJsonArray>
#include <QJsonObject>
#include <QDesktopServices>
#include <QTemporaryDir>
#include <QFile>
#include <QSysInfo>
#include <QProcess>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QWidget>
#include <QLabel>
#include <QTextEdit>
#include <QDialogButtonBox>
#include "../../Common/Logging/Log.h"

using namespace UserInterface::Dialog;

UpdateDialog::UpdateDialog(QWidget *parent, QJsonObject jsonObject, bool forced) 
    : QDialog(parent)
{
    this->jsonObject = jsonObject;

    // Create UI components
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    
    // Create and set up the label
    label = new QLabel(this);
    QString tagName = jsonObject.value(QStringLiteral("tag_name")).toString();
    label->setText(QStringLiteral("%1 Available").arg(tagName));
    mainLayout->addWidget(label);

    // Create and set up the text edit
    textEdit = new QTextEdit(this);
    textEdit->setText(jsonObject.value(QStringLiteral("body")).toString());
    textEdit->setReadOnly(true);
    mainLayout->addWidget(textEdit);

    // Create and set up the button box
    buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    QPushButton* updateButton = buttonBox->button(QDialogButtonBox::Ok);
    updateButton->setText(QStringLiteral("Update"));
    mainLayout->addWidget(buttonBox);

    // Connect signals
    connect(buttonBox, &QDialogButtonBox::accepted, this, &UpdateDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &UpdateDialog::reject);

    // Set the layout
    setLayout(mainLayout);
    setWindowTitle(QStringLiteral("Update Available"));
    resize(400, 300);
}

UpdateDialog::~UpdateDialog()
{
}

void UpdateDialog::accept()
{
    QJsonArray jsonArray = jsonObject[QStringLiteral("assets")].toArray();
    QString filenameToDownload;
    QString urlToDownload;

    for (const QJsonValue& value : jsonArray)
    {
        QJsonObject object = value.toObject();

        QString filenameBlob = object.value(QStringLiteral("name")).toString();
        QString downloadUrl(object.value(QStringLiteral("browser_download_url")).toString());

        #ifdef _WIN32
        if (filenameBlob.contains(QStringLiteral("win32")) || 
            filenameBlob.contains(QStringLiteral("windows")) || 
            filenameBlob.contains(QStringLiteral("win64")))
        {
            filenameToDownload = filenameBlob;
            urlToDownload = downloadUrl;
            break;
        }
        #endif
        #ifdef __APPLE__
        if (filenameBlob.contains(QStringLiteral("darwin")) || 
            filenameBlob.contains(QStringLiteral("macOS")))
        {
            filenameToDownload = filenameBlob;
            urlToDownload = downloadUrl;
            break;
        }
        #endif
    }

    this->url = urlToDownload;
    this->filename = filenameToDownload;
    QDialog::accept();

    DownloadUpdateDialog downloadDialog(this, urlToDownload, filenameToDownload);
}
