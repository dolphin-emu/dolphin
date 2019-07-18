// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/Config/Mapping/WiimoteEmuExtension.h"

#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>

#include "Core/HW/Wiimote.h"
#include "Core/HW/WiimoteEmu/Extension/BalanceBoard.h"
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

#include "InputCommon/InputConfig.h"

WiimoteEmuExtension::WiimoteEmuExtension(MappingWindow* window) : MappingWidget(window)
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
  CreateBalanceBoardLayout();
  CreateMainLayout();

  ChangeExtensionType(WiimoteEmu::ExtensionNumber::NONE);
}

void WiimoteEmuExtension::CreateClassicLayout()
{
  auto* layout = new QGridLayout();
  m_classic_box = new QGroupBox(tr("Classic Controller"), this);

  layout->addWidget(
      CreateGroupBox(tr("Buttons"),
                     Wiimote::GetClassicGroup(GetPort(), WiimoteEmu::ClassicGroup::Buttons)),
      0, 0);
  layout->addWidget(CreateGroupBox(tr("D-Pad"), Wiimote::GetClassicGroup(
                                                    GetPort(), WiimoteEmu::ClassicGroup::DPad)),
                    1, 0);
  layout->addWidget(
      CreateGroupBox(tr("Left Stick"),
                     Wiimote::GetClassicGroup(GetPort(), WiimoteEmu::ClassicGroup::LeftStick)),
      0, 1, -1, 1);
  layout->addWidget(
      CreateGroupBox(tr("Right Stick"),
                     Wiimote::GetClassicGroup(GetPort(), WiimoteEmu::ClassicGroup::RightStick)),
      0, 2, -1, 1);
  layout->addWidget(
      CreateGroupBox(tr("Triggers"),
                     Wiimote::GetClassicGroup(GetPort(), WiimoteEmu::ClassicGroup::Triggers)),
      0, 3, -1, 1);

  m_classic_box->setLayout(layout);
}

void WiimoteEmuExtension::CreateDrumsLayout()
{
  auto* layout = new QGridLayout();
  m_drums_box = new QGroupBox(tr("Drum Kit"), this);

  layout->addWidget(
      CreateGroupBox(tr("Stick"), Wiimote::GetDrumsGroup(GetPort(), WiimoteEmu::DrumsGroup::Stick)),
      0, 0, -1, 1);

  layout->addWidget(
      CreateGroupBox(tr("Pads"), Wiimote::GetDrumsGroup(GetPort(), WiimoteEmu::DrumsGroup::Pads)),
      0, 1);
  layout->addWidget(CreateGroupBox(tr("Buttons"), Wiimote::GetDrumsGroup(
                                                      GetPort(), WiimoteEmu::DrumsGroup::Buttons)),
                    1, 1);

  m_drums_box->setLayout(layout);
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
  auto* layout = new QGridLayout();
  m_nunchuk_box = new QGroupBox(tr("Nunchuk"), this);

  layout->addWidget(CreateGroupBox(tr("Stick"), Wiimote::GetNunchukGroup(
                                                    GetPort(), WiimoteEmu::NunchukGroup::Stick)),
                    0, 0);
  layout->addWidget(
      CreateGroupBox(tr("Buttons"),
                     Wiimote::GetNunchukGroup(GetPort(), WiimoteEmu::NunchukGroup::Buttons)),
      0, 1);

  m_nunchuk_box->setLayout(layout);
}

void WiimoteEmuExtension::CreateGuitarLayout()
{
  auto* hbox = new QHBoxLayout();
  m_guitar_box = new QGroupBox(tr("Guitar"), this);

  auto* vbox = new QVBoxLayout();
  vbox->addWidget(CreateGroupBox(
      tr("Stick"), Wiimote::GetGuitarGroup(GetPort(), WiimoteEmu::GuitarGroup::Stick)));
  hbox->addLayout(vbox);

  auto* vbox2 = new QVBoxLayout();
  vbox2->addWidget(CreateGroupBox(
      tr("Strum"), Wiimote::GetGuitarGroup(GetPort(), WiimoteEmu::GuitarGroup::Strum)));
  vbox2->addWidget(CreateGroupBox(
      tr("Frets"), Wiimote::GetGuitarGroup(GetPort(), WiimoteEmu::GuitarGroup::Frets)));
  hbox->addLayout(vbox2);

  auto* vbox3 = new QVBoxLayout();
  vbox3->addWidget(CreateGroupBox(
      tr("Buttons"), Wiimote::GetGuitarGroup(GetPort(), WiimoteEmu::GuitarGroup::Buttons)));
  vbox3->addWidget(CreateGroupBox(
      tr("Whammy"), Wiimote::GetGuitarGroup(GetPort(), WiimoteEmu::GuitarGroup::Whammy)));
  vbox3->addWidget(CreateGroupBox(
      tr("Slider Bar"), Wiimote::GetGuitarGroup(GetPort(), WiimoteEmu::GuitarGroup::SliderBar)));
  hbox->addLayout(vbox3);

  m_guitar_box->setLayout(hbox);
}

