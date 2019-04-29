// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinQt/Config/Mapping/MappingNumeric.h"

#include "DolphinQt/Config/Mapping/MappingWidget.h"

#include "InputCommon/ControllerEmu/ControllerEmu.h"
#include "InputCommon/ControllerInterface/ControllerInterface.h"

MappingDouble::MappingDouble(MappingWidget* parent, ControllerEmu::NumericSetting<double>* setting)
    : QDoubleSpinBox(parent), m_setting(*setting)
{
  setRange(m_setting.GetMinValue(), m_setting.GetMaxValue());
  setDecimals(2);

  if (const auto ui_suffix = m_setting.GetUISuffix())
    setSuffix(QStringLiteral(" ") + tr(ui_suffix));

  if (const auto ui_description = m_setting.GetUIDescription())
    setToolTip(tr(ui_description));

  connect(this, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
          [this, parent](double value) {
            m_setting.SetValue(value);
            parent->SaveSettings();
          });

  connect(parent, &MappingWidget::ConfigChanged, this, &MappingDouble::ConfigChanged);
}

// Overriding QDoubleSpinBox's fixup to set the default value when input is cleared.
void MappingDouble::fixup(QString& input) const
{
  input = QString::number(m_setting.GetDefaultValue());
}

void MappingDouble::ConfigChanged()
{
  const QSignalBlocker blocker(this);
  setValue(m_setting.GetValue());
}

MappingBool::MappingBool(MappingWidget* parent, ControllerEmu::NumericSetting<bool>* setting)
    : QCheckBox(parent), m_setting(*setting)
{
  connect(this, &QCheckBox::stateChanged, this, [this, parent](int value) {
    m_setting.SetValue(value != 0);
    parent->SaveSettings();
  });

  connect(parent, &MappingWidget::ConfigChanged, this, &MappingBool::ConfigChanged);
}

void MappingBool::ConfigChanged()
{
  const QSignalBlocker blocker(this);
  setChecked(m_setting.GetValue());
}
