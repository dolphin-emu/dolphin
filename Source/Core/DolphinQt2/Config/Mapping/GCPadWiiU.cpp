// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <QCheckBox>
#include <QLabel>
#include <QVBoxLayout>

#include "DolphinQt2/Config/Mapping/GCPadWiiU.h"

#include "Core/ConfigManager.h"
#include "InputCommon/GCAdapter.h"

GCPadWiiU::GCPadWiiU(MappingWindow* window) : MappingWidget(window)
{
  CreateLayout();
  ConnectWidgets();

  LoadSettings();
}

void GCPadWiiU::CreateLayout()
{
  const bool detected = GCAdapter::IsDetected();
  m_layout = new QVBoxLayout();
  m_status_label = new QLabel(detected ? tr("Adapter Detected") : tr("No Adapter Detected"));
  m_rumble = new QCheckBox(tr("Enable Rumble"));
  m_simulate_bongos = new QCheckBox(tr("Simulate DK Bongos"));

  m_layout->addWidget(m_status_label);
  m_layout->addWidget(m_rumble);
  m_layout->addWidget(m_simulate_bongos);

  if (!detected)
  {
    m_rumble->setEnabled(false);
    m_simulate_bongos->setEnabled(false);
  }

  setLayout(m_layout);
}

void GCPadWiiU::ConnectWidgets()
{
  connect(m_rumble, &QCheckBox::toggled, this, &GCPadWiiU::SaveSettings);
  connect(m_simulate_bongos, &QCheckBox::toggled, this, &GCPadWiiU::SaveSettings);
}

void GCPadWiiU::LoadSettings()
{
  m_rumble->setChecked(SConfig::GetInstance().m_AdapterRumble[GetPort()]);
  m_simulate_bongos->setChecked(SConfig::GetInstance().m_AdapterKonga[GetPort()]);
}

void GCPadWiiU::SaveSettings()
{
  SConfig::GetInstance().m_AdapterRumble[GetPort()] = m_rumble->isChecked();
  SConfig::GetInstance().m_AdapterKonga[GetPort()] = m_simulate_bongos->isChecked();
}

InputConfig* GCPadWiiU::GetConfig()
{
  return nullptr;
}
