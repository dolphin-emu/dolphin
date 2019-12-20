// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.
//
#define AX_WII  // Used in AXVoice.
#define AX_WII_OLD

#include "Core/HW/DSPHLE/UCodes/AXWiiOld.h"

#include <algorithm>
#include <array>

#include "Common/ChunkFile.h"
#include "Common/CommonTypes.h"
#include "Common/Logging/Log.h"
#include "Common/Swap.h"
#include "Core/HW/DSPHLE/DSPHLE.h"
#include "Core/HW/DSPHLE/MailHandler.h"
#include "Core/HW/DSPHLE/UCodes/AXStructs.h"
#include "Core/HW/DSPHLE/UCodes/AXVoice.h"
#include "Core/HW/DSPHLE/UCodes/UCodes.h"

namespace DSP::HLE
{
AXWiiOldUCode::AXWiiOldUCode(DSPHLE* dsphle, u32 crc) : AXWiiUCode(dsphle, crc)
{
  INFO_LOG(DSPHLE, "Instantiating AXWiiOldUCode");
}

AXWiiOldUCode::~AXWiiOldUCode()
{
}

void AXWiiOldUCode::HandleCommandList()
{
  // Temp variables for addresses computation
  u16 addr_hi, addr_lo;
  u16 addr2_hi, addr2_lo;
  u16 volume;

  u32 pb_addr = 0;

  // WARN_LOG(DSPHLE, "Command list:");
  // for (u32 i = 0; m_cmdlist[i] != CMD_END; ++i)
  //     WARN_LOG(DSPHLE, "%04x", m_cmdlist[i]);
  // WARN_LOG(DSPHLE, "-------------");

  u32 curr_idx = 0;
  bool end = false;
  while (!end)
  {
    u16 cmd = m_cmdlist[curr_idx++];

    switch (cmd)
    {
      // Some of these commands are unknown, or unused in this AX HLE.
      // We still need to skip their arguments using "curr_idx += N".

    case CMD_SETUP:
      addr_hi = m_cmdlist[curr_idx++];
      addr_lo = m_cmdlist[curr_idx++];
      SetupProcessing(HILO_TO_32(addr));
      break;

    case CMD_ADD_TO_LR:
    case CMD_SUB_TO_LR:
      addr_hi = m_cmdlist[curr_idx++];
      addr_lo = m_cmdlist[curr_idx++];
      AddToLR(HILO_TO_32(addr), cmd == CMD_SUB_TO_LR);
      break;

    case CMD_ADD_SUB_TO_LR:
      addr_hi = m_cmdlist[curr_idx++];
      addr_lo = m_cmdlist[curr_idx++];
      AddSubToLR(HILO_TO_32(addr));
      break;

    case CMD_PB_ADDR:
      addr_hi = m_cmdlist[curr_idx++];
      addr_lo = m_cmdlist[curr_idx++];
      pb_addr = HILO_TO_32(addr);
      break;

    case CMD_PROCESS:
      ProcessPBList(pb_addr);
      break;

    case CMD_MIX_AUXA:
    case CMD_MIX_AUXB:
    case CMD_MIX_AUXC:
      volume = m_cmdlist[curr_idx++];
      addr_hi = m_cmdlist[curr_idx++];
      addr_lo = m_cmdlist[curr_idx++];
      addr2_hi = m_cmdlist[curr_idx++];
      addr2_lo = m_cmdlist[curr_idx++];
      MixAUXSamples(cmd - CMD_MIX_AUXA, HILO_TO_32(addr), HILO_TO_32(addr2), volume);
      break;

    case CMD_UPL_AUXA_MIX_LRSC:
    case CMD_UPL_AUXB_MIX_LRSC:
    {
      volume = m_cmdlist[curr_idx++];
      u32 addresses[6] = {
          (u32)(m_cmdlist[curr_idx + 0] << 16) | m_cmdlist[curr_idx + 1],
          (u32)(m_cmdlist[curr_idx + 2] << 16) | m_cmdlist[curr_idx + 3],
          (u32)(m_cmdlist[curr_idx + 4] << 16) | m_cmdlist[curr_idx + 5],
          (u32)(m_cmdlist[curr_idx + 6] << 16) | m_cmdlist[curr_idx + 7],
          (u32)(m_cmdlist[curr_idx + 8] << 16) | m_cmdlist[curr_idx + 9],
          (u32)(m_cmdlist[curr_idx + 10] << 16) | m_cmdlist[curr_idx + 11],
      };
      curr_idx += 12;
      UploadAUXMixLRSC(cmd == CMD_UPL_AUXB_MIX_LRSC, addresses, volume);
      break;
    }

    // TODO(delroth): figure this one out, it's used by almost every
    // game I've tested so far.
    case CMD_UNK_0B:
      curr_idx += 4;
      break;

    case CMD_OUTPUT:
    case CMD_OUTPUT_DPL2:
      addr_hi = m_cmdlist[curr_idx++];
      addr_lo = m_cmdlist[curr_idx++];
      addr2_hi = m_cmdlist[curr_idx++];
      addr2_lo = m_cmdlist[curr_idx++];
      OutputSamples(HILO_TO_32(addr2), HILO_TO_32(addr), 0x8000, cmd == CMD_OUTPUT_DPL2);
      break;

    case CMD_WM_OUTPUT:
    {
      u32 addresses[4] = {
          (u32)(m_cmdlist[curr_idx + 0] << 16) | m_cmdlist[curr_idx + 1],
          (u32)(m_cmdlist[curr_idx + 2] << 16) | m_cmdlist[curr_idx + 3],
          (u32)(m_cmdlist[curr_idx + 4] << 16) | m_cmdlist[curr_idx + 5],
          (u32)(m_cmdlist[curr_idx + 6] << 16) | m_cmdlist[curr_idx + 7],
      };
      curr_idx += 8;
      OutputWMSamples(addresses);
      break;
    }

    case CMD_END:
      end = true;
      break;
    }
  }
}

void AXWiiOldUCode::ProcessPBList(u32 pb_addr)
{
  // Samples per millisecond. In theory DSP sampling rate can be changed from
  // 32KHz to 48KHz, but AX always process at 32KHz.
  constexpr u32 spms = 32;

  AXPBWiiOld pb;

  while (pb_addr)
  {
    AXBuffers buffers = {{m_samples_left,      m_samples_right,      m_samples_surround,
                          m_samples_auxA_left, m_samples_auxA_right, m_samples_auxA_surround,
                          m_samples_auxB_left, m_samples_auxB_right, m_samples_auxB_surround,
                          m_samples_auxC_left, m_samples_auxC_right, m_samples_auxC_surround,
                          m_samples_wm0,       m_samples_aux0,       m_samples_wm1,
                          m_samples_aux1,      m_samples_wm2,        m_samples_aux2,
                          m_samples_wm3,       m_samples_aux3}};

    ReadPB(pb_addr, pb, m_crc);

    u32 updates_addr = HILO_TO_32(pb.updates.data);
    u16* updates = static_cast<u16*>(HLEMemory_Get_Pointer(updates_addr));

    for (int curr_ms = 0; curr_ms < 3; ++curr_ms)
    {
      ApplyUpdatesForMs(curr_ms, pb, pb.updates.num_updates, updates);

      ProcessVoice(pb, buffers, spms, ConvertMixerControl(HILO_TO_32(pb.mixer_control)),
                   m_coeffs_available ? m_coeffs : nullptr);

      // Forward the buffers
      for (auto& ptr : buffers.ptrs)
        ptr += spms;
    }

    WritePB(pb_addr, pb, m_crc);
    pb_addr = HILO_TO_32(pb.next_pb);
  }
}
}  // namespace DSP::HLE
