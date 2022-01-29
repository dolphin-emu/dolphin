// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "DolphinQt/Config/Mapping/MappingWidget.h"

class QHBoxLayout;
class QGroupBox;
class QComboBox;
class QLabel;
class QRadioButton;
class WiimoteEmuExtension;

class WiimoteEmuMetroid final : public MappingWidget
{
  
public:
  explicit WiimoteEmuMetroid(MappingWindow* window, WiimoteEmuExtension* extension);

  InputConfig* GetConfig() override;

  QGroupBox* camera_control;
  QRadioButton* m_radio_mouse;
  QRadioButton* m_radio_controller;
  QPushButton* m_help_button;
  QComboBox* m_morphball_combobox;

private:
  void LoadSettings() override;
  void SaveSettings() override;
  void CreateMainLayout();
  void UpdateMorphProfileBackupFile();
  void PopulateMorphBallProfiles();
  void Connect();

  void OnDeviceSelected();
  void OnMorphControlSelectionChanged();
  void ConfigChanged();
  void MappingWindowProfileSave();
  void MappingWindowProfileLoad();
  void Update();

  WiimoteEmuExtension* m_extension_widget;
};