void WiimoteEmuExtension::CreateTurntableLayout()
{
  auto* layout = new QGridLayout();
  m_turntable_box = new QGroupBox(tr("DJ Turntable"), this);

  layout->addWidget(CreateGroupBox(tr("Stick"), Wiimote::GetTurntableGroup(
                                                    GetPort(), WiimoteEmu::TurntableGroup::Stick)),
                    0, 0, -1, 1);

  layout->addWidget(
      CreateGroupBox(tr("Buttons"),
                     Wiimote::GetTurntableGroup(GetPort(), WiimoteEmu::TurntableGroup::Buttons)),
      0, 1);
  layout->addWidget(
      CreateGroupBox(tr("Effect"),
                     Wiimote::GetTurntableGroup(GetPort(), WiimoteEmu::TurntableGroup::EffectDial)),
      1, 1, -1, 1);

  layout->addWidget(
      // i18n: "Table" refers to a turntable
      CreateGroupBox(tr("Left Table"),
                     Wiimote::GetTurntableGroup(GetPort(), WiimoteEmu::TurntableGroup::LeftTable)),
      0, 2);
  layout->addWidget(CreateGroupBox(
                        // i18n: "Table" refers to a turntable
                        tr("Right Table"), Wiimote::GetTurntableGroup(
                                               GetPort(), WiimoteEmu::TurntableGroup::RightTable)),
                    1, 2);
  layout->addWidget(
      CreateGroupBox(tr("Crossfade"),
                     Wiimote::GetTurntableGroup(GetPort(), WiimoteEmu::TurntableGroup::Crossfade)),
      2, 2);

  m_turntable_box->setLayout(layout);
}

void WiimoteEmuExtension::CreateUDrawTabletLayout()
{
  auto* hbox = new QHBoxLayout();
  m_udraw_tablet_box = new QGroupBox(tr("uDraw GameTablet"), this);

  hbox->addWidget(CreateGroupBox(
      tr("Buttons"),
      Wiimote::GetUDrawTabletGroup(GetPort(), WiimoteEmu::UDrawTabletGroup::Buttons)));

  hbox->addWidget(CreateGroupBox(
      tr("Stylus"), Wiimote::GetUDrawTabletGroup(GetPort(), WiimoteEmu::UDrawTabletGroup::Stylus)));

  hbox->addWidget(CreateGroupBox(
      tr("Touch"), Wiimote::GetUDrawTabletGroup(GetPort(), WiimoteEmu::UDrawTabletGroup::Touch)));

  m_udraw_tablet_box->setLayout(hbox);
}

void WiimoteEmuExtension::CreateDrawsomeTabletLayout()
{
  const auto hbox = new QHBoxLayout();
  m_drawsome_tablet_box = new QGroupBox(tr("Drawsome Tablet"), this);

  hbox->addWidget(CreateGroupBox(
      tr("Stylus"),
      Wiimote::GetDrawsomeTabletGroup(GetPort(), WiimoteEmu::DrawsomeTabletGroup::Stylus)));

  hbox->addWidget(CreateGroupBox(
      tr("Touch"),
      Wiimote::GetDrawsomeTabletGroup(GetPort(), WiimoteEmu::DrawsomeTabletGroup::Touch)));

  m_drawsome_tablet_box->setLayout(hbox);
}

void WiimoteEmuExtension::CreateTaTaConLayout()
{
  auto* hbox = new QHBoxLayout();
  m_tatacon_box = new QGroupBox(tr("Taiko Drum"), this);

  hbox->addWidget(CreateGroupBox(
      tr("Center"), Wiimote::GetTaTaConGroup(GetPort(), WiimoteEmu::TaTaConGroup::Center)));
  hbox->addWidget(CreateGroupBox(
      tr("Rim"), Wiimote::GetTaTaConGroup(GetPort(), WiimoteEmu::TaTaConGroup::Rim)));

  m_tatacon_box->setLayout(hbox);
}

void WiimoteEmuExtension::CreateShinkansenLayout()
{
  auto* hbox = new QHBoxLayout();
  m_shinkansen_box = new QGroupBox(tr("Shinkansen"), this);

  hbox->addWidget(CreateGroupBox(
      tr("Levers"), Wiimote::GetShinkansenGroup(GetPort(), WiimoteEmu::ShinkansenGroup::Levers)));
  hbox->addWidget(CreateGroupBox(
      tr("Buttons"), Wiimote::GetShinkansenGroup(GetPort(), WiimoteEmu::ShinkansenGroup::Buttons)));
  hbox->addWidget(CreateGroupBox(
      tr("Light"), Wiimote::GetShinkansenGroup(GetPort(), WiimoteEmu::ShinkansenGroup::Light)));

  m_shinkansen_box->setLayout(hbox);
}

void WiimoteEmuExtension::CreateBalanceBoardLayout()
{
  auto* layout = new QGridLayout();
  m_balance_board_box = new QGroupBox(tr("Balance Board"), this);

  layout->addWidget(
      CreateGroupBox(tr("Balance"), Wiimote::GetBalanceBoardGroup(
                                        GetPort(), WiimoteEmu::BalanceBoardGroup::Balance)),
      0, 0);
  layout->addWidget(
      CreateGroupBox(tr("Balance"), Wiimote::GetBalanceBoardGroup(
                                        GetPort(), WiimoteEmu::BalanceBoardGroup::Weight)),
      0, 1);

  m_balance_board_box->setLayout(layout);
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
  m_main_layout->addWidget(m_udraw_tablet_box);
  m_main_layout->addWidget(m_drawsome_tablet_box);
  m_main_layout->addWidget(m_tatacon_box);
  m_main_layout->addWidget(m_shinkansen_box);
  m_main_layout->addWidget(m_balance_board_box);

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

void WiimoteEmuExtension::ChangeExtensionType(u32 type)
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
  m_balance_board_box->setHidden(type != ExtensionNumber::BALANCE_BOARD);
}
