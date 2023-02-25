// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "DolphinQt/Config/Mapping/MappingWidget.h"

class QComboBox;
class QLabel;
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
  void Connect();

  // Index changed by code/expression.
  void OnAttachmentChanged(int index);
  // Selection chosen by user.
  void OnAttachmentSelected(int index);

  void ConfigChanged();
  void Update();

  // Extensions
  QComboBox* m_extension_combo;
  QLabel* m_extension_combo_dynamic_indicator;

  WiimoteEmuExtension* m_extension_widget;
};
