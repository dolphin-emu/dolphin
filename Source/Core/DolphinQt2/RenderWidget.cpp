// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <QKeyEvent>

#include "DolphinQt2/Host.h"
#include "DolphinQt2/RenderWidget.h"

RenderWidget::RenderWidget(QWidget* parent)
	: QWidget(parent)
{
	setAttribute(Qt::WA_OpaquePaintEvent, true);
	setAttribute(Qt::WA_NoSystemBackground, true);

	connect(Host::GetInstance(), &Host::RequestTitle, this, &RenderWidget::setWindowTitle);
	connect(this, &RenderWidget::FocusChanged, Host::GetInstance(), &Host::SetRenderFocus);
	connect(this, &RenderWidget::StateChanged, Host::GetInstance(), &Host::SetRenderFullscreen);
	connect(this, &RenderWidget::HandleChanged, Host::GetInstance(), &Host::SetRenderHandle);
	emit HandleChanged((void*) winId());
}

bool RenderWidget::event(QEvent* event)
{
	switch (event->type())
	{
	case QEvent::KeyPress:
	{
		QKeyEvent* ke = static_cast<QKeyEvent*>(event);
		if (ke->key() == Qt::Key_Escape)
			emit EscapePressed();
		break;
	}
	case QEvent::WinIdChange:
		emit HandleChanged((void*) winId());
		break;
	case QEvent::FocusIn:
	case QEvent::FocusOut:
		emit FocusChanged(hasFocus());
		break;
	case QEvent::WindowStateChange:
		emit StateChanged(isFullScreen());
		break;
	case QEvent::Close:
		emit Closed();
		break;
	default:
		break;
	}
	return QWidget::event(event);
}
