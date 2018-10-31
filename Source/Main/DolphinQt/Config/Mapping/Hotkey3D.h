// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "DolphinQt/Config/Mapping/MappingWidget.h"

class QHBoxLayout;

class Hotkey3D final : public MappingWidget
{
  Q_OBJECT
public:
  explicit Hotkey3D(MappingWindow* window);

  InputConfig* GetConfig() override;

private:
  void LoadSettings() override;
  void SaveSettings() override;
  void CreateMainLayout();

  // Main
  QHBoxLayout* m_main_layout;
};
