// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <QWidget>

class DRenderWidget final : public QWidget
{
	Q_OBJECT

public:
	DRenderWidget(QWidget* parent_widget = nullptr);

protected:
	// Some window managers start window dragging if an "empty" window area was clicked.
	// Prevent this by intercepting the mouse press event.
	void mousePressEvent(QMouseEvent*) override {}
	void paintEvent(QPaintEvent*) override {}

private:
	void closeEvent(QCloseEvent* e) override;
};

