// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "DolphinQt/Config/Mapping/MappingWidget.h"

class QGridLayout;

class HotkeyDebugging final : public MappingWidget
{
  Q_OBJECT
public:
  explicit HotkeyDebugging(MappingWindow* window);

  InputConfig* GetConfig() override;

private:
  void LoadSettings() override;
  void SaveSettings() override;
  void CreateMainLayout();

  // Main
  QGridLayout* m_main_layout;
};
