// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinQt/Config/Mapping/WiimoteEmuMetroid.h"

#include <QComboBox>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QRadioButton>
#include <QTimer>

#include "Core/HW/Wiimote.h"
#include "Core/HW/WiimoteEmu/WiimoteEmu.h"
#include "Core/HW/WiimoteEmu/Extension/Nunchuk.h"
#include "Core/PrimeHack/HackConfig.h"

#include "DolphinQt/Config/Mapping/MappingWindow.h"
#include "DolphinQt/Config/Mapping/WiimoteEmuExtension.h"

#include "InputCommon/ControllerEmu/ControlGroup/Attachments.h"
#include "InputCommon/ControllerEmu/ControlGroup/PrimeHackModes.h"
#include "InputCommon/InputConfig.h"

#include <QDesktopServices>
#include <QUrl>

WiimoteEmuMetroid::WiimoteEmuMetroid(MappingWindow* window, WiimoteEmuExtension* extension)
    : MappingWidget(window), m_extension_widget(extension)
{
  CreateMainLayout();
  Connect();

  auto* ce_extension = static_cast<ControllerEmu::Attachments*>(
    Wiimote::GetWiimoteGroup(GetPort(), WiimoteEmu::WiimoteGroup::Attachments));

  // Nunchuk
  ce_extension->SetSelectedAttachment(1);

  ConfigChanged();
  SaveSettings();
}

void WiimoteEmuMetroid::CreateMainLayout()
{
  auto* layout = new QHBoxLayout;

  // Column 0

  auto* groupbox0 = new QVBoxLayout();

  auto* wiimote_buttons = CreateGroupBox(tr("Wiimote Buttons"),
    Wiimote::GetWiimoteGroup(GetPort(), WiimoteEmu::WiimoteGroup::Buttons));

  wiimote_buttons->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  groupbox0->addWidget(wiimote_buttons);

  auto* nunchuk_buttons = CreateGroupBox(tr("Nunchuk Buttons"),
    Wiimote::GetNunchukGroup(GetPort(), WiimoteEmu::NunchukGroup::Buttons));

  nunchuk_buttons->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  groupbox0->addWidget(nunchuk_buttons);

  auto* wiimote_options = CreateGroupBox(tr("Wiimote Properties"),
    Wiimote::GetWiimoteGroup(GetPort(), WiimoteEmu::WiimoteGroup::Options));
  groupbox0->addWidget(wiimote_options, 1);

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

  auto* beam_box = CreateGroupBox(tr("Beams"),
    Wiimote::GetWiimoteGroup(GetPort(), WiimoteEmu::WiimoteGroup::Beams));
  groupbox1->addWidget(beam_box);

  auto* visor_box = CreateGroupBox(tr("Visors"), Wiimote::GetWiimoteGroup(
      GetPort(), WiimoteEmu::WiimoteGroup::Visors));
  groupbox1->addWidget(visor_box);


  auto* rumble_box = CreateGroupBox(tr("Rumble"), Wiimote::GetWiimoteGroup(
    GetPort(), WiimoteEmu::WiimoteGroup::Rumble));
  groupbox1->addWidget(rumble_box, 1);

  // Column 2

  auto* groupbox2 = new QVBoxLayout();

  auto* misc_box = CreateGroupBox(tr("Motion Interactions"),
    Wiimote::GetWiimoteGroup(GetPort(), WiimoteEmu::WiimoteGroup::Misc));
  groupbox2->addWidget(misc_box, 0, Qt::AlignTop);

  auto* movement_stick = CreateGroupBox(tr("Movement Stick"), Wiimote::GetNunchukGroup(
    GetPort(), WiimoteEmu::NunchukGroup::Stick));

  movement_stick->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
  groupbox2->addWidget(movement_stick, 1);

  // Column 3

  auto* groupbox3 = new QVBoxLayout();

  auto* modes_group = Wiimote::GetWiimoteGroup(GetPort(), WiimoteEmu::WiimoteGroup::Modes);
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
      Wiimote::GetWiimoteGroup(GetPort(), WiimoteEmu::WiimoteGroup::Camera));
  groupbox3->addWidget(camera_options, 0, Qt::AlignTop);

  camera_control = CreateGroupBox(tr("Camera Control"), Wiimote::GetWiimoteGroup(
    GetPort(), WiimoteEmu::WiimoteGroup::ControlStick));
  camera_control->setEnabled(ce_modes->GetSelectedDevice() == 1);
  groupbox3->addWidget(camera_control, 1);

  layout->addLayout(groupbox0);
  layout->addLayout(groupbox1);
  layout->addLayout(groupbox2);
  layout->addLayout(groupbox3);

  setLayout(layout);
}

void WiimoteEmuMetroid::Connect()
{
  connect(this, &MappingWidget::ConfigChanged, this, &WiimoteEmuMetroid::ConfigChanged);
  connect(this, &MappingWidget::Update, this, &WiimoteEmuMetroid::Update);
  connect(m_radio_mouse, &QRadioButton::toggled, this,
    &WiimoteEmuMetroid::OnDeviceSelected);
  connect(m_radio_controller, &QRadioButton::toggled, this,
    &WiimoteEmuMetroid::OnDeviceSelected);
}

void WiimoteEmuMetroid::OnDeviceSelected()
{
  auto* ce_modes = static_cast<ControllerEmu::PrimeHackModes*>(
    Wiimote::GetWiimoteGroup(GetPort(), WiimoteEmu::WiimoteGroup::Modes));

  ce_modes->SetSelectedDevice(m_radio_mouse->isChecked() ? 0 : 1);
  camera_control->setEnabled(!m_radio_mouse->isChecked());

  ConfigChanged();
  SaveSettings();
}

void WiimoteEmuMetroid::ConfigChanged()
{
  return; // All this does is update the extension UI, which we do not have.
}

void WiimoteEmuMetroid::Update()
{
  bool checked = Wiimote::PrimeUseController();

  camera_control->setEnabled(checked);

  if (m_radio_controller->isChecked() != checked) {
    m_radio_controller->setChecked(checked);
    m_radio_mouse->setChecked(!checked);
  }
}

void WiimoteEmuMetroid::LoadSettings()
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
  m_radio_mouse->setEnabled(false);
  m_radio_controller->setEnabled(false);
#endif

  m_radio_mouse->setChecked(checked);
  m_radio_controller->setChecked(!checked);
  camera_control->setEnabled(!checked);
}

void WiimoteEmuMetroid::SaveSettings()
{
  Wiimote::GetConfig()->SaveConfig();

  prime::UpdateHackSettings();
}

InputConfig* WiimoteEmuMetroid::GetConfig()
{
  return Wiimote::GetConfig();
}
