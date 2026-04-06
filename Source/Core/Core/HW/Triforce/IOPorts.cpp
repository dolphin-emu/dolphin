// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/HW/Triforce/IOPorts.h"

#include <algorithm>

#include <fmt/ranges.h>

#include "Common/Assert.h"
#include "Common/BitUtils.h"
#include "Common/ChunkFile.h"

#include "Core/HW/DVD/AMMediaboard.h"
#include "Core/HW/GCPad.h"
#include "Core/HW/SI/SI.h"
#include "Core/HW/SI/SI_Device.h"
#include "Core/HW/Triforce/ICCardReader.h"
#include "Core/System.h"

#include "InputCommon/GCPadStatus.h"
#include "VideoCommon/OnScreenDisplay.h"

namespace Triforce
{

void IOPorts::Update()
{
  m_system_inputs = 0x00;
  m_switch_inputs.fill(0x00);
  m_analog_inputs.fill(NEUTRAL_ANALOG_VALUE);
  m_coin_inputs.fill(false);

  std::ranges::for_each(m_io_adapters, &IOAdapter::Update);
}

std::span<u8> IOPorts::GetSwitchInputs(u32 player_index)
{
  ASSERT(player_index < PLAYER_COUNT);

  return std::span{m_switch_inputs}.subspan(SWITCH_INPUT_BYTES_PER_PLAYER * player_index,
                                            SWITCH_INPUT_BYTES_PER_PLAYER);
}

std::span<const u8> IOPorts::GetSwitchInputs(u32 player_index) const
{
  ASSERT(player_index < PLAYER_COUNT);

  return std::span{m_switch_inputs}.subspan(SWITCH_INPUT_BYTES_PER_PLAYER * player_index,
                                            SWITCH_INPUT_BYTES_PER_PLAYER);
}

void IOPorts::DoState(PointerWrap& p)
{
  p.Do(m_status_switches);

  // Input states need not be state saved. They are updated before use.

  p.Do(m_generic_outputs);

  for (auto& io_adapter : m_io_adapters)
    io_adapter->DoState(p);
}

void IOPorts::AddIOAdapter(std::unique_ptr<IOAdapter> adapter)
{
  adapter->SetIOPorts(this);
  m_io_adapters.emplace_back(std::move(adapter));
}

IOAdapter::IOAdapter() = default;
IOAdapter::~IOAdapter() = default;

void IOPorts::SetGenericOutputs(std::span<const u8> bytes)
{
  if (bytes.size() > m_generic_outputs.size())
  {
    WARN_LOG_FMT(SERIALINTERFACE_JVSIO, "SetGenericOutputs: Unexpected byte count: {}",
                 bytes.size());
  }

  const auto bytes_to_copy = std::min(bytes.size(), GENERIC_OUTPUT_BYTE_COUNT);

  const bool no_change = std::ranges::equal(bytes.first(bytes_to_copy),
                                            std::span{m_generic_outputs}.first(bytes_to_copy));
  if (no_change)
    return;

  decltype(m_generic_outputs) bits_set{};
  decltype(m_generic_outputs) bits_cleared{};

  for (std::size_t i = 0; i != bytes_to_copy; ++i)
  {
    bits_set[i] = u8(~m_generic_outputs[i]) & bytes[i];
    bits_cleared[i] = m_generic_outputs[i] & u8(~bytes[i]);

    m_generic_outputs[i] = bytes[i];
  }

  DEBUG_LOG_FMT(SERIALINTERFACE_JVSIO, "SetGenericOutputs: set:{:02x} clr:{:02x} ({:02x})",
                fmt::join(bits_set, " "), fmt::join(bits_cleared, " "),
                fmt::join(m_generic_outputs, " "));

  for (auto& adapter : m_io_adapters)
  {
    adapter->HandleGenericOutputsChanged(bits_set, bits_cleared);
  }
}

void IOPorts::ResetGenericOutputs()
{
  SetGenericOutputs(std::array<u8, GENERIC_OUTPUT_BYTE_COUNT>{});
}

void IOAdapter::Update()
{
}

void IOAdapter::DoState(PointerWrap& p)
{
}

void IOAdapter::HandleGenericOutputsChanged(std::span<const u8> bits_set,
                                            std::span<const u8> bits_cleared)
{
}

void Common_IOAdapter::Update()
{
  auto* const io_ports = GetIOPorts();
  const auto coin_inputs = io_ports->GetCoinInputs();

  // Test/Service input is also possible this way, but we handle them via JVS IO instead.
  // const auto status_switches = io_ports->GetStatusSwitches();
  // status_switches[0] &= ~0x80; // Test
  // status_switches[0] &= ~0x40; // Service

  for (int i = 0; i != IOPorts::PLAYER_COUNT; ++i)
  {
    if (m_system.GetSerialInterface().GetDeviceType(i) != SerialInterface::SIDEVICE_AM_BASEBOARD)
      continue;

    const GCPadStatus pad_status = Pad::GetStatus(i);

    // Test button
    if (pad_status.switches & SWITCH_TEST)
    {
      if (AMMediaboard::GetTestMenu())
      {
        *io_ports->GetSystemInputs() |= 0x80u;
      }
      else
      {
        // Trying to access the test menu without SegaBoot present will cause a crash.
        OSD::AddMessage("Test menu is disabled due to missing SegaBoot.", OSD::Duration::NORMAL,
                        OSD::Color::RED);
      }
    }

    const auto switch_inputs = io_ports->GetSwitchInputs(i);

    // Service button
    if (pad_status.switches & SWITCH_SERVICE)
      switch_inputs[0] |= 0x40;

    // Coin button
    if (pad_status.switches & SWITCH_COIN)
      coin_inputs[i] = true;
  }
}

void VirtuaStriker3_IOAdapter::Update()
{
  auto* const io_ports = GetIOPorts();

  for (int i = 0; i != IOPorts::PLAYER_COUNT; ++i)
  {
    const auto switch_inputs = io_ports->GetSwitchInputs(i);
    const GCPadStatus pad_status = Pad::GetStatus(i);

    // Start
    if (pad_status.button & PAD_BUTTON_START)
      switch_inputs[0] |= 0x80;
    // Up
    if (pad_status.button & PAD_BUTTON_UP)
      switch_inputs[0] |= 0x20;
    // Down
    if (pad_status.button & PAD_BUTTON_DOWN)
      switch_inputs[0] |= 0x10;
    // Left
    if (pad_status.button & PAD_BUTTON_LEFT)
      switch_inputs[0] |= 0x08;
    // Right
    if (pad_status.button & PAD_BUTTON_RIGHT)
      switch_inputs[0] |= 0x04;
    // Long Pass
    if (pad_status.button & PAD_BUTTON_X)
      switch_inputs[0] |= 0x02;
    // Shoot
    if (pad_status.button & PAD_BUTTON_B)
      switch_inputs[0] |= 0x01;

    // Short Pass
    if (pad_status.button & PAD_BUTTON_A)
      switch_inputs[1] |= 0x80;
  }
}

void VirtuaStriker4Common_IOAdapter::Update()
{
  auto* const io_ports = GetIOPorts();
  const auto generic_outputs = io_ports->GetGenericOutputs();

  const bool is_slot_a_locked = generic_outputs[0] & SLOT_A_LOCK_BIT;
  const bool is_slot_b_locked = generic_outputs[0] & SLOT_B_LOCK_BIT;

  // FYI: VS4 and VS4_2006 won't accept cards when both are presented at the same time.
  // It expects to load and lock one before the second is inserted.

  if (is_slot_a_locked)
  {
    // If slot A is locked, insert slot B.
    if (m_card_readers[1]->IsReadyToInsertCard())
      m_card_readers[1]->InsertCard();
  }
  else
  {
    // If slot A is unlocked, remove slot B if unlocked.
    if (m_card_readers[1]->IsCardPresent() && !is_slot_b_locked)
      m_card_readers[1]->EjectCard();
  }

  // Insert slot A.
  if (m_card_readers[0]->IsReadyToInsertCard())
    m_card_readers[0]->InsertCard();

  const auto analog_inputs = io_ports->GetAnalogInputs();

  for (int i = 0; i != IOPorts::PLAYER_COUNT; ++i)
  {
    const auto switch_inputs = io_ports->GetSwitchInputs(i);

    // Bit 0x10 of each player's 1st byte is a card presence switch.
    Common::SetBit(switch_inputs[0], 4, m_card_readers[i]->IsCardPresent());

    // Bit 0x20 of each player's 2nd byte is some kind of "eject" sensor.
    Common::SetBit(switch_inputs[1], 5, m_card_readers[i]->IsEjecting());

    const GCPadStatus pad_status = Pad::GetStatus(i);

    // Start
    if (pad_status.button & PAD_BUTTON_START)
      switch_inputs[0] |= 0x80;
    // Tactics (U)
    if (pad_status.button & PAD_BUTTON_LEFT)
      switch_inputs[0] |= 0x20;
    // Tactics (M)
    if (pad_status.button & PAD_BUTTON_UP)
      switch_inputs[0] |= 0x08;
    // Tactics (D)
    if (pad_status.button & PAD_BUTTON_RIGHT)
      switch_inputs[0] |= 0x04;
    // Short Pass
    if (pad_status.button & PAD_BUTTON_A)
      switch_inputs[0] |= 0x02;
    // Long Pass
    if (pad_status.button & PAD_BUTTON_X)
      switch_inputs[0] |= 0x01;

    // Shoot
    if (pad_status.button & PAD_BUTTON_B)
      switch_inputs[1] |= 0x80;
    // Dash
    if (pad_status.button & PAD_BUTTON_Y)
      switch_inputs[1] |= 0x40;

    // Movement
    analog_inputs[(2 * i) + 0] = Common::ExpandValue<u16>(0xff - pad_status.stickY, 8);
    analog_inputs[(2 * i) + 1] = Common::ExpandValue<u16>(pad_status.stickX, 8);
  }
}

void VirtuaStriker4Common_IOAdapter::HandleGenericOutputsChanged(std::span<const u8> bits_set,
                                                                 std::span<const u8> bits_cleared)
{
  const u8 bits_changed_0 = bits_set[0] | bits_cleared[0];

  if (bits_changed_0 & SLOT_A_LOCK_BIT)
  {
    INFO_LOG_FMT(SERIALINTERFACE_JVSIO, "JVS-IO: Slot 1: {}",
                 (bits_set[0] & SLOT_A_LOCK_BIT) ? "Locked" : "Unlocked");
  }

  if (bits_changed_0 & SLOT_B_LOCK_BIT)
  {
    INFO_LOG_FMT(SERIALINTERFACE_JVSIO, "JVS-IO: Slot 2: {}",
                 (bits_set[0] & SLOT_B_LOCK_BIT) ? "Locked" : "Unlocked");
  }
}

void VirtuaStriker4_2006_IOAdapter::HandleGenericOutputsChanged(std::span<const u8> bits_set,
                                                                std::span<const u8> bits_cleared)
{
  (void)bits_cleared;

  // Unlike VirtuaStriker4, which uses ICCardCommand::Eject,
  //  VirtuaStriker4_2006 triggers card ejection with JVS-IO.

  if (bits_set[0] & SLOT_A_EJECT_BIT)
    m_card_readers[0]->EjectCard();

  if (bits_set[0] & SLOT_B_EJECT_BIT)
    m_card_readers[1]->EjectCard();
}

void GekitouProYakyuu_IOAdapter::Update()
{
  auto* const io_ports = GetIOPorts();

  for (int i = 0; i != IOPorts::PLAYER_COUNT; ++i)
  {
    // Gekitou isn't as picky as VS4. We can insert cards whenever.
    if (m_card_readers[i]->IsReadyToInsertCard())
      m_card_readers[i]->InsertCard();

    const auto switch_inputs = io_ports->GetSwitchInputs(i);

    // Bit 0x40 of each player's 2nd byte is a card presence switch.
    Common::SetBit(switch_inputs[1], 6, m_card_readers[i]->IsCardPresent());

    const GCPadStatus pad_status = Pad::GetStatus(i);

    // Start
    if (pad_status.button & PAD_BUTTON_START)
      switch_inputs[0] |= 0x80;
    // Up
    if (pad_status.button & PAD_BUTTON_UP)
      switch_inputs[0] |= 0x20;
    // Down
    if (pad_status.button & PAD_BUTTON_DOWN)
      switch_inputs[0] |= 0x10;
    // Left
    if (pad_status.button & PAD_BUTTON_LEFT)
      switch_inputs[0] |= 0x08;
    // Right
    if (pad_status.button & PAD_BUTTON_RIGHT)
      switch_inputs[0] |= 0x04;
    // B
    if (pad_status.button & PAD_BUTTON_A)
      switch_inputs[0] |= 0x02;
    // A
    if (pad_status.button & PAD_BUTTON_B)
      switch_inputs[0] |= 0x01;

    // Gekitou
    if (pad_status.button & PAD_TRIGGER_L)
      switch_inputs[1] |= 0x80;
  }
}

void GekitouProYakyuu_IOAdapter::HandleGenericOutputsChanged(std::span<const u8> bits_set,
                                                             std::span<const u8> bits_cleared)
{
  (void)bits_set;

  // When a card is inserted Gekitou sets the relevant "lock" bit.
  // When done with the card it clears this bit, then expects the card presence to be cleared.
  // I guess we need to treat this as an alternative to ICCardCommand::Eject.

  if (bits_cleared[0] & SLOT_A_LOCK_BIT)
    m_card_readers[0]->EjectCard();

  if (bits_cleared[0] & SLOT_B_LOCK_BIT)
    m_card_readers[1]->EjectCard();
}

void KeyOfAvalon_IOAdapter::Update()
{
  // Note that the game sometimes does a few InsertCheck when it still wants a card.
  constexpr u8 insert_check_count_before_removing_card = 10;

  if (m_card_reader->GetCardPresentInsertCheckCount() >= insert_check_count_before_removing_card)
    m_card_reader->EjectCard();

  if (m_card_reader->IsReadyToInsertCard())
    m_card_reader->InsertCard();

  const auto switch_inputs = GetIOPorts()->GetSwitchInputs(0);
  const GCPadStatus pad_status = Pad::GetStatus(0);

  // Debug On
  if (pad_status.button & PAD_BUTTON_START)
    switch_inputs[0] |= 0x80;
  // Switch 2
  if (pad_status.button & PAD_BUTTON_B)
    switch_inputs[0] |= 0x08;
  // Switch 1
  if (pad_status.button & PAD_BUTTON_A)
    switch_inputs[0] |= 0x04;
}

}  // namespace Triforce
