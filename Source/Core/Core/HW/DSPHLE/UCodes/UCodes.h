// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <cstring>
#include <memory>

#include "Common/CommonTypes.h"

namespace Memory
{
class MemoryManager;
}
class PointerWrap;

namespace DSP::HLE
{
class CMailHandler;
class DSPHLE;

#define UCODE_ROM 0x00000000
#define UCODE_INIT_AUDIO_SYSTEM 0x00000001
#define UCODE_NULL 0xFFFFFFFF

u8 HLEMemory_Read_U8(Memory::MemoryManager& memory, u32 address);
void HLEMemory_Write_U8(Memory::MemoryManager& memory, u32 address, u8 value);

u16 HLEMemory_Read_U16LE(Memory::MemoryManager& memory, u32 address);
u16 HLEMemory_Read_U16(Memory::MemoryManager& memory, u32 address);

void HLEMemory_Write_U16LE(Memory::MemoryManager& memory, u32 address, u16 value);
void HLEMemory_Write_U16(Memory::MemoryManager& memory, u32 address, u16 value);

u32 HLEMemory_Read_U32LE(Memory::MemoryManager& memory, u32 address);
u32 HLEMemory_Read_U32(Memory::MemoryManager& memory, u32 address);

void HLEMemory_Write_U32LE(Memory::MemoryManager& memory, u32 address, u32 value);
void HLEMemory_Write_U32(Memory::MemoryManager& memory, u32 address, u32 value);

void* HLEMemory_Get_Pointer(Memory::MemoryManager& memory, u32 address);

class UCodeInterface
{
public:
  UCodeInterface(DSPHLE* dsphle, u32 crc);
  virtual ~UCodeInterface();

  virtual void Initialize() = 0;
  virtual void HandleMail(u32 mail) = 0;
  virtual void Update() = 0;

  virtual void DoState(PointerWrap& p) = 0;
  static u32 GetCRC(UCodeInterface* ucode) { return ucode ? ucode->m_crc : UCODE_NULL; }

protected:
  void PrepareBootUCode(u32 mail);

  // Some ucodes (notably zelda) require a resume mail to be
  // sent if they are be started via PrepareBootUCode.
  // The HLE can use this to
  bool NeedsResumeMail();

  void DoStateShared(PointerWrap& p);

  CMailHandler& m_mail_handler;

  static constexpr u32 TASK_MAIL_MASK = 0xFFFF'0000;
  // Task-management mails, used for uCode switching. The DSP sends mail with 0xDCD1 in the high
  // word to the CPU to report its status. The CPU reacts in different ways for different mails.
  // Also, Zelda uCode titles use a slightly different handler compared to AX uCode titles. Libogc's
  // mail handler is based on the AX one.
  static constexpr u32 TASK_MAIL_TO_CPU = 0xDCD1'0000;
  // Triggers a callback. No response is sent.
  static constexpr u32 DSP_INIT = TASK_MAIL_TO_CPU | 0x0000;
  // Triggers a callback. No response is sent.
  static constexpr u32 DSP_RESUME = TASK_MAIL_TO_CPU | 0x0001;
  // If the current task is canceled by the CPU, the CPU processes this mail as if it were DSP_DONE
  // instead. Otherwise, it is handled differently for Zelda uCode games and AX games.
  // On Zelda uCode, the CPU will always respond with MAIL_NEW_UCODE.
  // On AX uCode, the CPU will respond with MAIL_NEW_UCODE if it has a new task, and otherwise
  // will use MAIL_CONTINUE.
  static constexpr u32 DSP_YIELD = TASK_MAIL_TO_CPU | 0x0002;
  // Triggers a callback. The response is handled differently for Zelda uCode games and AX games.
  // On Zelda uCode, the CPU will always respond with MAIL_NEW_UCODE.
  // On AX uCode, the CPU will respond with MAIL_NEW_UCODE if there is a new task, and otherwise
  // will use MAIL_RESET if the finished task was the only task.
  static constexpr u32 DSP_DONE = TASK_MAIL_TO_CPU | 0x0003;
  // Triggers a callback. No response is sent.
  static constexpr u32 DSP_SYNC = TASK_MAIL_TO_CPU | 0x0004;
  // Used by Zelda uCode only. Zelda uCode titles (or at least Super Mario Sunshine and Super Mario
  // Galaxy) will log "Audio Yield Start", send MAIL_NEW_UCODE, and then log "Audio Yield Finish" if
  // they have a task to execute (e.g. the card uCode), and otherwise will send MAIL_CONTINUE.
  static constexpr u32 DSP_FRAME_END = TASK_MAIL_TO_CPU | 0x0005;

  // The CPU will send a mail prefixed with 0xCDD1 in the high word in response.
  static constexpr u32 TASK_MAIL_TO_DSP = 0xCDD1'0000;
  // On AX, this sends DSP_RESUME and then returns to normal execution. On Zelda, this immediately
  // HALTs. Other uCodes (e.g. Card, GBA) do not implement (and instead ignore) this mail.
  // This mail does not seem to be sent in practice.
  static constexpr u32 MAIL_RESUME = TASK_MAIL_TO_DSP | 0x0000;
  // Starts populating info for a new uCode (see PrepareBootUCode and m_upload_setup_in_progress).
  // The current uCode's state can optionally be saved, although the Card and GBA uCode do not
  // support this (as there is no reason to reload their old state).
  static constexpr u32 MAIL_NEW_UCODE = TASK_MAIL_TO_DSP | 0x0001;
  // Immediately switches to ROM uCode. Implemented by the Zelda uCode, but the Zelda uCode task
  // handler doesn't seem to send it.
  static constexpr u32 MAIL_RESET = TASK_MAIL_TO_DSP | 0x0002;
  // Jumps back to the main loop. Not implemented by all uCode (in particular Card and GBA which do
  // not have a main loop).
  static constexpr u32 MAIL_CONTINUE = TASK_MAIL_TO_DSP | 0x0003;

  // UCode is forwarding mails to PrepareBootUCode
  // UCode only needs to set this to true, UCodeInterface will set to false when done!
  bool m_upload_setup_in_progress = false;

  // Need a pointer back to DSPHLE to switch ucodes.
  DSPHLE* m_dsphle;

  // used for reconstruction when loading saves,
  // and for variations within certain ucodes.
  u32 m_crc;

private:
  struct NextUCodeInfo
  {
    u32 mram_dest_addr;
    u16 mram_size;
    u16 mram_dram_addr;
    u32 iram_mram_addr;
    u16 iram_size;
    u16 iram_dest;
    u16 iram_startpc;
    u32 dram_mram_addr;
    u16 dram_size;
    u16 dram_dest;
  };
  NextUCodeInfo m_next_ucode{};
  int m_next_ucode_steps = 0;

  bool m_needs_resume_mail = false;
};

std::unique_ptr<UCodeInterface> UCodeFactory(u32 crc, DSPHLE* dsphle, bool wii);
}  // namespace DSP::HLE
