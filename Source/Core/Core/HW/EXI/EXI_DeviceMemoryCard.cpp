// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/HW/EXI/EXI_DeviceMemoryCard.h"

#include <array>
#include <cstring>
#include <functional>
#include <memory>
#include <string>
#include <utility>

#include <fmt/format.h>

#include "Common/ChunkFile.h"
#include "Common/CommonPaths.h"
#include "Common/CommonTypes.h"
#include "Common/Config/Config.h"
#include "Common/EnumMap.h"
#include "Common/FileUtil.h"
#include "Common/IniFile.h"
#include "Common/Logging/Log.h"
#include "Core/CommonTitles.h"
#include "Core/Config/MainSettings.h"
#include "Core/ConfigManager.h"
#include "Core/CoreTiming.h"
#include "Core/HW/EXI/EXI.h"
#include "Core/HW/EXI/EXI_Channel.h"
#include "Core/HW/EXI/EXI_Device.h"
#include "Core/HW/GCMemcard/GCMemcard.h"
#include "Core/HW/GCMemcard/GCMemcardDirectory.h"
#include "Core/HW/GCMemcard/GCMemcardRaw.h"
#include "Core/HW/Memmap.h"
#include "Core/HW/Sram.h"
#include "Core/HW/SystemTimers.h"
#include "Core/Movie.h"
#include "Core/System.h"
#include "DiscIO/Enums.h"

