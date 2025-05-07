// Copyright 2025 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QWidget>

#include "Core/Core.h"

class QCheckBox;
class QComboBox;

class WiiSpeakWindow : public QWidget
{
  Q_OBJECT
public:
  explicit WiiSpeakWindow(QWidget* parent = nullptr);
  ~WiiSpeakWindow() override;

private:
  void CreateMainWindow();
  void OnEmulationStateChanged(Core::State state);
  void EmulateWiiSpeak(bool emulate);
  void OnInputDeviceChange();

  QCheckBox* m_checkbox_enabled;
  QComboBox* m_combobox_microphones;
};
