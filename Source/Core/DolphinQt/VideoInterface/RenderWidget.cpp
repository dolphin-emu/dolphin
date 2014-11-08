// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include <QMessageBox>

#include "DolphinQt/VideoInterface/RenderWidget.h"

DRenderWidget::DRenderWidget(QWidget* parent_widget)
	: QWidget(parent_widget)
{
	setAttribute(Qt::WA_NativeWindow, true);
	setAttribute(Qt::WA_OpaquePaintEvent, true);
	setAttribute(Qt::WA_NoSystemBackground, true);
}

void DRenderWidget::closeEvent(QCloseEvent* e)
{
	// TODO: update render window positions in config

	// TODO: Do this differently...
	emit Closed();
	QWidget::closeEvent(e);
}
