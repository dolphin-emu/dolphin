// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinQt2/Host.h"

#include <QAbstractEventDispatcher>
#include <QApplication>
#include <QCursor>
#include <QProgressDialog>

#include "Common/Common.h"
#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/Debugger/PPCDebugInterface.h"
#include "Core/Host.h"
#include "Core/PowerPC/PowerPC.h"
#include "DolphinQt2/Settings.h"
#include "VideoCommon/RenderBase.h"

Host::Host() = default;

Host* Host::GetInstance()
{
  static Host* s_instance = new Host();
  return s_instance;
}

void* Host::GetRenderHandle()
{
  return m_render_handle;
}

void Host::SetRenderHandle(void* handle)
{
  m_render_handle = handle;
}

bool Host::GetRenderFocus()
{
  return m_render_focus;
}

void Host::SetRenderFocus(bool focus)
{
  m_render_focus = focus;
}

bool Host::GetRenderFullscreen()
{
  return m_render_fullscreen;
}

void Host::SetRenderFullscreen(bool fullscreen)
{
  m_render_fullscreen = fullscreen;
}

void Host::MoveSurface(int x, int y)
{
  m_surface_x = x;
  m_surface_y = y;
}

void Host::ResizeSurface(int new_width, int new_height)
{
  if (g_renderer)
    g_renderer->ResizeSurface(new_width, new_height);

  m_surface_width = new_width;
  m_surface_height = new_height;
}

double Host::GetCursorX() const
{
  auto x = (QCursor::pos().x() - m_surface_x - (m_surface_width / 2.)) / (m_surface_width);

  return x * 2.5;
}
double Host::GetCursorY() const
{
  auto y = -(QCursor::pos().y() - m_surface_y - (m_surface_height / 2.)) / (m_surface_height / 2.) *
           (m_surface_height / static_cast<double>(m_surface_width));
  return y * 1.5;
}

void Host_Message(int id)
{
  if (id == WM_USER_STOP)
  {
    emit Host::GetInstance()->RequestStop();
  }
  else if (id == WM_USER_JOB_DISPATCH)
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

void* Host_GetRenderHandle()
{
  return Host::GetInstance()->GetRenderHandle();
}

bool Host_RendererHasFocus()
{
  return Host::GetInstance()->GetRenderFocus();
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
}

void Host_UpdateProgressDialog(const char* caption, int position, int total)
{
  emit Host::GetInstance()->UpdateProgressDialog(QString::fromUtf8(caption), position, total);
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

bool Host_UINeedsControllerState()
{
  return Settings::Instance().IsControllerStateNeeded();
}
void Host_NotifyMapLoaded()
{
}
void Host_ShowVideoConfig(void* parent, const std::string& backend_name)
{
}
void Host_RefreshDSPDebuggerWindow()
{
}

double Host_GetCursorX()
{
  return Host::GetInstance()->GetCursorX();
}

double Host_GetCursorY()
{
  return Host::GetInstance()->GetCursorY();
}
