// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>

#include "DolphinQt2/Config/Mapping/WiimoteEmuExtension.h"

#include "Core/HW/Wiimote.h"
#include "Core/HW/WiimoteEmu/WiimoteEmu.h"
#include "InputCommon/InputConfig.h"

WiimoteEmuExtension::WiimoteEmuExtension(MappingWindow* window) : MappingWidget(window)
{
  CreateClassicLayout();
  CreateDrumsLayout();
  CreateGuitarLayout();
  CreateNoneLayout();
  CreateNunchukLayout();
  CreateTurntableLayout();
  CreateMainLayout();

  ChangeExtensionType(Type::NONE);
}

void WiimoteEmuExtension::CreateClassicLayout()
{
  auto* hbox = new QHBoxLayout();
  m_classic_box = new QGroupBox(tr("Classic Controller"), this);

  hbox->addWidget(CreateGroupBox(
      tr("Buttons"), Wiimote::GetClassicGroup(GetPort(), WiimoteEmu::ClassicGroup::Buttons)));
  hbox->addWidget(CreateGroupBox(
      tr("Left Stick"), Wiimote::GetClassicGroup(GetPort(), WiimoteEmu::ClassicGroup::LeftStick)));
  hbox->addWidget(
      CreateGroupBox(tr("Right Stick"),
                     Wiimote::GetClassicGroup(GetPort(), WiimoteEmu::ClassicGroup::RightStick)));

  auto* vbox = new QVBoxLayout();
  vbox->addWidget(CreateGroupBox(
      tr("D-Pad"), Wiimote::GetClassicGroup(GetPort(), WiimoteEmu::ClassicGroup::DPad)));
  vbox->addWidget(CreateGroupBox(
      tr("Triggers"), Wiimote::GetClassicGroup(GetPort(), WiimoteEmu::ClassicGroup::Triggers)));
  hbox->addItem(vbox);

  m_classic_box->setLayout(hbox);
}

void WiimoteEmuExtension::CreateDrumsLayout()
{
  auto* hbox = new QHBoxLayout();
  m_drums_box = new QGroupBox(tr("Drums"), this);

  hbox->addWidget(CreateGroupBox(
      tr("Buttons"), Wiimote::GetDrumsGroup(GetPort(), WiimoteEmu::DrumsGroup::Buttons)));

  auto* vbox = new QVBoxLayout();
  vbox->addWidget(
      CreateGroupBox(tr("Pads"), Wiimote::GetDrumsGroup(GetPort(), WiimoteEmu::DrumsGroup::Pads)));
  vbox->addWidget(CreateGroupBox(tr("Stick"),
                                 Wiimote::GetDrumsGroup(GetPort(), WiimoteEmu::DrumsGroup::Stick)));
  hbox->addItem(vbox);

  m_drums_box->setLayout(hbox);
}

void WiimoteEmuExtension::CreateNoneLayout()
{
  m_none_box = new QGroupBox(this);
  auto* hbox = new QHBoxLayout();
  auto* label = new QLabel(tr("No extension selected."));

  label->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
  label->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
  hbox->addWidget(label);
  m_none_box->setLayout(hbox);
}

void WiimoteEmuExtension::CreateNunchukLayout()
{
  auto* hbox = new QHBoxLayout();
  m_nunchuk_box = new QGroupBox(tr("Nunchuk"), this);

  hbox->addWidget(CreateGroupBox(
      tr("Stick"), Wiimote::GetNunchukGroup(GetPort(), WiimoteEmu::NunchukGroup::Stick)));
  hbox->addWidget(CreateGroupBox(
      tr("Tilt"), Wiimote::GetNunchukGroup(GetPort(), WiimoteEmu::NunchukGroup::Tilt)));
  hbox->addWidget(CreateGroupBox(
      tr("Swing"), Wiimote::GetNunchukGroup(GetPort(), WiimoteEmu::NunchukGroup::Swing)));

  auto* vbox = new QVBoxLayout();
  vbox->addWidget(CreateGroupBox(
      tr("Buttons"), Wiimote::GetNunchukGroup(GetPort(), WiimoteEmu::NunchukGroup::Buttons)));
  vbox->addWidget(CreateGroupBox(
      tr("Shake"), Wiimote::GetNunchukGroup(GetPort(), WiimoteEmu::NunchukGroup::Shake)));
  hbox->addItem(vbox);

  m_nunchuk_box->setLayout(hbox);
}

