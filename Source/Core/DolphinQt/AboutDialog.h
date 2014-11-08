// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <QDialog>

// Predefinitions
namespace Ui
{
class DAboutDialog;
}

class DAboutDialog : public QDialog
{
	Q_OBJECT

public:
	explicit DAboutDialog(QWidget* parent_widget = nullptr);
	~DAboutDialog();

private:
	std::unique_ptr<Ui::DAboutDialog> m_ui;
};
