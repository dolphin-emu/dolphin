// Copyright 2015 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/Host.h"

#include <functional>

#include <QAbstractEventDispatcher>
#include <QApplication>
#include <QLocale>

#include <imgui.h>

#ifdef _WIN32
#include <windows.h>
#endif

#include "Common/Common.h"

#include "Core/Config/MainSettings.h"
#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/Debugger/PPCDebugInterface.h"
#include "Core/Host.h"
#include "Core/NetPlayProto.h"
#include "Core/PowerPC/PowerPC.h"
#include "Core/State.h"
#include "Core/System.h"

#ifdef HAS_LIBMGBA
#include "DolphinQt/GBAWidget.h"
#endif
#include "DolphinQt/QtUtils/QueueOnObject.h"
#include "DolphinQt/Settings.h"

#include "InputCommon/ControllerInterface/ControllerInterface.h"

#include "UICommon/DiscordPresence.h"

#include "VideoCommon/AbstractGfx.h"
#include "VideoCommon/Fifo.h"
#include "VideoCommon/Present.h"
#include "VideoCommon/VideoConfig.h"

Host::Host()
{
  State::SetOnAfterLoadCallback([] { Host_UpdateDisasmDialog(); });
}

Host::~Host()
{
  State::SetOnAfterLoadCallback(nullptr);
}

Host* Host::GetInstance()
{
  static Host* s_instance = new Host();
  return s_instance;
}

void Host::SetRenderHandle(void* handle)
{
  m_render_to_main = Config::Get(Config::MAIN_RENDER_TO_MAIN);

  if (m_render_handle == handle)
    return;

  m_render_handle = handle;
  if (g_presenter)
  {
    g_presenter->ChangeSurface(handle);
    g_controller_interface.ChangeWindow(handle);
  }
}

void Host::SetMainWindowHandle(void* handle)
{
  m_main_window_handle = handle;
}

static void RunWithGPUThreadInactive(std::function<void()> f)
{
  // Potentially any thread which shows panic alerts can be blocked on this returning.
  // This means that, in order to avoid deadlocks, we need to be careful with how we
  // synchronize with other threads. Note that the panic alert handler temporarily declares
  // us as the CPU and/or GPU thread if the panic alert was requested by that thread.

  // TODO: What about the unlikely case where the GPU thread calls the panic alert handler
  // while the panic alert handler is processing a panic alert from the CPU thread?

  if (Core::IsGPUThread())
  {
    // If we are the GPU thread, we can't call Core::PauseAndLock without getting a deadlock,
    // since it would try to pause the GPU thread while that thread is waiting for us.
    // However, since we know that the GPU thread is inactive, we can just run f directly.

    f();
  }
  else if (Core::IsCPUThread())
  {
    // If we are the CPU thread in dual core mode, we can't call Core::PauseAndLock, for the
    // same reason as above. Instead, we use Fifo::PauseAndLock to pause the GPU thread only.
    // (Note that this case cannot be reached in single core mode, because in single core mode,
    // the CPU and GPU threads are the same thread, and we already checked for the GPU thread.)

    const bool was_running = Core::GetState() == Core::State::Running;
    auto& system = Core::System::GetInstance();
    auto& fifo = system.GetFifo();
    fifo.PauseAndLock(true, was_running);
    f();
    fifo.PauseAndLock(false, was_running);
  }
  else
  {
    // If we reach here, we can call Core::PauseAndLock (which we do using RunAsCPUThread).

    Core::RunAsCPUThread(std::move(f));
  }
}

bool Host::GetRenderFocus()
{
#ifdef _WIN32
  // Unfortunately Qt calls SetRenderFocus() with a slight delay compared to what we actually need
  // to avoid inputs that cause a focus loss to be processed by the emulation
  if (m_render_to_main && !m_render_fullscreen)
    return GetForegroundWindow() == (HWND)m_main_window_handle.load();
  return GetForegroundWindow() == (HWND)m_render_handle.load();
#else
  return m_render_focus;
#endif
}

bool Host::GetRenderFullFocus()
{
  return m_render_full_focus;
}

void Host::SetRenderFocus(bool focus)
{
  m_render_focus = focus;
  if (g_gfx && m_render_fullscreen && g_ActiveConfig.ExclusiveFullscreenEnabled())
  {
    RunWithGPUThreadInactive([focus] {
      if (!Config::Get(Config::MAIN_RENDER_TO_MAIN))
        g_gfx->SetFullscreen(focus);
    });
  }
}

void Host::SetRenderFullFocus(bool focus)
{
  m_render_full_focus = focus;
}

bool Host::GetGBAFocus()
{
#ifdef HAS_LIBMGBA
  return qobject_cast<GBAWidget*>(QApplication::activeWindow()) != nullptr;
#else
  return false;
#endif
}

bool Host::GetRenderFullscreen()
{
  return m_render_fullscreen;
}

void Host::SetRenderFullscreen(bool fullscreen)
{
  m_render_fullscreen = fullscreen;

  if (g_gfx && g_gfx->IsFullscreen() != fullscreen && g_ActiveConfig.ExclusiveFullscreenEnabled())
  {
    RunWithGPUThreadInactive([fullscreen] { g_gfx->SetFullscreen(fullscreen); });
  }
}

void Host::ResizeSurface(int new_width, int new_height)
{
  if (g_presenter)
    g_presenter->ResizeSurface();
}

std::vector<std::string> Host_GetPreferredLocales()
{
  const QStringList ui_languages = QLocale::system().uiLanguages();
  std::vector<std::string> converted_languages(ui_languages.size());

  for (int i = 0; i < ui_languages.size(); ++i)
    converted_languages[i] = ui_languages[i].toStdString();

  return converted_languages;
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
  return Host::GetInstance()->GetRenderFocus() || Host::GetInstance()->GetGBAFocus();
}

bool Host_RendererHasFullFocus()
{
  return Host::GetInstance()->GetRenderFullFocus();
}

bool Host_RendererIsFullscreen()
{
  return Host::GetInstance()->GetRenderFullscreen();
}

void Host_YieldToUI()
{
  qApp->processEvents(QEventLoop::ExcludeUserInputEvents);
}

void Host_UpdateDisasmDialog()
{
  QueueOnObject(QApplication::instance(), [] { emit Host::GetInstance()->UpdateDisasmDialog(); });
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

void Host_UpdateDiscordClientID(const std::string& client_id)
{
#ifdef USE_DISCORD_PRESENCE
  Discord::UpdateClientID(client_id);
#endif
}

bool Host_UpdateDiscordPresenceRaw(const std::string& details, const std::string& state,
                                   const std::string& large_image_key,
                                   const std::string& large_image_text,
                                   const std::string& small_image_key,
                                   const std::string& small_image_text,
                                   const int64_t start_timestamp, const int64_t end_timestamp,
                                   const int party_size, const int party_max)
{
#ifdef USE_DISCORD_PRESENCE
  return Discord::UpdateDiscordPresenceRaw(details, state, large_image_key, large_image_text,
                                           small_image_key, small_image_text, start_timestamp,
                                           end_timestamp, party_size, party_max);
#else
  return false;
#endif
}

#ifndef HAS_LIBMGBA
std::unique_ptr<GBAHostInterface> Host_CreateGBAHost(std::weak_ptr<HW::GBA::Core> core)
{
  return nullptr;
}
#endif
