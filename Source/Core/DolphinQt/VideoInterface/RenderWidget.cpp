// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <QCloseEvent>
#include <QMessageBox>

#include "DolphinQt/MainWindow.h"
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
	if (!g_main_window->OnStop())
	{
		e->ignore();
		return;
	}
	QWidget::closeEvent(e);
}
