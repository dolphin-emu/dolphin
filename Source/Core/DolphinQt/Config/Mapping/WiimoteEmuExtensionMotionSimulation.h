// Copyright 2019 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "DolphinQt/Config/Mapping/MappingWidget.h"

#include "Core/HW/WiimoteEmu/ExtensionPort.h"

class QGroupBox;
class QHBoxLayout;

class WiimoteEmuExtensionMotionSimulation final : public MappingWidget
{
  Q_OBJECT
public:
  explicit WiimoteEmuExtensionMotionSimulation(MappingWindow* window);

  InputConfig* GetConfig() override;

private:
  void LoadSettings() override;
  void SaveSettings() override;

  void CreateNunchukLayout();
  void CreateMainLayout();

  // Main
  QHBoxLayout* m_main_layout;
  QGroupBox* m_nunchuk_box;
};
