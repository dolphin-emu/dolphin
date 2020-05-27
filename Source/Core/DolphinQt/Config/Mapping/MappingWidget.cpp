// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinQt/Config/Mapping/MappingWidget.h"

#include <QCheckBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>

#include "DolphinQt/Config/Mapping/IOWindow.h"
#include "DolphinQt/Config/Mapping/MappingButton.h"
#include "DolphinQt/Config/Mapping/MappingIndicator.h"
#include "DolphinQt/Config/Mapping/MappingNumeric.h"
#include "DolphinQt/Config/Mapping/MappingWindow.h"

#include "InputCommon/ControlReference/ControlReference.h"
#include "InputCommon/ControllerEmu/Control/Control.h"
#include "InputCommon/ControllerEmu/ControlGroup/ControlGroup.h"
#include "InputCommon/ControllerEmu/ControlGroup/MixedTriggers.h"
#include "InputCommon/ControllerEmu/ControllerEmu.h"
#include "InputCommon/ControllerEmu/Setting/NumericSetting.h"
#include "InputCommon/ControllerEmu/StickGate.h"

MappingWidget::MappingWidget(MappingWindow* parent) : m_parent(parent)
{
  connect(parent, &MappingWindow::Update, this, &MappingWidget::Update);
  connect(parent, &MappingWindow::Save, this, &MappingWidget::SaveSettings);
  connect(parent, &MappingWindow::ConfigChanged, this, &MappingWidget::ConfigChanged);
}

MappingWindow* MappingWidget::GetParent() const
{
  return m_parent;
}

int MappingWidget::GetPort() const
{
  return m_parent->GetPort();
}

QGroupBox* MappingWidget::CreateGroupBox(ControllerEmu::ControlGroup* group)
{
  return CreateGroupBox(tr(group->ui_name.c_str()), group);
}

