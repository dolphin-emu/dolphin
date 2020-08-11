// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "DolphinQt/Config/Mapping/MappingWidget.h"
#include "InputCommon/ControllerEmu/ControlGroup/PrimeHackModes.h"

class QRadioButton;
class QLabel;
class PrimeHackModes;

class PrimeHackEmuGC final : public MappingWidget
{
public:
  explicit PrimeHackEmuGC(MappingWindow* window);

  InputConfig* GetConfig() override;

  QGroupBox* controller_box;
  QRadioButton* m_radio_button;
  QRadioButton* m_radio_controller;

private:
  void LoadSettings() override;
  void SaveSettings() override;

  void CreateMainLayout();
  void Connect(MappingWindow* window);
  void OnDeviceSelected();
  void ConfigChanged();
  void Update();
};
