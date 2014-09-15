// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <QDialog>

// Predefinitions
namespace Ui {
class DAboutDialog;
}

class DAboutDialog : public QDialog
{
	Q_OBJECT

public:
	explicit DAboutDialog(QWidget* p = nullptr);
	~DAboutDialog();

private slots:
	void on_label_linkActivated(const QString& link);

private:
	std::unique_ptr<Ui::DAboutDialog> ui;
};
