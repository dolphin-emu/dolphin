// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/HW/Triforce/IOPorts.h"

#include <functional>

#include <fmt/ranges.h>

#include "Common/BitUtils.h"
#include "Common/ChunkFile.h"

#include "Core/HW/Triforce/ICCardReader.h"

namespace Triforce
{

void IOPorts::Update()
{
  std::ranges::for_each(m_io_adapters, &IOAdapter::Update);
}

void IOPorts::DoState(PointerWrap& p)
{
  p.Do(m_switch_input_data);
  p.Do(m_generic_output_data);
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
  const auto bytes_to_copy = std::min(bytes.size(), GENERIC_OUTPUT_BYTE_COUNT);

  if (bytes.size() > m_generic_output_data.size())
  {
    WARN_LOG_FMT(SERIALINTERFACE_JVSIO, "JVS-IO: GenericOutputs: Unexpected byte count: {}",
                 bytes.size());
  }

  decltype(m_generic_output_data) bits_set{};
  decltype(m_generic_output_data) bits_cleared{};

  for (std::size_t i = 0; i != bytes_to_copy; ++i)
  {
    bits_set[i] = u8(~m_generic_output_data[i]) & bytes[i];
    bits_cleared[i] = m_generic_output_data[i] & u8(~bytes[i]);

    m_generic_output_data[i] = bytes[i];
  }

  bool bits_changed = false;

  if (std::ranges::any_of(bits_set, std::bind_front(std::not_equal_to{}, 0x00)))
  {
    bits_changed = true;
    INFO_LOG_FMT(SERIALINTERFACE_JVSIO, "JVS-IO: GenericOutputs: bits_set: {:02x}",
                 fmt::join(bits_set, " "));
  }
  if (std::ranges::any_of(bits_cleared, std::bind_front(std::not_equal_to{}, 0x00)))
  {
    bits_changed = true;
    INFO_LOG_FMT(SERIALINTERFACE_JVSIO, "JVS-IO: GenericOutputs: bits_cleared: {:02x}",
                 fmt::join(bits_cleared, " "));
  }

  if (!bits_changed)
    return;

  for (auto& adapter : m_io_adapters)
  {
    adapter->HandleGenericOutputsChanged(bits_set, bits_cleared);
  }
}

void IOAdapter::Update()
{
}

void IOAdapter::HandleGenericOutputsChanged(std::span<const u8> bits_set,
                                            std::span<const u8> bits_cleared)
{
}

void MarioKartGPCommon_IOAdapter::HandleGenericOutputsChanged(std::span<const u8> bits_set,
                                                              std::span<const u8> bits_cleared)
{
  const u8 bits_changed_0 = bits_set[0] | bits_cleared[0];

  if (bits_changed_0 & ITEM_LIGHT_BIT)
  {
    INFO_LOG_FMT(SERIALINTERFACE_JVSIO, "JVS-IO: Item Button: {}",
                 (bits_set[0] & ITEM_LIGHT_BIT) ? "ON" : "OFF");
  }

  if (bits_changed_0 & CANCEL_LIGHT_BIT)
  {
    INFO_LOG_FMT(SERIALINTERFACE_JVSIO, "JVS-IO: Cancel Button: {}",
                 (bits_set[0] & CANCEL_LIGHT_BIT) ? "ON" : "OFF");
  }
}

void VirtuaStriker4Common_IOAdapter::Update()
{
  const auto generic_outputs = GetIOPorts()->GetGenericOutputs();

  const bool is_slot_a_locked = generic_outputs[0] & SLOT_A_LOCK_BIT;
  const bool is_slot_b_locked = generic_outputs[0] & SLOT_B_LOCK_BIT;

  // FYI: VS4 and VS4_2006 won't accept cards when both are presented at the same time.
  // It expects to load and lock one before the second is inserted.

  if (is_slot_a_locked)
  {
    // If slot 1 is locked, insert slot 2.
    if (m_card_reader_b->IsReadyToInsertCard())
      m_card_reader_b->InsertCard();
  }
  else
  {
    // If slot 1 is unlocked, remove slot 2 if unlocked.
    if (m_card_reader_b->IsCardPresent() && !is_slot_b_locked)
      m_card_reader_b->EjectCard();
  }

  if (m_card_reader_a->IsReadyToInsertCard())
    m_card_reader_a->InsertCard();

  const auto p1_inputs = GetIOPorts()->GetSwitchInputs(0);
  const auto p2_inputs = GetIOPorts()->GetSwitchInputs(1);

  // Bit 0x10 of each player's 1st byte is a card presence switch.
  Common::SetBit(p1_inputs[0], 4, m_card_reader_a->IsCardPresent());
  Common::SetBit(p2_inputs[0], 4, m_card_reader_b->IsCardPresent());

  // Bit 0x20 of each player's 2nd byte is some kind of "eject" sensor.
  Common::SetBit(p1_inputs[1], 5, m_card_reader_a->IsEjecting());
  Common::SetBit(p2_inputs[1], 5, m_card_reader_b->IsEjecting());
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
    m_card_reader_a->EjectCard();

  if (bits_set[0] & SLOT_B_EJECT_BIT)
    m_card_reader_b->EjectCard();
}

void GekitouProYakyuu_IOAdapter::Update()
{
  // Gekitou isn't as picky as VS4. We can insert both cards simultaneously.
  for (const auto& card_reader : {m_card_reader_a, m_card_reader_b})
  {
    if (card_reader->IsReadyToInsertCard())
      card_reader->InsertCard();
  }

  const auto p1_inputs = GetIOPorts()->GetSwitchInputs(0);
  const auto p2_inputs = GetIOPorts()->GetSwitchInputs(1);

  // Bit 0x40 of each player's 2nd byte is a card presence switch.
  Common::SetBit(p1_inputs[1], 6, m_card_reader_a->IsCardPresent());
  Common::SetBit(p2_inputs[1], 6, m_card_reader_b->IsCardPresent());
}

void GekitouProYakyuu_IOAdapter::HandleGenericOutputsChanged(std::span<const u8> bits_set,
                                                             std::span<const u8> bits_cleared)
{
  (void)bits_set;

  // When a card is inserted Gekitou sets the relevant "lock" bit.
  // When done with the card it clears this bit, then expects the card presence to be cleared.
  // I guess we need to treat this as an alternative to ICCardCommand::Eject.

  if (bits_cleared[0] & SLOT_A_LOCK_BIT)
    m_card_reader_a->EjectCard();

  if (bits_cleared[0] & SLOT_B_LOCK_BIT)
    m_card_reader_b->EjectCard();
}

void KeyOfAvalon_IOAdapter::Update()
{
  // Note that the game sometimes does a few InsertCheck when it still wants a card.
  constexpr u8 insert_check_count_before_removing_card = 10;

  if (m_card_reader->GetCardPresentInsertCheckCount() >= insert_check_count_before_removing_card)
    m_card_reader->EjectCard();

  if (m_card_reader->IsReadyToInsertCard())
    m_card_reader->InsertCard();
}

}  // namespace Triforce
