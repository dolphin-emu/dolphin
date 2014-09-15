// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include <QDesktopServices>
#include <QUrl>

#include "AboutDialog.h"
#include "ui_AboutDialog.h"

#include "Common/StdMakeUnique.h"

// TODO
#define scm_desc_str "00000"
#define scm_branch_str "unknown"
#define scm_rev_git_str "unknown"

DAboutDialog::DAboutDialog(QWidget* p)
	: QDialog(p)
{
	ui = std::make_unique<Ui::DAboutDialog>();
	ui->setupUi(this);
	ui->label->setText(ui->label->text().arg(QLatin1String(scm_desc_str),
	                                         QStringLiteral("2014"),
	                                         QLatin1String(scm_branch_str),
	                                         QLatin1String(scm_rev_git_str),
	                                         QStringLiteral(__DATE__),
	                                         QStringLiteral(__TIME__)));
}

DAboutDialog::~DAboutDialog()
{
}

void DAboutDialog::on_label_linkActivated(const QString &link)
{
    QDesktopServices::openUrl(QUrl(link));
}
