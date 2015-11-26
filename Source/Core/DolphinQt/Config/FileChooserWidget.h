// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <QWidget>

// Predefinitions
namespace Ui
{
class DFileChooser;
}

class DFileChooser final : public QWidget
{
	Q_OBJECT

public:
	explicit DFileChooser(QWidget* parent_widget = nullptr);
	~DFileChooser();

	void setPath(QString path);
	QString path();

signals:
	void changed();

private:
	std::unique_ptr<Ui::DFileChooser> m_ui;
};
