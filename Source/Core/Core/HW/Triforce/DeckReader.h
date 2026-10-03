// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <algorithm>
#include <map>
#include <span>

#include "Common/BitUtils.h"
#include "Common/DirectIOFile.h"
#include "Common/Swap.h"

#include "Core/HW/Triforce/ICCardReader.h"
#include "Core/HW/Triforce/SerialDevice.h"

class PointerWrap;

namespace Triforce
{

// This structure mirrors the 3 bytes per card the deck reader hardware produces.
// It seems to be some sort of not-yet-understood transformation of the scanned barcode.
// Multiple "identifiers" may ultimately map to the same card "number" in game.
// i.e. It seems some physical cards were re-released with different barcodes.
#pragma pack(push, 1)
struct CardIdentifier
{
  // When the 0x01 bit is set, Avalon indexes a separate smaller table.
  // Avalon specifically requires 0x80, 0x40, and 0x20 bits are not set.
  // We don't know the relevance of this second table.
  // There are even some duplicates between the two tables.
  u8 table_index;

  Common::BigEndianValue<u16> card_index;

  bool operator==(const CardIdentifier& other) const
  {
    return std::ranges::equal(Common::AsU8Span(*this), Common::AsU8Span(other));
  }
};
#pragma pack(pop)

// Used for UX purposes, to map a printed card name/number to the game's internal "identifier".
struct CardDatabaseEntry
{
  std::string name_eng;
  std::string name_jpn;

  // Card category: yellow, blue, red, green, magic, or support.
  // Just the first letter, in lowercase.
  std::string attribute;

  // String of lowercase movement color letters, 'w' for white/colorless.
  std::string movement;

  std::optional<int> attack;
  std::optional<int> defense;

  // FYI: We just load the first identifier (the transformed barcode) for each card.
  // The game seems to treat them all identically.
  CardIdentifier card_id;
};

// The map key is the printed card number, e.g. "N27" or "Ex11".
using CardDatabase = std::map<std::string, CardDatabaseEntry>;

CardDatabase LoadCardDatabase();

// It's semi-odd to use CardIdentifier here when cards may be specified by "number".
using CardDeck = std::vector<CardIdentifier>;

std::optional<CardDeck> LoadCardDeck(const CardDatabase&);
std::optional<CardDeck> LoadDefaultCardDeck(const CardDatabase&);

struct DeckEntry
{
  std::string number;  // The printed card number.
  int quantity;
};

bool SaveCardDeck(std::span<DeckEntry> deck);

// Serial deck reader used by The Key of Avalon games.
class DeckReader final : public SerialDevice
{
public:
  void Update() override;

  void DoState(PointerWrap& p) override;

  auto* GetICCardReader() { return &m_ic_card_reader; }

private:
  // It seems that the IC Card Reader must be connected through the Deck Reader.
  // The Deck Reader forwards appropriate commands to the IC Card Reader.
  // IC Card Reader responses are passed through back to the baseboard as-is.
  // It can't be the other way around because FirmwareUpdate sends many raw bytes.
  // Unless FirmwareUpdate temporarily cuts off the IC Card Reader ?
  ICCardReader m_ic_card_reader{0};

  void HandleFirmwareUpdate();

  u8 m_firmware_update_timeout = 0;

  File::DirectIOFile m_firmware_dump_file;
};

}  // namespace Triforce
