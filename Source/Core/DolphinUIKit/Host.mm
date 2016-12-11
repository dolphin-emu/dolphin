// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <Foundation/Foundation.h>

#include "Common/Common.h"
#include "Core/Core.h"
#include "DolphinUIKit/Host.h"

bool Host_UIHasFocus()
{
  return true;
}

bool Host_RendererHasFocus()
{
  return true;
}

bool Host_RendererIsFullscreen()
{
  return true;
}

void Host_ConnectWiimote(int wm_idx, bool connect)
{
}

void Host_Message(int Id)
{
  switch (Id)
  {
  case WM_USER_JOB_DISPATCH:
    dispatch_async(dispatch_get_main_queue(), ^{
      Core::HostDispatchJobs();
    });
    break;
  case WM_USER_STOP:
    break;
  case WM_USER_CREATE:
    break;
  case WM_USER_SETCURSOR:
    break;
  }
}

void Host_NotifyMapLoaded()
{
}

void Host_RefreshDSPDebuggerWindow()
{
}

void Host_RequestRenderWindowSize(int width, int height)
{
}

void Host_SetStartupDebuggingParameters()
{
}

void Host_SetWiiMoteConnectionState(int _State)
{
}

void Host_UpdateDisasmDialog()
{
  NSLog(@"update disasm dialog");
}

void Host_UpdateMainFrame()
{
}

void Host_UpdateTitle(const std::string& title)
{
  NSLog(@"Title: %s", title.c_str());
}

void Host_ShowVideoConfig(void* parent, const std::string& backend_name)
{
  NSLog(@"Video config: backend %s", backend_name.c_str());
}

void Host_YieldToUI()
{
}

static void* gRenderHandle = nullptr;

void* Host_GetRenderHandle()
{
  return gRenderHandle;
}

void Host_SetRenderHandle(void* render_handle)
{
  gRenderHandle = render_handle;
}
