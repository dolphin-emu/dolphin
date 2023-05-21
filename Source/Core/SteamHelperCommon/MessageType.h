// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include <cstdint>

namespace Steam
{
enum class MessageType : uint8_t
{
  Invalid = 0,
  InitRequest,
  InitReply,
  ShutdownRequest
};
}  // namespace Steam
