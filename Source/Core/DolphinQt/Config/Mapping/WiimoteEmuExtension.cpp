// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/Config/Mapping/WiimoteEmuExtension.h"

#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>

#include "Core/HW/Wiimote.h"
#include "Core/HW/WiimoteEmu/Extension/Classic.h"
#include "Core/HW/WiimoteEmu/Extension/DrawsomeTablet.h"
#include "Core/HW/WiimoteEmu/Extension/Drums.h"
#include "Core/HW/WiimoteEmu/Extension/Guitar.h"
#include "Core/HW/WiimoteEmu/Extension/Nunchuk.h"
#include "Core/HW/WiimoteEmu/Extension/Shinkansen.h"
#include "Core/HW/WiimoteEmu/Extension/TaTaCon.h"
#include "Core/HW/WiimoteEmu/Extension/Turntable.h"
#include "Core/HW/WiimoteEmu/Extension/UDrawTablet.h"
#include "Core/HW/WiimoteEmu/WiimoteEmu.h"

#include "DolphinQt/Config/Mapping/WiimoteEmuGeneral.h"

#include "InputCommon/InputConfig.h"

WiimoteEmuExtension::WiimoteEmuExtension(MappingWindow* window, WiimoteEmuGeneral* wm_emu_general)
    : MappingWidget(window)
{
  CreateClassicLayout();
  CreateDrumsLayout();
  CreateGuitarLayout();
  CreateNoneLayout();
  CreateNunchukLayout();
  CreateTurntableLayout();
  CreateUDrawTabletLayout();
  CreateDrawsomeTabletLayout();
  CreateTaTaConLayout();
  CreateShinkansenLayout();
  CreateMainLayout();

  ChangeExtensionType(WiimoteEmu::ExtensionNumber::NONE);

  connect(wm_emu_general, &WiimoteEmuGeneral::AttachmentChanged, this,
          &WiimoteEmuExtension::ChangeExtensionType);
}

void WiimoteEmuExtension::CreateClassicLayout()
{
  m_classic_box = new QGroupBox(tr("Classic Controller"), this);
  auto* const layout = new QHBoxLayout{m_classic_box};

  auto* const col0 = new QVBoxLayout;
  layout->addLayout(col0);
  col0->addWidget(
      CreateGroupBox(Wiimote::GetClassicGroup(GetPort(), WiimoteEmu::ClassicGroup::Buttons)), 2);
  col0->addWidget(
      CreateGroupBox(Wiimote::GetClassicGroup(GetPort(), WiimoteEmu::ClassicGroup::DPad)), 1);

  layout->addWidget(
      CreateGroupBox(Wiimote::GetClassicGroup(GetPort(), WiimoteEmu::ClassicGroup::LeftStick)));
  layout->addWidget(
      CreateGroupBox(Wiimote::GetClassicGroup(GetPort(), WiimoteEmu::ClassicGroup::RightStick)));
  layout->addWidget(
      CreateGroupBox(Wiimote::GetClassicGroup(GetPort(), WiimoteEmu::ClassicGroup::Triggers)));
}

void WiimoteEmuExtension::CreateDrumsLayout()
{
  m_drums_box = new QGroupBox(tr("Drum Kit"), this);
  auto* const layout = new QHBoxLayout{m_drums_box};

  layout->addWidget(
      CreateGroupBox(Wiimote::GetDrumsGroup(GetPort(), WiimoteEmu::DrumsGroup::Stick)));

  auto* const col1 = new QVBoxLayout;
  layout->addLayout(col1);
  col1->addWidget(CreateGroupBox(Wiimote::GetDrumsGroup(GetPort(), WiimoteEmu::DrumsGroup::Pads)),
                  2);
  col1->addWidget(
      CreateGroupBox(Wiimote::GetDrumsGroup(GetPort(), WiimoteEmu::DrumsGroup::Buttons)), 1);
}

void WiimoteEmuExtension::CreateNoneLayout()
{
  m_none_box = new QGroupBox(this);
  auto* const hbox = new QHBoxLayout{m_none_box};
  auto* const label = new QLabel(tr("No extension selected."));

  label->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
  label->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
  hbox->addWidget(label);
}

