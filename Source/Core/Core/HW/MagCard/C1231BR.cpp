// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

// Based on: https://github.com/GXTX/YACardEmu
// Copyright (C) 2020-2023 wutno (https://github.com/GXTX)
// Copyright (C) 2022-2023 tugpoat (https://github.com/tugpoat)
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/HW/MagCard/C1231BR.h"

namespace MagCard
{

void C1231BR::Command_10_Initialize()
{
  // TODO: Not hardware tested.
  if (!m_current_packet.empty())
  {
    if (InitMode(m_current_packet[0]) == InitMode::EjectAfter)
    {
      m_is_shutter_open = true;
      MoveCard(R::ENTRY_SLOT);
    }
  }

  // m_status.SoftReset(); // TODO: Needed ?
  FinishCommand();
}

void C1231BR::Command_D0_ControlShutter()
{
  enum class Action : u8
  {
    Close = 0x30,
    Open = 0x31,
  };

  if (m_current_packet.empty())
  {
    SetPError(P::SYSTEM_ERR);
    FinishCommand();
    return;
  }

  m_is_shutter_open = Action(m_current_packet[0]) == Action::Open;

  INFO_LOG_FMT(SERIALINTERFACE_CARD, "Shutter: {}", m_is_shutter_open ? "Open" : "Close");

  FinishCommand();
}

u8 C1231BR::GetPositionValue()
{
  // True "R" (Card Position) values.
  // FYI: I see a 0xd0 command transition from: 11000 -> 01110 -> 00111
  // Are these 5 bits individual sensors placed within the machine ?
  static constexpr std::array<u8, std::size_t(R::MAX_POSITIONS)> POSITION_VALUES{
      0b00000,  // NO_CARD
      0b11000,  // READ_WRITE_HEAD
      0b00111,  // THERMAL_HEAD
      0b11100,  // DISPENSER_THERMAL
      0b00001,  // ENTRY_SLOT
  };

  return (m_is_shutter_open ? 0x80u : 0x40u) |
         (m_card_settings->report_dispenser_empty ? 0x00u : 0x20u) |
         POSITION_VALUES[u8(m_status.r)];
}

void C1231BR::DoState(PointerWrap& p)
{
  MagneticCardReader::DoState(p);

  p.Do(m_is_shutter_open);
}

bool C1231BR::IsReadyForCard()
{
  return !IsCardPresent() && m_is_shutter_open;
}

}  // namespace MagCard
