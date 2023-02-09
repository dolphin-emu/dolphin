// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "Common/EnumMap.h"

// The main point of this is to allow other parts of dolphin to set ImGui's key map without
// having to import ImGui headers.
// But the idea is that it can be expanded in the future with more keys to support more things.
enum class DolphinKey
{
  Tab,
  LeftArrow,
  RightArrow,
  UpArrow,
  DownArrow,
  PageUp,
  PageDown,
  Home,
  End,
  Insert,
  Delete,
  Backspace,
  Space,
  Enter,
  Escape,
  KeyPadEnter,
  A,  // for text edit CTRL+A: select all
  C,  // for text edit CTRL+C: copy
  V,  // for text edit CTRL+V: paste
  X,  // for text edit CTRL+X: cut
  Y,  // for text edit CTRL+Y: redo
  Z,  // for text edit CTRL+Z: undo
};

using DolphinKeyMap = Common::EnumMap<int, DolphinKey::Z>;
