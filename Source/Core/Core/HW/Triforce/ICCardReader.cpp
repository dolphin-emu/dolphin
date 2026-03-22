// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/HW/Triforce/ICCardReader.h"

#include <numeric>

#include <fmt/ranges.h>

#include "Common/BitUtils.h"
#include "Common/ChunkFile.h"
#include "Common/DirectIOFile.h"
#include "Common/FileUtil.h"
#include "Common/Logging/Log.h"
#include "Common/ScopeGuard.h"
#include "Common/Swap.h"

#include "Core/ConfigManager.h"
#include "Core/HW/DVD/AMMediaboard.h"

namespace
{

// We return a card session with our card index in the lower bits.
constexpr u16 CARD_SESSION_BASE = 0x2300;

// The serial number is located here. Games seem to expect it to be read-only.
constexpr u32 READ_ONLY_PAGE_INDEX = 4;

constexpr u8 CheckSumXOR(std::span<const u8> data)
{
  return std::accumulate(data.data(), data.data() + data.size(), u8{}, std::bit_xor{});
}

bool LoadCardData(const std::string& filename, std::span<u8> data)
{
  File::DirectIOFile file{filename, File::AccessMode::Read};

  if (!File::Exists(filename))
    return false;

  const auto file_size = file.GetSize();
  if (file_size > data.size())
  {
    WARN_LOG_FMT(SERIALINTERFACE_CARD, "Unexpected size of {} for file: {}", file_size, filename);
  }

  const auto read_size = std::min<u64>(file_size, data.size());

  if (!file.Read(std::span{data}.first(read_size)))
  {
    ERROR_LOG_FMT(SERIALINTERFACE_CARD, "Failed to read from file: {}", filename);
    return false;
  }

  INFO_LOG_FMT(SERIALINTERFACE_CARD, "Loaded {} bytes from file: {}", read_size, filename);
  return true;
}

void SanitizeSerialNumber(std::span<u8> data)
{
  DEBUG_ASSERT(data.size() == 8);

  // This seems to be treated as a 16-digit BCD value.

  // Avalon: 9571 264_ XXXX XXXX
  // Where _ is mod 10 of the sum of all the X digits.

  // VirtuaStriker4: 9571 440_ XXXX XXXX
  // I think this might be generated from the SN physically printed on the card.
  // The game code that deals with this is very complicated.
  // We're lucky that all zeros seems to work.

  // Gekitou: 9571 765_ XXXX XXXX
  // How does the checksum work ?

  data[0] = 0x95;
  data[1] = 0x71;

  switch (AMMediaboard::GetGameType())
  {
  case AMMediaboard::KeyOfAvalon:
  {
    data[2] = 0x26;
    data[3] = 0x40;

    u32 checksum = 0;
    for (auto bcd_pair : data.subspan(4))
      checksum += (bcd_pair & 0x0f) + (bcd_pair >> 4);
    data[3] |= (checksum % 10);

    break;
  }
  case AMMediaboard::VirtuaStriker4:
  case AMMediaboard::VirtuaStriker4_2006:
  {
    data[2] = 0x44;
    data[3] = 0x00;
    break;
  }
  case AMMediaboard::GekitouProYakyuu:
  {
    data[2] = 0x76;
    data[3] = 0x50;
    break;
  }
  default:
    break;
  }
}

void InitializeCardData(std::span<u8> data)
{
  SanitizeSerialNumber(data.subspan(READ_ONLY_PAGE_INDEX * 8, 8));

  constexpr u32 use_count_offset = 0x28;

  constexpr u16 use_count = 0;

  // The use count seems to be a big endian count down from 0xffff.
  Common::WriteSwap16(data.data() + use_count_offset, 0xffff - use_count);
}

// TODO: I'm not so sure that this is really a device status.
// The values that Avalon seems to test for seem very command-specific.
// It might be more like a command result code.
enum ICCardStatus : u16
{
  Okay = 0x0000,
  NoCard = 0x8000,
  Unknown = 0x800e,
  BadCard = 0xffff,
  // TODO: Figure out what's actually returned in these situations.
  HardwareUntestedErrorCode = 0x0080,
};

enum class ICCardCommand : u8
{
  GetStatus = 0x10,
  SetBaudrate = 0x11,
  FieldOn = 0x14,
  FieldOff = 0x15,
  EjectCard = 0x16,
  InsertCheck = 0x20,
  AntiCollision = 0x21,
  SelectCard = 0x22,
  ReadPage = 0x24,
  WritePage = 0x25,
  DecreaseUseCount = 0x26,
  HaltCard = 0x27,
  ReadUseCount = 0x33,
  ReadPages = 0x34,
  WritePages = 0x35,
};

}  // namespace

