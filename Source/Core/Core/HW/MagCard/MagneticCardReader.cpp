// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

// Based on: https://github.com/GXTX/YACardEmu
// Copyright (C) 2020-2023 wutno (https://github.com/GXTX)
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/HW/MagCard/MagneticCardReader.h"

#include <algorithm>
#include <numeric>

#include "Common/BitUtils.h"
#include "Common/DirectIOFile.h"
#include "Common/FileUtil.h"
#include "Common/Lazy.h"
#include "Common/Logging/Log.h"
#include "Common/SmallVector.h"
#include "Common/StringUtil.h"
#include "Core/Core.h"

namespace
{

constexpr std::string GetHexDump(const auto& data)
{
  const auto u8_span = Common::AsU8Span(data);
  return HexDump(u8_span.data(), u8_span.size());
}

void AppendRange(auto* container, const auto& range)
{
  const auto prev_size = container->size();
  container->resize(prev_size + range.size());
  std::ranges::copy(range, container->data() + prev_size);
}

}  // namespace

namespace MagCard
{

constexpr std::string_view VERSION_STRING = "AP:S1234-5678,OS:S9012-3456,0000";

constexpr u8 START_OF_TEXT = 0x02;
constexpr u8 END_OF_TEXT = 0x03;
constexpr u8 ENQUIRY = 0x05;
constexpr u8 ACK = 0x06;
constexpr u8 NACK = 0x15;

enum class BitMode : u8
{
  SevenBitParity = 0x30,
  EightBitNoParity = 0x31,
};

enum class Track : u8
{
  Track_1 = 0x30,
  Track_2 = 0x31,
  Track_3 = 0x32,
  Track_1_And_2 = 0x33,
  Track_1_And_3 = 0x34,
  Track_2_And_3 = 0x35,
  Track_1_2_And_3 = 0x36,
};

static auto GetTrackIndicesForTrackType(Track track)
{
  Common::SmallVector<std::size_t, MagneticCardReader::NUM_TRACKS> result;

  switch (track)
  {
  case Track::Track_1:
  case Track::Track_2:
  case Track::Track_3:
    result.emplace_back(std::size_t(track) - 0x30);
    break;

  case Track::Track_1_And_2:
  case Track::Track_1_And_3:
  case Track::Track_2_And_3:
    result.emplace_back((track == Track::Track_2_And_3) ? 1 : 0);
    result.emplace_back((track == Track::Track_1_And_2) ? 1 : 2);
    break;

  case Track::Track_1_2_And_3:
    result.emplace_back(0);
    result.emplace_back(1);
    result.emplace_back(2);
    break;

  default:
    break;
  }

  return result;
}

MagneticCardReader::MagneticCardReader(MagneticCardReader::Settings* settings)
    : m_card_settings{settings}
{
}

MagneticCardReader::~MagneticCardReader()
{
  if (IsCardLoaded())
  {
    // Games do seem to write the entire save data in a single command.
    // But the printed messages are another story.
    // I suppose there's potential for data loss here.
    WARN_LOG_FMT(SERIALINTERFACE_CARD, "Shutdown before ejecting card!");
  }
}

void MagneticCardReader::Command_10_Initialize()
{
  EjectCard();
  m_status.SoftReset();
  FinishCommand();
}

void MagneticCardReader::Command_20_ReadStatus()
{
  m_status.SoftReset();
  FinishCommand();
}

void MagneticCardReader::Command_33_ReadData()
{
  enum class Mode : u8
  {
    Standard = 0x30,      // read 69-bytes
    ReadVariable = 0x31,  // variable length read
    CardCapture = 0x32,   // pull in card
  };

  if (m_current_packet.size() < 3)
  {
    SetPError(P::SYSTEM_ERR);
    return;
  }

  const auto mode = Mode(m_current_packet[0]);
  // const auto bit_mode = BitMode(m_current_packet[1]);
  const auto track = Track(m_current_packet[2]);

  switch (m_current_step)
  {
  case 1:
    if (mode == Mode::CardCapture)
      INFO_LOG_FMT(SERIALINTERFACE_CARD, "Command_33_ReadData: CardCapture");
    break;
  case 2:
  {
    // Doesn't return card data.
    if (mode == Mode::CardCapture)
    {
      if (!IsCardPresent())
      {
        m_status.s = S::WAITING_FOR_CARD;
        --m_current_step;
        break;
      }

      // How does hardware behave when the card is already elsewhere in the machine ?
      if (m_status.r != R::READ_WRITE_HEAD)
      {
        NOTICE_LOG_FMT(SERIALINTERFACE_CARD, "Capturing Card.");

        MoveCard(R::READ_WRITE_HEAD);
        ReadCardFile();

        Core::DisplayMessage("Inserted Magnetic Card", 4000);
      }
      break;
    }

    DEBUG_LOG_FMT(SERIALINTERFACE_CARD, "Command_33_ReadData: mode {:02x} track {:02x}", u8(mode),
                  u8(track));

    const auto track_indices = GetTrackIndicesForTrackType(track);

    if (!IsCardLoaded() || track_indices.size() == 0)
    {
      SetPError(P::ILLEGAL_ERR);
      return;
    }

    for (auto track_index : track_indices)
    {
      auto& track_data = m_card_data[track_index];
      if (!track_data.has_value())
      {
        SetPError(P::READ_ERR);
        WARN_LOG_FMT(SERIALINTERFACE_CARD, "Command_33_ReadData: Empty track {:02x}", track_index);
        // TODO: Are we supposed to return partial data ?
        return;
      }

      AppendRange(&m_command_payload, *track_data);
    }
  }
  default:
    break;
  }

  if (m_current_step >= 2)
    FinishCommand();
}

void MagneticCardReader::Command_35_GetData()
{
  switch (m_current_step)
  {
  case 2:
    if (!IsCardLoaded())
    {
      // TODO: Is this correct ?
      SetPError(P::ILLEGAL_ERR);
      return;
    }

    for (auto& track_data : m_card_data)
    {
      if (!track_data.has_value())
      {
        SetPError(P::READ_ERR);
        // TODO: Are we supposed to return partial data ?
        return;
      }

      AppendRange(&m_command_payload, *track_data);
    }
    break;
  default:
    break;
  }

  if (m_current_step >= 2)
    FinishCommand();
}

void MagneticCardReader::Command_40_Cancel()
{
  m_status.SoftReset();
  FinishCommand();
}

void MagneticCardReader::Command_53_WriteData()
{
  enum class Mode : u8
  {
    Standard = 0x30,  // 69-bytes
  };

  if (m_current_packet.size() < 3)
  {
    SetPError(P::SYSTEM_ERR);
    return;
  }

  if (m_current_step < 2)
    return;

  const auto mode = Mode(m_current_packet[0]);
  // const auto bit_mode = BitMode(m_current_packet[1]);
  auto track = Track(m_current_packet[2]);

  const auto track_indices = GetTrackIndicesForTrackType(track);
  const auto proper_payload_size = track_indices.size() * TRACK_SIZE;
  const auto payload = std::span{m_current_packet}.subspan(3);

  if (mode != Mode::Standard || !IsCardLoaded() || track_indices.size() == 0 ||
      payload.size() != proper_payload_size)
  {
    WARN_LOG_FMT(SERIALINTERFACE_CARD, "Command_53_WriteData: Failed.");
    SetPError(P::ILLEGAL_ERR);
    return;
  }

  INFO_LOG_FMT(SERIALINTERFACE_CARD, "Command_53_WriteData: Writing {} track(s) to memory.",
               track_indices.size());

  for (auto track_index : track_indices)
  {
    auto& track_data = m_card_data[track_index];
    track_data.emplace();
    std::ranges::copy(payload.subspan(track_index * TRACK_SIZE, TRACK_SIZE), track_data->data());
  }

  Core::DisplayMessage("Writing Magnetic Card", 4000);

  WriteCardFile();

  FinishCommand();
}

void MagneticCardReader::Command_78_PrintSettings()
{
  m_status.SoftReset();
  FinishCommand();
}

void MagneticCardReader::Command_7A_RegisterFont()
{
  m_status.SoftReset();
  FinishCommand();
}

void MagneticCardReader::Command_7B_PrintImage()
{
  // TODO: Are these print functions supposed to succeed before the card is actually grabbed ?
  if (!IsCardPresent())
  {
    SetPError(P::PRINT_ERR);
    return;
  }
  m_status.SoftReset();
  FinishCommand();
}

void MagneticCardReader::Command_7C_PrintLine()
{
  if (m_current_packet.size() < 3)
  {
    SetPError(P::SYSTEM_ERR);
    return;
  }

  switch (m_current_step)
  {
  case 1:
  {
    if (!IsCardPresent())
    {
      SetPError(P::PRINT_ERR);
      return;
    }

    // FIXME: Should we only move the head when we're actually about to print?
    MoveCard(R::THERMAL_HEAD);
    break;
  }
  case 2:
    MoveCard(R::READ_WRITE_HEAD);
    break;
  default:
    break;
  }

  if (m_current_step >= 2)
    FinishCommand();
}

void MagneticCardReader::Command_7D_Erase()
{
  switch (m_current_step)
  {
  case 1:
    if (!IsCardPresent())
    {
      SetPError(P::PRINT_ERR);
      return;
    }
    MoveCard(R::THERMAL_HEAD);
    break;
  case 2:
    MoveCard(R::READ_WRITE_HEAD);
    break;
  default:
    break;
  }

  if (m_current_step >= 2)
    FinishCommand();
}

void MagneticCardReader::Command_7E_PrintBarcode()
{
  switch (m_current_step)
  {
  case 2:
    if (!IsCardPresent())
    {
      SetPError(P::ILLEGAL_ERR);
      return;
    }
    break;
  default:
    break;
  }

  if (m_current_step >= 2)
    FinishCommand();
}

void MagneticCardReader::Command_80_EjectCard()
{
  if (m_current_step >= 3)
  {
    EjectCard();
    FinishCommand();
  }
}

void MagneticCardReader::Command_A0_Clean()
{
  switch (m_current_step)
  {
  case 2:
    if (!IsCardPresent())
    {
      m_status.s = S::WAITING_FOR_CARD;
      --m_current_step;
      break;
    }
    break;
  case 3:
    MoveCard(R::THERMAL_HEAD);
    break;
  case 4:
    EjectCard();
    break;
  default:
    break;
  }

  if (m_current_step >= 4)
    FinishCommand();
}

void MagneticCardReader::Command_B0_DispenseCard()
{
  enum class Mode : u8
  {
    Dispense = 0x31,
    CheckOnly = 0x32,
  };

  // MarioKart GP1 issues this command without options.
  auto mode = Mode::Dispense;
  if (!m_current_packet.empty())
  {
    mode = Mode(m_current_packet[0]);
  }

  switch (m_current_step)
  {
  case 2:
    if (mode == Mode::CheckOnly)
    {
      if (m_card_settings->report_dispenser_empty)
      {
        m_status.s = S::DISPENSER_EMPTY;
      }
      else
      {
        m_status.s = S::CARD_FULL;
      }
    }
    else
    {
      DispenseCard();
    }
    break;
  case 3:
    if (m_status.r == R::DISPENSER_THERMAL)
    {
      MoveCard(R::READ_WRITE_HEAD);
    }
    break;
  default:
    break;
  }

  if (m_current_step >= 3)
    FinishCommand();
}

void MagneticCardReader::Command_C0_ControlLED()
{
  // We don't need to handle this properly but let's leave some notes.
  enum class Mode : u8
  {
    Off = 0x30,
    On = 0x31,
    SlowBlink = 0x32,
    FastBlink = 0x33,
  };

  m_status.SoftReset();
  FinishCommand();
}

void MagneticCardReader::Command_C1_SetPrintRetry()
{
  // We don't need to handle this properly but let's leave some notes
  // m_current_packet[0] == 0x31 NONE ~ 0x39 MAX8
  m_status.SoftReset();
  FinishCommand();
}

void MagneticCardReader::Command_D0_ControlShutter()
{
  // Only BR model supports this command.
  SetSError(S::ILLEGAL_COMMAND);
}

void MagneticCardReader::Command_F0_GetVersion()
{
  AppendRange(&m_command_payload, VERSION_STRING);

  m_status.SoftReset();
  FinishCommand();
}

void MagneticCardReader::ClearCardInMemory()
{
  m_card_data.fill(std::nullopt);
}

void MagneticCardReader::ReadCardFile()
{
  NOTICE_LOG_FMT(SERIALINTERFACE_CARD, "Reading card data from: {}", m_card_settings->card_name);

  ClearCardInMemory();

  const std::string full_path = m_card_settings->card_path + m_card_settings->card_name;

  File::DirectIOFile file{full_path, File::AccessMode::Read};

  const auto file_size = file.GetSize();
  const auto complete_track_count = file_size / TRACK_SIZE;
  const auto excess_byte_count = file_size % TRACK_SIZE;

  if (file_size == 0 || excess_byte_count != 0 || complete_track_count > NUM_TRACKS)
  {
    ERROR_LOG_FMT(SERIALINTERFACE_CARD, "ReadCardFile: Incorrect card file size: {}", file_size);
  }

  const auto tracks_to_read = std::min<u32>(complete_track_count, NUM_TRACKS);
  for (u32 track_num = 0; track_num != tracks_to_read; ++track_num)
  {
    auto& track_data = m_card_data[track_num];
    track_data.emplace();

    if (!file.Read(*track_data))
    {
      ERROR_LOG_FMT(SERIALINTERFACE_CARD, "File read failed.");
      track_data.reset();
      break;
    }
  }

  DEBUG_LOG_FMT(SERIALINTERFACE_CARD, "ReadCard: 0:{} 1:{} 2:{}", GetHexDump(m_card_data[0]),
                GetHexDump(m_card_data[1]), GetHexDump(m_card_data[2]));
}

void MagneticCardReader::WriteCardFile()
{
  // Note: Creating empty upsets games when reinserting them later.
  // Games expect to dispense their own blank cards, not have such cards inserted.
  // We only write files when the game itself writes, so this won't happen.

  NOTICE_LOG_FMT(SERIALINTERFACE_CARD, "Writing card data to: {}", m_card_settings->card_name);
  DEBUG_LOG_FMT(SERIALINTERFACE_CARD, "WriteCard: 0:{} 1:{} 2:{}", GetHexDump(m_card_data[0]),
                GetHexDump(m_card_data[1]), GetHexDump(m_card_data[2]));

  auto full_path = m_card_settings->card_path + m_card_settings->card_name;

  File::DirectIOFile file{full_path, File::AccessMode::Write};

  for (auto& track_data : m_card_data)
  {
    if (track_data.has_value() && !file.Write(*track_data))
    {
      ERROR_LOG_FMT(SERIALINTERFACE_CARD, "File write failed.");
      break;
    }
  }
}

void MagneticCardReader::SetPError(P error_code)
{
  WARN_LOG_FMT(SERIALINTERFACE_CARD, "SetPError: {:02x}", u8(error_code));
  m_status.p = error_code;
  FinishCommand();
}

void MagneticCardReader::SetSError(S error_code)
{
  WARN_LOG_FMT(SERIALINTERFACE_CARD, "SetSError: {:02x}", u8(error_code));
  m_status.s = error_code;
  FinishCommand();
}

void MagneticCardReader::Process(std::vector<u8>* read, std::vector<u8>* write)
{
  while (!read->empty())
  {
    const u8 first_byte = read->front();
    if (first_byte == ENQUIRY)
    {
      // ENQUIRY
      // The host sends this regularly.
      // This triggers sending of the latest command buffer.
      // We also step the state machine here.

      DEBUG_LOG_FMT(SERIALINTERFACE_CARD, "Process: ENQ");

      // TODO: Get time from Core rather than assuming.
      const auto elapsed_time = std::chrono::milliseconds{50};

      StepStateMachine(elapsed_time);
      StepStatePerson(elapsed_time);

      BuildPacket(*write);

      read->erase(read->begin());  // TODO: SLOW !
      continue;
    }

    if (first_byte != START_OF_TEXT)
    {
      ERROR_LOG_FMT(SERIALINTERFACE_CARD, "Process: Unexpected {:02x}", first_byte);
      read->erase(read->begin());  // TODO: SLOW !
      continue;
    }

    // START_OF_TEXT
    // This is a command packet. The next byte provides the size.
    // Upon read they ACK or NACK and start processing of a command.

    if (read->size() < 2)
      break;  // Wait for more data.

    const std::size_t packet_size = (*read)[1];
    if (packet_size > read->size() - 2)
      break;  // Wait for more data.

    if (ReceivePacket(std::span{*read}.subspan(2, packet_size)))
    {
      DEBUG_LOG_FMT(SERIALINTERFACE_CARD, "Writing ACK");
      write->emplace_back(ACK);
    }
    else
    {
      WARN_LOG_FMT(SERIALINTERFACE_CARD, "Writing NACK");
      write->emplace_back(NACK);
    }

    read->erase(read->begin(), read->begin() + packet_size + 2);  // TODO: SLOW !
  }
}

bool MagneticCardReader::ReceivePacket(std::span<const u8> packet)
{
  DEBUG_LOG_FMT(SERIALINTERFACE_CARD, "ReceivePacket: {}", GetHexDump(packet));

  if (packet.size() < 6)
  {
    WARN_LOG_FMT(SERIALINTERFACE_CARD, "ReceivePacket: Bad packet size!");
    return false;
  }

  if (packet[packet.size() - 2] != END_OF_TEXT)
  {
    WARN_LOG_FMT(SERIALINTERFACE_CARD, "ReceivePacket: Missing END_OF_TEXT!");
    return false;
  }

  // Checksum is XOR of the previous bytes.
  // This includes the count byte that we already chopped off.
  const u8 read_checksum = packet.back();

  // TODO C++23:
  // std::ranges::fold_left(packet.first(packet.size() - 1), u8(packet.size()), std::bit_xor{});
  const auto range_to_checksum = packet.first(packet.size() - 1);
  const auto proper_checksum = std::accumulate(range_to_checksum.begin(), range_to_checksum.end(),
                                               u8(packet.size()), std::bit_xor{});

  // Verify checksum. Skip packet if invalid.
  if (read_checksum != proper_checksum)
  {
    WARN_LOG_FMT(SERIALINTERFACE_CARD, "ReceivePacket: Bad checksum!");
    return false;
  }

  const u8 new_command = packet[0];

  if (IsRunningCommand())
  {
    if (new_command == 0x40)  // Cancel.
    {
      INFO_LOG_FMT(SERIALINTERFACE_CARD, "ReceivePacket: Cancelling command: {:02x}",
                   m_current_command);
    }
    else
    {
      // What does the real hardware do here ?
      WARN_LOG_FMT(SERIALINTERFACE_CARD,
                   "ReceivePacket: Running command: {:02x} interrupted by command: {:02x}",
                   m_current_command, new_command);
    }
  }

  m_current_command = new_command;

  // Copy the payload.
  // We're skipping the command number and the host's 3 status bytes,
  //  and dropping the END_OF_TEXT and checksum.
  m_current_packet.resize(packet.size() - 6);
  std::ranges::copy(packet.subspan(4, m_current_packet.size()), m_current_packet.data());

  // TODO: Is this sensible for every command ?
  m_status.SoftReset();

  // Start the new command.
  m_status.s = S::RUNNING_COMMAND;
  m_current_step = 1;
  m_command_payload.clear();

  return true;
}

void MagneticCardReader::StepStatePerson(DT elapsed_time)
{
  m_time_since_person_moved_card += elapsed_time;

  // F-Zero AX: Polls for a card with command 0x20 ReadStatus().
  //  It demands a card is removed from "ENTRY_SLOT" during boot.
  //  It pauses and spams ReadStatus() until that happens.
  //  When wanted, it loads a card detected at the "ENTRY_SLOT" position.
  //  There's not a perfectly reliably way to detect when it wants a card.
  //
  // MarioKart GP: Polls for wanted cards with command 0x33 ReadData(Mode:CardCapture).
  //  It does this regardless of the card being detected in the "ENTRY_SLOT" position.
  //  This makes it easy to detect when cards are wanted.
  //  It pauses until cards are pulled from "ENTRY_SLOT" on game over.

  // This logic tries to emulate a person following those rules.

  // People can't interact with cards inside the machine.
  if (IsCardLoaded())
    return;

  Common::Lazy<bool> card_exists{[&] {
    const auto full_path = m_card_settings->card_path + m_card_settings->card_name;
    return File::Exists(full_path);
  }};

  if (!IsCardPresent())
  {
    if (m_card_settings->insert_card_when_possible && IsReadyForCard() &&
        m_time_since_person_moved_card > std::chrono::milliseconds{500} && *card_exists)
    {
      // Place the card in the entry slot after a delay.
      NOTICE_LOG_FMT(SERIALINTERFACE_CARD, "Inserting card.");
      m_status.r = R::ENTRY_SLOT;
      m_time_since_person_moved_card = {};
    }

    return;
  }

  const bool machine_moved_card_last =
      m_time_since_machine_moved_card < m_time_since_person_moved_card;
  if ((machine_moved_card_last &&
       m_time_since_machine_moved_card > std::chrono::milliseconds{100}) ||
      !*card_exists)
  {
    // Take the card from the machine when ejected or if the file is deleted.
    NOTICE_LOG_FMT(SERIALINTERFACE_CARD, "Removing card.");
    m_status.r = R::NO_CARD;
    m_time_since_person_moved_card = {};
  }
}

void MagneticCardReader::StepStateMachine(DT elapsed_time)
{
  m_time_since_machine_moved_card += elapsed_time;

  if (!IsRunningCommand())
    return;

  DEBUG_LOG_FMT(SERIALINTERFACE_CARD, "StepStateMachine command: {:02x}", GetCurrentCommand());

  switch (GetCurrentCommand())
  {
  case 0x10:
    Command_10_Initialize();
    break;
  case 0x20:
    Command_20_ReadStatus();
    break;
  case 0x33:
    Command_33_ReadData();
    break;
  case 0x35:
    Command_35_GetData();
    break;
  case 0x40:
    Command_40_Cancel();
    break;
  case 0x53:
    Command_53_WriteData();
    break;
  case 0x78:
    Command_78_PrintSettings();
    break;
  case 0x7A:
    Command_7A_RegisterFont();
    break;
  case 0x7B:
    Command_7B_PrintImage();
    break;
  case 0x7C:
    Command_7C_PrintLine();
    break;
  case 0x7D:
    Command_7D_Erase();
    break;
  case 0x7E:
    Command_7E_PrintBarcode();
    break;
  case 0x80:
    Command_80_EjectCard();
    break;
  case 0xA0:
    Command_A0_Clean();
    break;
  case 0xB0:
    Command_B0_DispenseCard();
    break;
  case 0xC0:
    Command_C0_ControlLED();
    break;
  case 0xC1:
    Command_C1_SetPrintRetry();
    break;
  case 0xD0:
    Command_D0_ControlShutter();
    break;
  case 0xF0:
    Command_F0_GetVersion();
    break;
  default:
    WARN_LOG_FMT(SERIALINTERFACE_CARD, "HandlePacket: Unhandled command {:02x}",
                 GetCurrentCommand());
    SetSError(S::ILLEGAL_COMMAND);
    break;
  }

  if (IsRunningCommand())
    ++m_current_step;
}

void MagneticCardReader::BuildPacket(std::vector<u8>& write_buffer)
{
  // Header and footer add 6 bytes.
  const u8 payload_size = u8(m_command_payload.size() + 6);
  // + START_OF_TEXT + the count byte
  const auto total_write_size = payload_size + 2;

  const auto prev_buffer_size = write_buffer.size();
  write_buffer.resize(prev_buffer_size + total_write_size);

  auto* out_ptr = write_buffer.data() + prev_buffer_size;
  u8 packet_checksum = 0;

  const auto write_and_checksum = [&](u8 value) {
    *(out_ptr++) = value;
    packet_checksum ^= value;
  };

  // Write the header.
  *(out_ptr++) = START_OF_TEXT;
  write_and_checksum(payload_size);
  write_and_checksum(GetCurrentCommand());
  write_and_checksum(GetPositionValue());
  write_and_checksum(u8(m_status.p));
  write_and_checksum(u8(m_status.s));

  // Write the payload.
  std::ranges::for_each(m_command_payload, write_and_checksum);

  // Write the footer.
  write_and_checksum(END_OF_TEXT);
  *(out_ptr++) = packet_checksum;

  DEBUG_LOG_FMT(
      SERIALINTERFACE_CARD, "BuildPacket: {}",
      GetHexDump(std::span{write_buffer}.subspan(write_buffer.size() - payload_size - 2)));
}

bool MagneticCardReader::IsRunningCommand() const
{
  return m_current_step > 0;
}

void MagneticCardReader::FinishCommand()
{
  m_current_step = 0;

  if (m_status.s != S::ILLEGAL_COMMAND)
    m_status.s = S::NO_JOB;
}

u8 MagneticCardReader::GetCurrentCommand() const
{
  return m_current_command;
}

bool MagneticCardReader::IsCardPresent() const
{
  return m_status.r != R::NO_CARD;
}

bool MagneticCardReader::IsCardLoaded() const
{
  return m_status.r != R::NO_CARD && m_status.r != R::ENTRY_SLOT;
}

void MagneticCardReader::MoveCard(R new_position)
{
  if (m_status.r == new_position)
    return;

  static constexpr const char* position_descriptions[] = {
      "NO_CARD", "READ_WRITE_HEAD", "THERMAL_HEAD", "DISPENSER_THERMAL", "ENTRY_SLOT",
  };

  INFO_LOG_FMT(SERIALINTERFACE_CARD, "MoveCard: {}",
               position_descriptions[std::size_t(new_position)]);

  m_status.r = new_position;
  m_time_since_machine_moved_card = {};
}

void MagneticCardReader::EjectCard()
{
  if (IsCardLoaded())
  {
    NOTICE_LOG_FMT(SERIALINTERFACE_CARD, "Ejecting card.");

    ClearCardInMemory();
    MoveCard(R::ENTRY_SLOT);

    Core::DisplayMessage("Ejected Magnetic Card", 4000);
  }
}

void MagneticCardReader::DispenseCard()
{
  // TODO: Think about how multiple card management UX should work.
  // Since this is a new card, we maybe shouldn't be overwriting existing files.

  if (IsCardLoaded())
  {
    SetSError(S::ILLEGAL_COMMAND);
    return;
  }

  if (m_card_settings->report_dispenser_empty)
  {
    m_status.s = S::DISPENSER_EMPTY;
    return;
  }

  NOTICE_LOG_FMT(SERIALINTERFACE_CARD, "Dispensing a new card.");
  Core::DisplayMessage("Created Magnetic Card", 4000);

  // TODO: This behavior doesn't seem to match protocol dumps.
  MoveCard(R::DISPENSER_THERMAL);
}

bool MagneticCardReader::IsReadyForCard()
{
  return !IsCardPresent();
}

void MagneticCardReader::DoState(PointerWrap& p)
{
  // Outgoing packet.
  p.Do(m_status);
  p.Do(m_current_command);
  p.Do(m_command_payload);

  // Command state.
  p.Do(m_current_packet);
  p.Do(m_current_step);

  // The card data in memory.
  for (auto& track : m_card_data)
    p.Do(track);

  p.Do(m_time_since_machine_moved_card);
  p.Do(m_time_since_person_moved_card);
}

}  // namespace MagCard
