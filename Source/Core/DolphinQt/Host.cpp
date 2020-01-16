// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinQt/Host.h"

#include <QAbstractEventDispatcher>
#include <QApplication>
#include <QProgressDialog>

#include <imgui.h>

#include "Common/Common.h"

#include "Core/Config/MainSettings.h"
#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/Debugger/PPCDebugInterface.h"
#include "Core/Host.h"
#include "Core/NetPlayProto.h"
#include "Core/PowerPC/PowerPC.h"

#include "DolphinQt/QtUtils/QueueOnObject.h"
#include "DolphinQt/Settings.h"

#include "InputCommon/ControllerInterface/ControllerInterface.h"

#include "UICommon/DiscordPresence.h"

#include "VideoCommon/RenderBase.h"
#include "VideoCommon/VideoConfig.h"

Host::Host() = default;

Host* Host::GetInstance()
{
  static Host* s_instance = new Host();
  return s_instance;
}

void Host::SetRenderHandle(void* handle)
{
  if (m_render_handle == handle)
    return;

  m_render_handle = handle;
  if (g_renderer)
  {
    g_renderer->ChangeSurface(handle);
    if (g_controller_interface.IsInit())
      g_controller_interface.ChangeWindow(handle);
  }
}

bool Host::GetRenderFocus() const
{
  return m_render_focus.load();
}

bool Host::IsFullscreenEnabled() const
{
  return m_fullscreen_enabled.load();
}

bool Host::IsFullscreenActive() const
{
  return g_renderer ? g_renderer->IsFullscreen() : false;
}

void Host::SetRenderFocus(bool focus)
{
  m_render_focus.store(focus);

  if (m_fullscreen_enabled.load())
  {
    Core::RunAsCPUThread([focus] {
      if (!Config::Get(Config::MAIN_RENDER_TO_MAIN) && g_renderer)
        g_renderer->SetFullscreen(focus);
    });
  }
}

void Host::SetFullscreenEnabled(bool enabled)
{
  bool old_value = !enabled;
  if (!m_fullscreen_enabled.compare_exchange_strong(old_value, enabled) || old_value == enabled)
    return;

  Core::RunAsCPUThread([enabled]() {
    if (g_renderer)
      g_renderer->SetFullscreen(enabled);
  });
}

void Host::UpdateFullscreen(bool disable_temporarily)
{
  const bool enabled = m_fullscreen_enabled.load() && !disable_temporarily;
  Core::RunAsCPUThread([enabled]() {
    if (g_renderer && g_renderer->IsFullscreen() != enabled)
      g_renderer->SetFullscreen(enabled);
  });
}

void Host::ResizeSurface(int new_width, int new_height)
{
  if (g_renderer)
    g_renderer->ResizeSurface();
}

void Host_Message(HostMessageID id)
{
  if (id == HostMessageID::WMUserStop)
  {
    emit Host::GetInstance()->RequestStop();
  }
  else if (id == HostMessageID::WMUserJobDispatch)
  {
    // Just poke the main thread to get it to wake up, job dispatch
    // will happen automatically before it goes back to sleep again.
    QAbstractEventDispatcher::instance(qApp->thread())->wakeUp();
  }
}

void Host_UpdateTitle(const std::string& title)
{
  emit Host::GetInstance()->RequestTitle(QString::fromStdString(title));
}

bool Host_RendererHasFocus()
{
  return Host::GetInstance()->GetRenderFocus();
}

void Host_YieldToUI()
{
  qApp->processEvents(QEventLoop::ExcludeUserInputEvents);
}

void Host_UpdateDisasmDialog()
{
  QueueOnObject(QApplication::instance(), [] { emit Host::GetInstance()->UpdateDisasmDialog(); });
}

void Host_UpdateProgressDialog(const char* caption, int position, int total)
{
  emit Host::GetInstance()->UpdateProgressDialog(QString::fromUtf8(caption), position, total);
}

void Host::RequestNotifyMapLoaded()
{
  QueueOnObject(QApplication::instance(), [this] { emit NotifyMapLoaded(); });
}

void Host_NotifyMapLoaded()
{
  Host::GetInstance()->RequestNotifyMapLoaded();
}

// We ignore these, and their purpose should be questioned individually.
// In particular, RequestRenderWindowSize, RequestFullscreen, and
// UpdateMainFrame should almost certainly be removed.
void Host_UpdateMainFrame()
{
}

void Host_RequestFullscreen(bool active, float refresh_rate)
{
  emit Host::GetInstance()->RequestFullscreen(active, refresh_rate);
}

void Host_RequestRenderWindowSize(int w, int h)
{
  emit Host::GetInstance()->RequestRenderSize(w, h);
}

bool Host_UIBlocksControllerState()
{
  return ImGui::GetCurrentContext() && ImGui::GetIO().WantCaptureKeyboard;
}

void Host_RefreshDSPDebuggerWindow()
{
}

void Host_TitleChanged()
{
#ifdef USE_DISCORD_PRESENCE
  // TODO: Not sure if the NetPlay check is needed.
  if (!NetPlay::IsNetPlayRunning())
    Discord::UpdateDiscordPresence();
#endif
}
