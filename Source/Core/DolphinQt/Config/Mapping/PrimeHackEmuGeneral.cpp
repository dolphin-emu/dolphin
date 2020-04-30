// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinQt/Config/Mapping/PrimeHackEmuGeneral.h"

#include <QComboBox>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QRadioButton>
#include <QLabel>

#include "Core/HW/Wiimote.h"
#include "Core/HW/WiimoteEmu/WiimoteEmu.h"

#include "DolphinQt/Config/Mapping/MappingWindow.h"
#include "InputCommon/InputConfig.h"

#include "Core/PrimeHack/HackConfig.h"

class PrimeHackMisc;

PrimeHackEmuGeneral::PrimeHackEmuGeneral(MappingWindow* window)
    : MappingWidget(window)
{
  CreateMainLayout();
  Connect(window);
  LoadSettings();
}

void PrimeHackEmuGeneral::CreateMainLayout()
{
  auto* layout = new QGridLayout;

  auto* groupbox1 = new QVBoxLayout();
  auto* groupbox2 = new QVBoxLayout();

  auto* misc_group = Wiimote::GetWiimoteGroup(GetPort(), WiimoteEmu::WiimoteGroup::Misc);
  auto* misc = CreateGroupBox(tr("Miscellaneous"), misc_group);

  auto* ce_misc = static_cast<ControllerEmu::PrimeHackMisc*>(misc_group);

  const auto combo_hbox = new QHBoxLayout;
  combo_hbox->setAlignment(Qt::AlignCenter);
  combo_hbox->setSpacing(10);

  combo_hbox->addWidget(new QLabel(tr("Mouse")));
  combo_hbox->addWidget(m_radio_button = new QRadioButton());
  combo_hbox->addSpacing(95);
  combo_hbox->addWidget(new QLabel(tr("Controller")));
  combo_hbox->addWidget(m_radio_controller = new QRadioButton());

  misc->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  static_cast<QFormLayout*>(misc->layout())->insertRow(0, combo_hbox);
  static_cast<QFormLayout*>(misc->layout())->setVerticalSpacing(20);

  groupbox1->addWidget(misc, 0, Qt::AlignTop);

  auto* beam_box = CreateGroupBox(tr("Beams"),
    Wiimote::GetWiimoteGroup(GetPort(), WiimoteEmu::WiimoteGroup::Beams));

  beam_box->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

  groupbox1->addWidget(beam_box, 1, Qt::AlignTop);

  auto* visor_box = 
    CreateGroupBox(tr("Visors"), Wiimote::GetWiimoteGroup(
      GetPort(), WiimoteEmu::WiimoteGroup::Visors));

  visor_box->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

  groupbox1->addWidget(visor_box, 2, Qt::AlignTop);

  layout->addLayout(groupbox1, 0, 0);

  auto* camera = 
    CreateGroupBox(tr("Camera"),
      Wiimote::GetWiimoteGroup(GetPort(), WiimoteEmu::WiimoteGroup::Camera));

  groupbox2->addWidget(camera, 0, Qt::AlignTop);

  controller_box = CreateGroupBox(tr("Camera Control"), Wiimote::GetWiimoteGroup(
    GetPort(), WiimoteEmu::WiimoteGroup::ControlStick));

  controller_box->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

  controller_box->setEnabled(ce_misc->GetSelectedDevice() == 1);

  groupbox2->addWidget(controller_box, 2, Qt::AlignTop);

  layout->addLayout(groupbox2, 0, 1, Qt::AlignTop);

  setLayout(layout);
}

void PrimeHackEmuGeneral::Connect(MappingWindow* window)
{
  connect(window, &MappingWindow::ConfigChanged, this, &PrimeHackEmuGeneral::ConfigChanged);
  connect(window, &MappingWindow::Update, this, &PrimeHackEmuGeneral::Update);
  connect(m_radio_button, &QRadioButton::toggled, this,
    &PrimeHackEmuGeneral::OnDeviceSelected);
  connect(m_radio_controller, &QRadioButton::toggled, this,
    &PrimeHackEmuGeneral::OnDeviceSelected);
}

void PrimeHackEmuGeneral::OnDeviceSelected()
{
  auto* ce_misc = static_cast<ControllerEmu::PrimeHackMisc*>(
    Wiimote::GetWiimoteGroup(GetPort(), WiimoteEmu::WiimoteGroup::Misc));

  ce_misc->SetSelectedDevice(m_radio_button->isChecked() ? 0 : 1);
  controller_box->setEnabled(!m_radio_button->isChecked());

  ConfigChanged();
  SaveSettings();
}


void PrimeHackEmuGeneral::ConfigChanged()
{
  Update();

  prime::UpdateHackSettings();
}

void PrimeHackEmuGeneral::Update()
{
  auto* misc = static_cast<ControllerEmu::PrimeHackMisc*>(
    Wiimote::GetWiimoteGroup(GetPort(), WiimoteEmu::WiimoteGroup::Misc));

  controller_box->setEnabled(misc->GetSelectedDevice() == 1);
}

void PrimeHackEmuGeneral::LoadSettings()
{
  Wiimote::LoadConfig(); // No need to update hack settings since it's already in LoadConfig.

  auto* misc = static_cast<ControllerEmu::PrimeHackMisc*>(
    Wiimote::GetWiimoteGroup(GetPort(), WiimoteEmu::WiimoteGroup::Misc));

  bool checked = misc->GetSelectedDevice() == 0;
  m_radio_button->setChecked(checked);
  m_radio_controller->setChecked(!checked);
  controller_box->setEnabled(!checked);
}

void PrimeHackEmuGeneral::SaveSettings()
{
  Wiimote::GetConfig()->SaveConfig();

  prime::UpdateHackSettings();
}

InputConfig* PrimeHackEmuGeneral::GetConfig()
{
  return Wiimote::GetConfig();
}
