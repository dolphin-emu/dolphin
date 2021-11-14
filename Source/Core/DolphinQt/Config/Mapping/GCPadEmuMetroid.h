// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "DolphinQt/Config/Mapping/MappingWidget.h"

class QComboBox;
class QLabel;
class QRadioButton;

class GCPadEmuMetroid final : public MappingWidget
{

public:
  explicit GCPadEmuMetroid(MappingWindow* window);

  InputConfig* GetConfig() override;

  QGroupBox* camera_control;
  QRadioButton* m_radio_mouse;
  QRadioButton* m_radio_controller;
  QPushButton* m_help_button;
private:
  void LoadSettings() override;
  void SaveSettings() override;
  void CreateMainLayout();
  void Connect();

  void OnDeviceSelected();
  void ConfigChanged();
  void Update();
};
