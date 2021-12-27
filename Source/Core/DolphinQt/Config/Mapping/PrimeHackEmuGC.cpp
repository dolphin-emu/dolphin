// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinQt/Config/Mapping/PrimeHackEmuGC.h"

#include <QComboBox>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QRadioButton>
#include <QLabel>

#include "DolphinQt/Config/Mapping/MappingWindow.h"
#include "InputCommon/InputConfig.h"

#include "Core/PrimeHack/HackConfig.h"
#include "Core/HW/GCPad.h"
#include "Core/HW/GCPadEmu.h"

class PrimeHackModes;

PrimeHackEmuGC::PrimeHackEmuGC(MappingWindow* window)
    : MappingWidget(window)
{
  CreateMainLayout();
  Connect(window);
  LoadSettings();
}

void PrimeHackEmuGC::CreateMainLayout()
{
  auto* layout = new QGridLayout;

  auto* groupbox1 = new QVBoxLayout();
  auto* groupbox2 = new QVBoxLayout();

  auto* modes_group = Pad::GetGroup(GetPort(), PadGroup::Modes);
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

  groupbox1->addWidget(modes);

  auto* camera = 
    CreateGroupBox(tr("Camera"),
      Pad::GetGroup(GetPort(), PadGroup::Camera));

  camera->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  groupbox1->addWidget(camera, 0, Qt::AlignTop);

  // May be used later.
  //auto* misc_box = CreateGroupBox(tr("Miscellaneous"),
  //  Pad::GetGroup(GetPort(), PadGroup::Misc));

  //misc_box->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  ////groupbox1->addWidget(misc_box, 0, Qt::AlignTop);

  layout->addLayout(groupbox1, 0, 0);

  controller_box = CreateGroupBox(tr("Camera Control"), Pad::GetGroup(
    GetPort(), PadGroup::ControlStick));

  controller_box->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
  controller_box->setEnabled(ce_modes->GetSelectedDevice() == 1);

  groupbox2->addWidget(controller_box, 2, Qt::AlignTop);

  layout->addLayout(groupbox2, 0, 1, Qt::AlignTop);

  setLayout(layout);
}

void PrimeHackEmuGC::Connect(MappingWindow* window)
{
  connect(window, &MappingWindow::ConfigChanged, this, &PrimeHackEmuGC::ConfigChanged);
  connect(window, &MappingWindow::Update, this, &PrimeHackEmuGC::Update);
  connect(m_radio_button, &QRadioButton::toggled, this,
    &PrimeHackEmuGC::OnDeviceSelected);
  connect(m_radio_controller, &QRadioButton::toggled, this,
    &PrimeHackEmuGC::OnDeviceSelected);
}

void PrimeHackEmuGC::OnDeviceSelected()
{
  auto* ce_modes = static_cast<ControllerEmu::PrimeHackModes*>(
    Pad::GetGroup(GetPort(), PadGroup::Modes));

  ce_modes->SetSelectedDevice(m_radio_button->isChecked() ? 0 : 1);
  controller_box->setEnabled(!m_radio_button->isChecked());

  ConfigChanged();
  SaveSettings();
}


void PrimeHackEmuGC::ConfigChanged()
{
  Update();

  prime::UpdateHackSettings();
}

void PrimeHackEmuGC::Update()
{
  bool checked = Pad::PrimeUseController();

  controller_box->setEnabled(checked);

  if (m_radio_controller->isChecked() != checked) {
    m_radio_controller->setChecked(checked);
    m_radio_button->setChecked(!checked);
  }
}

void PrimeHackEmuGC::LoadSettings()
{
  Pad::LoadConfig(); // No need to update hack settings since it's already in LoadConfig.

  auto* modes = static_cast<ControllerEmu::PrimeHackModes*>(
    Pad::GetGroup(GetPort(), PadGroup::Modes));

  bool checked;

  // Do not allow mouse mode on platforms with input APIs we do not support.
  if (modes->GetMouseSupported()) {
    checked = modes->GetSelectedDevice() == 0;
  }
  else {
    checked = 1;
    m_radio_button->setEnabled(false);
    m_radio_controller->setEnabled(false);
  }

  m_radio_button->setChecked(checked);
  m_radio_controller->setChecked(!checked);
  controller_box->setEnabled(!checked);
}

void PrimeHackEmuGC::SaveSettings()
{
  Pad::GetConfig()->SaveConfig();

  prime::UpdateHackSettings();
}

InputConfig* PrimeHackEmuGC::GetConfig()
{
  return Pad::GetConfig();
}
