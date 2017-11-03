// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QLabel>
#include <QVBoxLayout>

#include "DolphinQt2/Config/Mapping/GCPadWiiU.h"

#include "Core/ConfigManager.h"
#include "InputCommon/GCAdapter.h"

GCPadWiiU::GCPadWiiU(int port, QWidget* parent) : QDialog(parent), m_port{port}
{
  CreateLayout();
  ConnectWidgets();

  LoadSettings();
}

void GCPadWiiU::CreateLayout()
{
  setWindowTitle(tr("GameCube Adapter for Wii U at Port %1").arg(m_port + 1));

  const bool detected = GCAdapter::IsDetected();
  m_layout = new QVBoxLayout();
  m_status_label = new QLabel(detected ? tr("Adapter Detected") : tr("No Adapter Detected"));
  m_rumble = new QCheckBox(tr("Enable Rumble"));
  m_simulate_bongos = new QCheckBox(tr("Simulate DK Bongos"));
  m_button_box = new QDialogButtonBox(QDialogButtonBox::Ok);

  m_layout->addWidget(m_status_label);
  m_layout->addWidget(m_rumble);
  m_layout->addWidget(m_simulate_bongos);
  m_layout->addWidget(m_button_box);

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
  connect(m_button_box, &QDialogButtonBox::accepted, this, &GCPadWiiU::accept);
}

void GCPadWiiU::LoadSettings()
{
  m_rumble->setChecked(SConfig::GetInstance().m_AdapterRumble[m_port]);
  m_simulate_bongos->setChecked(SConfig::GetInstance().m_AdapterKonga[m_port]);
}

void GCPadWiiU::SaveSettings()
{
  SConfig::GetInstance().m_AdapterRumble[m_port] = m_rumble->isChecked();
  SConfig::GetInstance().m_AdapterKonga[m_port] = m_simulate_bongos->isChecked();
}
