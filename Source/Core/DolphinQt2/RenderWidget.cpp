// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <QKeyEvent>
#include <QTimer>

#include "DolphinQt2/Host.h"
#include "DolphinQt2/RenderWidget.h"
#include "DolphinQt2/Settings.h"

RenderWidget::RenderWidget(QWidget* parent) : QWidget(parent)
{
  setAttribute(Qt::WA_OpaquePaintEvent, true);
  setAttribute(Qt::WA_NoSystemBackground, true);

  connect(Host::GetInstance(), &Host::RequestTitle, this, &RenderWidget::setWindowTitle);
  connect(this, &RenderWidget::StateChanged, Host::GetInstance(), &Host::SetRenderFullscreen);
  connect(this, &RenderWidget::HandleChanged, Host::GetInstance(), &Host::SetRenderHandle);
  emit HandleChanged((void*)winId());

  m_mouse_timer = new QTimer(this);
  connect(m_mouse_timer, &QTimer::timeout, this, &RenderWidget::HandleCursorTimer);
  m_mouse_timer->setSingleShot(true);
  setMouseTracking(true);

  connect(&Settings::Instance(), &Settings::HideCursorChanged, this,
          &RenderWidget::OnHideCursorChanged);
  OnHideCursorChanged();
  m_mouse_timer->start(MOUSE_HIDE_DELAY);
}

void RenderWidget::OnHideCursorChanged()
{
  setCursor(Settings::Instance().GetHideCursor() ? Qt::BlankCursor : Qt::ArrowCursor);
}

void RenderWidget::HandleCursorTimer()
{
  if (isActiveWindow())
    setCursor(Qt::BlankCursor);
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
  case QEvent::MouseMove:
  case QEvent::MouseButtonPress:
    if (!Settings::Instance().GetHideCursor() && isActiveWindow())
    {
      setCursor(Qt::ArrowCursor);
      m_mouse_timer->start(MOUSE_HIDE_DELAY);
    }
    break;
  case QEvent::WinIdChange:
    emit HandleChanged((void*)winId());
    break;
  case QEvent::WindowActivate:
    Host::GetInstance()->SetRenderFocus(true);
    break;
  case QEvent::WindowDeactivate:
    Host::GetInstance()->SetRenderFocus(false);
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
