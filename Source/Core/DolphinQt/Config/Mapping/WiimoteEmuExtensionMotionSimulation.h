// Copyright 2019 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "DolphinQt/Config/Mapping/MappingWidget.h"

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
