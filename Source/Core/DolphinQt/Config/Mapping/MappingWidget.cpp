// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/Config/Mapping/MappingWidget.h"

#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QObject>
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
  QPushButton* advanced_button = nullptr;

  QDialog* advanced_dialogue = new QDialog{this};
  QPushButton* reset_to_defaults = new QPushButton{tr("Reset All")};

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
    advanced_button = new QPushButton{tr("Advanced")};
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
    CreateControl(control.get(), form_layout, !indicator);

  QFormLayout* advanced_layout = new QFormLayout();

  for (auto& setting : group->numeric_settings)
  {
    QWidget* setting_widget = nullptr;

    std::map<std::string, std::vector<QWidget*>>::iterator settings_widgets_iterator =
        m_settings_widgets.end();
    switch (setting->GetType())
    {
    case ControllerEmu::SettingType::Double:
      if (setting->IsAdvanced())
      {
        setting_widget = new MappingDouble(
            advanced_dialogue, static_cast<ControllerEmu::NumericSetting<double>*>(setting.get()),
            false);
      }
      else
      {
        setting_widget = new MappingDouble(
            this, static_cast<ControllerEmu::NumericSetting<double>*>(setting.get()), true);
      }
      if (indicator)
      {
        connect(static_cast<MappingDouble*>(setting_widget),
                qOverload<double>(&QDoubleSpinBox::valueChanged), indicator,
                qOverload<>(&MappingIndicator::update));
      }
      settings_widgets_iterator = m_settings_widgets.find("double");
      if (!(m_settings_widgets.end() == settings_widgets_iterator))
        settings_widgets_iterator->second.push_back(setting_widget);
      else
        m_settings_widgets.insert({"double", std::vector<QWidget*>{setting_widget}});
      break;

    case ControllerEmu::SettingType::Bool:

      if (setting->IsAdvanced())
      {
        setting_widget = new MappingBool(
            this, static_cast<ControllerEmu::NumericSetting<bool>*>(setting.get()), false);
      }
      else
      {
        setting_widget = new MappingBool(
            this, static_cast<ControllerEmu::NumericSetting<bool>*>(setting.get()), true);
      }
      if (indicator)
      {
        connect(static_cast<MappingBool*>(setting_widget), &QCheckBox::stateChanged, indicator,
                qOverload<>(&MappingIndicator::update));
      }
      settings_widgets_iterator = m_settings_widgets.find("bool");
      if (!(m_settings_widgets.end() == settings_widgets_iterator))
        settings_widgets_iterator->second.push_back(setting_widget);
      else
        m_settings_widgets.insert({"bool", std::vector<QWidget*>{setting_widget}});
      break;

    default:
      // FYI: Widgets for additional types can be implemented as needed.
      break;
    }

    if (setting_widget)
    {
      if (advanced_button && setting->IsAdvanced())
      {
        QHBoxLayout* setting_layout = new QHBoxLayout();

        setting_layout->addWidget(setting_widget);
        setting_layout->addWidget(CreateSettingAdvancedMappingButton(*setting));

        advanced_layout->addRow(tr(setting->GetUIName()), setting_layout);
      }
      else
      {
        const auto hbox = new QHBoxLayout;

        hbox->addWidget(setting_widget);
        hbox->addWidget(CreateSettingAdvancedMappingButton(*setting));

        form_layout->addRow(tr(setting->GetUIName()), hbox);
      }
    }
  }

  if (advanced_button)
  {
    form_layout->setAlignment(Qt::AlignHCenter);
    form_layout->addWidget(advanced_button);

    QGroupBox* advanced_group_box = new QGroupBox(tr("Advanced"));
    connect(
        advanced_button, &QPushButton::clicked,
        [this, group, advanced_group_box, advanced_layout, advanced_dialogue, reset_to_defaults] {
          setWindowTitle(tr("Advanced Settings"));
          setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

          QVBoxLayout* primary_advanced_layout = new QVBoxLayout{};
          primary_advanced_layout->addWidget(advanced_group_box);

          connect(reset_to_defaults, &QPushButton::clicked, [this, group] {
            for (auto& setting : group->numeric_settings)
            {
              if (setting->IsAdvanced())
              {
                setting->ResetToDefaultValue();

                auto settings_widgets_iterator = m_settings_widgets.end();
                settings_widgets_iterator = m_settings_widgets.find("double");
                if (!(m_settings_widgets.end() == settings_widgets_iterator))
                {
                  for (auto& qwidget : settings_widgets_iterator->second)
                  {
                    static_cast<MappingDouble*>(qwidget)->ConfigChanged();
                  }
                }
                settings_widgets_iterator = m_settings_widgets.find("bool");
                if (!(m_settings_widgets.end() == settings_widgets_iterator))
                {
                  for (auto& qwidget : settings_widgets_iterator->second)
                  {
                    static_cast<MappingBool*>(qwidget)->ConfigChanged();
                  }
                }
              }
            }
          });
          advanced_layout->addRow(reset_to_defaults);

          advanced_group_box->setLayout(advanced_layout);

          advanced_dialogue->setLayout(primary_advanced_layout);
          advanced_dialogue->setMinimumSize(QSize{200, 100});
          advanced_dialogue->show();
          advanced_dialogue->raise();
          advanced_dialogue->activateWindow();
        });
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

QGroupBox* MappingWidget::CreateControlsBox(const QString& name, ControllerEmu::ControlGroup* group,
                                            int columns)
{
  auto* group_box = new QGroupBox(name);
  auto* hbox_layout = new QHBoxLayout();

  group_box->setLayout(hbox_layout);

  std::vector<QFormLayout*> form_layouts;
  for (int i = 0; i < columns; ++i)
  {
    form_layouts.push_back(new QFormLayout());
    hbox_layout->addLayout(form_layouts[i]);
  }

  for (size_t i = 0; i < group->controls.size(); ++i)
  {
    CreateControl(group->controls[i].get(), form_layouts[i % columns], true);
  }

  return group_box;
}

void MappingWidget::CreateControl(const ControllerEmu::Control* control, QFormLayout* layout,
                                  bool indicator)
{
  auto* button = new MappingButton(this, control->control_ref.get(), indicator);

  button->setMinimumWidth(100);
  button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  const bool translate = control->translate == ControllerEmu::Translate;
  const QString translated_name =
      translate ? tr(control->ui_name.c_str()) : QString::fromStdString(control->ui_name);
  layout->addRow(translated_name, button);
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
