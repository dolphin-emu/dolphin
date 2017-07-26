// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "DolphinQt2/Config/Mapping/MappingWidget.h"

class QCheckBox;
class QLabel;
class QVBoxLayout;

class GCPadWiiU final : public MappingWidget
{
public:
  explicit GCPadWiiU(MappingWindow* window);

  InputConfig* GetConfig() override;

private:
  void LoadSettings() override;
  void SaveSettings() override;

  void CreateLayout();
  void ConnectWidgets();

  QVBoxLayout* m_layout;
  QLabel* m_status_label;

  // Checkboxes
  QCheckBox* m_rumble;
  QCheckBox* m_simulate_bongos;
};
