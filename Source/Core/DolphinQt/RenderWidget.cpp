// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinQt/RenderWidget.h"

#include <array>

#include <QApplication>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileInfo>
#include <QGuiApplication>
#include <QIcon>
#include <QKeyEvent>
#include <QMimeData>
#include <QMouseEvent>
#include <QPalette>
#include <QScreen>
#include <QTimer>
#include <QWindow>

#include "imgui.h"

#include "Core/Config/MainSettings.h"
#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/State.h"

#include "DolphinQt/Host.h"
#include "DolphinQt/QtUtils/ModalMessageBox.h"
#include "DolphinQt/Resources.h"
#include "DolphinQt/Settings.h"

#include "VideoCommon/RenderBase.h"
#include "VideoCommon/VertexShaderManager.h"
#include "VideoCommon/VideoConfig.h"

RenderWidget::RenderWidget(QWidget* parent) : QWidget(parent)
{
  setWindowTitle(QStringLiteral("Dolphin"));
  setWindowIcon(Resources::GetAppIcon());
  setWindowRole(QStringLiteral("renderer"));
  setAcceptDrops(true);

  QPalette p;
  p.setColor(QPalette::Window, Qt::black);
  setPalette(p);

  connect(Host::GetInstance(), &Host::RequestTitle, this, &RenderWidget::setWindowTitle);
  connect(Host::GetInstance(), &Host::RequestRenderSize, this, [this](int w, int h) {
    if (!Config::Get(Config::MAIN_RENDER_WINDOW_AUTOSIZE) || isFullScreen() || isMaximized())
      return;

    resize(w, h);
  });

  connect(&Settings::Instance(), &Settings::EmulationStateChanged, this, [this](Core::State state) {
    // Stop filling the background once emulation starts, but fill it until then (Bug 10958)
    SetFillBackground(Config::Get(Config::MAIN_RENDER_TO_MAIN) &&
                      state == Core::State::Uninitialized);
    if (state == Core::State::Running)
      SetImGuiKeyMap();
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
  setAutoFillBackground(fill);
  setAttribute(Qt::WA_OpaquePaintEvent, !fill);
  setAttribute(Qt::WA_NoSystemBackground, !fill);
  setAttribute(Qt::WA_PaintOnScreen, !fill);
}

QPaintEngine* RenderWidget::paintEngine() const
{
  return autoFillBackground() ? QWidget::paintEngine() : nullptr;
}

void RenderWidget::dragEnterEvent(QDragEnterEvent* event)
{
  if (event->mimeData()->hasUrls() && event->mimeData()->urls().size() == 1)
    event->acceptProposedAction();
}

void RenderWidget::dropEvent(QDropEvent* event)
{
  const auto& urls = event->mimeData()->urls();
  if (urls.empty())
    return;

  const auto& url = urls[0];
  QFileInfo file_info(url.toLocalFile());

  auto path = file_info.filePath();

  if (!file_info.exists() || !file_info.isReadable())
  {
    ModalMessageBox::critical(this, tr("Error"), tr("Failed to open '%1'").arg(path));
    return;
  }

  if (!file_info.isFile())
  {
    return;
  }

  State::LoadAs(path.toStdString());
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

  QScreen* screen = window()->windowHandle()->screen();

  const auto dpr = screen->devicePixelRatio();

  emit SizeChanged(width() * dpr, height() * dpr);
}

bool RenderWidget::event(QEvent* event)
{
  PassEventToImGui(event);

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
    [[fallthrough]];

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

    QScreen* screen = window()->windowHandle()->screen();

    const auto dpr = screen->devicePixelRatio();

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
  const auto mouse_move = event->pos() - m_last_mouse;
  m_last_mouse = event->pos();

  if (event->buttons() & Qt::RightButton)
  {
    // Camera Pitch and Yaw:
    VertexShaderManager::RotateView(mouse_move.y() / 200.f, mouse_move.x() / 200.f, 0.f);
  }
  else if (event->buttons() & Qt::MidButton)
  {
    // Camera Roll:
    VertexShaderManager::RotateView(0.f, 0.f, mouse_move.x() / 200.f);
  }
}

void RenderWidget::PassEventToImGui(const QEvent* event)
{
  if (!Core::IsRunningAndStarted())
    return;

  switch (event->type())
  {
  case QEvent::KeyPress:
  case QEvent::KeyRelease:
  {
    // As the imgui KeysDown array is only 512 elements wide, and some Qt keys which
    // we need to track (e.g. alt) are above this value, we mask the lower 9 bits.
    // Even masked, the key codes are still unique, so conflicts aren't an issue.
    // The actual text input goes through AddInputCharactersUTF8().
    const QKeyEvent* key_event = static_cast<const QKeyEvent*>(event);
    const bool is_down = event->type() == QEvent::KeyPress;
    const u32 key = static_cast<u32>(key_event->key() & 0x1FF);
    auto lock = g_renderer->GetImGuiLock();
    if (key < std::size(ImGui::GetIO().KeysDown))
      ImGui::GetIO().KeysDown[key] = is_down;

    if (is_down)
    {
      auto utf8 = key_event->text().toUtf8();
      ImGui::GetIO().AddInputCharactersUTF8(utf8.constData());
    }
  }
  break;

  case QEvent::MouseMove:
  {
    auto lock = g_renderer->GetImGuiLock();

    // Qt multiplies all coordinates by the scaling factor in highdpi mode, giving us "scaled" mouse
    // coordinates (as if the screen was standard dpi). We need to update the mouse position in
    // native coordinates, as the UI (and game) is rendered at native resolution.
    const float scale = devicePixelRatio();
    ImGui::GetIO().MousePos.x = static_cast<const QMouseEvent*>(event)->x() * scale;
    ImGui::GetIO().MousePos.y = static_cast<const QMouseEvent*>(event)->y() * scale;
  }
  break;

  case QEvent::MouseButtonPress:
  case QEvent::MouseButtonRelease:
  {
    auto lock = g_renderer->GetImGuiLock();
    const u32 button_mask = static_cast<u32>(static_cast<const QMouseEvent*>(event)->buttons());
    for (size_t i = 0; i < std::size(ImGui::GetIO().MouseDown); i++)
      ImGui::GetIO().MouseDown[i] = (button_mask & (1u << i)) != 0;
  }
  break;

  default:
    break;
  }
}

void RenderWidget::SetImGuiKeyMap()
{
  static constexpr std::array<std::array<int, 2>, 21> key_map{{
      {ImGuiKey_Tab, Qt::Key_Tab},
      {ImGuiKey_LeftArrow, Qt::Key_Left},
      {ImGuiKey_RightArrow, Qt::Key_Right},
      {ImGuiKey_UpArrow, Qt::Key_Up},
      {ImGuiKey_DownArrow, Qt::Key_Down},
      {ImGuiKey_PageUp, Qt::Key_PageUp},
      {ImGuiKey_PageDown, Qt::Key_PageDown},
      {ImGuiKey_Home, Qt::Key_Home},
      {ImGuiKey_End, Qt::Key_End},
      {ImGuiKey_Insert, Qt::Key_Insert},
      {ImGuiKey_Delete, Qt::Key_Delete},
      {ImGuiKey_Backspace, Qt::Key_Backspace},
      {ImGuiKey_Space, Qt::Key_Space},
      {ImGuiKey_Enter, Qt::Key_Return},
      {ImGuiKey_Escape, Qt::Key_Escape},
      {ImGuiKey_A, Qt::Key_A},
      {ImGuiKey_C, Qt::Key_C},
      {ImGuiKey_V, Qt::Key_V},
      {ImGuiKey_X, Qt::Key_X},
      {ImGuiKey_Y, Qt::Key_Y},
      {ImGuiKey_Z, Qt::Key_Z},
  }};
  auto lock = g_renderer->GetImGuiLock();
  for (auto entry : key_map)
    ImGui::GetIO().KeyMap[entry[0]] = entry[1] & 0x1FF;
}
