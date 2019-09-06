// Copyright 2019 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinQt/Config/Mapping/WiimoteEmuMotionControlIMU.h"

#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QString>
#include <QVBoxLayout>

#include "Core/HW/Wiimote.h"
#include "Core/HW/WiimoteEmu/WiimoteEmu.h"

#include "InputCommon/InputConfig.h"

WiimoteEmuMotionControlIMU::WiimoteEmuMotionControlIMU(MappingWindow* window)
    : MappingWidget(window)
{
  CreateMainLayout();
}

QGroupBox* WiimoteEmuMotionControlIMU::AddWarning(QGroupBox* groupbox)
{
  QFormLayout* layout = static_cast<QFormLayout*>(groupbox->layout());
  QLabel* label;

  label = new QLabel(QLatin1String(""));
  layout->addRow(label);

  label = new QLabel(tr("WARNING"));
  label->setStyleSheet(QLatin1String("QLabel { color : red; }"));
  layout->addRow(label);

  label = new QLabel(
      tr("These controls are not intended for mapping regular buttons, triggers or axes."));
  label->setWordWrap(true);
  layout->addRow(label);

  return groupbox;
}

void WiimoteEmuMotionControlIMU::CreateMainLayout()
{
  m_main_layout = new QHBoxLayout();

  m_main_layout->addWidget(
      CreateGroupBox(Wiimote::GetWiimoteGroup(GetPort(), WiimoteEmu::WiimoteGroup::IMUPoint)));
  m_main_layout->addWidget(AddWarning(CreateGroupBox(
      Wiimote::GetWiimoteGroup(GetPort(), WiimoteEmu::WiimoteGroup::IMUAccelerometer))));
  m_main_layout->addWidget(AddWarning(
      CreateGroupBox(Wiimote::GetWiimoteGroup(GetPort(), WiimoteEmu::WiimoteGroup::IMUGyroscope))));

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