void WiimoteEmuExtension::CreateNunchukLayout()
{
  m_nunchuk_box = new QGroupBox(tr("Nunchuk"), this);
  auto* const layout = new QHBoxLayout{m_nunchuk_box};

  layout->addWidget(
      CreateGroupBox(Wiimote::GetNunchukGroup(GetPort(), WiimoteEmu::NunchukGroup::Stick)));
  layout->addWidget(
      CreateGroupBox(Wiimote::GetNunchukGroup(GetPort(), WiimoteEmu::NunchukGroup::Buttons)));
}

void WiimoteEmuExtension::CreateGuitarLayout()
{
  m_guitar_box = new QGroupBox(tr("Guitar"), this);
  auto* const hbox = new QHBoxLayout{m_guitar_box};

  auto* const vbox = new QVBoxLayout();
  vbox->addWidget(
      CreateGroupBox(Wiimote::GetGuitarGroup(GetPort(), WiimoteEmu::GuitarGroup::Stick)));
  hbox->addLayout(vbox);

  auto* const vbox2 = new QVBoxLayout();
  vbox2->addWidget(
      CreateGroupBox(Wiimote::GetGuitarGroup(GetPort(), WiimoteEmu::GuitarGroup::Strum)), 2);
  vbox2->addWidget(
      CreateGroupBox(Wiimote::GetGuitarGroup(GetPort(), WiimoteEmu::GuitarGroup::Frets)), 3);
  hbox->addLayout(vbox2);

  auto* const vbox3 = new QVBoxLayout();
  vbox3->addWidget(
      CreateGroupBox(Wiimote::GetGuitarGroup(GetPort(), WiimoteEmu::GuitarGroup::Buttons)), 3);
  vbox3->addWidget(
      CreateGroupBox(Wiimote::GetGuitarGroup(GetPort(), WiimoteEmu::GuitarGroup::Whammy)), 3);
  vbox3->addWidget(
      CreateGroupBox(Wiimote::GetGuitarGroup(GetPort(), WiimoteEmu::GuitarGroup::SliderBar)), 4);
  hbox->addLayout(vbox3);
}

void WiimoteEmuExtension::CreateTurntableLayout()
{
  m_turntable_box = new QGroupBox(tr("DJ Turntable"), this);
  auto* const layout = new QHBoxLayout{m_turntable_box};

  auto* const col0 = new QVBoxLayout;
  layout->addLayout(col0);
  col0->addWidget(
      CreateGroupBox(Wiimote::GetTurntableGroup(GetPort(), WiimoteEmu::TurntableGroup::Stick)));

  auto* const col1 = new QVBoxLayout;
  layout->addLayout(col1);
  col1->addWidget(
      CreateGroupBox(Wiimote::GetTurntableGroup(GetPort(), WiimoteEmu::TurntableGroup::LeftTable)));
  col1->addWidget(
      CreateGroupBox(Wiimote::GetTurntableGroup(GetPort(), WiimoteEmu::TurntableGroup::Crossfade)));

  auto* const col2 = new QVBoxLayout;
  layout->addLayout(col2);
  col2->addWidget(CreateGroupBox(
      Wiimote::GetTurntableGroup(GetPort(), WiimoteEmu::TurntableGroup::RightTable)));
  col2->addWidget(CreateGroupBox(
      Wiimote::GetTurntableGroup(GetPort(), WiimoteEmu::TurntableGroup::EffectDial)));

  layout->addWidget(
      CreateGroupBox(Wiimote::GetTurntableGroup(GetPort(), WiimoteEmu::TurntableGroup::Buttons)));
}

void WiimoteEmuExtension::CreateUDrawTabletLayout()
{
  m_udraw_tablet_box = new QGroupBox(tr("uDraw GameTablet"), this);
  auto* const hbox = new QHBoxLayout{m_udraw_tablet_box};

  hbox->addWidget(CreateGroupBox(
      Wiimote::GetUDrawTabletGroup(GetPort(), WiimoteEmu::UDrawTabletGroup::Buttons)));
  hbox->addWidget(CreateGroupBox(
      Wiimote::GetUDrawTabletGroup(GetPort(), WiimoteEmu::UDrawTabletGroup::Stylus)));
  hbox->addWidget(
      CreateGroupBox(Wiimote::GetUDrawTabletGroup(GetPort(), WiimoteEmu::UDrawTabletGroup::Touch)));
}

