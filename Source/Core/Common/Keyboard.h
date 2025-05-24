// Copyright 2025 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>
#include <memory>

#include "Common/CommonTypes.h"

namespace Common
{
namespace HIDUsageID
{
// See HID Usage Tables - Keyboard (0x07):
// https://usb.org/sites/default/files/hut1_21.pdf
enum
{
  A = 0x04,
  B,
  C,
  D,
  E,
  F,
  G,
  H,
  I,
  J,
  K,
  L,
  M,
  N,
  O,
  P,
  Q,
  R,
  S,
  T,
  U,
  V,
  W,
  X,
  Y,
  Z,
  COLON = 0x33,
  A_AZERTY = Q,
  M_AZERTY = COLON,
  Q_AZERTY = A,
  W_AZERTY = Z,
  Z_AZERTY = W,
  Y_QWERTZ = Z,
  Z_QWERTZ = Y,
};
}  // namespace HIDUsageID

namespace KeyboardLayout
{
enum
{
  AUTO = 0,
  QWERTY = 1,
  AZERTY = 2,
  QWERTZ = 4,
  // Translation
  QWERTY_AZERTY = QWERTY | AZERTY,
  QWERTY_QWERTZ = QWERTY | QWERTZ,
  AZERTY_QWERTZ = AZERTY | QWERTZ
};
}
using HIDPressedKeys = std::array<u8, 6>;

#pragma pack(push, 1)
struct HIDPressedState
{
  u8 modifiers = 0;
  u8 oem = 0;
  HIDPressedKeys pressed_keys{};

  auto operator<=>(const HIDPressedState&) const = default;
};
#pragma pack(pop)

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
  static void UpdateLayout();
  static const void* GetWindowHandle();
  static std::shared_ptr<KeyboardContext> GetInstance();

  HIDPressedState GetPressedState() const;

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
  HIDPressedKeys PollHIDPressedKeys() const;

  bool m_is_ready = false;
  int m_host_layout = KeyboardLayout::AUTO;
  int m_game_layout = KeyboardLayout::AUTO;
#ifdef HAVE_SDL2
  const u8* m_keyboard_state = nullptr;
#endif
};
}  // namespace Common
