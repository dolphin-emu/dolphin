// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "DolphinQt/Config/Mapping/MappingWidget.h"

class GCPadEmu final : public MappingWidget
{
  Q_OBJECT
public:
  enum class SubType
  {
    StandardController,
    AMBaseboard,
  };

  GCPadEmu(MappingWindow* window, SubType sub_type);

  InputConfig* GetConfig() override;

private:
  void LoadSettings() override;
  void SaveSettings() override;
  void CreateMainLayout(SubType sub_type);
};

[[nodiscard]] inline auto CreateStandardControllerMappingWidget(MappingWindow* parent)
{
  return new GCPadEmu{parent, GCPadEmu::SubType::StandardController};
}

[[nodiscard]] inline auto CreateAMBaseboardMappingWidget(MappingWindow* parent)
{
  return new GCPadEmu{parent, GCPadEmu::SubType::AMBaseboard};
}