namespace ExpansionInterface
{
#define MC_STATUS_BUSY 0x80
#define MC_STATUS_UNLOCKED 0x40
#define MC_STATUS_SLEEP 0x20
#define MC_STATUS_ERASEERROR 0x10
#define MC_STATUS_PROGRAMEERROR 0x08
#define MC_STATUS_READY 0x01
#define SIZE_TO_Mb (1024 * 8 * 16)

static const u32 MC_TRANSFER_RATE_READ = 512 * 1024;
static const auto MC_TRANSFER_RATE_WRITE = static_cast<u32>(96.125f * 1024.0f);

static Common::EnumMap<CoreTiming::EventType*, MAX_MEMCARD_SLOT> s_et_cmd_done;
static Common::EnumMap<CoreTiming::EventType*, MAX_MEMCARD_SLOT> s_et_transfer_complete;
static Common::EnumMap<char, MAX_MEMCARD_SLOT> s_card_short_names{'A', 'B'};

// Takes care of the nasty recovery of the 'this' pointer from card_slot,
// stored in the userdata parameter of the CoreTiming event.
void CEXIMemoryCard::EventCompleteFindInstance(Core::System& system, u64 userdata,
                                               std::function<void(CEXIMemoryCard*)> callback)
{
  Slot card_slot = static_cast<Slot>(userdata);
  IEXIDevice* self = system.GetExpansionInterface().GetDevice(card_slot);
  if (self != nullptr)
  {
    if (self->m_device_type == EXIDeviceType::MemoryCard ||
        self->m_device_type == EXIDeviceType::MemoryCardFolder)
    {
      callback(static_cast<CEXIMemoryCard*>(self));
    }
  }
}

void CEXIMemoryCard::CmdDoneCallback(Core::System& system, u64 userdata, s64)
{
  EventCompleteFindInstance(system, userdata,
                            [](CEXIMemoryCard* instance) { instance->CmdDone(); });
}

void CEXIMemoryCard::TransferCompleteCallback(Core::System& system, u64 userdata, s64)
{
  EventCompleteFindInstance(system, userdata,
                            [](CEXIMemoryCard* instance) { instance->TransferComplete(); });
}

void CEXIMemoryCard::Init()
{
  static_assert(s_et_cmd_done.size() == s_et_transfer_complete.size(), "Event array size differs");
  static_assert(s_et_cmd_done.size() == MEMCARD_SLOTS.size(), "Event array size differs");
  auto& system = Core::System::GetInstance();
  auto& core_timing = system.GetCoreTiming();
  for (Slot slot : MEMCARD_SLOTS)
  {
    s_et_cmd_done[slot] = core_timing.RegisterEvent(
        fmt::format("memcardDone{}", s_card_short_names[slot]), CmdDoneCallback);
    s_et_transfer_complete[slot] = core_timing.RegisterEvent(
        fmt::format("memcardTransferComplete{}", s_card_short_names[slot]),
        TransferCompleteCallback);
  }
}

void CEXIMemoryCard::Shutdown()
{
  s_et_cmd_done.fill(nullptr);
  s_et_transfer_complete.fill(nullptr);
}

CEXIMemoryCard::CEXIMemoryCard(Core::System& system, const Slot slot, bool gci_folder,
                               const Memcard::HeaderData& header_data)
    : IEXIDevice(system), m_card_slot(slot)
{
  ASSERT_MSG(EXPANSIONINTERFACE, IsMemcardSlot(slot), "Trying to create invalid memory card in {}.",
             slot);

  // NOTE: When loading a save state, DMA completion callbacks (s_et_transfer_complete) and such
  //   may have been restored, we need to anticipate those arriving.

  m_interrupt_switch = 0;
  m_interrupt_set = false;
  m_command = Command::NintendoID;
  m_status = MC_STATUS_BUSY | MC_STATUS_UNLOCKED | MC_STATUS_READY;
  m_position = 0;
  m_programming_buffer.fill(0);
  // Nintendo Memory Card EXI IDs
  // 0x00000004 Memory Card 59     4Mbit
  // 0x00000008 Memory Card 123    8Mb
  // 0x00000010 Memory Card 251    16Mb
  // 0x00000020 Memory Card 507    32Mb
  // 0x00000040 Memory Card 1019   64Mb
  // 0x00000080 Memory Card 2043   128Mb

  // 0x00000510 16Mb "bigben" card
  // card_id = 0xc243;
  m_card_id = 0xc221;  // It's a Nintendo brand memcard

  if (gci_folder)
  {
    SetupGciFolder(header_data);
  }
  else
  {
    SetupRawMemcard(header_data.m_size_mb);
  }

  m_memory_card_size = m_memory_card->GetCardId() * SIZE_TO_Mb;
  std::array<u8, 20> header{};
  m_memory_card->Read(0, static_cast<s32>(header.size()), header.data());
  auto& sram = system.GetSRAM();
  SetCardFlashID(&sram, header.data(), m_card_slot);
}

std::pair<std::string /* path */, bool /* migrate */>
CEXIMemoryCard::GetGCIFolderPath(Slot card_slot, AllowMovieFolder allow_movie_folder)
{
  std::string path_override = Config::Get(Config::GetInfoForGCIPathOverride(card_slot));

  if (!path_override.empty())
    return {std::move(path_override), false};

  const bool use_movie_folder = allow_movie_folder == AllowMovieFolder::Yes &&
                                Movie::IsPlayingInput() && Movie::IsConfigSaved() &&
                                Movie::IsUsingMemcard(card_slot) &&
                                Movie::IsStartingFromClearSave();

  const DiscIO::Region region = Config::ToGameCubeRegion(SConfig::GetInstance().m_region);
  if (use_movie_folder)
  {
    return {fmt::format("{}{}/Movie/Card {}", File::GetUserPath(D_GCUSER_IDX),
                        Config::GetDirectoryForRegion(region), s_card_short_names[card_slot]),
            false};
  }

  return {Config::GetGCIFolderPath(card_slot, region), true};
}

void CEXIMemoryCard::SetupGciFolder(const Memcard::HeaderData& header_data)
{
  const std::string& game_id = SConfig::GetInstance().GetGameID();
  u32 current_game_id = 0;
  if (game_id.length() >= 4 && game_id != "00000000" &&
      SConfig::GetInstance().GetTitleID() != Titles::SYSTEM_MENU)
  {
    current_game_id = Common::swap32(reinterpret_cast<const u8*>(game_id.c_str()));
  }

  const auto [dir_path, migrate] = GetGCIFolderPath(m_card_slot, AllowMovieFolder::Yes);

  const File::FileInfo file_info(dir_path);
  if (!file_info.Exists())
  {
    if (migrate)  // first use of memcard folder, migrate automatically
      MigrateFromMemcardFile(dir_path + DIR_SEP, m_card_slot, SConfig::GetInstance().m_region);
    else
      File::CreateFullPath(dir_path + DIR_SEP);
  }
  else if (!file_info.IsDirectory())
  {
    if (File::Rename(dir_path, dir_path + ".original"))
    {
      PanicAlertFmtT("{0} was not a directory, moved to *.original", dir_path);
      if (migrate)
        MigrateFromMemcardFile(dir_path + DIR_SEP, m_card_slot, SConfig::GetInstance().m_region);
      else
        File::CreateFullPath(dir_path + DIR_SEP);
    }
    else  // we tried but the user wants to crash
    {
      // TODO more user friendly abort
      PanicAlertFmtT("{0} is not a directory, failed to move to *.original.\n Verify your "
                     "write permissions or move the file outside of Dolphin",
                     dir_path);
      std::exit(0);
    }
  }

  m_memory_card = std::make_unique<GCMemcardDirectory>(dir_path + DIR_SEP, m_card_slot, header_data,
                                                       current_game_id);
}

void CEXIMemoryCard::SetupRawMemcard(u16 size_mb)
{
  std::string filename;
  if (Movie::IsPlayingInput() && Movie::IsConfigSaved() && Movie::IsUsingMemcard(m_card_slot) &&
      Movie::IsStartingFromClearSave())
  {
    filename = File::GetUserPath(D_GCUSER_IDX) +
               fmt::format("Movie{}.raw", s_card_short_names[m_card_slot]);
  }
  else
  {
    filename = Config::GetMemcardPath(m_card_slot, SConfig::GetInstance().m_region, size_mb);
  }

  m_memory_card = std::make_unique<MemoryCard>(filename, m_card_slot, size_mb);
}

CEXIMemoryCard::~CEXIMemoryCard()
{
  auto& core_timing = m_system.GetCoreTiming();
  core_timing.RemoveEvent(s_et_cmd_done[m_card_slot]);
  core_timing.RemoveEvent(s_et_transfer_complete[m_card_slot]);
}

bool CEXIMemoryCard::UseDelayedTransferCompletion() const
{
  return true;
}

bool CEXIMemoryCard::IsPresent() const
{
  return true;
}

void CEXIMemoryCard::CmdDone()
{
  m_status |= MC_STATUS_READY;
  m_status &= ~MC_STATUS_BUSY;

  m_interrupt_set = true;
  m_system.GetExpansionInterface().UpdateInterrupts();
}

void CEXIMemoryCard::TransferComplete()
{
  // Transfer complete, send interrupt
  m_system.GetExpansionInterface()
      .GetChannel(ExpansionInterface::SlotToEXIChannel(m_card_slot))
      ->SendTransferComplete();
}

void CEXIMemoryCard::CmdDoneLater(u64 cycles)
{
  auto& core_timing = m_system.GetCoreTiming();
  core_timing.RemoveEvent(s_et_cmd_done[m_card_slot]);
  core_timing.ScheduleEvent(cycles, s_et_cmd_done[m_card_slot], static_cast<u64>(m_card_slot));
}

void CEXIMemoryCard::SetCS(int cs)
{
  if (cs)  // not-selected to selected
  {
    m_position = 0;
  }
  else
  {
    switch (m_command)
    {
    case Command::SectorErase:
      if (m_position > 2)
      {
        m_memory_card->ClearBlock(m_address & (m_memory_card_size - 1));
        m_status |= MC_STATUS_BUSY;
        m_status &= ~MC_STATUS_READY;

        //???

        CmdDoneLater(5000);
      }
      break;

    case Command::ChipErase:
      if (m_position > 2)
      {
        m_memory_card->ClearAll();
        m_status &= ~MC_STATUS_BUSY;
      }
      break;

    case Command::PageProgram:
      if (m_position >= 5)
      {
        int count = m_position - 5;
        int i = 0;
        m_status &= ~MC_STATUS_BUSY;

        while (count--)
        {
          m_memory_card->Write(m_address, 1, &(m_programming_buffer[i++]));
          i &= 127;
          m_address = (m_address & ~0x1FF) | ((m_address + 1) & 0x1FF);
        }

        CmdDoneLater(5000);
      }
      break;

    default:
      break;
    }
  }
}

bool CEXIMemoryCard::IsInterruptSet()
{
  if (m_interrupt_switch)
    return m_interrupt_set;
  return false;
}

void CEXIMemoryCard::TransferByte(u8& byte)
{
  DEBUG_LOG_FMT(EXPANSIONINTERFACE, "EXI MEMCARD: > {:02x}", byte);
  if (m_position == 0)
  {
    m_command = static_cast<Command>(byte);  // first byte is command
    byte = 0xFF;                             // would be tristate, but we don't care.

    switch (m_command)  // This seems silly, do we really need it?
    {
    case Command::NintendoID:
    case Command::ReadArray:
    case Command::ArrayToBuffer:
    case Command::SetInterrupt:
    case Command::WriteBuffer:
    case Command::ReadStatus:
    case Command::ReadID:
    case Command::ReadErrorBuffer:
    case Command::WakeUp:
    case Command::Sleep:
    case Command::ClearStatus:
    case Command::SectorErase:
    case Command::PageProgram:
    case Command::ExtraByteProgram:
    case Command::ChipErase:
      DEBUG_LOG_FMT(EXPANSIONINTERFACE, "EXI MEMCARD: command {:02x} at position 0. seems normal.",
                    static_cast<u8>(m_command));
      break;
    default:
      WARN_LOG_FMT(EXPANSIONINTERFACE, "EXI MEMCARD: command {:02x} at position 0",
                   static_cast<u8>(m_command));
      break;
    }
    if (m_command == Command::ClearStatus)
    {
      m_status &= ~MC_STATUS_PROGRAMEERROR;
      m_status &= ~MC_STATUS_ERASEERROR;

      m_status |= MC_STATUS_READY;

      m_interrupt_set = false;

      byte = 0xFF;
      m_position = 0;
    }
  }
  else
  {
    switch (m_command)
    {
    case Command::NintendoID:
      //
      // Nintendo card:
      // 00 | 80 00 00 00 10 00 00 00
      // "bigben" card:
      // 00 | ff 00 00 05 10 00 00 00 00 00 00 00 00 00 00
      // we do it the Nintendo way.
      if (m_position == 1)
        byte = 0x80;  // dummy cycle
      else
        byte = static_cast<u8>(m_memory_card->GetCardId() >> (24 - (((m_position - 2) & 3) * 8)));
      break;

    case Command::ReadArray:
      switch (m_position)
      {
      case 1:  // AD1
        m_address = byte << 17;
        byte = 0xFF;
        break;
      case 2:  // AD2
        m_address |= byte << 9;
        break;
      case 3:  // AD3
        m_address |= (byte & 3) << 7;
        break;
      case 4:  // BA
        m_address |= (byte & 0x7F);
        break;
      }
      if (m_position > 1)  // not specified for 1..8, anyway
      {
        m_memory_card->Read(m_address & (m_memory_card_size - 1), 1, &byte);
        // after 9 bytes, we start incrementing the address,
        // but only the sector offset - the pointer wraps around
        if (m_position >= 9)
          m_address = (m_address & ~0x1FF) | ((m_address + 1) & 0x1FF);
      }
      break;

    case Command::ReadStatus:
      // (unspecified for byte 1)
      byte = m_status;
      break;

    case Command::ReadID:
      if (m_position == 1)  // (unspecified)
        byte = static_cast<u8>(m_card_id >> 8);
      else
        byte = static_cast<u8>((m_position & 1) ? (m_card_id) : (m_card_id >> 8));
      break;

    case Command::SectorErase:
      switch (m_position)
      {
      case 1:  // AD1
        m_address = byte << 17;
        break;
      case 2:  // AD2
        m_address |= byte << 9;
        break;
      }
      byte = 0xFF;
      break;

    case Command::SetInterrupt:
      if (m_position == 1)
      {
        m_interrupt_switch = byte;
      }
      byte = 0xFF;
      break;

    case Command::ChipErase:
      byte = 0xFF;
      break;

    case Command::PageProgram:
      switch (m_position)
      {
      case 1:  // AD1
        m_address = byte << 17;
        break;
      case 2:  // AD2
        m_address |= byte << 9;
        break;
      case 3:  // AD3
        m_address |= (byte & 3) << 7;
        break;
      case 4:  // BA
        m_address |= (byte & 0x7F);
        break;
      }

      if (m_position >= 5)
        m_programming_buffer[((m_position - 5) & 0x7F)] = byte;  // wrap around after 128 bytes

      byte = 0xFF;
      break;

    default:
      WARN_LOG_FMT(EXPANSIONINTERFACE, "EXI MEMCARD: unknown command byte {:02x}", byte);
      byte = 0xFF;
    }
  }
  m_position++;
  DEBUG_LOG_FMT(EXPANSIONINTERFACE, "EXI MEMCARD: < {:02x}", byte);
}

void CEXIMemoryCard::DoState(PointerWrap& p)
{
  // for movie sync, we need to save/load memory card contents (and other data) in savestates.
  // otherwise, we'll assume the user wants to keep their memcards and saves separate,
  // unless we're loading (in which case we let the savestate contents decide, in order to stay
  // aligned with them).
  bool storeContents = (Movie::IsMovieActive());
  p.Do(storeContents);

  if (storeContents)
  {
    p.Do(m_interrupt_switch);
    p.Do(m_interrupt_set);
    p.Do(m_command);
    p.Do(m_status);
    p.Do(m_position);
    p.Do(m_programming_buffer);
    p.Do(m_address);
    m_memory_card->DoState(p);
    p.Do(m_card_slot);
  }
}

// DMA reads are preceded by all of the necessary setup via IMMRead
// read all at once instead of single byte at a time as done by IEXIDevice::DMARead
void CEXIMemoryCard::DMARead(u32 addr, u32 size)
{
  auto& memory = m_system.GetMemory();
  m_memory_card->Read(m_address, size, memory.GetPointer(addr));

  if ((m_address + size) % Memcard::BLOCK_SIZE == 0)
  {
    INFO_LOG_FMT(EXPANSIONINTERFACE, "reading from block: {:x}", m_address / Memcard::BLOCK_SIZE);
  }

  // Schedule transfer complete later based on read speed
  m_system.GetCoreTiming().ScheduleEvent(
      size * (SystemTimers::GetTicksPerSecond() / MC_TRANSFER_RATE_READ),
      s_et_transfer_complete[m_card_slot], static_cast<u64>(m_card_slot));
}

// DMA write are preceded by all of the necessary setup via IMMWrite
// write all at once instead of single byte at a time as done by IEXIDevice::DMAWrite
void CEXIMemoryCard::DMAWrite(u32 addr, u32 size)
{
  auto& memory = m_system.GetMemory();
  m_memory_card->Write(m_address, size, memory.GetPointer(addr));

  if (((m_address + size) % Memcard::BLOCK_SIZE) == 0)
  {
    INFO_LOG_FMT(EXPANSIONINTERFACE, "writing to block: {:x}", m_address / Memcard::BLOCK_SIZE);
  }

  // Schedule transfer complete later based on write speed
  m_system.GetCoreTiming().ScheduleEvent(
      size * (SystemTimers::GetTicksPerSecond() / MC_TRANSFER_RATE_WRITE),
      s_et_transfer_complete[m_card_slot], static_cast<u64>(m_card_slot));
}
}  // namespace ExpansionInterface
