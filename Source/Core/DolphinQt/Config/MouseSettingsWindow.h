// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QDialog>

class GraphicsBool;
class GraphicsDouble;
class GraphicsInteger;
class QLabel;
class QWidget;

class MouseSettingsWindow final : public QDialog
{
  Q_OBJECT
public:
  explicit MouseSettingsWindow(QWidget* parent);

private:
  void Update();

  GraphicsDouble* m_spinbox_horizontal_scale;
  GraphicsDouble* m_spinbox_vertical_scale;
  GraphicsBool* m_checkbox_recenter_on_boot;
  GraphicsBool* m_checkbox_mouse_gate_enabled;
  QLabel* m_label_radius;
  QLabel* m_label_radius_unit;
  GraphicsInteger* m_spinbox_radius;
  QLabel* m_label_snapping_distance;
  QLabel* m_label_snapping_distance_unit;
  GraphicsDouble* m_spinbox_snapping_distance;
};
