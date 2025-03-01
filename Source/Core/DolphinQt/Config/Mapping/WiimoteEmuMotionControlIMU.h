// Copyright 2019 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "DolphinQt/Config/Mapping/MappingWidget.h"

class QCheckBox;
class QFormLayout;
class QGroupBox;
class QHBoxLayout;
class QLabel;
class QVBoxLayout;

class WiimoteEmuGeneral;

class WiimoteEmuMotionControlIMU final : public MappingWidget
{
  Q_OBJECT
public:
  explicit WiimoteEmuMotionControlIMU(MappingWindow* window, WiimoteEmuGeneral* wm_emu_general);

  InputConfig* GetConfig() override;

  void OnAttachmentChanged(int number);

private:
  void LoadSettings() override;
  void SaveSettings() override;
};
