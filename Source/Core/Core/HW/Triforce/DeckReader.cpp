// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/HW/Triforce/DeckReader.h"

#include <string_view>

#include <fmt/ranges.h>

#include <picojson.h>

#include "Common/BitUtils.h"
#include "Common/ChunkFile.h"
#include "Common/FileUtil.h"
#include "Common/JsonUtil.h"
#include "Common/Logging/Log.h"
#include "Common/ScopeGuard.h"
#include "Common/Swap.h"

namespace
{

// Note: Avalon literally skips `strlen("Version ")` bytes in the display.
constexpr std::string_view CDR_PROGRAM_VERSION = "Version 1.22,2003/09/19,171-8213B";
constexpr std::string_view CDR_BOOT_VERSION = "Version 1.04,2003/06/17,171-8213B";

constexpr std::size_t EXPECTED_VERSION_STR_LENGTH = 48;

// Avalon seems to not care about the actual value.
constexpr u32 SHUTTER_TIME = 1234567890;

constexpr u32 FIRMWARE_UPDATE_TIMEOUT = 240;

auto GetFirmwareDumpFilename()
{
  return fmt::format("{}card_deck_reader_firmware.bin", File::GetUserPath(D_TRIUSER_IDX));
}

auto GetCardDatabaseFilename()
{
  // TODO: Better name ?
  return fmt::format("{}avalon_card_database.json", File::GetSysDirectory());
}

auto GetCardDeckFilename()
{
  // TODO: Better name ?
  return fmt::format("{}tricard_deck.json", File::GetUserPath(D_TRIUSER_IDX));
}

auto GetDefaultCardDeckFilename()
{
  // TODO: Better name ?
  return fmt::format("{}avalon_default_deck.json", File::GetSysDirectory());
}

enum class CDReaderCommand : u8
{
  ShutterAuto = 0x61,
  BootVersion = 0x62,
  SensLock = 0x63,
  SensCard = 0x65,
  FirmwareUpdate = 0x66,
  ShutterGet = 0x67,
  CameraCheck = 0x68,
  Shutter = 0x69,
  ProgramChecksum = 0x6b,
  ShutterAlt = 0x6c,  // Not sure what the difference is. Avalon seems to only use 0x69.
  BootChecksum = 0x6d,
  ShutterLoad = 0x6f,
  ReadCard = 0x72,
  ShutterSave = 0x73,
  SelfTest = 0x74,
  ProgramVersion = 0x76,
};

}  // namespace

