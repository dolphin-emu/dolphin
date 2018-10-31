// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <QApplication>
#include <QDesktopWidget>
#include <QGuiApplication>
#include <QIcon>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPalette>
#include <QScreen>
#include <QTimer>

#include "Core/ConfigManager.h"
#include "Core/Core.h"

#include "DolphinQt/Host.h"
#include "DolphinQt/RenderWidget.h"
#include "DolphinQt/Resources.h"
#include "DolphinQt/Settings.h"

#include "VideoCommon/VertexShaderManager.h"
#include "VideoCommon/VideoConfig.h"

RenderWidget::RenderWidget(QWidget* parent) : QWidget(parent)
{
  setWindowTitle(QStringLiteral("Dolphin"));
  setWindowIcon(Resources::GetAppIcon());

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
  connect(this, &RenderWidget::FocusChanged, Host::GetInstance(), &Host::SetRenderFocus,
          Qt::DirectConnection);

  m_mouse_timer = new QTimer(this);
  connect(m_mouse_timer, &QTimer::timeout, this, &RenderWidget::HandleCursorTimer);
  m_mouse_timer->setSingleShot(true);
  setMouseTracking(true);

  connect(&Settings::Instance(), &Settings::HideCursorChanged, this,
          &RenderWidget::OnHideCursorChanged);
  OnHideCursorChanged();
  connect(&Settings::Instance(), &Settings::KeepWindowOnTopChanged, this,
          &RenderWidget::OnKeepOnTopChanged);
  OnKeepOnTopChanged(Settings::Instance().IsKeepWindowOnTopEnabled());
  m_mouse_timer->start(MOUSE_HIDE_DELAY);

  // We need a native window to render into.
  setAttribute(Qt::WA_NativeWindow);

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

void RenderWidget::OnKeepOnTopChanged(bool top)
{
  const bool was_visible = isVisible();

  setWindowFlags(top ? windowFlags() | Qt::WindowStaysOnTopHint :
                       windowFlags() & ~Qt::WindowStaysOnTopHint);

  if (was_visible)
    show();
}

void RenderWidget::HandleCursorTimer()
{
  if (isActiveWindow())
    setCursor(Qt::BlankCursor);
}

void RenderWidget::showFullScreen()
{
  QWidget::showFullScreen();

  const auto dpr =
      QGuiApplication::screens()[QApplication::desktop()->screenNumber(this)]->devicePixelRatio();

  emit SizeChanged(width() * dpr, height() * dpr);
}

bool RenderWidget::event(QEvent* event)
{
  switch (event->type())
  {
  case QEvent::Paint:
    return !autoFillBackground();
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
    if (g_Config.bFreeLook)
      OnFreeLookMouseMove(static_cast<QMouseEvent*>(event));

  // [[fallthrough]]
  case QEvent::MouseButtonPress:
    if (!Settings::Instance().GetHideCursor() && isActiveWindow())
    {
      setCursor(Qt::ArrowCursor);
      m_mouse_timer->start(MOUSE_HIDE_DELAY);
    }
    break;
  case QEvent::WinIdChange:
    emit HandleChanged(reinterpret_cast<void*>(winId()));
    break;
  case QEvent::WindowActivate:
    if (SConfig::GetInstance().m_PauseOnFocusLost && Core::GetState() == Core::State::Paused)
      Core::SetState(Core::State::Running);

    emit FocusChanged(true);
    break;
  case QEvent::WindowDeactivate:
    if (SConfig::GetInstance().m_PauseOnFocusLost && Core::GetState() == Core::State::Running)
      Core::SetState(Core::State::Paused);

    emit FocusChanged(false);
    break;
  case QEvent::Resize:
  {
    const QResizeEvent* se = static_cast<QResizeEvent*>(event);
    QSize new_size = se->size();

    auto* desktop = QApplication::desktop();

    int screen_nr = desktop->screenNumber(this);

    if (screen_nr == -1)
      screen_nr = desktop->screenNumber(parentWidget());

    const auto dpr = desktop->screen(screen_nr)->devicePixelRatio();

    emit SizeChanged(new_size.width() * dpr, new_size.height() * dpr);
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

void RenderWidget::OnFreeLookMouseMove(QMouseEvent* event)
{
  if (event->buttons() & Qt::MidButton)
  {
    // Mouse Move
    VertexShaderManager::TranslateView((event->x() - m_last_mouse[0]) / 50.0f,
                                       (event->y() - m_last_mouse[1]) / 50.0f);
  }
  else if (event->buttons() & Qt::RightButton)
  {
    // Mouse Look
    VertexShaderManager::RotateView((event->x() - m_last_mouse[0]) / 200.0f,
                                    (event->y() - m_last_mouse[1]) / 200.0f);
  }

  m_last_mouse[0] = event->x();
  m_last_mouse[1] = event->y();
}
