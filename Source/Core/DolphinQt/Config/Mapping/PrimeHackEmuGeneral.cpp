// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinQt/Config/Mapping/PrimeHackEmuGeneral.h"

#include <QComboBox>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>

#include "Core/HW/Wiimote.h"
#include "Core/HW/WiimoteEmu/WiimoteEmu.h"

#include "DolphinQt/Config/Mapping/MappingWindow.h"
#include "DolphinQt/Config/Mapping/WiimoteEmuExtension.h"

#include "InputCommon/ControllerEmu/ControlGroup/Attachments.h"
#include "InputCommon/InputConfig.h"

#include "Core/PrimeHack/HackConfig.h"

PrimeHackEmuGeneral::PrimeHackEmuGeneral(MappingWindow* window, WiimoteEmuExtension* extension)
    : MappingWidget(window), m_extension_widget(extension)
{
  CreateMainLayout();
  Connect(window);
}

void PrimeHackEmuGeneral::CreateMainLayout()
{
  auto* layout = new QGridLayout;

  layout->addWidget(
    CreateGroupBox(tr("Beams"),
                     Wiimote::GetWiimoteGroup(GetPort(), WiimoteEmu::WiimoteGroup::Beams)),
      0, 0, -1, 1);

  layout->addWidget(
    CreateGroupBox(tr("Visors"), Wiimote::GetWiimoteGroup(
                                                     GetPort(), WiimoteEmu::WiimoteGroup::Visors)),
      0, 1, -1, 1);

  layout->addWidget(
     CreateGroupBox(tr("Camera"),
                     Wiimote::GetWiimoteGroup(GetPort(), WiimoteEmu::WiimoteGroup::Camera)),
      0, 2, 1, 1);

  layout->addWidget(
     CreateGroupBox(tr("Misc."), Wiimote::GetWiimoteGroup(
                                                     GetPort(), WiimoteEmu::WiimoteGroup::Misc)),
      1, 2, 1, 1);

  setLayout(layout);
}

void PrimeHackEmuGeneral::Connect(MappingWindow* window)
{
  connect(window, &MappingWindow::ConfigChanged, this, &PrimeHackEmuGeneral::ConfigChanged);
}

void PrimeHackEmuGeneral::OnAttachmentChanged(int extension)
{
  SaveSettings();
}

void PrimeHackEmuGeneral::ConfigChanged()
{
  prime::UpdateHackSettings();
}

void PrimeHackEmuGeneral::LoadSettings()
{
  Wiimote::LoadConfig(); // No need to update hack settings since it's already in LoadConfig.
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
