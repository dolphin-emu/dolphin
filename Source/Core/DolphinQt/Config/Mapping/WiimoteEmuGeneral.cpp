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

WiimoteEmuGeneral::WiimoteEmuGeneral(MappingWindow* window) : MappingWidget(window)
{
  CreateMainLayout();
  Connect();
}

void WiimoteEmuGeneral::CreateMainLayout()
{
  auto* layout = new QHBoxLayout{this};

  auto* const col0 = new QVBoxLayout;
  layout->addLayout(col0);
  col0->addWidget(
      CreateGroupBox(Wiimote::GetWiimoteGroup(GetPort(), WiimoteEmu::WiimoteGroup::Buttons)), 3);
  col0->addWidget(
      CreateGroupBox(Wiimote::GetWiimoteGroup(GetPort(), WiimoteEmu::WiimoteGroup::Rumble)), 1);

  auto* const col1 = new QVBoxLayout;
  layout->addLayout(col1);
  col1->addWidget(CreateGroupBox(
      tr("D-Pad"), Wiimote::GetWiimoteGroup(GetPort(), WiimoteEmu::WiimoteGroup::DPad)));
  col1->addWidget(CreateGroupBox(
      tr("Hotkeys"), Wiimote::GetWiimoteGroup(GetPort(), WiimoteEmu::WiimoteGroup::Hotkeys)));

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
  auto* const ext_layout = static_cast<QFormLayout*>(extension->layout());
  ext_layout->insertRow(0, combo_hbox);

  m_configure_ext_button = new QPushButton(tr("Configure Extension"));
  m_configure_ext_button->setDisabled(true);
  m_configure_ext_button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  ext_layout->insertRow(1, m_configure_ext_button);

  auto* const col2 = new QVBoxLayout;
  layout->addLayout(col2);
  col2->addWidget(extension, 3);

  col2->addWidget(
      CreateGroupBox(Wiimote::GetWiimoteGroup(GetPort(), WiimoteEmu::WiimoteGroup::Options)), 4);
}

void WiimoteEmuGeneral::Connect()
{
  connect(m_extension_combo, &QComboBox::currentIndexChanged, this, [this](int extension) {
    emit AttachmentChanged(extension);
    m_configure_ext_button->setEnabled(extension != WiimoteEmu::ExtensionNumber::NONE);
  });

  connect(m_extension_combo, &QComboBox::activated, this, &WiimoteEmuGeneral::OnAttachmentSelected);
  connect(this, &MappingWidget::ConfigChanged, this, &WiimoteEmuGeneral::ConfigChanged);
  connect(this, &MappingWidget::Update, this, &WiimoteEmuGeneral::Update);
  connect(m_configure_ext_button, &QPushButton::clicked, GetParent(),
          &MappingWindow::ActivateExtensionTab);
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
