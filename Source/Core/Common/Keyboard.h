// Copyright 2025 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>
#include <memory>

#include "Common/CommonTypes.h"

namespace Common
{
enum
{
  KBD_LAYOUT_QWERTY = 0,
  KBD_LAYOUT_AZERTY = 1
};

using HIDPressedKeys = std::array<u8, 6>;

struct HIDPressedState
{
  u8 modifiers = 0;
  HIDPressedKeys pressed_keys{};

  auto operator<=>(const HIDPressedState&) const = default;
};

class KeyboardContext
{
public:
  ~KeyboardContext();

  struct HandlerState
  {
    const void* main_handle = nullptr;
    const void* renderer_handle = nullptr;
    bool is_fullscreen = false;
    bool is_rendering_to_main = false;

    const void* GetHandle() const;
  };

  static void NotifyInit();
  static void NotifyHandlerChanged(const HandlerState& state);
  static void NotifyQuit();
  static const void* GetWindowHandle();
  static std::shared_ptr<KeyboardContext> GetInstance();

  HIDPressedState GetPressedState(int keyboard_layout) const;

#ifdef HAVE_SDL2
  static u32 s_sdl_init_event_type;
  static u32 s_sdl_update_event_type;
  static u32 s_sdl_quit_event_type;
#endif

private:
  KeyboardContext();

  void Init();
  void Quit();
  bool IsVirtualKeyPressed(int virtual_key) const;
  u8 PollHIDModifiers() const;
  HIDPressedKeys PollHIDPressedKeys(int keyboard_layout) const;

  bool m_is_ready = false;
#ifdef HAVE_SDL2
  const u8* m_keyboard_state = nullptr;
#endif
};
}  // namespace Common
