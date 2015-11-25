// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <QMainWindow>

// Predefinitions
class QAbstractButton;
namespace Ui
{
class DConfigDialog;
}

class DConfigDialog final : public QMainWindow
{
	Q_OBJECT

public:
	explicit DConfigDialog(QWidget* parent_widget = nullptr);
	~DConfigDialog();

private:
	std::unique_ptr<Ui::DConfigDialog> m_ui;

	void UpdateIcons();
	void LoadSettings();
};
