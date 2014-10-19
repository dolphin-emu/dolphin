// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <QButtonGroup>
#include <QDialog>

// Predefinitions
namespace Ui {
class DConfigDialog;
}

class DConfigDialog : public QDialog
{
	Q_OBJECT

public:
	explicit DConfigDialog(QWidget* p = nullptr);
	~DConfigDialog();

private slots:
	// Event callbacks here?

private:
	std::unique_ptr<Ui::DConfigDialog> ui;

	QButtonGroup *CPUEngineButtonGroup;
};
