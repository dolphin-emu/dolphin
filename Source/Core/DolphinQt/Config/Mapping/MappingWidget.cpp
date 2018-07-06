// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinQt/Config/Mapping/MappingWidget.h"

#include <QFormLayout>
#include <QGroupBox>
#include <QPushButton>

#include "DolphinQt/Config/Mapping/IOWindow.h"
#include "DolphinQt/Config/Mapping/MappingBool.h"
#include "DolphinQt/Config/Mapping/MappingButton.h"
#include "DolphinQt/Config/Mapping/MappingIndicator.h"
#include "DolphinQt/Config/Mapping/MappingNumeric.h"
#include "DolphinQt/Config/Mapping/MappingRadio.h"
#include "DolphinQt/Config/Mapping/MappingWindow.h"

#include "InputCommon/ControlReference/ControlReference.h"
#include "InputCommon/ControllerEmu/Control/Control.h"
#include "InputCommon/ControllerEmu/ControlGroup/ControlGroup.h"
#include "InputCommon/ControllerEmu/Setting/BooleanSetting.h"
#include "InputCommon/ControllerEmu/Setting/NumericSetting.h"

MappingWidget::MappingWidget(MappingWindow* window) : m_parent(window)
{
  connect(window, &MappingWindow::ClearFields, this, &MappingWidget::OnClearFields);
  connect(window, &MappingWindow::Update, this, &MappingWidget::Update);
  connect(window, &MappingWindow::Save, this, &MappingWidget::SaveSettings);
}

MappingWindow* MappingWidget::GetParent() const
{
  return m_parent;
}

bool MappingWidget::IsIterativeInput() const
{
  return m_parent->IsIterativeInput();
}

void MappingWidget::NextButton(MappingButton* button)
{
  auto iterator = std::find(m_buttons.begin(), m_buttons.end(), button);

  if (iterator == m_buttons.end())
    return;

  if (++iterator == m_buttons.end())
    return;

  MappingButton* next = *iterator;

  if (next->IsInput() && next->isVisible())
    next->Detect();
  else
    NextButton(next);
}

std::shared_ptr<ciface::Core::Device> MappingWidget::GetDevice() const
{
  return m_parent->GetDevice();
}

int MappingWidget::GetPort() const
{
  return m_parent->GetPort();
}

QGroupBox* MappingWidget::CreateGroupBox(const QString& name, ControllerEmu::ControlGroup* group)
{
  QGroupBox* group_box = new QGroupBox(name);
  QFormLayout* form_layout = new QFormLayout();

  group_box->setLayout(form_layout);

  bool need_indicator = group->type == ControllerEmu::GroupType::Cursor ||
                        group->type == ControllerEmu::GroupType::Stick ||
                        group->type == ControllerEmu::GroupType::Tilt ||
                        group->type == ControllerEmu::GroupType::MixedTriggers;

  for (auto& control : group->controls)
  {
    auto* button = new MappingButton(this, control->control_ref.get(), !need_indicator);

    button->setMinimumWidth(100);
    button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    const bool translate = control->translate == ControllerEmu::Translate;
    const QString translated_name =
        translate ? tr(control->ui_name.c_str()) : QString::fromStdString(control->ui_name);
    form_layout->addRow(translated_name, button);

    auto* control_ref = control->control_ref.get();

    connect(button, &MappingButton::AdvancedPressed, [this, button, control_ref] {
      if (m_parent->GetDevice() == nullptr)
        return;

      IOWindow io(this, m_parent->GetController(), control_ref,
                  control_ref->IsInput() ? IOWindow::Type::Input : IOWindow::Type::Output);
      io.exec();
      SaveSettings();
      button->Update();
    });

    m_buttons.push_back(button);
  }

  for (auto& numeric : group->numeric_settings)
  {
    auto* spinbox = new MappingNumeric(this, numeric.get());
    form_layout->addRow(tr(numeric->m_name.c_str()), spinbox);
    m_numerics.push_back(spinbox);
  }

  for (auto& boolean : group->boolean_settings)
  {
    if (!boolean->IsExclusive())
      continue;

    auto* checkbox = new MappingRadio(this, boolean.get());

    form_layout->addRow(checkbox);
    m_radio.push_back(checkbox);
  }

  for (auto& boolean : group->boolean_settings)
  {
    if (boolean->IsExclusive())
      continue;

    auto* checkbox = new MappingBool(this, boolean.get());

    form_layout->addRow(checkbox);
    m_bools.push_back(checkbox);
  }

  if (need_indicator)
    form_layout->addRow(new MappingIndicator(group));

  return group_box;
}

void MappingWidget::OnClearFields()
{
  for (auto* button : m_buttons)
    button->Clear();

  for (auto* spinbox : m_numerics)
    spinbox->Clear();

  for (auto* checkbox : m_bools)
    checkbox->Clear();

  for (auto* radio : m_radio)
    radio->Clear();
}

void MappingWidget::Update()
{
  for (auto* button : m_buttons)
    button->Update();

  for (auto* spinbox : m_numerics)
    spinbox->Update();

  for (auto* checkbox : m_bools)
    checkbox->Update();

  for (auto* radio : m_radio)
    radio->Update();

  SaveSettings();
}

ControllerEmu::EmulatedController* MappingWidget::GetController() const
{
  return m_parent->GetController();
}
