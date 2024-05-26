// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <memory>
#include <string>
#include <vector>

#include "Common/CommonTypes.h"

// Host - defines an interface for the emulator core to communicate back to the
// OS-specific layer
//
// The emulator core is abstracted from the OS using 2 interfaces:
// Common and Host.
//
// Common simply provides OS-neutral implementations of things like threads, mutexes,
// INI file manipulation, memory mapping, etc.
//
// Host is an abstract interface for communicating things back to the host. The emulator
// core is treated as a library, not as a main program, because it is far easier to
// write GUI interfaces that control things than to squash GUI into some model that wasn't
// designed for it.
//
// The host can be just a command line app that opens a window, or a full blown debugger
// interface.

namespace HW::GBA
{
class Core;
}  // namespace HW::GBA

class GBAHostInterface
{
public:
  virtual ~GBAHostInterface() = default;
  virtual void GameChanged() = 0;
  virtual void FrameEnded(const std::vector<u32>& video_buffer) = 0;
};

enum class HostMessageID
{
  // Begin at 10 in case there is already messages with wParam = 0, 1, 2 and so on
  WMUserStop = 10,
  WMUserCreate,
  WMUserSetCursor,
  WMUserJobDispatch,
};

std::vector<std::string> Host_GetPreferredLocales();
bool Host_UIBlocksControllerState();
bool Host_RendererHasFocus();
bool Host_RendererHasFullFocus();
bool Host_RendererIsFullscreen();
bool Host_TASInputHasFocus();

void Host_Message(HostMessageID id);
void Host_PPCSymbolsChanged();
void Host_RefreshDSPDebuggerWindow();
void Host_RequestRenderWindowSize(int width, int height);
void Host_UpdateDisasmDialog();
void Host_UpdateMainFrame();
void Host_UpdateTitle(const std::string& title);
void Host_YieldToUI();
void Host_TitleChanged();

void Host_UpdateDiscordClientID(const std::string& client_id = {});
bool Host_UpdateDiscordPresenceRaw(const std::string& details = {}, const std::string& state = {},
                                   const std::string& large_image_key = {},
                                   const std::string& large_image_text = {},
                                   const std::string& small_image_key = {},
                                   const std::string& small_image_text = {},
                                   const int64_t start_timestamp = 0,
                                   const int64_t end_timestamp = 0, const int party_size = 0,
                                   const int party_max = 0);

std::unique_ptr<GBAHostInterface> Host_CreateGBAHost(std::weak_ptr<HW::GBA::Core> core);