void WiimoteEmuExtension::CreateGuitarLayout()
{
  auto* hbox = new QHBoxLayout();
  m_guitar_box = new QGroupBox(tr("Guitar"), this);

  auto* vbox = new QVBoxLayout();
  vbox->addWidget(CreateGroupBox(
      tr("Buttons"), Wiimote::GetGuitarGroup(GetPort(), WiimoteEmu::GuitarGroup::Buttons)));
  vbox->addWidget(CreateGroupBox(
      tr("Stick"), Wiimote::GetGuitarGroup(GetPort(), WiimoteEmu::GuitarGroup::Stick)));
  hbox->addItem(vbox);

  auto* vbox2 = new QVBoxLayout();
  vbox2->addWidget(CreateGroupBox(
      tr("Strum"), Wiimote::GetGuitarGroup(GetPort(), WiimoteEmu::GuitarGroup::Strum)));
  vbox2->addWidget(CreateGroupBox(
      tr("Frets"), Wiimote::GetGuitarGroup(GetPort(), WiimoteEmu::GuitarGroup::Frets)));
  vbox2->addWidget(CreateGroupBox(
      tr("Whammy"), Wiimote::GetGuitarGroup(GetPort(), WiimoteEmu::GuitarGroup::Whammy)));
  hbox->addItem(vbox2);

  m_guitar_box->setLayout(hbox);
}

void WiimoteEmuExtension::CreateTurntableLayout()
{
  auto* hbox = new QHBoxLayout();
  m_turntable_box = new QGroupBox(tr("Turntable"), this);

  hbox->addWidget(CreateGroupBox(
      tr("Stick"), Wiimote::GetTurntableGroup(GetPort(), WiimoteEmu::TurntableGroup::Stick)));
  hbox->addWidget(CreateGroupBox(
      tr("Buttons"), Wiimote::GetTurntableGroup(GetPort(), WiimoteEmu::TurntableGroup::Buttons)));

  auto* vbox = new QVBoxLayout();
  vbox->addWidget(CreateGroupBox(
      tr("Effect"), Wiimote::GetTurntableGroup(GetPort(), WiimoteEmu::TurntableGroup::EffectDial)));
  vbox->addWidget(
      // i18n: "Table" refers to a turntable
      CreateGroupBox(tr("Left Table"),
                     Wiimote::GetTurntableGroup(GetPort(), WiimoteEmu::TurntableGroup::LeftTable)));
  vbox->addWidget(CreateGroupBox(
      // i18n: "Table" refers to a turntable
      tr("Right Table"),
      Wiimote::GetTurntableGroup(GetPort(), WiimoteEmu::TurntableGroup::RightTable)));
  vbox->addWidget(
      CreateGroupBox(tr("Crossfade"),
                     Wiimote::GetTurntableGroup(GetPort(), WiimoteEmu::TurntableGroup::Crossfade)));
  hbox->addItem(vbox);

  m_turntable_box->setLayout(hbox);
}

void WiimoteEmuExtension::CreateMainLayout()
{
  m_main_layout = new QHBoxLayout();

  m_main_layout->addWidget(m_classic_box);
  m_main_layout->addWidget(m_drums_box);
  m_main_layout->addWidget(m_guitar_box);
  m_main_layout->addWidget(m_none_box);
  m_main_layout->addWidget(m_nunchuk_box);
  m_main_layout->addWidget(m_turntable_box);

  setLayout(m_main_layout);
}

void WiimoteEmuExtension::LoadSettings()
{
  Wiimote::LoadConfig();
}

void WiimoteEmuExtension::SaveSettings()
{
  Wiimote::GetConfig()->SaveConfig();
}

InputConfig* WiimoteEmuExtension::GetConfig()
{
  return Wiimote::GetConfig();
}

void WiimoteEmuExtension::ChangeExtensionType(WiimoteEmuExtension::Type type)
{
  m_classic_box->setHidden(type != Type::CLASSIC_CONTROLLER);
  m_drums_box->setHidden(type != Type::DRUMS);
  m_guitar_box->setHidden(type != Type::GUITAR);
  m_none_box->setHidden(type != Type::NONE);
  m_nunchuk_box->setHidden(type != Type::NUNCHUK);
  m_turntable_box->setHidden(type != Type::TURNTABLE);
}
