// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include <QDesktopServices>
#include <QUrl>

#include "ui_MainWindow.h"

#include "Common/StdMakeUnique.h"

#include "DolphinQt/AboutDialog.h"
#include "DolphinQt/MainWindow.h"
#include "DolphinQt/Utils/Resources.h"
#include "DolphinQt/Utils/Utils.h"

DMainWindow::DMainWindow(QWidget* parent_widget)
	: QMainWindow(parent_widget)
{
	ui = std::make_unique<Ui::DMainWindow>();
	ui->setupUi(this);

	Resources::Init();
	ui->actOpen->setIcon(Resources::GetIcon(Resources::TOOLBAR_OPEN));
}

DMainWindow::~DMainWindow()
{
}

void DMainWindow::on_actWebsite_triggered()
{
    QDesktopServices::openUrl(QUrl(SL("https://dolphin-emu.org/")));
}

void DMainWindow::on_actOnlineDocs_triggered()
{
	QDesktopServices::openUrl(QUrl(SL("https://dolphin-emu.org/docs/guides/")));
}

void DMainWindow::on_actGitHub_triggered()
{
	QDesktopServices::openUrl(QUrl(SL("https://github.com/dolphin-emu/dolphin/")));
}

void DMainWindow::on_actAbout_triggered()
{
	DAboutDialog dlg;
	dlg.exec();
}
