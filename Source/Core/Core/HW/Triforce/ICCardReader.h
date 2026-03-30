// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>
#include <memory>
#include <span>
#include <string>
#include <vector>

#include "Common/CommonTypes.h"

#include "Core/HW/DVD/AMMediaboard.h"
#include "Core/HW/Triforce/SerialDevice.h"

namespace Triforce
{

// IC card reader.
// Used by: GekitouProYakyuu, KeyOfAvalon, VirtuaStriker4[_2006]
class ICCardReader final : public SerialDevice
{
public:
  explicit ICCardReader(u8 slot_index);

  void Update() override;

  static bool CreateBlankCardFile(const std::string& filename, AMMediaboard::GameType game_type);

  bool IsCardPresent() const;
  bool IsEjecting() const;
  bool IsReadyToInsertCard() const;
  bool WantsCardInserted() const { return m_want_card_inserted; }

  void InsertCard();
  void EjectCard();

  void SetConfiguredCardFiles(std::vector<std::string> card_filenames);
  const std::vector<std::string>& GetConfiguredCardFiles() const
  {
    return m_configured_card_filenames;
  }

  void SetWantsCardInserted(bool want_card_inserted) { m_want_card_inserted = want_card_inserted; }

  u8 GetCardPresentInsertCheckCount() const { return m_card_present_insert_check_count; }

  void DoState(PointerWrap& p) override;

private:
  void CreateCards(u8 card_count);

  void SendReply(u8 command, u16 status_code, std::span<const u8> payload);

  static constexpr std::size_t PAGE_SIZE = 8;
  static constexpr u32 PAGE_COUNT = 256;

  // TODO: Maybe we're supposed to error for out-of-bound pages instead?
  static constexpr u32 PAGE_INDEX_MASK = 0xff;

  class ICCard
  {
  public:
    using UID = std::array<u8, PAGE_SIZE>;

    ICCard(std::string filename, const UID& uid, AMMediaboard::GameType game_type);

    // Load from file if it exists, else create fresh card data.
    void Initialize();

    const UID& GetUID() const { return m_uid; }
    const std::string& GetFilename() const { return m_filename; }
    bool IsHalted() const { return m_current_state == State::Halted; }

    // Returns an empty span on error.
    std::span<const u8> ReadData(u16 page, u16 page_count);

    bool WriteData(u16 page, std::span<const u8> write_span);

    // Returns the new value.
    u16 DecreaseUseCount(u16 page, u16 amount);

    void SetHalted() { m_current_state = State::Halted; }
    void SetIdle() { m_current_state = State::Idle; }

    void DoState(PointerWrap& p);

  private:
    void FlushData(u32 byte_offset, u32 byte_count);

    std::array<u8, PAGE_SIZE * PAGE_COUNT> m_data{};

    const std::string m_filename;

    const UID m_uid;
    const AMMediaboard::GameType m_game_type;

    enum class State : u8
    {
      Idle = 0,
      Halted = 1,
    };

    // I believe "Halted" cards don't respond in "AntiCollision".
    State m_current_state = State::Idle;
  };

  // FYI: These are more like multiple RFID tags on a single card.
  // Avalon makes use of this for not yet fully understood reasons.
  std::vector<std::unique_ptr<ICCard>> m_ic_cards;

  std::vector<std::string> m_configured_card_filenames;

  bool m_is_field_on = false;
  bool m_want_card_inserted = true;

  const u8 m_slot_index;

  u8 m_eject_timer{};

  // Avalon spams InsertCheck when it's waiting for a card to be removed.
  // We listen for that to automatically remove the card.
  u8 m_card_present_insert_check_count{};
};

}  // namespace Triforce
