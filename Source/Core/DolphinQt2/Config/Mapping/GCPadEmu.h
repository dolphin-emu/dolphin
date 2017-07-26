// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinQt2/Config/Mapping/MappingWidget.h"

class QCheckBox;
class QFormLayout;
class QGroupBox;
class QHBoxLayout;
class QLabel;
class QVBoxLayout;

class GCPadEmu final : public MappingWidget
{
public:
  explicit GCPadEmu(MappingWindow* window);

  InputConfig* GetConfig() override;

private:
  void LoadSettings() override;
  void SaveSettings() override;
  void CreateMainLayout();

  // Main
  QHBoxLayout* m_main_layout;

  // Buttons
  QGroupBox* m_buttons_box;
  QFormLayout* m_buttons_layout;

  // Control Stick
  QGroupBox* m_controlstick_box;
  QFormLayout* m_controlstick_layout;

  // C Stick
  QGroupBox* m_cstick_box;
  QFormLayout* m_cstick_layout;

  // Triggers
  QGroupBox* m_triggers_box;
  QFormLayout* m_triggers_layout;

  // D-Pad
  QGroupBox* m_dpad_box;
  QFormLayout* m_dpad_layout;
};
