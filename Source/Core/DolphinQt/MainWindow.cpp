// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include <QDesktopServices>
#include <QUrl>

#include "AboutDialog.h"

#include "MainWindow.h"
#include "ui_MainWindow.h"

DMainWindow::DMainWindow(QWidget *p) :
    QMainWindow(p),
    ui(new Ui::DMainWindow)
{
	ui->setupUi(this);
}

DMainWindow::~DMainWindow()
{
	delete ui;
}

void DMainWindow::on_actWebsite_triggered()
{
    QDesktopServices::openUrl(QUrl("https://dolphin-emu.org/"));
}

void DMainWindow::on_actOnlineDocs_triggered()
{
    QDesktopServices::openUrl(QUrl("https://dolphin-emu.org/docs/guides/"));
}

void DMainWindow::on_actGitHub_triggered()
{
    QDesktopServices::openUrl(QUrl("https://github.com/dolphin-emu/dolphin/"));
}

void DMainWindow::on_actAbout_triggered()
{
	DAboutDialog dlg;
	dlg.exec();
}
