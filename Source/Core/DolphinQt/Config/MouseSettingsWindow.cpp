// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/Config/MouseSettingsWindow.h"

#include <memory>

#include <QApplication>
#include <QCheckBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QGridLayout>
#include <QLabel>
#include <QSpinBox>
#include <QVBoxLayout>

#include "Core/Config/MainSettings.h"

#include "DolphinQt/Config/Graphics/GraphicsBool.h"
#include "DolphinQt/Config/Graphics/GraphicsDouble.h"
#include "DolphinQt/Config/Graphics/GraphicsInteger.h"
#include "DolphinQt/QtUtils/SignalBlocking.h"
#include "DolphinQt/Settings.h"

MouseSettingsWindow::MouseSettingsWindow(QWidget* parent) : QDialog(parent)
{
  auto* button_box = new QDialogButtonBox(QDialogButtonBox::Close);
  connect(button_box, &QDialogButtonBox::rejected, this, &QDialog::reject);

  auto* sub_layout = new QGridLayout();
  int row = 0;

  const auto scale_x_title = tr("Horizontal Sensitivity");
  const auto scale_y_title = tr("Vertical Sensitivity");
  const auto scale_tool_tip =
      tr("Adjusts the scaling factor between the mouse position within the rendering window and "
         "the Cursor input system variables. At 1.0, the edges of the window map to 1.0 on their "
         "respective half axis.");
  QLabel* scale_x_label = new QLabel(scale_x_title);
  sub_layout->addWidget(scale_x_label, row, 0);

  m_spinbox_horizontal_scale =
      new GraphicsDouble(0.0, 100.0, Config::MAIN_MOUSE_CURSOR_SCALE_HORIZONTAL, 0.05, 2);
  m_spinbox_horizontal_scale->SetTitle(scale_x_title);
  m_spinbox_horizontal_scale->SetDescription(scale_tool_tip);
  sub_layout->addWidget(m_spinbox_horizontal_scale, row, 1, 1, 2);

  ++row;

  QLabel* scale_y_label = new QLabel(scale_y_title);
  sub_layout->addWidget(scale_y_label, row, 0);

  m_spinbox_vertical_scale =
      new GraphicsDouble(0.0, 100.0, Config::MAIN_MOUSE_CURSOR_SCALE_VERTICAL, 0.05, 2);
  m_spinbox_vertical_scale->SetTitle(scale_y_title);
  m_spinbox_vertical_scale->SetDescription(scale_tool_tip);
  sub_layout->addWidget(m_spinbox_vertical_scale, row, 1, 1, 2);

  ++row;

  m_checkbox_recenter_on_boot =
      new GraphicsBool(tr("Center Mouse Cursor on Boot"), Config::MAIN_MOUSE_RECENTER_ON_BOOT);
  m_checkbox_recenter_on_boot->SetDescription(
      tr("Snaps the cursor to the center of the render window on game start. This is recommended "
         "to enable when using the mouse cursor as the control stick input, as games usually "
         "calibrate the control stick and assume the position the stick is in on boot is the "
         "center position of the stick."));
  sub_layout->addWidget(m_checkbox_recenter_on_boot, row, 0, 1, 3);

  ++row;

  m_checkbox_mouse_gate_enabled =
      new GraphicsBool(tr("Mouse Gate"), Config::MAIN_MOUSE_STICK_GATE_ENABLED);
  m_checkbox_mouse_gate_enabled->SetDescription(
      tr("Locks the mouse cursor into a simulated octagonal gate, similar to the plastic shell of "
         "an actual GameCube controller. This is intended for use in combination with mapping the "
         "Cursor input system variables to the main control stick.\n\nLocking is only active while "
         "a game is running and the render window is in focus."));
  sub_layout->addWidget(m_checkbox_mouse_gate_enabled, row, 0, 1, 3);

  ++row;

  const auto gate_radius_tooltip = tr("The radius of the simulated mouse gate. In pixels.");

  m_label_radius = new QLabel(tr("Radius"));
  sub_layout->addWidget(m_label_radius, row, 0);

  m_spinbox_radius = new GraphicsInteger(0, 50000, Config::MAIN_MOUSE_STICK_GATE_RADIUS);
  m_spinbox_radius->SetTitle(tr("Mouse Gate Radius"));
  m_spinbox_radius->SetDescription(gate_radius_tooltip);
  sub_layout->addWidget(m_spinbox_radius, row, 1);

  m_label_radius_unit = new QLabel(tr("pixels"));
  sub_layout->addWidget(m_label_radius_unit, row, 2);

  ++row;

  const QString snapping_distance_tool_tip =
      tr("Distance around each of the eight corner notches where the mouse cursor will be snapped "
         "into the corner. In pixels. Disabled if set to 0.");

  m_label_snapping_distance = new QLabel(tr("Snapping Distance"));
  sub_layout->addWidget(m_label_snapping_distance, row, 0);

  m_spinbox_snapping_distance =
      new GraphicsDouble(0.0, 10000.0, Config::MAIN_MOUSE_STICK_GATE_SNAPPING_DISTANCE, 0.5, 2);
  m_spinbox_snapping_distance->SetTitle(tr("Snapping Distance"));
  m_spinbox_snapping_distance->SetDescription(snapping_distance_tool_tip);
  sub_layout->addWidget(m_spinbox_snapping_distance, row, 1);

  m_label_snapping_distance_unit = new QLabel(tr("pixels"));
  sub_layout->addWidget(m_label_snapping_distance_unit, row, 2);

  ++row;

  auto* main_layout = new QVBoxLayout();
  main_layout->addLayout(sub_layout);
  main_layout->addWidget(button_box);
  setLayout(main_layout);

  connect(&Settings::Instance(), &Settings::ConfigChanged, this, [this] { Update(); });

  setWindowTitle(tr("Mouse"));
  setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

  Update();
}

void MouseSettingsWindow::Update()
{
  const bool gate_enabled = Config::Get(Config::MAIN_MOUSE_STICK_GATE_ENABLED);
  SignalBlocking(m_label_radius)->setEnabled(gate_enabled);
  SignalBlocking(m_spinbox_radius)->setEnabled(gate_enabled);
  SignalBlocking(m_label_radius_unit)->setEnabled(gate_enabled);
  SignalBlocking(m_label_snapping_distance)->setEnabled(gate_enabled);
  SignalBlocking(m_spinbox_snapping_distance)->setEnabled(gate_enabled);
  SignalBlocking(m_label_snapping_distance_unit)->setEnabled(gate_enabled);
}
