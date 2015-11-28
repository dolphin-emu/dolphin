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

	void setFormat(bool folder, const QString& filter) { m_folder = folder; m_filter = filter; }

	void setPath(const QString& path);
	QString path();

signals:
	void changed();

private:
	std::unique_ptr<Ui::DFileChooser> m_ui;
	bool m_folder;
	QString m_filter;
};
