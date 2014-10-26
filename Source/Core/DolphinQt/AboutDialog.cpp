// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include <QDesktopServices>
#include <QUrl>

#include "ui_AboutDialog.h"

#include "Common/Common.h"
#include "Common/StdMakeUnique.h"

#include "DolphinQt/AboutDialog.h"
#include "DolphinQt/Utils/Utils.h"

DAboutDialog::DAboutDialog(QWidget* parent_widget)
	: QDialog(parent_widget)
{
	setWindowModality(Qt::WindowModal);
	setAttribute(Qt::WA_DeleteOnClose);

	m_ui = std::make_unique<Ui::DAboutDialog>();
	m_ui->setupUi(this);
	m_ui->label->setText(m_ui->label->text().arg(SC(scm_desc_str),
	    SL("2014"), SC(scm_branch_str), SC(scm_rev_git_str),
	    SL(__DATE__), SL(__TIME__)));
}

DAboutDialog::~DAboutDialog()
{
}
