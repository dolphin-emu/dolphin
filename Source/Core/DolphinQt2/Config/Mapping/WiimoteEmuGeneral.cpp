// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <QComboBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QVBoxLayout>

#include "DolphinQt2/Config/Mapping/WiimoteEmuGeneral.h"

#include "Core/HW/Wiimote.h"
#include "Core/HW/WiimoteEmu/WiimoteEmu.h"
#include "DolphinQt2/Config/Mapping/WiimoteEmuExtension.h"
#include "InputCommon/ControllerEmu/ControlGroup/Extension.h"
#include "InputCommon/InputConfig.h"

WiimoteEmuGeneral::WiimoteEmuGeneral(MappingWindow* window, WiimoteEmuExtension* extension)
    : MappingWidget(window), m_extension_widget(extension)
{
  CreateMainLayout();
  Connect();
}

void WiimoteEmuGeneral::CreateMainLayout()
{
  m_main_layout = new QHBoxLayout();

  auto* vbox_layout = new QVBoxLayout();

  m_main_layout->addWidget(CreateGroupBox(
      tr("Buttons"), Wiimote::GetWiimoteGroup(GetPort(), WiimoteEmu::WiimoteGroup::Buttons)));
  m_main_layout->addWidget(CreateGroupBox(
      tr("D-Pad"), Wiimote::GetWiimoteGroup(GetPort(), WiimoteEmu::WiimoteGroup::DPad)));
  m_main_layout->addWidget(CreateGroupBox(
      tr("Hotkeys"), Wiimote::GetWiimoteGroup(GetPort(), WiimoteEmu::WiimoteGroup::Hotkeys)));

  auto* extension_group = Wiimote::GetWiimoteGroup(GetPort(), WiimoteEmu::WiimoteGroup::Extension);
  auto* extension = CreateGroupBox(tr("Extension"), extension_group);
  auto* ce_extension = static_cast<ControllerEmu::Extension*>(extension_group);
  m_extension_combo = new QComboBox();

  for (const auto& attachment : ce_extension->attachments)
  {
    // TODO: Figure out how to localize this
    m_extension_combo->addItem(QString::fromStdString(attachment->GetName()));
  }

  extension->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

  static_cast<QFormLayout*>(extension->layout())->addRow(m_extension_combo);

  vbox_layout->addWidget(extension);
  vbox_layout->addWidget(CreateGroupBox(
      tr("Rumble"), Wiimote::GetWiimoteGroup(GetPort(), WiimoteEmu::WiimoteGroup::Rumble)));
  vbox_layout->addWidget(CreateGroupBox(
      tr("Options"), Wiimote::GetWiimoteGroup(GetPort(), WiimoteEmu::WiimoteGroup::Options)));

  m_main_layout->addItem(vbox_layout);

  setLayout(m_main_layout);
}

void WiimoteEmuGeneral::Connect()
{
  connect(m_extension_combo, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
          this, &WiimoteEmuGeneral::OnAttachmentChanged);
}

void WiimoteEmuGeneral::OnAttachmentChanged(int extension)
{
  const QString value = m_extension_combo->currentText();

  static const QMap<QString, WiimoteEmuExtension::Type> value_map = {
      {QStringLiteral("None"), WiimoteEmuExtension::Type::NONE},
      {QStringLiteral("Classic"), WiimoteEmuExtension::Type::CLASSIC_CONTROLLER},
      {QStringLiteral("Drums"), WiimoteEmuExtension::Type::DRUMS},
      {QStringLiteral("Guitar"), WiimoteEmuExtension::Type::GUITAR},
      {QStringLiteral("Nunchuk"), WiimoteEmuExtension::Type::NUNCHUK},
      {QStringLiteral("Turntable"), WiimoteEmuExtension::Type::TURNTABLE}};

  m_extension_widget->ChangeExtensionType(value_map[value]);

  auto* ce_extension = static_cast<ControllerEmu::Extension*>(
      Wiimote::GetWiimoteGroup(GetPort(), WiimoteEmu::WiimoteGroup::Extension));
  ce_extension->switch_extension = extension;
  SaveSettings();
}

void WiimoteEmuGeneral::LoadSettings()
{
  auto* ce_extension = static_cast<ControllerEmu::Extension*>(
      Wiimote::GetWiimoteGroup(GetPort(), WiimoteEmu::WiimoteGroup::Extension));

  m_extension_combo->setCurrentIndex(ce_extension->switch_extension);
  OnAttachmentChanged(ce_extension->switch_extension);

  Wiimote::LoadConfig();
}

void WiimoteEmuGeneral::SaveSettings()
{
  Wiimote::GetConfig()->SaveConfig();
}

InputConfig* WiimoteEmuGeneral::GetConfig()
{
  return Wiimote::GetConfig();
}
