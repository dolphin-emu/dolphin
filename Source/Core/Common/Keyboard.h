// Copyright 2026 Dolphin Emulator Project
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

  static std::shared_ptr<KeyboardContext> GetInstance();

  HIDPressedState GetPressedState() const;

private:
  KeyboardContext();

  void Init();
  void Quit();
  bool IsVirtualKeyPressed(int virtual_key) const;
  u8 PollHIDModifiers() const;
  HIDPressedKeys PollHIDPressedKeys() const;
  void UpdateLayout();

  bool m_is_ready = false;
  int m_keyboard_layout = KBD_LAYOUT_QWERTY;
};
}  // namespace Common
