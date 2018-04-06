// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <QKeyEvent>
#include <QPalette>
#include <QTimer>

#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "DolphinQt2/Host.h"
#include "DolphinQt2/RenderWidget.h"
#include "DolphinQt2/Settings.h"

RenderWidget::RenderWidget(QWidget* parent) : QWidget(parent)
{
  QPalette p;
  p.setColor(QPalette::Background, Qt::black);
  setPalette(p);

  connect(Host::GetInstance(), &Host::RequestTitle, this, &RenderWidget::setWindowTitle);
  connect(Host::GetInstance(), &Host::RequestRenderSize, this, [this](int w, int h) {
    if (!SConfig::GetInstance().bRenderWindowAutoSize || isFullScreen() || isMaximized())
      return;

    resize(w, h);
  });

  connect(&Settings::Instance(), &Settings::EmulationStateChanged, this, [this](Core::State state) {
    SetFillBackground(SConfig::GetInstance().bRenderToMain && state == Core::State::Uninitialized);
  });

  // We have to use Qt::DirectConnection here because we don't want those signals to get queued
  // (which results in them not getting called)
  connect(this, &RenderWidget::StateChanged, Host::GetInstance(), &Host::SetRenderFullscreen,
          Qt::DirectConnection);
  connect(this, &RenderWidget::HandleChanged, Host::GetInstance(), &Host::SetRenderHandle,
          Qt::DirectConnection);
  connect(this, &RenderWidget::SizeChanged, Host::GetInstance(), &Host::ResizeSurface,
          Qt::DirectConnection);

  emit HandleChanged((void*)winId());

  m_mouse_timer = new QTimer(this);
  connect(m_mouse_timer, &QTimer::timeout, this, &RenderWidget::HandleCursorTimer);
  m_mouse_timer->setSingleShot(true);
  setMouseTracking(true);

  connect(&Settings::Instance(), &Settings::HideCursorChanged, this,
          &RenderWidget::OnHideCursorChanged);
  OnHideCursorChanged();
  m_mouse_timer->start(MOUSE_HIDE_DELAY);

  SetFillBackground(true);
}

void RenderWidget::SetFillBackground(bool fill)
{
  setAttribute(Qt::WA_OpaquePaintEvent, !fill);
  setAttribute(Qt::WA_NoSystemBackground, !fill);
  setAutoFillBackground(fill);
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

void RenderWidget::showFullScreen()
{
  QWidget::showFullScreen();
  emit SizeChanged(width(), height());
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

    // The render window might flicker on some platforms because Qt tries to change focus to a new
    // element when there is none (?) Handling this event before it reaches QWidget fixes the issue.
    if (ke->key() == Qt::Key_Tab)
      return true;

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
  case QEvent::Resize:
  {
    const QResizeEvent* se = static_cast<QResizeEvent*>(event);
    QSize new_size = se->size();
    emit SizeChanged(new_size.width(), new_size.height());
    break;
  }
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
