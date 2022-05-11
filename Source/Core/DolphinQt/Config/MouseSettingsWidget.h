// Copyright 20122 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <QGroupBox>

class QCheckBox;
class QDoubleSpinBox;
class QHBoxLayout;
class QLabel;

class MouseSettingsWidget final : public QGroupBox
{
  Q_OBJECT
public:
  MouseSettingsWidget(QWidget* parent);

private:
  void CreateLayout();
  void ConnectWidgets();

  void LoadSettings();
  void SaveSettings();

  QHBoxLayout* CreateSensitivityLayout();
  QHBoxLayout* CreateSnappingDistanceLayout();
  QHBoxLayout* CreateOctagonalMouseJailIsEnabledLayout();

  QDoubleSpinBox* m_sensitivity_control = nullptr;
  QDoubleSpinBox* m_snapping_distance_control = nullptr;
  QLabel* m_snapping_distance_label = nullptr;
  QCheckBox* m_octagonal_mouse_jail_is_enabled_control = nullptr;
};
