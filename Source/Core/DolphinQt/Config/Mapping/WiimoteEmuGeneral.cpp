// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/Config/Mapping/WiimoteEmuGeneral.h"

#include <QComboBox>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>

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
  Connect();
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

  const auto combo_hbox = new QHBoxLayout;
  combo_hbox->addWidget(m_extension_combo = new QComboBox());
  combo_hbox->addWidget(m_extension_combo_dynamic_indicator = new QLabel(QString::fromUtf8("🎮")));
  combo_hbox->addWidget(CreateSettingAdvancedMappingButton(ce_extension->GetSelectionSetting()));

  m_extension_combo_dynamic_indicator->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Ignored);

  for (const auto& attachment : ce_extension->GetAttachmentList())
    m_extension_combo->addItem(tr(attachment->GetDisplayName().c_str()));

  extension->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

  static_cast<QFormLayout*>(extension->layout())->insertRow(0, combo_hbox);

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

void WiimoteEmuGeneral::Connect()
{
  connect(m_extension_combo, &QComboBox::currentIndexChanged, this,
          &WiimoteEmuGeneral::OnAttachmentChanged);
  connect(m_extension_combo, &QComboBox::activated, this, &WiimoteEmuGeneral::OnAttachmentSelected);
  connect(this, &MappingWidget::ConfigChanged, this, &WiimoteEmuGeneral::ConfigChanged);
  connect(this, &MappingWidget::Update, this, &WiimoteEmuGeneral::Update);
}

void WiimoteEmuGeneral::OnAttachmentChanged(int extension)
{
  GetParent()->ShowExtensionMotionTabs(extension == WiimoteEmu::ExtensionNumber::NUNCHUK);

  m_extension_widget->ChangeExtensionType(extension);
}

void WiimoteEmuGeneral::OnAttachmentSelected(int extension)
{
  auto* ce_extension = static_cast<ControllerEmu::Attachments*>(
      Wiimote::GetWiimoteGroup(GetPort(), WiimoteEmu::WiimoteGroup::Attachments));

  ce_extension->SetSelectedAttachment(extension);

  ConfigChanged();
  SaveSettings();
}

void WiimoteEmuGeneral::ConfigChanged()
{
  auto* ce_extension = static_cast<ControllerEmu::Attachments*>(
      Wiimote::GetWiimoteGroup(GetPort(), WiimoteEmu::WiimoteGroup::Attachments));

  m_extension_combo->setCurrentIndex(ce_extension->GetSelectedAttachment());

  m_extension_combo_dynamic_indicator->setVisible(
      !ce_extension->GetSelectionSetting().IsSimpleValue());
}

void WiimoteEmuGeneral::Update()
{
  auto* ce_extension = static_cast<ControllerEmu::Attachments*>(
      Wiimote::GetWiimoteGroup(GetPort(), WiimoteEmu::WiimoteGroup::Attachments));

  m_extension_combo->setCurrentIndex(ce_extension->GetSelectedAttachment());
}

void WiimoteEmuGeneral::LoadSettings()
{
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
