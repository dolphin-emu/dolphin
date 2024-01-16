// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/HW/DSPHLE/UCodes/ROM.h"

#include <string>

#ifdef _WIN32
#include <Windows.h>
#endif

#include "Common/ChunkFile.h"
#include "Common/CommonTypes.h"
#include "Common/Hash.h"
#include "Common/Logging/Log.h"
#include "Core/Config/MainSettings.h"
#include "Core/ConfigManager.h"
#include "Core/DSP/DSPCodeUtil.h"
#include "Core/HW/DSPHLE/DSPHLE.h"
#include "Core/HW/DSPHLE/MailHandler.h"
#include "Core/HW/DSPHLE/UCodes/UCodes.h"
#include "Core/System.h"

namespace DSP::HLE
{
ROMUCode::ROMUCode(DSPHLE* dsphle, u32 crc)
    : UCodeInterface(dsphle, crc), m_current_ucode(), m_boot_task_num_steps(0), m_next_parameter(0)
{
  INFO_LOG_FMT(DSPHLE, "UCode_Rom - initialized");
}

void ROMUCode::Initialize()
{
  m_mail_handler.PushMail(0x8071FEED);
}

void ROMUCode::Update()
{
}

void ROMUCode::HandleMail(u32 mail)
{
  if (m_next_parameter == 0)
  {
    // wait for beginning of UCode
    if ((mail & 0xFFFF0000) != 0x80F30000)
    {
      u32 Message = 0xFEEE0000 | (mail & 0xFFFF);
      m_mail_handler.PushMail(Message);
    }
    else
    {
      m_next_parameter = mail;
    }
  }
  else
  {
    switch (m_next_parameter)
    {
    case 0x80F3A001:
      m_current_ucode.m_ram_address = mail;
      break;

    case 0x80F3A002:
      m_current_ucode.m_length = mail & 0xffff;
      break;

    case 0x80F3B002:
      m_current_ucode.m_dmem_length = mail & 0xffff;
      if (m_current_ucode.m_dmem_length)
      {
        NOTICE_LOG_FMT(DSPHLE, "m_current_ucode.m_dmem_length = {:#06x}.",
                       m_current_ucode.m_dmem_length);
      }
      break;

    case 0x80F3C002:
      m_current_ucode.m_imem_address = mail & 0xffff;
      break;

    case 0x80F3D001:
      m_current_ucode.m_start_pc = mail & 0xffff;
      BootUCode();
      return;  // Important! BootUCode indirectly does "delete this;". Must exit immediately.

    default:
      break;
    }

    // THE GODDAMN OVERWRITE WAS HERE. Without the return above, since BootUCode may delete "this",
    // well ...
    m_next_parameter = 0;
  }
}

void ROMUCode::BootUCode()
{
  auto& memory = m_dsphle->GetSystem().GetMemory();
  const u32 ector_crc = Common::HashEctor(
      static_cast<u8*>(HLEMemory_Get_Pointer(memory, m_current_ucode.m_ram_address)),
      m_current_ucode.m_length);

  if (Config::Get(Config::MAIN_DUMP_UCODE))
  {
    DSP::DumpDSPCode(static_cast<u8*>(HLEMemory_Get_Pointer(memory, m_current_ucode.m_ram_address)),
                     m_current_ucode.m_length, ector_crc);
  }

  INFO_LOG_FMT(DSPHLE, "CurrentUCode SOURCE Addr: {:#010x}", m_current_ucode.m_ram_address);
  INFO_LOG_FMT(DSPHLE, "CurrentUCode Length:      {:#010x}", m_current_ucode.m_length);
  INFO_LOG_FMT(DSPHLE, "CurrentUCode DEST Addr:   {:#010x}", m_current_ucode.m_imem_address);
  INFO_LOG_FMT(DSPHLE, "CurrentUCode DMEM Length: {:#010x}", m_current_ucode.m_dmem_length);
  INFO_LOG_FMT(DSPHLE, "CurrentUCode init_vector: {:#010x}", m_current_ucode.m_start_pc);
  INFO_LOG_FMT(DSPHLE, "CurrentUCode CRC:         {:#010x}", ector_crc);
  INFO_LOG_FMT(DSPHLE, "BootTask - done");

  m_dsphle->SetUCode(ector_crc);
}

void ROMUCode::DoState(PointerWrap& p)
{
  p.Do(m_current_ucode);
  p.Do(m_boot_task_num_steps);
  p.Do(m_next_parameter);

  DoStateShared(p);
}
}  // namespace DSP::HLE
