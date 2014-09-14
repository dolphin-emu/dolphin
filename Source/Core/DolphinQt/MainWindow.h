// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include <QMainWindow>

// Predefinitions
namespace Ui {
class DMainWindow;
}

class DMainWindow : public QMainWindow
{
	Q_OBJECT

public:
	explicit DMainWindow(QWidget *p = 0);
	~DMainWindow();

private slots:

	// Help menu
	void on_actWebsite_triggered();
	void on_actOnlineDocs_triggered();
	void on_actGitHub_triggered();
	void on_actAbout_triggered();

private:
	Ui::DMainWindow *ui;
};
