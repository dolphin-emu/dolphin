// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/Config/Mapping/GCPadWiiUConfigDialog.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QLabel>
#include <QVBoxLayout>

#include "Core/ConfigManager.h"
#include "Core/HW/SI/SI_Device.h"
#include "DolphinQt/QtUtils/QueueOnObject.h"

#include "InputCommon/GCAdapter.h"

GCPadWiiUConfigDialog::GCPadWiiUConfigDialog(int port, QWidget* parent)
    : QDialog(parent), m_port{port}
{
  setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

  CreateLayout();

  LoadSettings();
  ConnectWidgets();
}

GCPadWiiUConfigDialog::~GCPadWiiUConfigDialog()
{
  GCAdapter::SetAdapterCallback(nullptr);
}

void GCPadWiiUConfigDialog::CreateLayout()
{
  setWindowTitle(tr("GameCube Adapter for Wii U at Port %1").arg(m_port + 1));

  m_layout = new QVBoxLayout();
  m_status_label = new QLabel();
  m_rumble = new QCheckBox(tr("Enable Rumble"));

  m_simulated_type_combo = new QComboBox();
  m_simulated_type_combo->addItem(tr("GameCube Controller"),
                                  SerialInterface::SIDEVICE_GC_CONTROLLER);
  m_simulated_type_combo->addItem(tr("DK Bongos"), SerialInterface::SIDEVICE_GC_TARUKONGA);
  m_simulated_type_combo->addItem(tr("Dance Mat"), SerialInterface::SIDEVICE_DANCEMAT);

  m_button_box = new QDialogButtonBox(QDialogButtonBox::Ok);

  UpdateAdapterStatus();

  auto callback = [this] { QueueOnObject(this, &GCPadWiiUConfigDialog::UpdateAdapterStatus); };
  GCAdapter::SetAdapterCallback(callback);

  m_layout->addWidget(m_status_label);
  m_layout->addWidget(m_rumble);
  m_layout->addWidget(m_simulated_type_combo);
  m_layout->addWidget(m_button_box);

  setLayout(m_layout);
}

void GCPadWiiUConfigDialog::ConnectWidgets()
{
  connect(m_rumble, &QCheckBox::toggled, this, &GCPadWiiUConfigDialog::SaveSettings);
  connect(m_simulated_type_combo, qOverload<int>(&QComboBox::currentIndexChanged), this,
          &GCPadWiiUConfigDialog::SaveSettings);
  connect(m_button_box, &QDialogButtonBox::accepted, this, &GCPadWiiUConfigDialog::accept);
}

void GCPadWiiUConfigDialog::UpdateAdapterStatus()
{
  const char* error_message = nullptr;
  const bool detected = GCAdapter::IsDetected(&error_message);
  QString status_text;

  if (detected)
  {
    status_text = tr("Adapter Detected");
  }
  else if (error_message)
  {
    status_text = tr("Error Opening Adapter: %1").arg(QString::fromUtf8(error_message));
  }
  else
  {
    status_text = tr("No Adapter Detected");
  }

  m_status_label->setText(status_text);

  m_rumble->setEnabled(detected);
  m_simulated_type_combo->setEnabled(detected);
}

void GCPadWiiUConfigDialog::LoadSettings()
{
  m_rumble->setChecked(SConfig::GetInstance().m_AdapterRumble[m_port]);
  const int index = m_simulated_type_combo->findData(
      static_cast<int>(SConfig::GetInstance().m_AdapterSimulatedType[m_port]));
  m_simulated_type_combo->setCurrentIndex(index);
}

void GCPadWiiUConfigDialog::SaveSettings()
{
  SConfig::GetInstance().m_AdapterRumble[m_port] = m_rumble->isChecked();
  SConfig::GetInstance().m_AdapterSimulatedType[m_port] =
      static_cast<SerialInterface::SIDevices>(m_simulated_type_combo->currentData().toInt());
}