namespace Triforce
{

ICCardReader::ICCardReader(u8 slot_index) : m_slot_index{slot_index}
{
}

void ICCardReader::CreateCards(u8 card_count)
{
  m_ic_cards.clear();

  // Filled in with slot index and card number just for uniqueness.
  ICCard::UID card_id = {0x00, 0x00, 0x54, 0x4D, 0x50, 0x00, m_slot_index, 0x00};

  for (u8 i = 0; i != card_count; ++i)
  {
    std::string slot_name = fmt::format("slot{}", m_slot_index + 1);

    // Files for additional tags get naming like "slot1b.bin"
    if (i > 0)
      slot_name += char('a' + i);

    const auto filename = fmt::format("{}tricard_{}_{}.bin", File::GetUserPath(D_TRIUSER_IDX),
                                      SConfig::GetInstance().GetGameID(), slot_name);

    card_id.back() = i;

    m_ic_cards.emplace_back(std::make_unique<ICCard>(filename, card_id));
  }
}

void ICCardReader::Update()
{
  if (m_eject_timer != 0)
    --m_eject_timer;

  const auto rx_byte_span = GetRxByteSpan();
  if (rx_byte_span.size() < 4)
    return;  // Wait for more data.

  // For reference:
  // struct RequestPacket
  // {
  //   u8 fixed;   // Seems to be always 0x00.
  //   u8 ic_card_command;
  //   u16 payload_size;  // Big-endian.
  //   u8 payload[payload_size];
  //   u8 checksum;  // XOR of all previous bytes.
  // };

  const u16 input_payload_size = Common::swap16(rx_byte_span.data() + 2);
  // 4 header bytes + 1 checksum byte
  const u32 entire_request_size = input_payload_size + 5u;

  if (rx_byte_span.size() < entire_request_size)
    return;  // Wait for more data.

  if (CheckSumXOR(rx_byte_span.first(entire_request_size)) != 0)
  {
    ERROR_LOG_FMT(SERIALINTERFACE_CARD, "Bad checksum!");
    ConsumeRxBytes(1);
    return;
  }

  Common::ScopeGuard consume_request{[&] { ConsumeRxBytes(entire_request_size); }};

  const u8 card_command = rx_byte_span[1];
  const auto input_payload = rx_byte_span.subspan(4, input_payload_size);

  u16 status_code = 0x00;

  const auto validate_input_payload_size = [&](u32 expected_size) {
    if (input_payload_size < expected_size)
    {
      ERROR_LOG_FMT(SERIALINTERFACE_CARD, "Undersized payload size {} for command: {:02x}",
                    input_payload_size, card_command);

      status_code = HardwareUntestedErrorCode;
      return false;
    }

    if (input_payload_size > expected_size)
    {
      WARN_LOG_FMT(SERIALINTERFACE_CARD, "Oversized payload size {} for command: {:02x}",
                   input_payload_size, card_command);
    }

    return true;
  };

  const auto get_card_for_session = [&](u16 card_session) -> ICCard* {
    if ((card_session & 0xff00) == CARD_SESSION_BASE)
    {
      // FYI: We don't verify that this session was actually created before using it.
      // This is fine, but I'm sure it's not really hardware accurate.
      const std::size_t index = card_session & 0xffu;
      if (index < m_ic_cards.size())
        return m_ic_cards[index].get();
    }

    ERROR_LOG_FMT(SERIALINTERFACE_CARD, "Unexpected card session: {:04x}", card_session);

    status_code = HardwareUntestedErrorCode;
    return nullptr;
  };

  // Note: Command and response payload sizes are multiples of 8-bytes.
  std::array<u8, 8> small_reply_payload{};

  // Will be later assigned to `small_reply_payload` or some region of the card data itself.
  std::span<const u8> reply_payload_span;

  // FYI: Many of the big-endian u16 parameters may just be u8 values.
  // Avalon writes single bytes, but at odd addresses, so u16 seems like the intention.

  // TODO: Many of these functions should probably fail when the field is off.
  // It might make sense for that logic to be inside ICCard.

  if (ICCardCommand(card_command) != ICCardCommand::InsertCheck)
  {
    m_card_present_insert_check_count = 0;
  }

  switch (ICCardCommand(card_command))
  {
  case ICCardCommand::GetStatus:
  {
    if (!validate_input_payload_size(0))
      break;

    INFO_LOG_FMT(SERIALINTERFACE_CARD, "GetStatus");

    // Avalon's tests expect one of these specific values.
    status_code = m_is_field_on ? 0x30 : 0x20;

    break;
  }
  case ICCardCommand::SetBaudrate:
  {
    if (!validate_input_payload_size(8))
      break;

    INFO_LOG_FMT(SERIALINTERFACE_CARD, "SetBaudrate: {:02x}", fmt::join(input_payload, " "));
    break;
  }
  case ICCardCommand::FieldOn:
  {
    if (!validate_input_payload_size(0))
      break;

    INFO_LOG_FMT(SERIALINTERFACE_CARD, "FieldOn");

    m_is_field_on = true;

    // FYI: Avalon's tests expect status code here to be 0x0000.

    break;
  }
  case ICCardCommand::FieldOff:
  {
    if (!validate_input_payload_size(0))
      break;

    INFO_LOG_FMT(SERIALINTERFACE_CARD, "FieldOff");

    m_is_field_on = false;

    std::ranges::for_each(m_ic_cards, &ICCard::SetIdle);

    break;
  }
  case ICCardCommand::EjectCard:
  {
    if (!validate_input_payload_size(0))
      break;

    EjectCard();
    break;
  }
  case ICCardCommand::InsertCheck:
  {
    if (!validate_input_payload_size(8))
      break;

    // Avalon sends 0 or 1 here, not sure what the meaning is.
    // This is maybe the "Time Slot Number" (TSN)? 0=1-slot, 1=2-slots, etc.
    const u16 unknown_parameter = Common::swap16(input_payload.data() + 0);

    INFO_LOG_FMT(SERIALINTERFACE_CARD, "InsertCheck: {}", unknown_parameter);

    const bool is_card_present = std::ranges::any_of(m_ic_cards, std::not_fn(&ICCard::IsHalted));

    if (is_card_present &&
        m_card_present_insert_check_count <
            std::numeric_limits<decltype(m_card_present_insert_check_count)>::max())
    {
      ++m_card_present_insert_check_count;
    }

    // Avalon seems to test specifically for zero.
    status_code = is_card_present ? 0x00 : HardwareUntestedErrorCode;
    break;
  }
  case ICCardCommand::AntiCollision:
  {
    if (!validate_input_payload_size(16))
      break;

    // Avalon's logic optionally sets param0=0x20 and 8 memcpy'd bytes but never seems to do it.
    const u16 unknown_param0 = Common::swap16(input_payload.data() + 0);
    const auto unknown_param1 = input_payload.subspan(2, 8);

    INFO_LOG_FMT(SERIALINTERFACE_CARD, "AntiCollision: unk0:{:04x} unk1:{:02x}", unknown_param0,
                 fmt::join(unknown_param1, " "));

    const auto first_non_halted_card = std::ranges::find_if_not(m_ic_cards, &ICCard::IsHalted);

    if (first_non_halted_card != m_ic_cards.end())
    {
      reply_payload_span = (*first_non_halted_card)->GetUID();

      const bool additional_cards =
          std::ranges::count_if(m_ic_cards, std::not_fn(&ICCard::IsHalted)) > 1;

      // Avalon seems to like a value of 0x00 or 0x01. 0x01 causes two cards to be processed.
      status_code = additional_cards ? 0x01 : 0x00;
    }
    else
    {
      status_code = HardwareUntestedErrorCode;
    }

    break;
  }
  case ICCardCommand::SelectCard:
  {
    if (!validate_input_payload_size(8))
      break;

    // FYI: The request includes the UID that we produced in `AntiCollision`.
    const auto card_uid = input_payload.first(8);

    INFO_LOG_FMT(SERIALINTERFACE_CARD, "SelectCard: {:02x}", fmt::join(card_uid, " "));

    const auto found_card = std::ranges::find_if(
        m_ic_cards, std::bind_front(std::ranges::equal, card_uid), &ICCard::GetUID);

    if (found_card != m_ic_cards.end())
    {
      const u16 card_session = CARD_SESSION_BASE | u16(found_card - m_ic_cards.begin());

      Common::WriteSwap16(small_reply_payload.data(), card_session);
      reply_payload_span = small_reply_payload;

      // Avalon's tests specifically want 0x00.
      status_code = 0x00;
    }
    else
    {
      WARN_LOG_FMT(SERIALINTERFACE_CARD, "SelectCard: Unexpected Card ID.");

      status_code = HardwareUntestedErrorCode;
      break;
    }

    break;
  }
  // FYI: These two seem to have the same parameters.
  // Maybe ReadUseCount is meant to copy just 2 bytes? The response is still expected to be 8.
  case ICCardCommand::ReadPage:
  case ICCardCommand::ReadUseCount:
  {
    if (!validate_input_payload_size(8))
      break;

    const u16 card_session = Common::swap16(input_payload.data() + 0);
    const u16 page = Common::swap16(input_payload.data() + 2) & PAGE_INDEX_MASK;

    INFO_LOG_FMT(SERIALINTERFACE_CARD, "ReadPage: session:{:04x} page:{}", card_session, page);

    auto* const ic_card = get_card_for_session(card_session);
    if (!ic_card)
      break;

    const auto read_span = ic_card->ReadData(page, 1);
    if (read_span.empty())
    {
      status_code = HardwareUntestedErrorCode;
    }
    else
    {
      reply_payload_span = read_span;
    }

    break;
  }
  case ICCardCommand::WritePage:
  {
    if (!validate_input_payload_size(16))
      break;

    const u16 card_session = Common::swap16(input_payload.data() + 0);
    // Avalon specifically puts a zero here.
    const u16 unknown = Common::swap16(input_payload.data() + 2);
    const u16 page = Common::swap16(input_payload.data() + 4) & PAGE_INDEX_MASK;

    INFO_LOG_FMT(SERIALINTERFACE_CARD, "WritePage: session:{:04x} unk:{} page:{}", card_session,
                 unknown, page);

    auto* const ic_card = get_card_for_session(card_session);
    if (!ic_card)
      break;

    if (!ic_card->WriteData(page, input_payload.subspan(6, PAGE_SIZE)))
    {
      status_code = HardwareUntestedErrorCode;
    }

    break;
  }
  case ICCardCommand::DecreaseUseCount:
  {
    if (!validate_input_payload_size(8))
      break;

    const u16 card_session = Common::swap16(input_payload.data() + 0);
    // Avalon seems to always sends 5 and 1. Guessing on the meaning and behavior.
    const u16 page = Common::swap16(input_payload.data() + 2) & PAGE_INDEX_MASK;
    const u16 amount = Common::swap16(input_payload.data() + 4);

    INFO_LOG_FMT(SERIALINTERFACE_CARD, "DecreaseUseCount: session:{:04x}, page:{} amount:{}",
                 card_session, page, amount);

    auto* const ic_card = get_card_for_session(card_session);
    if (!ic_card)
      break;

    const u16 new_count = ic_card->DecreaseUseCount(page, amount);

    Common::WriteSwap16(small_reply_payload.data(), new_count);
    reply_payload_span = small_reply_payload;

    break;
  }
  case ICCardCommand::HaltCard:
  {
    if (!validate_input_payload_size(8))
      break;

    const u16 card_session = Common::swap16(input_payload.data() + 0);

    INFO_LOG_FMT(SERIALINTERFACE_CARD, "HaltCard: session:{:04x}", card_session);

    auto* const ic_card = get_card_for_session(card_session);
    if (!ic_card)
      break;

    ic_card->SetHalted();

    break;
  }
  case ICCardCommand::ReadPages:
  {
    if (!validate_input_payload_size(8))
      break;

    const u16 card_session = Common::swap16(input_payload.data() + 0);
    const u16 page = Common::swap16(input_payload.data() + 2) & PAGE_INDEX_MASK;
    const u16 page_count = Common::swap16(input_payload.data() + 4);

    INFO_LOG_FMT(SERIALINTERFACE_CARD, "ReadPages: session:{:04x} page:{} page_count:{}",
                 card_session, page, page_count);

    auto* const ic_card = get_card_for_session(card_session);
    if (!ic_card)
      break;

    const auto read_span = ic_card->ReadData(page, page_count);
    if (read_span.empty())
    {
      status_code = HardwareUntestedErrorCode;
    }
    else
    {
      reply_payload_span = read_span;
    }

    break;
  }
  case ICCardCommand::WritePages:
  {
    if (input_payload_size < 8)
    {
      ERROR_LOG_FMT(SERIALINTERFACE_CARD, "WritePages: Undersized payload size: {}",
                    input_payload_size);
      break;
    }

    const u16 card_session = Common::swap16(input_payload.data() + 0);
    const u32 page = Common::swap16(input_payload.data() + 2) & PAGE_INDEX_MASK;
    const u32 page_count = Common::swap16(input_payload.data() + 4);

    INFO_LOG_FMT(SERIALINTERFACE_CARD, "WritePages: session:{:04x} page:{} page_count:{}",
                 card_session, page, page_count);

    auto* const ic_card = get_card_for_session(card_session);
    if (!ic_card)
      break;

    const u32 byte_count = page_count * PAGE_SIZE;

    if (!validate_input_payload_size(8 + byte_count))
      break;

    if (!ic_card->WriteData(page, input_payload.subspan(6, byte_count)))
    {
      status_code = HardwareUntestedErrorCode;
    }

    break;
  }
  default:
  {
    ERROR_LOG_FMT(SERIALINTERFACE_CARD, "Unknown command: {:02x}", card_command);
    break;
  }
  }

  SendReply(card_command, status_code, reply_payload_span);
}

void ICCardReader::SendReply(u8 command, u16 status_code, std::span<const u8> payload)
{
  struct ICCardReplyHeader
  {
    u8 fixed = 0x10;  // Games seem to expect 0x10.
    u8 command;
    Common::BigEndianValue<u16> length;  // Includes status and payload bytes.
    Common::BigEndianValue<u16> status;
  };

  ICCardReplyHeader header{.command = command};
  header.length = u16(sizeof(header.status) + payload.size());
  header.status = status_code;

  const auto header_span = Common::AsU8Span(header);

  const u8 checksum = CheckSumXOR(header_span) ^ CheckSumXOR(payload);

  WriteTxBytes(header_span);
  WriteTxBytes(payload);
  WriteTxByte(checksum);
}

std::span<const u8> ICCardReader::ICCard::ReadData(u16 page, u16 page_count)
{
  const u32 byte_offset = page * PAGE_SIZE;
  const u32 byte_count = page_count * PAGE_SIZE;

  if (byte_count + byte_offset > m_data.size())
  {
    WARN_LOG_FMT(SERIALINTERFACE_CARD, "ReadData: Attempt to read beyond end of card.");
    return {};
  }

  const auto read_span = std::span{m_data}.subspan(byte_offset, byte_count);

  DEBUG_LOG_FMT(SERIALINTERFACE_CARD, "\n{}", HexDump(read_span));

  return read_span;
}

bool ICCardReader::ICCard::WriteData(u16 page, std::span<const u8> write_span)
{
  constexpr u32 read_only_area_begin = READ_ONLY_PAGE_INDEX * PAGE_SIZE;
  constexpr u32 read_only_area_end = read_only_area_begin + PAGE_SIZE;

  const u32 byte_offset = page * PAGE_SIZE;

  if ((byte_offset < read_only_area_end) &&
      (byte_offset + write_span.size() > read_only_area_begin))
  {
    WARN_LOG_FMT(SERIALINTERFACE_CARD, "WriteData: Read-only page.");
    // Read-only page, must return error.
    return false;
  }

  if (write_span.size() + byte_offset > m_data.size())
  {
    WARN_LOG_FMT(SERIALINTERFACE_CARD, "WriteData: Attempt to write beyond end of card.");
    return false;
  }

  std::ranges::copy(write_span, m_data.data() + byte_offset);

  DEBUG_LOG_FMT(SERIALINTERFACE_CARD, "\n{}", HexDump(write_span));

  FlushData(byte_offset, u32(write_span.size()));

  return true;
}

u16 ICCardReader::ICCard::DecreaseUseCount(u16 page, u16 amount)
{
  const u32 byte_offset = page * PAGE_SIZE;
  auto* const addr = m_data.data() + byte_offset;

  const u16 previous_use_count = Common::swap16(addr);

  // We're not sure how the real hardware functions when decreasing below 0.
  // We'll clamp it to 0 for now.
  const u16 new_use_count = MathUtil::SaturatingCast<u16>(s32(previous_use_count) - amount);

  NOTICE_LOG_FMT(SERIALINTERFACE_CARD, "DecreaseUseCount: {} -> {}", previous_use_count,
                 new_use_count);

  Common::WriteSwap16(addr, new_use_count);

  FlushData(byte_offset, sizeof(u16));

  return new_use_count;
}

// TODO: Maybe in the future we should write to disk after a delay.
// Games seem to write many chunks when saving.
void ICCardReader::ICCard::FlushData(u32 byte_offset, u32 byte_count)
{
  File::DirectIOFile file{m_filename, File::AccessMode::Write, File::OpenMode::Always};
  if (!file.OffsetWrite(byte_offset, std::span{m_data}.subspan(byte_offset, byte_count)))
  {
    ERROR_LOG_FMT(SERIALINTERFACE_CARD, "FlushData: Failed to write to: {}", m_filename);
  }
}

void ICCardReader::DoState(PointerWrap& p)
{
  SerialDevice::DoState(p);

  // TODO: Think about what to do with the card data on the filesystem.

  auto card_count = u8(m_ic_cards.size());
  p.Do(card_count);

  if (card_count != m_ic_cards.size())
    CreateCards(card_count);

  for (auto& card : m_ic_cards)
    card->DoState(p);

  p.Do(m_is_field_on);

  p.Do(m_eject_timer);
  p.Do(m_card_present_insert_check_count);
}

void ICCardReader::ICCard::DoState(PointerWrap& p)
{
  p.Do(m_data);
  p.Do(m_uid);  // UID is deterministically generated, but we'll sync it in case that changes.
  p.Do(m_current_state);
}

ICCardReader::ICCard::ICCard(std::string filename, const UID& uid)
    : m_filename{std::move(filename)}, m_uid{uid}
{
}

void ICCardReader::ICCard::Initialize()
{
  if (!LoadCardData(m_filename, m_data))
  {
    NOTICE_LOG_FMT(SERIALINTERFACE_CARD, "Creating new IC Card data.");
    InitializeCardData(m_data);
    FlushData(READ_ONLY_PAGE_INDEX * PAGE_SIZE, PAGE_SIZE * 2);
  }
}

bool ICCardReader::IsCardPresent() const
{
  return !m_ic_cards.empty();
}

bool ICCardReader::IsEjecting() const
{
  return m_eject_timer > 60;
}

bool ICCardReader::IsReadyToInsertCard() const
{
  return !IsCardPresent() && m_eject_timer == 0;
}

void ICCardReader::InsertCard()
{
  if (IsCardPresent())
    return;

  NOTICE_LOG_FMT(SERIALINTERFACE_CARD, "InsertCard");

  CreateCards(1);
  std::ranges::for_each(m_ic_cards, &ICCard::Initialize);
}

void ICCardReader::EjectCard()
{
  if (!IsCardPresent())
    return;

  NOTICE_LOG_FMT(SERIALINTERFACE_CARD, "EjectCard");

  m_ic_cards.clear();

  m_eject_timer = 120;
  m_card_present_insert_check_count = 0;
}

}  // namespace Triforce
