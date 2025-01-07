/*
*  Dolphin for Mario Party Netplay
*  Copyright (C) 2025 Tabitha Hanegan <tabithahanegan.com>
*/
#pragma once

#include <QDialog>
#include <QJsonObject>
#include <QString>

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
    QString url;

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
    
private slots:
    void accept(void) Q_DECL_OVERRIDE;
};

} // namespace Dialog
} // namespace UserInterface
