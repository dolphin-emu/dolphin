// Copyright 2025 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

class InputSuppressor final
{
public:
  InputSuppressor();
  ~InputSuppressor();

  InputSuppressor(const InputSuppressor&) = delete;
  InputSuppressor(InputSuppressor&&) = delete;
  InputSuppressor& operator=(const InputSuppressor&) = delete;
  InputSuppressor& operator=(InputSuppressor&&) = delete;

  static bool IsSuppressed();
  static void AddSuppression();
  static void RemoveSuppression();
};
