// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>
#include <functional>
#include <memory>
#include <string>
#include <utility>

#include "Core/HW/EXI/EXI_Device.h"

class MemoryCardBase;
class PointerWrap;

namespace Core
{
class System;
}
namespace Memcard
{
struct HeaderData;
}

namespace ExpansionInterface
{
enum class Slot : int;

enum class AllowMovieFolder
{
  Yes,
  No,
};

class CEXIMemoryCard : public IEXIDevice
{
public:
  CEXIMemoryCard(Core::System& system, Slot slot, bool gci_folder,
                 const Memcard::HeaderData& header_data);
  ~CEXIMemoryCard() override;
  void SetCS(int cs) override;
  bool IsInterruptSet() override;
  bool UseDelayedTransferCompletion() const override;
  bool IsPresent() const override;
  void DoState(PointerWrap& p) override;
  void DMARead(u32 addr, u32 size) override;
  void DMAWrite(u32 addr, u32 size) override;

  // CoreTiming events need to be registered during boot since CoreTiming is DoState()-ed
  // before ExpansionInterface so we'll lose the save stated events if the callbacks are
  // not already registered first.
  static void Init();
  static void Shutdown();

  static std::pair<std::string /* path */, bool /* migrate */>
  GetGCIFolderPath(Slot card_slot, AllowMovieFolder allow_movie_folder);

private:
  void SetupGciFolder(const Memcard::HeaderData& header_data);
  void SetupRawMemcard(u16 size_mb);
  static void EventCompleteFindInstance(Core::System& system, u64 userdata,
                                        std::function<void(CEXIMemoryCard*)> callback);

  // Scheduled when a command that required delayed end signaling is done.
  static void CmdDoneCallback(Core::System& system, u64 userdata, s64 cyclesLate);

  // Scheduled when memory card is done transferring data
  static void TransferCompleteCallback(Core::System& system, u64 userdata, s64 cyclesLate);

  // Signals that the command that was previously executed is now done.
  void CmdDone();

  // Signals that the transfer that was previously executed is now done.
  void TransferComplete();

  // Variant of CmdDone which schedules an event later in the future to complete the command.
  void CmdDoneLater(u64 cycles);

  enum class Command
  {
    NintendoID = 0x00,
    ReadArray = 0x52,
    ArrayToBuffer = 0x53,
    SetInterrupt = 0x81,
    WriteBuffer = 0x82,
    ReadStatus = 0x83,
    ReadID = 0x85,
    ReadErrorBuffer = 0x86,
    WakeUp = 0x87,
    Sleep = 0x88,
    ClearStatus = 0x89,
    SectorErase = 0xF1,
    PageProgram = 0xF2,
    ExtraByteProgram = 0xF3,
    ChipErase = 0xF4,
  };

  Slot m_card_slot;
  //! memory card state

  // STATE_TO_SAVE
  int m_interrupt_switch;
  bool m_interrupt_set;
  Command m_command;
  int m_status;
  u32 m_position;
  std::array<u8, 128> m_programming_buffer;
  //! memory card parameters
  unsigned int m_card_id;
  unsigned int m_address;
  u32 m_memory_card_size;
  std::unique_ptr<MemoryCardBase> m_memory_card;

protected:
  void TransferByte(u8& byte) override;
};
}  // namespace ExpansionInterface
