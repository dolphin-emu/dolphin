/*
*  Dolphin for Mario Party Netplay
*  Copyright (C) 2025 Tabitha Hanegan <tabithahanegan.com>
*/
#pragma once

#include <QDialog>
#include <QJsonObject>
#include <QUrl>

class QLabel;
class QTextEdit;
class QDialogButtonBox;

namespace UserInterface
{
namespace Dialog
{

class UpdateDialog : public QDialog
{
    Q_OBJECT

private:
    QJsonObject jsonObject;
    QString filename;
    QUrl url;

    // UI elements
    QLabel* label;
    QTextEdit* textEdit;
    QDialogButtonBox* buttonBox;

#ifdef _WIN32
    bool isWin32Setup = false;
#endif // _WIN32

public:
    explicit UpdateDialog(QWidget *parent, QJsonObject jsonObject, bool forced);
    ~UpdateDialog();

    QString GetFileName();
    QUrl GetUrl();

private slots:
    void accept() override;
};

} // namespace Dialog
} // namespace UserInterface
