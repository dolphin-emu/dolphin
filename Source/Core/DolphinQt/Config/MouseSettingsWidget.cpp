// Copyright 20122 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later
#include "DolphinQt/Config/MouseSettingsWidget.h"

#ifdef _WIN32
#include <Windows.h>
#endif

#include <memory>

#include <QApplication>
#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QLabel>
#include <QVBoxLayout>

#include "Core/Config/MouseSettings.h"
#include "InputCommon/ControllerInterface/OctagonalMouseJail.h"

MouseSettingsWidget::MouseSettingsWidget(QWidget* parent) : QGroupBox(tr("Mouse"), parent)
{
  CreateLayout();
  LoadSettings();
  ConnectWidgets();
}

void MouseSettingsWidget::CreateLayout()
{
  QVBoxLayout* primary_layout = new QVBoxLayout{};

  QHBoxLayout* sensitivity_layout = CreateSensitivityLayout();
  QHBoxLayout* snapping_distance_layout = CreateSnappingDistanceLayout();

  QHBoxLayout* octagonal_mouse_jail_is_enabled = CreateOctagonalMouseJailIsEnabledLayout();

  primary_layout->addLayout(sensitivity_layout);
  primary_layout->addLayout(octagonal_mouse_jail_is_enabled);
  primary_layout->addLayout(snapping_distance_layout);

  this->setLayout(primary_layout);
}

void MouseSettingsWidget::ConnectWidgets()
{
  connect(m_sensitivity_control, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
          &MouseSettingsWidget::SaveSettings);
  connect(m_snapping_distance_control, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
          &MouseSettingsWidget::SaveSettings);
  connect(m_octagonal_mouse_jail_is_enabled_control, &QCheckBox::clicked, this,
          &MouseSettingsWidget::SaveSettings);
}

void MouseSettingsWidget::LoadSettings()
{
  m_sensitivity_control->setValue(Config::Get(Config::MOUSE_SENSITIVITY));
  m_snapping_distance_control->setValue(Config::Get(Config::MOUSE_SNAPPING_DISTANCE));
  m_octagonal_mouse_jail_is_enabled_control->setChecked(
      Config::Get(Config::MOUSE_OCTAGONAL_JAIL_ENABLED));
}

void MouseSettingsWidget::SaveSettings()
{
  Config::SetBaseOrCurrent(Config::MOUSE_SENSITIVITY, m_sensitivity_control->value());
  Config::SetBaseOrCurrent(Config::MOUSE_SNAPPING_DISTANCE, m_snapping_distance_control->value());

  const bool& octagonal_jail_checkbox_state =
      m_octagonal_mouse_jail_is_enabled_control->isChecked();
  Config::SetBaseOrCurrent(Config::MOUSE_OCTAGONAL_JAIL_ENABLED, octagonal_jail_checkbox_state);
  m_snapping_distance_control->setEnabled(octagonal_jail_checkbox_state);
  m_snapping_distance_label->setEnabled(octagonal_jail_checkbox_state);

  Config::Save();
  ciface::OctagonalMouseJail::GetInstance().RefreshConfigValues();
}

QHBoxLayout* MouseSettingsWidget::CreateSensitivityLayout()
{
  QHBoxLayout* sensitivity_layout = new QHBoxLayout{};

  m_sensitivity_control = new QDoubleSpinBox{};
  m_sensitivity_control->setRange(1.00, 9999.50);
  m_sensitivity_control->setDecimals(2);
  m_sensitivity_control->setSingleStep(0.5);
  m_sensitivity_control->setWrapping(true);
  QString sensitivity_tool_tip{
      tr("Adjusts how quickly the mouse cursor moves the Cursor X/Y +/- inputs."
         "\n\n1.0(default) sensitivity maps the mouse to the full screen."
         "\n\nRecommended value: 7.5")};
  m_sensitivity_control->setToolTip(sensitivity_tool_tip);

  QLabel* sensitivity_label = new QLabel{};
  sensitivity_label->setToolTip(sensitivity_tool_tip);
  sensitivity_label->setText(tr("Sensitivity"));

  sensitivity_layout->addWidget(sensitivity_label);
  sensitivity_layout->addWidget(m_sensitivity_control);

  return sensitivity_layout;
}

QHBoxLayout* MouseSettingsWidget::CreateSnappingDistanceLayout()
{
  QHBoxLayout* snapping_distance_layout = new QHBoxLayout{};
  m_snapping_distance_control = new QDoubleSpinBox{};

  m_snapping_distance_control->setRange(0.00, 9999.50);
  m_snapping_distance_control->setDecimals(2);
  m_snapping_distance_control->setSingleStep(0.5);
  m_snapping_distance_control->setWrapping(true);
  m_snapping_distance_control->setEnabled(Config::Get(Config::MOUSE_OCTAGONAL_JAIL_ENABLED));

  QString snapping_distance_tool_tip{
      tr("Distance(in pixels) around notches where cursor snaps to notch."
         "\nRequires Mouse Notches to be enabled."
         "\n\n0.0(default) disables snapping")};
  m_snapping_distance_control->setToolTip(snapping_distance_tool_tip);

  m_snapping_distance_label = new QLabel{};
  m_snapping_distance_label->setText(tr("Snapping Distance"));
  m_snapping_distance_label->setToolTip(snapping_distance_tool_tip);
  m_snapping_distance_label->setEnabled(Config::Get(Config::MOUSE_OCTAGONAL_JAIL_ENABLED));

  snapping_distance_layout->addWidget(m_snapping_distance_label);
  snapping_distance_layout->addWidget(m_snapping_distance_control);
  return snapping_distance_layout;
}
QHBoxLayout* MouseSettingsWidget::CreateOctagonalMouseJailIsEnabledLayout()
{
  QHBoxLayout* octagonal_mouse_jail_is_enabled_layout = new QHBoxLayout{};
  m_octagonal_mouse_jail_is_enabled_control = new QCheckBox{};
  QString octagonal_mouse_jail_is_enabled_tool_tip = {
      tr("Locks the mouse cursor into a simulated octagonal gate."
         "\n(Think of the octagon that a gamecube controller stick is inside)"
         "\nLow sensitivity(under 2.0ish) may not feel like anything is happening."
         "\n\nSnapping is disabled if this is disabled."
         "\n\nLocking and snapping only occur when a game is started and focused.")};
  m_octagonal_mouse_jail_is_enabled_control->setToolTip(octagonal_mouse_jail_is_enabled_tool_tip);
  QLabel* octagonal_mouse_jail_is_enabled_text = new QLabel{};
  octagonal_mouse_jail_is_enabled_text->setText(tr("Mouse Notches"));
  octagonal_mouse_jail_is_enabled_text->setToolTip(octagonal_mouse_jail_is_enabled_tool_tip);
  octagonal_mouse_jail_is_enabled_layout->addWidget(octagonal_mouse_jail_is_enabled_text);
  octagonal_mouse_jail_is_enabled_layout->addWidget(m_octagonal_mouse_jail_is_enabled_control);
  return octagonal_mouse_jail_is_enabled_layout;
}
