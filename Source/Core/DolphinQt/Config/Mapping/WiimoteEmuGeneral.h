// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "DolphinQt/Config/Mapping/MappingWidget.h"

class QComboBox;
class WiimoteEmuExtension;

class WiimoteEmuGeneral final : public MappingWidget
{
  Q_OBJECT
public:
  explicit WiimoteEmuGeneral(MappingWindow* window, WiimoteEmuExtension* extension);

  InputConfig* GetConfig() override;

private:
  void LoadSettings() override;
  void SaveSettings() override;
  void CreateMainLayout();
  void Connect(MappingWindow* window);
  void OnAttachmentChanged(int index);
  void ConfigChanged();

  // Extensions
  QComboBox* m_extension_combo;

  WiimoteEmuExtension* m_extension_widget;
};
