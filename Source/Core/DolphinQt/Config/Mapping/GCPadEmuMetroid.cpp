// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinQt/Config/Mapping/GCPadEmuMetroid.h"

#include <QComboBox>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QRadioButton>
#include <QTimer>

#include "Core/HW/GCPad.h"
#include "Core/HW/GCPadEmu.h"
#include "Core/PrimeHack/HackConfig.h"

#include "DolphinQt/Config/Mapping/MappingWindow.h"

#include "InputCommon/ControllerEmu/ControlGroup/Attachments.h"
#include "InputCommon/ControllerEmu/ControlGroup/PrimeHackModes.h"
#include "InputCommon/InputConfig.h"

#include <QDesktopServices>
#include <QUrl>

GCPadEmuMetroid::GCPadEmuMetroid(MappingWindow* window)
  : MappingWidget(window)
{
  CreateMainLayout();
  Connect();

  ConfigChanged();
  SaveSettings();
}

void GCPadEmuMetroid::CreateMainLayout()
{
  auto* layout = new QHBoxLayout;

  // Column 0

  auto* groupbox0 = new QVBoxLayout();

  auto* gamecube_buttons = CreateGroupBox(tr("GameCube Buttons"),
    Pad::GetGroup(GetPort(), PadGroup::Buttons));

  gamecube_buttons->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  groupbox0->addWidget(gamecube_buttons);

  auto* trigger_buttons = CreateGroupBox(tr("GameCube Triggers"),
    Pad::GetGroup(GetPort(), PadGroup::Triggers));

  trigger_buttons->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  groupbox0->addWidget(trigger_buttons);

  auto* gamecube_options = CreateGroupBox(tr("GameCube Controller Properties"),
    Pad::GetGroup(GetPort(), PadGroup::Options));

  gamecube_options->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  groupbox0->addWidget(gamecube_options, 1);

  QGroupBox* help_box = new QGroupBox(tr("Help"));
  const auto help_hbox = new QHBoxLayout;

  m_help_button = new QPushButton();
  m_help_button->setText(tr("Open Wiki Page"));
  connect(m_help_button, &QPushButton::clicked, this, []() {
    QDesktopServices::openUrl(QUrl(QStringLiteral("https://github.com/shiiion/dolphin/wiki/Installation#changing-primehack-settings")));
    });

  help_hbox->addWidget(m_help_button);
  help_box->setLayout(help_hbox);

  help_box->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  groupbox0->addWidget(help_box);

  // Column 1

  auto* groupbox1 = new QVBoxLayout();

  auto* beam_box = CreateGroupBox(tr("Beams (C-Stick)"),
    Pad::GetGroup(GetPort(), PadGroup::CStick));
  groupbox1->addWidget(beam_box);

  auto* visor_box = CreateGroupBox(tr("Visors (D-Pad)"), Pad::GetGroup(
    GetPort(), PadGroup::DPad));
  groupbox1->addWidget(visor_box);


  // Column 2

  auto* groupbox2 = new QVBoxLayout();

  auto* movement_stick = CreateGroupBox(tr("Movement Stick"), Pad::GetGroup(
    GetPort(), PadGroup::MainStick));

  movement_stick->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
  groupbox2->addWidget(movement_stick, 0);

  auto* rumble_box = CreateGroupBox(tr("Rumble"), Pad::GetGroup(
    GetPort(), PadGroup::Rumble));
  groupbox2->addWidget(rumble_box, 1);

  // Column 3

  auto* groupbox3 = new QVBoxLayout();

  auto* modes_group = Pad::GetGroup(GetPort(), PadGroup::Modes);
  auto* modes = CreateGroupBox(tr("Mode"), modes_group);

  auto* ce_modes = static_cast<ControllerEmu::PrimeHackModes*>(modes_group);

  const auto combo_hbox = new QHBoxLayout;
  combo_hbox->setAlignment(Qt::AlignCenter);
  combo_hbox->setSpacing(10);

  combo_hbox->addWidget(new QLabel(tr("Mouse")));
  combo_hbox->addWidget(m_radio_mouse = new QRadioButton());
  combo_hbox->addSpacing(65);
  combo_hbox->addWidget(new QLabel(tr("Controller")));
  combo_hbox->addWidget(m_radio_controller = new QRadioButton());

  modes->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  static_cast<QFormLayout*>(modes->layout())->insertRow(0, combo_hbox);
  groupbox3->addWidget(modes, 0);

  auto* camera_options =
    CreateGroupBox(tr("Camera"),
      Pad::GetGroup(GetPort(), PadGroup::Camera));
  groupbox3->addWidget(camera_options, 0, Qt::AlignTop);

  camera_control = CreateGroupBox(tr("Camera Control"), Pad::GetGroup(
    GetPort(), PadGroup::ControlStick));
  camera_control->setEnabled(ce_modes->GetSelectedDevice() == 1);
  groupbox3->addWidget(camera_control, 1);

  layout->addLayout(groupbox0);
  layout->addLayout(groupbox1);
  layout->addLayout(groupbox2);
  layout->addLayout(groupbox3);

  setLayout(layout);
}

void GCPadEmuMetroid::Connect()
{
  connect(this, &MappingWidget::ConfigChanged, this, &GCPadEmuMetroid::ConfigChanged);
  connect(this, &MappingWidget::Update, this, &GCPadEmuMetroid::Update);
  connect(m_radio_mouse, &QRadioButton::toggled, this,
    &GCPadEmuMetroid::OnDeviceSelected);
  connect(m_radio_controller, &QRadioButton::toggled, this,
    &GCPadEmuMetroid::OnDeviceSelected);
}

void GCPadEmuMetroid::OnDeviceSelected()
{
  auto* ce_modes = static_cast<ControllerEmu::PrimeHackModes*>(
    Pad::GetGroup(GetPort(), PadGroup::Modes));

  ce_modes->SetSelectedDevice(m_radio_mouse->isChecked() ? 0 : 1);
  camera_control->setEnabled(!m_radio_mouse->isChecked());

  ConfigChanged();
  SaveSettings();
}

void GCPadEmuMetroid::ConfigChanged()
{
  return; // All this does is update the extension UI, which we do not have.
}

void GCPadEmuMetroid::Update()
{
  bool checked = Pad::PrimeUseController();

  camera_control->setEnabled(checked);

  if (m_radio_controller->isChecked() != checked) {
    m_radio_controller->setChecked(checked);
    m_radio_mouse->setChecked(!checked);
  }
}

void GCPadEmuMetroid::LoadSettings()
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
    m_radio_mouse->setEnabled(false);
    m_radio_controller->setEnabled(false);
  }

  m_radio_mouse->setChecked(checked);
  m_radio_controller->setChecked(!checked);
  camera_control->setEnabled(!checked);
}

void GCPadEmuMetroid::SaveSettings()
{
  Pad::GetConfig()->SaveConfig();

  prime::UpdateHackSettings();
}

InputConfig* GCPadEmuMetroid::GetConfig()
{
  return Pad::GetConfig();
}