QGroupBox* MappingWidget::CreateGroupBox(const QString& name, ControllerEmu::ControlGroup* group)
{
  QGroupBox* group_box = new QGroupBox(name);
  QFormLayout* form_layout = new QFormLayout();

  group_box->setLayout(form_layout);

  MappingIndicator* indicator = nullptr;

  switch (group->type)
  {
  case ControllerEmu::GroupType::Shake:
    indicator = new ShakeMappingIndicator(*static_cast<ControllerEmu::Shake*>(group));
    break;

  case ControllerEmu::GroupType::MixedTriggers:
    indicator = new MixedTriggersIndicator(*static_cast<ControllerEmu::MixedTriggers*>(group));
    break;

  case ControllerEmu::GroupType::Tilt:
    indicator = new TiltIndicator(*static_cast<ControllerEmu::Tilt*>(group));
    break;

  case ControllerEmu::GroupType::Cursor:
    indicator = new CursorIndicator(*static_cast<ControllerEmu::Cursor*>(group));
    break;

  case ControllerEmu::GroupType::Force:
    indicator = new SwingIndicator(*static_cast<ControllerEmu::Force*>(group));
    break;

  case ControllerEmu::GroupType::IMUAccelerometer:
    indicator =
        new AccelerometerMappingIndicator(*static_cast<ControllerEmu::IMUAccelerometer*>(group));
    break;

  case ControllerEmu::GroupType::IMUGyroscope:
    indicator = new GyroMappingIndicator(*static_cast<ControllerEmu::IMUGyroscope*>(group));
    break;

  case ControllerEmu::GroupType::Stick:
    indicator = new AnalogStickIndicator(*static_cast<ControllerEmu::ReshapableInput*>(group));
    break;

  default:
    break;
  }

  if (indicator)
  {
    const auto indicator_layout = new QBoxLayout(QBoxLayout::Direction::Down);
    indicator_layout->addWidget(indicator);
    indicator_layout->setAlignment(Qt::AlignCenter);
    form_layout->addRow(indicator_layout);

    connect(this, &MappingWidget::Update, indicator, qOverload<>(&MappingIndicator::update));

    const bool need_calibration = group->type == ControllerEmu::GroupType::Cursor ||
                                  group->type == ControllerEmu::GroupType::Stick ||
                                  group->type == ControllerEmu::GroupType::Tilt ||
                                  group->type == ControllerEmu::GroupType::Force;

    if (need_calibration)
    {
      const auto calibrate =
          new CalibrationWidget(*static_cast<ControllerEmu::ReshapableInput*>(group),
                                *static_cast<ReshapableInputIndicator*>(indicator));

      form_layout->addRow(calibrate);
    }
  }

  for (auto& control : group->controls)
  {
    auto* button = new MappingButton(this, control->control_ref.get(), !indicator);

    button->setMinimumWidth(100);
    button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    const bool translate = control->translate == ControllerEmu::Translate;
    const QString translated_name =
        translate ? tr(control->ui_name.c_str()) : QString::fromStdString(control->ui_name);
    form_layout->addRow(translated_name, button);
  }

  for (auto& setting : group->numeric_settings)
  {
    QWidget* setting_widget = nullptr;

    switch (setting->GetType())
    {
    case ControllerEmu::SettingType::Double:
      setting_widget = new MappingDouble(
          this, static_cast<ControllerEmu::NumericSetting<double>*>(setting.get()));
      break;

    case ControllerEmu::SettingType::Bool:
      setting_widget =
          new MappingBool(this, static_cast<ControllerEmu::NumericSetting<bool>*>(setting.get()));
      break;

    default:
      // FYI: Widgets for additional types can be implemented as needed.
      break;
    }

    if (setting_widget)
    {
      const auto hbox = new QHBoxLayout;

      hbox->addWidget(setting_widget);
      hbox->addWidget(CreateSettingAdvancedMappingButton(*setting));

      form_layout->addRow(tr(setting->GetUIName()), hbox);
    }
  }

  if (group->default_value != ControllerEmu::ControlGroup::DefaultValue::AlwaysEnabled)
  {
    QLabel* group_enable_label = new QLabel(tr("Enable"));
    QCheckBox* group_enable_checkbox = new QCheckBox();
    group_enable_checkbox->setChecked(group->enabled);
    form_layout->insertRow(0, group_enable_label, group_enable_checkbox);
    auto enable_group_by_checkbox = [group, form_layout, group_enable_label,
                                     group_enable_checkbox] {
      group->enabled = group_enable_checkbox->isChecked();
      for (int i = 0; i < form_layout->count(); ++i)
      {
        QWidget* widget = form_layout->itemAt(i)->widget();
        if (widget != nullptr && widget != group_enable_label && widget != group_enable_checkbox)
          widget->setEnabled(group->enabled);
      }
    };
    enable_group_by_checkbox();
    connect(group_enable_checkbox, &QCheckBox::toggled, this, enable_group_by_checkbox);
    connect(this, &MappingWidget::ConfigChanged, this,
            [group_enable_checkbox, group] { group_enable_checkbox->setChecked(group->enabled); });
  }

  return group_box;
}

ControllerEmu::EmulatedController* MappingWidget::GetController() const
{
  return m_parent->GetController();
}

QPushButton*
MappingWidget::CreateSettingAdvancedMappingButton(ControllerEmu::NumericSettingBase& setting)
{
  const auto button = new QPushButton(tr("..."));
  button->setFixedWidth(QFontMetrics(font()).boundingRect(button->text()).width() * 2);

  button->connect(button, &QPushButton::clicked, [this, &setting]() {
    if (setting.IsSimpleValue())
      setting.SetExpressionFromValue();

    IOWindow io(this, GetController(), &setting.GetInputReference(), IOWindow::Type::Input);
    io.exec();

    setting.SimplifyIfPossible();

    ConfigChanged();
    SaveSettings();
  });

  return button;
}
