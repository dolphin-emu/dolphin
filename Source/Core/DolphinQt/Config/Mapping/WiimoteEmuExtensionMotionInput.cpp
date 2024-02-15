// Copyright 2019 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/Config/Mapping/WiimoteEmuExtensionMotionInput.h"

#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>

#include "Core/HW/Wiimote.h"
#include "Core/HW/WiimoteEmu/Extension/Nunchuk.h"
#include "Core/HW/WiimoteEmu/WiimoteEmu.h"

#include "DolphinQt/Config/ControllerInterface/ControllerInterfaceWindow.h"
#include "DolphinQt/QtUtils/SetWindowDecorations.h"

#include "InputCommon/InputConfig.h"

WiimoteEmuExtensionMotionInput::WiimoteEmuExtensionMotionInput(MappingWindow* window)
    : MappingWidget(window)
{
  CreateNunchukLayout();
  CreateMainLayout();
}

void WiimoteEmuExtensionMotionInput::CreateNunchukLayout()
{
  auto* layout = new QGridLayout();
  m_nunchuk_box = new QGroupBox(tr("Nunchuk"), this);

  auto* warning_layout = new QHBoxLayout();
  auto* warning_label = new QLabel(
      tr("WARNING: These controls are designed to interface directly with motion "
         "sensor hardware. They are not intended for mapping traditional buttons, triggers or "
         "axes. You might need to configure alternate input sources before using these controls."));
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
  layout->addLayout(warning_layout, 0, 0, 1, -1);

  layout->addWidget(CreateGroupBox(tr("Accelerometer"),
                                   Wiimote::GetNunchukGroup(
                                       GetPort(), WiimoteEmu::NunchukGroup::IMUAccelerometer)),
                    1, 0);

  m_nunchuk_box->setLayout(layout);
}

void WiimoteEmuExtensionMotionInput::CreateMainLayout()
{
  m_main_layout = new QHBoxLayout();

  m_main_layout->addWidget(m_nunchuk_box);

  setLayout(m_main_layout);
}

void WiimoteEmuExtensionMotionInput::LoadSettings()
{
  Wiimote::LoadConfig();
}

void WiimoteEmuExtensionMotionInput::SaveSettings()
{
  Wiimote::GetConfig()->SaveConfig();
}

InputConfig* WiimoteEmuExtensionMotionInput::GetConfig()
{
  return Wiimote::GetConfig();
}