void WiimoteEmuExtension::CreateDrawsomeTabletLayout()
{
  m_drawsome_tablet_box = new QGroupBox(tr("Drawsome Tablet"), this);
  auto* const hbox = new QHBoxLayout{m_drawsome_tablet_box};

  hbox->addWidget(CreateGroupBox(
      Wiimote::GetDrawsomeTabletGroup(GetPort(), WiimoteEmu::DrawsomeTabletGroup::Stylus)));

  hbox->addWidget(CreateGroupBox(
      Wiimote::GetDrawsomeTabletGroup(GetPort(), WiimoteEmu::DrawsomeTabletGroup::Touch)));
}

void WiimoteEmuExtension::CreateTaTaConLayout()
{
  m_tatacon_box = new QGroupBox(tr("Taiko Drum"), this);
  auto* const hbox = new QHBoxLayout{m_tatacon_box};

  hbox->addWidget(
      CreateGroupBox(Wiimote::GetTaTaConGroup(GetPort(), WiimoteEmu::TaTaConGroup::Center)));
  hbox->addWidget(
      CreateGroupBox(Wiimote::GetTaTaConGroup(GetPort(), WiimoteEmu::TaTaConGroup::Rim)));
}

void WiimoteEmuExtension::CreateShinkansenLayout()
{
  m_shinkansen_box = new QGroupBox(tr("Shinkansen"), this);
  auto* const hbox = new QHBoxLayout{m_shinkansen_box};

  hbox->addWidget(
      CreateGroupBox(Wiimote::GetShinkansenGroup(GetPort(), WiimoteEmu::ShinkansenGroup::Levers)));
  hbox->addWidget(
      CreateGroupBox(Wiimote::GetShinkansenGroup(GetPort(), WiimoteEmu::ShinkansenGroup::Buttons)));
  hbox->addWidget(
      CreateGroupBox(Wiimote::GetShinkansenGroup(GetPort(), WiimoteEmu::ShinkansenGroup::Light)));
}

void WiimoteEmuExtension::CreateMainLayout()
{
  m_main_layout = new QHBoxLayout{this};

  m_main_layout->addWidget(m_classic_box);
  m_main_layout->addWidget(m_drums_box);
  m_main_layout->addWidget(m_guitar_box);
  m_main_layout->addWidget(m_none_box);
  m_main_layout->addWidget(m_nunchuk_box);
  m_main_layout->addWidget(m_turntable_box);
  m_main_layout->addWidget(m_udraw_tablet_box);
  m_main_layout->addWidget(m_drawsome_tablet_box);
  m_main_layout->addWidget(m_tatacon_box);
  m_main_layout->addWidget(m_shinkansen_box);
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

void WiimoteEmuExtension::ChangeExtensionType(int type)
{
  using WiimoteEmu::ExtensionNumber;

  m_none_box->setHidden(type != ExtensionNumber::NONE);
  m_nunchuk_box->setHidden(type != ExtensionNumber::NUNCHUK);
  m_classic_box->setHidden(type != ExtensionNumber::CLASSIC);
  m_guitar_box->setHidden(type != ExtensionNumber::GUITAR);
  m_drums_box->setHidden(type != ExtensionNumber::DRUMS);
  m_turntable_box->setHidden(type != ExtensionNumber::TURNTABLE);
  m_udraw_tablet_box->setHidden(type != ExtensionNumber::UDRAW_TABLET);
  m_drawsome_tablet_box->setHidden(type != ExtensionNumber::DRAWSOME_TABLET);
  m_tatacon_box->setHidden(type != ExtensionNumber::TATACON);
  m_shinkansen_box->setHidden(type != ExtensionNumber::SHINKANSEN);
}
