// Copyright 2025 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QVBoxLayout>
#include <QWidget>

#include "Core/Config/MainSettings.h"
#include "Core/Core.h"

class ConfigBool;
class QComboBox;

class LogitechMicWindow final : public QWidget
{
  Q_OBJECT
public:
  explicit LogitechMicWindow(QWidget* parent = nullptr);

private:
  void CreateMainWindow();
  void CreateCheckboxGroup(QVBoxLayout* main_layout);
  void CreateMicrophoneConfigurationGroup(QVBoxLayout* main_layout);

  void OnEmulationStateChanged(Core::State state);
  void OnInputDeviceChange(std::size_t index);

  std::array<ConfigBool*, Config::EMULATED_LOGITECH_MIC_COUNT> m_mic_enabled_checkboxes;
  std::array<QComboBox*, 4> m_mic_device_comboboxes;
};