namespace Triforce
{

CardDatabase LoadCardDatabase()
{
  CardDatabase result;

  std::string file_contents;
  File::ReadFileToString(GetCardDatabaseFilename(), file_contents);

  picojson::value json_root;
  const auto err = picojson::parse(json_root, file_contents);

  if (!err.empty())
  {
    ERROR_LOG_FMT(SERIALINTERFACE_CARD, "LoadCardDatabaseFromFile: {}", err);

    return result;
  }

  if (!json_root.is<picojson::object>())
  {
    ERROR_LOG_FMT(SERIALINTERFACE_CARD, "LoadCardDatabaseFromFile: Invalid JSON root object.");
    return result;
  }

  const auto cards_obj = json_root.get("cards");
  if (!cards_obj.is<picojson::array>())
  {
    ERROR_LOG_FMT(SERIALINTERFACE_CARD, "LoadCardDatabaseFromFile: Invalid \"cards\" array.");
    return result;
  }

  for (const auto& item : cards_obj.get<picojson::array>())
  {
    if (!item.is<picojson::object>())
      continue;

    const auto& item_obj = item.get<picojson::object>();

    auto card_number = ReadStringFromJson(item_obj, "number");
    if (!card_number)
    {
      ERROR_LOG_FMT(SERIALINTERFACE_CARD, "LoadCardDatabaseFromFile: Invalid card number.");
      continue;
    }

    CardDatabaseEntry card_db_entry;

    card_db_entry.name_eng = ReadStringFromJson(item_obj, "name_eng").value_or("");
    card_db_entry.name_jpn = ReadStringFromJson(item_obj, "name_jpn").value_or("");

    card_db_entry.attribute = ReadStringFromJson(item_obj, "attrib").value_or("");

    card_db_entry.movement = ReadStringFromJson(item_obj, "movement").value_or("");

    card_db_entry.attack = ReadNumericFromJson<int>(item_obj, "atk");
    card_db_entry.defense = ReadNumericFromJson<int>(item_obj, "def");

    // TODO: Better name than "entries" ?
    const auto table_indices = item.get("entries");
    if (!table_indices.is<picojson::array>())
    {
      ERROR_LOG_FMT(SERIALINTERFACE_CARD, "LoadCardDatabaseFromFile: Invalid \"entries\" array.");
      continue;
    }

    for (const auto& table_index : table_indices.get<picojson::array>())
    {
      if (!table_index.is<picojson::object>())
        continue;

      const auto& table_index_obj = table_index.get<picojson::object>();

      const auto card_index = ReadNumericFromJson<u16>(table_index_obj, "index");
      if (!card_index)
      {
        ERROR_LOG_FMT(SERIALINTERFACE_CARD, "LoadCardDatabaseFromFile: Invalid \"index\" field.");
        continue;
      }

      card_db_entry.card_id.card_index = *card_index;
      card_db_entry.card_id.table_index =
          ReadNumericFromJson<u8>(table_index_obj, "table").value_or(0);

      // We currently just use the first valid identifier.
      result.emplace(*card_number, std::move(card_db_entry));
      break;
    }
  }

  INFO_LOG_FMT(SERIALINTERFACE_CARD, "LoadCardDatabaseFromFile: Loaded {} cards.", result.size());

  return result;
}

static std::optional<CardDeck> LoadCardDeckFromFile(const CardDatabase& card_database,
                                                    const std::string& filename)
{
  // Example json format:
  // index/table are used if number is unspecified.
  // table defaults to 0. quantity defaults to 1.
  //
  // {
  //   "cards": [
  //     {
  //       "number": "C23",
  //       "quantity": 2
  //     },
  //     {
  //       "index": 12
  //     },
  //     {
  //       "table": 1,
  //       "index": 13
  //     },
  //     {
  //       "index": 18,
  //       "quantity": 3
  //     }
  //   ]
  // }

  std::string file_contents;
  File::ReadFileToString(filename, file_contents);

  picojson::value json_root;
  const auto err = picojson::parse(json_root, file_contents);

  if (!err.empty())
  {
    ERROR_LOG_FMT(SERIALINTERFACE_CARD, "LoadCardDeckFromFile: {}", err);
    return std::nullopt;
  }

  if (!json_root.is<picojson::object>())
  {
    ERROR_LOG_FMT(SERIALINTERFACE_CARD, "LoadCardDeckFromFile: Invalid JSON root object.");
    return std::nullopt;
  }

  const auto cards_obj = json_root.get("cards");
  if (!cards_obj.is<picojson::array>())
  {
    ERROR_LOG_FMT(SERIALINTERFACE_CARD, "LoadCardDeckFromFile: Invalid \"cards\" array.");
    return std::nullopt;
  }

  std::optional<std::vector<CardIdentifier>> result;
  result.emplace();

  for (const auto& item : cards_obj.get<picojson::array>())
  {
    if (!item.is<picojson::object>())
      continue;

    const auto& item_obj = item.get<picojson::object>();

    CardIdentifier card_id{};

    const auto card_number = ReadStringFromJson(item_obj, "number");
    if (card_number)
    {
      auto card_db_entry = card_database.find(*card_number);
      if (card_db_entry != card_database.end())
      {
        card_id = card_db_entry->second.card_id;
      }
      else
      {
        ERROR_LOG_FMT(SERIALINTERFACE_CARD,
                      "LoadCardDeckFromFile: Card number \"{}\" not found in database.",
                      *card_number);
      }
    }
    else
    {
      // Fall back to "table" and "index" if "number" isn't specified.

      const auto card_index = ReadNumericFromJson<u16>(item_obj, "index");
      if (!card_index)
      {
        ERROR_LOG_FMT(SERIALINTERFACE_CARD, "LoadCardDeckFromFile: Invalid \"index\" field.");
        continue;
      }

      card_id.card_index = *card_index;
      card_id.table_index = ReadNumericFromJson<u8>(item_obj, "table").value_or(0);
    }

    const auto quantity = ReadNumericFromJson<u8>(item_obj, "quantity").value_or(1);
    result->resize(result->size() + quantity, card_id);
  }

  return result;
}

std::optional<CardDeck> LoadCardDeck(const CardDatabase& card_database)
{
  auto result = LoadCardDeckFromFile(card_database, GetCardDeckFilename());

  // Fall back to default deck.
  if (!result)
    result = LoadDefaultCardDeck(card_database);

  return result;
}

std::optional<CardDeck> LoadDefaultCardDeck(const CardDatabase& card_database)
{
  return LoadCardDeckFromFile(card_database, GetDefaultCardDeckFilename());
}

bool SaveCardDeck(std::span<DeckEntry> deck)
{
  picojson::array cards;

  for (const auto& entry : deck)
  {
    picojson::object card_obj;
    card_obj["number"] = picojson::value(entry.number);
    if (entry.quantity != 1)
      card_obj["quantity"] = picojson::value(double(entry.quantity));

    cards.emplace_back(std::move(card_obj));
  }

  picojson::object obj;
  obj["cards"] = picojson::value(std::move(cards));

  const std::string json = picojson::value(std::move(obj)).serialize(true);

  return File::WriteStringToFile(GetCardDeckFilename(), json);
}

void DeckReader::Update()
{
  Common::ScopeGuard update_ic_card_reader{[this] {
    // Update and take any response data from the IC card reader.
    m_ic_card_reader.Update();
    PassThroughTxBytes(m_ic_card_reader);
  }};

  // All Deck Reader commands are exactly one byte (except FirmwareUpdate).
  // Responses must be the correct length, which varies by command.
  // Only the ReadCard response actually includes some kind of length field.
  //
  // A leading 2 bytes and trailing 1 byte are expected for all responses.
  // Avalon seems to largely not check the actual values,
  //  but a few handlers do verify them.

  // FirmwareUpdate is the only known stateful command.
  if (m_firmware_update_timeout != 0)
  {
    HandleFirmwareUpdate();
    return;
  }

  const auto input_span = GetRxByteSpan();

  if (input_span.empty())
    return;  // Wait for more data.

  const u8 cd_reader_command = input_span.front();

  if (cd_reader_command == 0x00)
  {
    // This is an IC Card Reader command.

    // How does the actual hardware deal with this ?
    // Does the Deck Reader really parse the IC Card Reader commands ?

    if (input_span.size() < 4)
      return;  // Wait for more data.

    const u16 input_payload_size = Common::swap16(input_span.data() + 2);
    const u32 total_request_size = input_payload_size + 5u;

    if (input_span.size() >= total_request_size)
    {
      // Forward the data to the attached IC Card Reader.
      m_ic_card_reader.WriteRxBytes(input_span.first(total_request_size));
      ConsumeRxBytes(total_request_size);
    }

    return;
  }

  // Write header.
  WriteTxBytes(std::array<u8, 2>{0xaa, cd_reader_command});

  switch (CDReaderCommand(cd_reader_command))
  {
  case CDReaderCommand::SelfTest:
  {
    INFO_LOG_FMT(SERIALINTERFACE_CARD, "SelfTest");

    // Avalon appears to not inspect this value.
    WriteTxByte(0x00);

    break;
  }
  case CDReaderCommand::SensLock:
  {
    INFO_LOG_FMT(SERIALINTERFACE_CARD, "SensLock");

    // Avalon will ask the user to close the shutter if open.
    constexpr bool is_closed = true;

    WriteTxByte(is_closed ? 0x01 : 0x00);

    break;
  }
  case CDReaderCommand::ProgramVersion:
  {
    INFO_LOG_FMT(SERIALINTERFACE_CARD, "ProgramVersion");

    WriteTxBytes(Common::AsU8Span((CDR_PROGRAM_VERSION)));

    // Pad with zeros.
    WriteTxBytes(std::array<u8, EXPECTED_VERSION_STR_LENGTH - CDR_PROGRAM_VERSION.size()>{});

    break;
  }
  case CDReaderCommand::BootVersion:
  {
    INFO_LOG_FMT(SERIALINTERFACE_CARD, "BootVersion");

    WriteTxBytes(Common::AsU8Span(CDR_BOOT_VERSION));

    // Pad with zeros.
    WriteTxBytes(std::array<u8, EXPECTED_VERSION_STR_LENGTH - CDR_BOOT_VERSION.size()>{});

    break;
  }
  case CDReaderCommand::ProgramChecksum:
  {
    INFO_LOG_FMT(SERIALINTERFACE_CARD, "ProgramChecksum");

    // Avalon seems to not care about the actual value.
    Common::BigEndianValue<u32> fake_checksum{0x89abcdef};
    WriteTxBytes(Common::AsU8Span(fake_checksum));

    break;
  }
  case CDReaderCommand::BootChecksum:
  {
    INFO_LOG_FMT(SERIALINTERFACE_CARD, "BootChecksum");

    // Avalon seems to not care about the actual value.
    Common::BigEndianValue<u32> fake_checksum{0x01234567};
    WriteTxBytes(Common::AsU8Span(fake_checksum));

    break;
  }
  case CDReaderCommand::CameraCheck:
  {
    INFO_LOG_FMT(SERIALINTERFACE_CARD, "CameraCheck");

    // FYI: If `header[1] != 0x68` the game inspects some bits and 9 bytes. Some error state ?

    std::array<u8, 10> response{};
    WriteTxBytes(response);

    break;
  }
  case CDReaderCommand::ShutterGet:
  {
    INFO_LOG_FMT(SERIALINTERFACE_CARD, "ShutterGet");

    Common::BigEndianValue<u32> shutter_time{SHUTTER_TIME};
    WriteTxBytes(Common::AsU8Span(shutter_time));

    break;
  }
  case CDReaderCommand::ShutterAuto:
  {
    INFO_LOG_FMT(SERIALINTERFACE_CARD, "ShutterAuto");

    Common::BigEndianValue<u32> shutter_time{SHUTTER_TIME};
    WriteTxBytes(Common::AsU8Span(shutter_time));

    break;
  }
  case CDReaderCommand::Shutter:
  case CDReaderCommand::ShutterAlt:
  {
    INFO_LOG_FMT(SERIALINTERFACE_CARD, "Shutter");
    break;
  }
  case CDReaderCommand::ShutterLoad:
  {
    INFO_LOG_FMT(SERIALINTERFACE_CARD, "ShutterLoad");
    break;
  }
  case CDReaderCommand::ShutterSave:
  {
    INFO_LOG_FMT(SERIALINTERFACE_CARD, "ShutterSave");
    break;
  }
  case CDReaderCommand::SensCard:
  {
    INFO_LOG_FMT(SERIALINTERFACE_CARD, "SensCard");

    // TODO: What is the meaning of this value ?
    // Avalon tests for 0x01.
    WriteTxByte(0x01);

    break;
  }
  case CDReaderCommand::ReadCard:
  {
    INFO_LOG_FMT(SERIALINTERFACE_CARD, "ReadCard");

    constexpr bool is_deck_empty = false;

    if (is_deck_empty)
    {
      // I think this means that there are no cards or maybe "not ready".
      WriteTxBytes(std::array<u8, 2>{0xaa, 0x52});
    }
    else
    {
      // We've had one, yes, but what about second header ?
      WriteTxBytes(std::array<u8, 2>{0xaa, u8(CDReaderCommand::ReadCard)});

      const auto card_database = LoadCardDatabase();

      if (const auto deck = LoadCardDeck(card_database))
      {
        // What happens with more than 30 cards ?
        WriteTxByte(u8(deck->size() * sizeof(CardIdentifier)));

        WriteTxBytes(Common::AsU8Span(*deck));
      }
      else
      {
        constexpr u8 deck_size = 0;
        WriteTxByte(deck_size);
      }
    }

    break;
  }
  case CDReaderCommand::FirmwareUpdate:
  {
    INFO_LOG_FMT(SERIALINTERFACE_CARD, "FirmwareUpdate");

    m_firmware_update_timeout = FIRMWARE_UPDATE_TIMEOUT;

    // The game will now send the raw firmware data which we write to a file.
    // FYI: It appears to be SuperH4:LE code.

    const auto filename = GetFirmwareDumpFilename();
    if (m_firmware_dump_file.Open(filename, File::AccessMode::Write))
    {
      NOTICE_LOG_FMT(SERIALINTERFACE_CARD, "Writing firmware to: {}", filename);
    }
    else
    {
      ERROR_LOG_FMT(SERIALINTERFACE_CARD, "Failed to open: {}", filename);
    }

    break;
  }
  default:
  {
    // TODO: I think maybe we're supposed to produce a header of {0xaa, 0x55} ?

    // Responses seem to be variable in length and unspecified so we can't do much.
    ERROR_LOG_FMT(SERIALINTERFACE_CARD, "Unknown command: {:02x}", cd_reader_command);
    break;
  }
  }

  // Write footer.
  // TODO: Is this an error code maybe ?
  WriteTxByte(0x00);

  // Every known command (other than FirmwareUpdate) is just one byte.
  ConsumeRxBytes(1);
}

void DeckReader::HandleFirmwareUpdate()
{
  const auto input_span = GetRxByteSpan();

  // No idea how the real hardware works.
  // There doesn't seem to be any sort of size field so.. here's a timeout.
  if (input_span.empty())
  {
    if (--m_firmware_update_timeout == 0)
    {
      constexpr std::array<u8, 3> fw_update_done{0xaa, u8(CDReaderCommand::FirmwareUpdate), 0x00};
      WriteTxBytes(fw_update_done);

      INFO_LOG_FMT(SERIALINTERFACE_CARD, "Fake FirmwareUpdate done.");

      m_firmware_dump_file.Close();
    }
  }
  else
  {
    if (m_firmware_dump_file.IsOpen())
    {
      if (!m_firmware_dump_file.Write(input_span))
      {
        ERROR_LOG_FMT(SERIALINTERFACE_CARD, "Failed to write {} bytes to firmware.",
                      input_span.size());
      }
    }

    ConsumeRxBytes(input_span.size());
    m_firmware_update_timeout = FIRMWARE_UPDATE_TIMEOUT;
  }
}

void DeckReader::DoState(PointerWrap& p)
{
  SerialDevice::DoState(p);

  m_ic_card_reader.DoState(p);

  p.Do(m_firmware_update_timeout);

  if (p.IsReadMode())
    m_firmware_dump_file.Close();
}

}  // namespace Triforce
