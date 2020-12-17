// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinQt/Config/Mapping/PrimeHackEmuWii.h"

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

class PrimeHackModes;

PrimeHackEmuWii::PrimeHackEmuWii(MappingWindow* window)
    : MappingWidget(window)
{
  CreateMainLayout();
  Connect(window);
  LoadSettings();
}

void PrimeHackEmuWii::CreateMainLayout()
{
  auto* layout = new QGridLayout;

  auto* groupbox1 = new QVBoxLayout();
  auto* groupbox2 = new QVBoxLayout();

  auto* modes_group = Wiimote::GetWiimoteGroup(GetPort(), WiimoteEmu::WiimoteGroup::Modes);
  auto* modes = CreateGroupBox(tr("Mode"), modes_group);

  auto* ce_modes = static_cast<ControllerEmu::PrimeHackModes*>(modes_group);

  const auto combo_hbox = new QHBoxLayout;
  combo_hbox->setAlignment(Qt::AlignCenter);
  combo_hbox->setSpacing(10);

  combo_hbox->addWidget(new QLabel(tr("Mouse")));
  combo_hbox->addWidget(m_radio_button = new QRadioButton());
  combo_hbox->addSpacing(95);
  combo_hbox->addWidget(new QLabel(tr("Controller")));
  combo_hbox->addWidget(m_radio_controller = new QRadioButton());

  modes->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  static_cast<QFormLayout*>(modes->layout())->insertRow(0, combo_hbox);

  groupbox1->addWidget(modes, 0);

  auto* misc_box = CreateGroupBox(tr("Miscellaneous"),
    Wiimote::GetWiimoteGroup(GetPort(), WiimoteEmu::WiimoteGroup::Misc));

  misc_box->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  groupbox1->addWidget(misc_box, 0);

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
  controller_box->setEnabled(ce_modes->GetSelectedDevice() == 1);

  groupbox2->addWidget(controller_box, 2, Qt::AlignTop);

  layout->addLayout(groupbox2, 0, 1, Qt::AlignTop);

  setLayout(layout);
}

void PrimeHackEmuWii::Connect(MappingWindow* window)
{
  connect(window, &MappingWindow::ConfigChanged, this, &PrimeHackEmuWii::ConfigChanged);
  connect(window, &MappingWindow::Update, this, &PrimeHackEmuWii::Update);
  connect(m_radio_button, &QRadioButton::toggled, this,
    &PrimeHackEmuWii::OnDeviceSelected);
  connect(m_radio_controller, &QRadioButton::toggled, this,
    &PrimeHackEmuWii::OnDeviceSelected);
}

void PrimeHackEmuWii::OnDeviceSelected()
{
  auto* ce_modes = static_cast<ControllerEmu::PrimeHackModes*>(
    Wiimote::GetWiimoteGroup(GetPort(), WiimoteEmu::WiimoteGroup::Modes));

  ce_modes->SetSelectedDevice(m_radio_button->isChecked() ? 0 : 1);
  controller_box->setEnabled(!m_radio_button->isChecked());

  ConfigChanged();
  SaveSettings();
}


void PrimeHackEmuWii::ConfigChanged()
{
  Update();

  prime::UpdateHackSettings();
}

void PrimeHackEmuWii::Update()
{
  bool checked = Wiimote::PrimeUseController();

  controller_box->setEnabled(checked);

  if (m_radio_controller->isChecked() != checked) {
    m_radio_controller->setChecked(checked);
    m_radio_button->setChecked(!checked);
  }
}

void PrimeHackEmuWii::LoadSettings()
{
  Wiimote::LoadConfig(); // No need to update hack settings since it's already in LoadConfig.

  auto* modes = static_cast<ControllerEmu::PrimeHackModes*>(
    Wiimote::GetWiimoteGroup(GetPort(), WiimoteEmu::WiimoteGroup::Modes));

  bool checked;

// Do not allow mouse mode on platforms with input APIs we do not support.
#if defined CIFACE_USE_WIN32 || defined CIFACE_USE_XLIB
  checked = modes->GetSelectedDevice() == 0;
#else
  checked = 1;
  m_radio_button->setEnabled(false);
  m_radio_controller->setEnabled(false);
#endif

  m_radio_button->setChecked(checked);
  m_radio_controller->setChecked(!checked);
  controller_box->setEnabled(!checked);
}

void PrimeHackEmuWii::SaveSettings()
{
  Wiimote::GetConfig()->SaveConfig();

  prime::UpdateHackSettings();
}

InputConfig* PrimeHackEmuWii::GetConfig()
{
  return Wiimote::GetConfig();
}
