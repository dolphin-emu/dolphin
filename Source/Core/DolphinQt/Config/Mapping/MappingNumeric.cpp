// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/Config/Mapping/MappingNumeric.h"

#include <limits>

#include "DolphinQt/Config/Mapping/MappingWidget.h"

#include "InputCommon/ControllerEmu/ControllerEmu.h"
#include "InputCommon/ControllerInterface/ControllerInterface.h"

MappingDouble::MappingDouble(MappingWidget* parent, ControllerEmu::NumericSetting<double>* setting)
    : QDoubleSpinBox(parent), m_setting(*setting)
{
  setDecimals(2);

  if (const auto ui_description = m_setting.GetUIDescription())
    setToolTip(tr(ui_description));

  connect(this, &QDoubleSpinBox::valueChanged, this, [this, parent](double value) {
    m_setting.SetValue(value);
    ConfigChanged();
    parent->SaveSettings();
  });

  connect(parent, &MappingWidget::ConfigChanged, this, &MappingDouble::ConfigChanged);
  connect(parent, &MappingWidget::Update, this, &MappingDouble::Update);
}

// Overriding QDoubleSpinBox's fixup to set the default value when input is cleared.
void MappingDouble::fixup(QString& input) const
{
  input = QString::number(m_setting.GetDefaultValue());
}

void MappingDouble::ConfigChanged()
{
  const QSignalBlocker blocker(this);

  QString suffix;

  if (const auto ui_suffix = m_setting.GetUISuffix())
    suffix += QLatin1Char{' '} + tr(ui_suffix);

  if (m_setting.IsSimpleValue())
  {
    setRange(m_setting.GetMinValue(), m_setting.GetMaxValue());
    setButtonSymbols(ButtonSymbols::UpDownArrows);
  }
  else
  {
    constexpr auto inf = std::numeric_limits<double>::infinity();
    setRange(-inf, inf);
    setButtonSymbols(ButtonSymbols::NoButtons);
    suffix += QString::fromUtf8(" 🎮");
  }

  setSuffix(suffix);

  setValue(m_setting.GetValue());
}

void MappingDouble::Update()
{
  if (m_setting.IsSimpleValue() || hasFocus())
    return;

  const QSignalBlocker blocker(this);
  setValue(m_setting.GetValue());
}

MappingBool::MappingBool(MappingWidget* parent, ControllerEmu::NumericSetting<bool>* setting)
    : QCheckBox(parent), m_setting(*setting)
{
  if (const auto ui_description = m_setting.GetUIDescription())
    setToolTip(tr(ui_description));

  connect(this, &QCheckBox::stateChanged, this, [this, parent](int value) {
    m_setting.SetValue(value != 0);
    ConfigChanged();
    parent->SaveSettings();
  });

  connect(parent, &MappingWidget::ConfigChanged, this, &MappingBool::ConfigChanged);
  connect(parent, &MappingWidget::Update, this, &MappingBool::Update);

  setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Ignored);
}

void MappingBool::ConfigChanged()
{
  const QSignalBlocker blocker(this);

  if (m_setting.IsSimpleValue())
    setText({});
  else
    setText(QString::fromUtf8("🎮"));

  setChecked(m_setting.GetValue());
}

void MappingBool::Update()
{
  if (m_setting.IsSimpleValue())
    return;

  const QSignalBlocker blocker(this);
  setChecked(m_setting.GetValue());
}
