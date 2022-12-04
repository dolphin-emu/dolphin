// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "DolphinQt/Config/Mapping/MappingWidget.h"

class GCKeyboardEmu final : public MappingWidget
{
  Q_OBJECT
public:
  explicit GCKeyboardEmu(MappingWindow* window);

  InputConfig* GetConfig() override;

private:
  void LoadSettings() override;
  void SaveSettings() override;
  void CreateMainLayout();
};
