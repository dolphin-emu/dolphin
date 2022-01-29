// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "DolphinQt/Config/Mapping/MappingWidget.h"

class QRadioButton;
class QLabel;
class QComboBox;
class PrimeHackModes;

class PrimeHackEmuWii final : public MappingWidget
{
public:
  explicit PrimeHackEmuWii(MappingWindow* window);

  InputConfig* GetConfig() override;

  QGroupBox* controller_box;
  QRadioButton* m_radio_mouse;
  QRadioButton* m_radio_controller;
  QComboBox* m_morphball_combobox;

private:
  void LoadSettings() override;
  void SaveSettings() override;

  void CreateMainLayout();
  void Connect(MappingWindow* window);
  void OnMorphControlSelectionChanged();
  void UpdateMorphProfileBackupFile();
  void PopulateMorphBallProfiles();
  void MappingWindowProfileSave();
  void MappingWindowProfileLoad();
  void OnDeviceSelected();
  void ConfigChanged();
  void Update();
};
