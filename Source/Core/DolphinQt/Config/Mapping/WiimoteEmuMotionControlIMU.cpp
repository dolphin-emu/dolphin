// Copyright 2019 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/Config/Mapping/WiimoteEmuMotionControlIMU.h"

#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QString>
#include <QVBoxLayout>

#include "Core/HW/Wiimote.h"
#include "Core/HW/WiimoteEmu/WiimoteEmu.h"

#include "DolphinQt/Config/ControllerInterface/ControllerInterfaceWindow.h"
#include "DolphinQt/QtUtils/SetWindowDecorations.h"

#include "InputCommon/InputConfig.h"

WiimoteEmuMotionControlIMU::WiimoteEmuMotionControlIMU(MappingWindow* window)
    : MappingWidget(window)
{
  CreateMainLayout();
}

void WiimoteEmuMotionControlIMU::CreateMainLayout()
{
  auto* warning_layout = new QHBoxLayout();
  auto* warning_label =
      new QLabel(tr("WARNING: The controls under Accelerometer and Gyroscope are designed to "
                    "interface directly with motion sensor hardware. They are not intended for "
                    "mapping traditional buttons, triggers or axes. You might need to configure "
                    "alternate input sources before using these controls."));
  warning_label->setWordWrap(true);
  auto* warning_input_sources_button = new QPushButton(tr("Alternate Input Sources"));
  warning_layout->addWidget(warning_label, 1);
  warning_layout->addWidget(warning_input_sources_button, 0, Qt::AlignRight);
  connect(warning_input_sources_button, &QPushButton::clicked, this, [this] {
    ControllerInterfaceWindow* window = new ControllerInterfaceWindow(this);
    window->setAttribute(Qt::WA_DeleteOnClose, true);
    window->setWindowModality(Qt::WindowModality::WindowModal);
    SetQWidgetWindowDecorations(window);
    window->show();
  });

  auto* groups_layout = new QHBoxLayout();
  groups_layout->addWidget(
      CreateGroupBox(Wiimote::GetWiimoteGroup(GetPort(), WiimoteEmu::WiimoteGroup::IMUPoint)));
  groups_layout->addWidget(
      CreateGroupBox(Wiimote::GetWiimoteGroup(GetPort(), WiimoteEmu::WiimoteGroup::IRPassthrough)));
  groups_layout->addWidget(CreateGroupBox(
      Wiimote::GetWiimoteGroup(GetPort(), WiimoteEmu::WiimoteGroup::IMUAccelerometer)));
  groups_layout->addWidget(
      CreateGroupBox(Wiimote::GetWiimoteGroup(GetPort(), WiimoteEmu::WiimoteGroup::IMUGyroscope)));

  m_main_layout = new QVBoxLayout();
  m_main_layout->addLayout(warning_layout);
  m_main_layout->addLayout(groups_layout);

  setLayout(m_main_layout);
}

void WiimoteEmuMotionControlIMU::LoadSettings()
{
  Wiimote::LoadConfig();
}

void WiimoteEmuMotionControlIMU::SaveSettings()
{
  Wiimote::GetConfig()->SaveConfig();
}

InputConfig* WiimoteEmuMotionControlIMU::GetConfig()
{
  return Wiimote::GetConfig();
}
