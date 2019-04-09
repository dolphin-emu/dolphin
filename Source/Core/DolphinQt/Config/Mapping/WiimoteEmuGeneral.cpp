// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinQt/Config/Mapping/WiimoteEmuGeneral.h"

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

WiimoteEmuGeneral::WiimoteEmuGeneral(MappingWindow* window, WiimoteEmuExtension* extension)
    : MappingWidget(window), m_extension_widget(extension)
{
  CreateMainLayout();
  Connect(window);
}

void WiimoteEmuGeneral::CreateMainLayout()
{
  auto* layout = new QGridLayout;

  layout->addWidget(
      CreateGroupBox(tr("Buttons"),
                     Wiimote::GetWiimoteGroup(GetPort(), WiimoteEmu::WiimoteGroup::Buttons)),
      0, 0, -1, 1);
  layout->addWidget(CreateGroupBox(tr("D-Pad"), Wiimote::GetWiimoteGroup(
                                                    GetPort(), WiimoteEmu::WiimoteGroup::DPad)),
                    0, 1, -1, 1);
  layout->addWidget(
      CreateGroupBox(tr("Hotkeys"),
                     Wiimote::GetWiimoteGroup(GetPort(), WiimoteEmu::WiimoteGroup::Hotkeys)),
      0, 2, -1, 1);

  auto* extension_group =
      Wiimote::GetWiimoteGroup(GetPort(), WiimoteEmu::WiimoteGroup::Attachments);
  auto* extension = CreateGroupBox(tr("Extension"), extension_group);
  auto* ce_extension = static_cast<ControllerEmu::Attachments*>(extension_group);
  m_extension_combo = new QComboBox();

  for (const auto& attachment : ce_extension->GetAttachmentList())
  {
    // TODO: Figure out how to localize this
    m_extension_combo->addItem(QString::fromStdString(attachment->GetName()));
  }

  extension->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

  static_cast<QFormLayout*>(extension->layout())->addRow(m_extension_combo);

  layout->addWidget(extension, 0, 3);
  layout->addWidget(CreateGroupBox(tr("Rumble"), Wiimote::GetWiimoteGroup(
                                                     GetPort(), WiimoteEmu::WiimoteGroup::Rumble)),
                    1, 3);

  layout->addWidget(
      CreateGroupBox(tr("Options"),
                     Wiimote::GetWiimoteGroup(GetPort(), WiimoteEmu::WiimoteGroup::Options)),
      2, 3);

  setLayout(layout);
}

void WiimoteEmuGeneral::Connect(MappingWindow* window)
{
  connect(m_extension_combo, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
          this, &WiimoteEmuGeneral::OnAttachmentChanged);
  connect(window, &MappingWindow::Update, this, &WiimoteEmuGeneral::Update);
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

  auto* ce_extension = static_cast<ControllerEmu::Attachments*>(
      Wiimote::GetWiimoteGroup(GetPort(), WiimoteEmu::WiimoteGroup::Attachments));
  ce_extension->SetSelectedAttachment(extension);
  SaveSettings();
}

void WiimoteEmuGeneral::Update()
{
  auto* ce_extension = static_cast<ControllerEmu::Attachments*>(
      Wiimote::GetWiimoteGroup(GetPort(), WiimoteEmu::WiimoteGroup::Attachments));

  m_extension_combo->setCurrentIndex(ce_extension->GetSelectedAttachment());
}

void WiimoteEmuGeneral::LoadSettings()
{
  Update();
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
