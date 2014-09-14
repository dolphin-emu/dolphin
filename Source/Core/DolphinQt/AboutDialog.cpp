// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include <QDesktopServices>
#include <QUrl>

#include "AboutDialog.h"
#include "ui_AboutDialog.h"

// TODO
#define scm_desc_str "unknown"
#define scm_branch_str "unknown"
#define scm_rev_git_str "0000000"

DAboutDialog::DAboutDialog(QWidget *p) :
    QDialog(p),
    ui(new Ui::DAboutDialog)
{
	ui->setupUi(this);
	ui->label->setText(ui->label->text().arg(scm_desc_str,
	                                         "2014",
	                                         scm_branch_str,
	                                         scm_rev_git_str,
	                                         __DATE__,
	                                         __TIME__));
}

DAboutDialog::~DAboutDialog()
{
	delete ui;
}

void DAboutDialog::on_label_linkActivated(const QString &link)
{
    QDesktopServices::openUrl(QUrl(link));
}
