// Copyright 2025 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "DolphinQt/Config/Mapping/MappingWidget.h"

class BalanceBoardGeneral final : public MappingWidget
{
  Q_OBJECT
public:
  explicit BalanceBoardGeneral(MappingWindow* window);

  InputConfig* GetConfig() override;
  void LoadSettings() override;
  void SaveSettings() override;
};
