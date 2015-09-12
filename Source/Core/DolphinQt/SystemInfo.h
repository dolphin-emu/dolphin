// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <QDialog>

namespace Ui
{
class DSystemInfo;
}

class DSystemInfo final : public QDialog
{
	Q_OBJECT

public:
	explicit DSystemInfo(QWidget* parent_widget = nullptr);
	~DSystemInfo();

private slots:
	void CopyPressed();

private:
	std::unique_ptr<Ui::DSystemInfo> m_ui;

	void UpdateSystemInfo();
	QString GetOS() const;
};
