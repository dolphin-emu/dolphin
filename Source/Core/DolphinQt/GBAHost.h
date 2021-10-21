// Copyright 2021 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <vector>

#include "Core/Host.h"

namespace HW::GBA
{
class Core;
}  // namespace HW::GBA

class GBAWidgetController;

class GBAHost : public GBAHostInterface
{
public:
  explicit GBAHost(std::weak_ptr<HW::GBA::Core> core);
  ~GBAHost();
  void GameChanged() override;
  void FrameEnded(const std::vector<u32>& video_buffer) override;

private:
  GBAWidgetController* m_widget_controller{};
  std::weak_ptr<HW::GBA::Core> m_core;
};
