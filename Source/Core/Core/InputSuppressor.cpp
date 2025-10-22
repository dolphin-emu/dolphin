// Copyright 2025 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/InputSuppressor.h"

#include <atomic>

namespace
{
static std::atomic_int s_suppression_count = 0;
}  // namespace

InputSuppressor::InputSuppressor()
{
  AddSuppression();
}
InputSuppressor::~InputSuppressor()
{
  RemoveSuppression();
}

bool InputSuppressor::IsSuppressed()
{
  return s_suppression_count.load(std::memory_order_relaxed) > 0;
}

void InputSuppressor::AddSuppression()
{
  s_suppression_count.fetch_add(1, std::memory_order_relaxed);
}

void InputSuppressor::RemoveSuppression()
{
  s_suppression_count.fetch_sub(1, std::memory_order_relaxed);
}
