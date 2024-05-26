// Copyright 2021 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <cstdio>
#include <thread>

#include <memory>
#include <string>

#include "Common/Flag.h"
#include "Common/WindowSystemInfo.h"

#include "Core/Core.h"
#include "Core/DolphinAnalytics.h"
#include "Core/Host.h"

// Begin stubs needed to satisfy Core dependencies

std::vector<std::string> Host_GetPreferredLocales()
{
  return {};
}

void Host_PPCSymbolsChanged()
{
}

void Host_RefreshDSPDebuggerWindow()
{
}

bool Host_UIBlocksControllerState()
{
  return false;
}

void Host_Message(HostMessageID id)
{
}

void Host_UpdateTitle(const std::string& title)
{
}

void Host_UpdateDiscordClientID(const std::string& client_id)
{
}

bool Host_UpdateDiscordPresenceRaw(const std::string& details, const std::string& state,
                                   const std::string& large_image_key,
                                   const std::string& large_image_text,
                                   const std::string& small_image_key,
                                   const std::string& small_image_text,
                                   const int64_t start_timestamp, const int64_t end_timestamp,
                                   const int party_size, const int party_max)
{
  return false;
}

void Host_UpdateDisasmDialog()
{
}

void Host_UpdateMainFrame()
{
}

void Host_RequestRenderWindowSize(int width, int height)
{
}

bool Host_RendererHasFocus()
{
  return false;
}

bool Host_RendererHasFullFocus()
{
  return false;
}

bool Host_RendererIsFullscreen()
{
  return false;
}

bool Host_TASInputHasFocus()
{
  return false;
}

void Host_YieldToUI()
{
}

void Host_TitleChanged()
{
}

std::unique_ptr<GBAHostInterface> Host_CreateGBAHost(std::weak_ptr<HW::GBA::Core> core)
{
  return nullptr;
}
// End stubs to satisfy Core dependencies
