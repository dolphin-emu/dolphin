// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "DolphinQt2/Config/Mapping/MappingWidget.h"

class QComboBox;
class WiimoteEmuExtension;

class WiimoteEmuGeneral final : public MappingWidget
{
public:
  explicit WiimoteEmuGeneral(MappingWindow* window, WiimoteEmuExtension* extension);

  InputConfig* GetConfig() override;

private:
  void LoadSettings() override;
  void SaveSettings() override;
  void CreateMainLayout();
  void Connect();
  void OnAttachmentChanged(int index);

  // Main
  QHBoxLayout* m_main_layout;

  // Extensions
  QComboBox* m_extension_combo;

  WiimoteEmuExtension* m_extension_widget;
};
