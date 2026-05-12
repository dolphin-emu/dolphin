// Copyright 2024 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#ifdef HAS_LIBMGBA

#include <memory>
#include <span>
#include <vector>
#include "Common/CommonTypes.h"
#include "Core/Host.h"

namespace HW::GBA
{
class Core;
}

class AndroidGBAHost final : public GBAHostInterface
{
public:
  explicit AndroidGBAHost(std::weak_ptr<HW::GBA::Core> core, int device_number);
  ~AndroidGBAHost() override;
  void GameChanged() override;
  void FrameEnded(std::span<const u32> video_buffer) override;

private:
  std::weak_ptr<HW::GBA::Core> m_core;
  int m_device_number;
};

#endif  // HAS_LIBMGBA
