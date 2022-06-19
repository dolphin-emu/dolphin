// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later
//
#define AX_WII  // Used in AXVoice.

#include "Core/HW/DSPHLE/UCodes/AXWii.h"

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
AXWiiUCode::AXWiiUCode(DSPHLE* dsphle, u32 crc) : AXUCode(dsphle, crc), m_last_main_volume(0x8000)
{
  for (u16& volume : m_last_aux_volumes)
    volume = 0x8000;

  INFO_LOG_FMT(DSPHLE, "Instantiating AXWiiUCode");

  m_old_axwii = (crc == 0xfa450138) || (crc == 0x7699af32);
}

void AXWiiUCode::HandleCommandList()
{
  // Temp variables for addresses computation
  u16 addr_hi, addr_lo;
  u16 addr2_hi, addr2_lo;
  u16 volume;

  u32 pb_addr = 0;

  u32 curr_idx = 0;
  bool end = false;
  while (!end)
  {
    u16 cmd = m_cmdlist[curr_idx++];

    if (m_old_axwii)
    {
      switch (cmd)
      {
        // Some of these commands are unknown, or unused in this AX HLE.
        // We still need to skip their arguments using "curr_idx += N".

      case CMD_SETUP_OLD:
        addr_hi = m_cmdlist[curr_idx++];
        addr_lo = m_cmdlist[curr_idx++];
        SetupProcessing(HILO_TO_32(addr));
        break;

      case CMD_ADD_TO_LR_OLD:
      case CMD_SUB_TO_LR_OLD:
        addr_hi = m_cmdlist[curr_idx++];
        addr_lo = m_cmdlist[curr_idx++];
        AddToLR(HILO_TO_32(addr), cmd == CMD_SUB_TO_LR_OLD);
        break;

      case CMD_ADD_SUB_TO_LR_OLD:
        addr_hi = m_cmdlist[curr_idx++];
        addr_lo = m_cmdlist[curr_idx++];
        AddSubToLR(HILO_TO_32(addr));
        break;

      case CMD_PB_ADDR_OLD:
        addr_hi = m_cmdlist[curr_idx++];
        addr_lo = m_cmdlist[curr_idx++];
        pb_addr = HILO_TO_32(addr);
        break;

      case CMD_PROCESS_OLD:
        ProcessPBList(pb_addr);
        break;

      case CMD_MIX_AUXA_OLD:
      case CMD_MIX_AUXB_OLD:
      case CMD_MIX_AUXC_OLD:
        volume = m_cmdlist[curr_idx++];
        addr_hi = m_cmdlist[curr_idx++];
        addr_lo = m_cmdlist[curr_idx++];
        addr2_hi = m_cmdlist[curr_idx++];
        addr2_lo = m_cmdlist[curr_idx++];
        MixAUXSamples(cmd - CMD_MIX_AUXA_OLD, HILO_TO_32(addr), HILO_TO_32(addr2), volume);
        break;

      case CMD_UPL_AUXA_MIX_LRSC_OLD:
      case CMD_UPL_AUXB_MIX_LRSC_OLD:
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
        UploadAUXMixLRSC(cmd == CMD_UPL_AUXB_MIX_LRSC_OLD, addresses, volume);
        break;
      }

      case CMD_COMPRESSOR_OLD:
      {
        u16 threshold = m_cmdlist[curr_idx++];
        u16 frames = m_cmdlist[curr_idx++];
        addr_hi = m_cmdlist[curr_idx++];
        addr_lo = m_cmdlist[curr_idx++];
        RunCompressor(threshold, frames, HILO_TO_32(addr), 3);
        break;
      }

      case CMD_OUTPUT_OLD:
      case CMD_OUTPUT_DPL2_OLD:
        addr_hi = m_cmdlist[curr_idx++];
        addr_lo = m_cmdlist[curr_idx++];
        addr2_hi = m_cmdlist[curr_idx++];
        addr2_lo = m_cmdlist[curr_idx++];
        OutputSamples(HILO_TO_32(addr2), HILO_TO_32(addr), 0x8000, cmd == CMD_OUTPUT_DPL2_OLD);
        break;

      case CMD_WM_OUTPUT_OLD:
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

      case CMD_END_OLD:
        end = true;
        break;
      }
    }
    else
    {
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

      case CMD_PROCESS:
        addr_hi = m_cmdlist[curr_idx++];
        addr_lo = m_cmdlist[curr_idx++];
        ProcessPBList(HILO_TO_32(addr));
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

      case CMD_COMPRESSOR:
      {
        u16 threshold = m_cmdlist[curr_idx++];
        u16 frames = m_cmdlist[curr_idx++];
        addr_hi = m_cmdlist[curr_idx++];
        addr_lo = m_cmdlist[curr_idx++];
        RunCompressor(threshold, frames, HILO_TO_32(addr), 3);
        break;
      }

      case CMD_OUTPUT:
      case CMD_OUTPUT_DPL2:
        volume = m_crc == 0xd9c4bf34 ? 0x8000 : m_cmdlist[curr_idx++];
        addr_hi = m_cmdlist[curr_idx++];
        addr_lo = m_cmdlist[curr_idx++];
        addr2_hi = m_cmdlist[curr_idx++];
        addr2_lo = m_cmdlist[curr_idx++];
        OutputSamples(HILO_TO_32(addr2), HILO_TO_32(addr), volume, cmd == CMD_OUTPUT_DPL2);
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
}

void AXWiiUCode::SetupProcessing(u32 init_addr)
{
  const std::array<BufferDesc, 20> buffers = {{
      {m_samples_main_left, 32}, {m_samples_main_right, 32}, {m_samples_main_surround, 32},
      {m_samples_auxA_left, 32}, {m_samples_auxA_right, 32}, {m_samples_auxA_surround, 32},
      {m_samples_auxB_left, 32}, {m_samples_auxB_right, 32}, {m_samples_auxB_surround, 32},
      {m_samples_auxC_left, 32}, {m_samples_auxC_right, 32}, {m_samples_auxC_surround, 32},

      {m_samples_wm0, 6},        {m_samples_aux0, 6},        {m_samples_wm1, 6},
      {m_samples_aux1, 6},       {m_samples_wm2, 6},         {m_samples_aux2, 6},
      {m_samples_wm3, 6},        {m_samples_aux3, 6},
  }};
  InitMixingBuffers<3 /*ms*/>(init_addr, buffers);
}

void AXWiiUCode::AddToLR(u32 val_addr, bool neg)
{
  int* ptr = (int*)HLEMemory_Get_Pointer(val_addr);
  for (int i = 0; i < 32 * 3; ++i)
  {
    int val = (int)Common::swap32(*ptr++);
    if (neg)
      val = -val;

    m_samples_main_left[i] += val;
    m_samples_main_right[i] += val;
  }
}

void AXWiiUCode::AddSubToLR(u32 val_addr)
{
  int* ptr = (int*)HLEMemory_Get_Pointer(val_addr);
  for (int i = 0; i < 32 * 3; ++i)
  {
    int val = (int)Common::swap32(*ptr++);
    m_samples_main_left[i] += val;
  }
  for (int i = 0; i < 32 * 3; ++i)
  {
    int val = (int)Common::swap32(*ptr++);
    m_samples_main_right[i] -= val;
  }
}

AXMixControl AXWiiUCode::ConvertMixerControl(u32 mixer_control)
{
  u32 ret = 0;

  if (mixer_control & 0x00000001)
    ret |= MIX_MAIN_L;
  if (mixer_control & 0x00000002)
    ret |= MIX_MAIN_R;
  if (mixer_control & 0x00000004)
    ret |= MIX_MAIN_L | MIX_MAIN_R | MIX_MAIN_L_RAMP | MIX_MAIN_R_RAMP;
  if (mixer_control & 0x00000008)
    ret |= MIX_MAIN_S;
  if (mixer_control & 0x00000010)
    ret |= MIX_MAIN_S | MIX_MAIN_S_RAMP;
  if (mixer_control & 0x00010000)
    ret |= MIX_AUXA_L;
  if (mixer_control & 0x00020000)
    ret |= MIX_AUXA_R;
  if (mixer_control & 0x00040000)
    ret |= MIX_AUXA_L | MIX_AUXA_R | MIX_AUXA_L_RAMP | MIX_AUXA_R_RAMP;
  if (mixer_control & 0x00080000)
    ret |= MIX_AUXA_S;
  if (mixer_control & 0x00100000)
    ret |= MIX_AUXA_S | MIX_AUXA_S_RAMP;
  if (mixer_control & 0x00200000)
    ret |= MIX_AUXB_L;
  if (mixer_control & 0x00400000)
    ret |= MIX_AUXB_R;
  if (mixer_control & 0x00800000)
    ret |= MIX_AUXB_L | MIX_AUXB_R | MIX_AUXB_L_RAMP | MIX_AUXB_R_RAMP;
  if (mixer_control & 0x01000000)
    ret |= MIX_AUXB_S;
  if (mixer_control & 0x02000000)
    ret |= MIX_AUXB_S | MIX_AUXB_S_RAMP;
  if (mixer_control & 0x04000000)
    ret |= MIX_AUXC_L;
  if (mixer_control & 0x08000000)
    ret |= MIX_AUXC_R;
  if (mixer_control & 0x10000000)
    ret |= MIX_AUXC_L | MIX_AUXC_R | MIX_AUXC_L_RAMP | MIX_AUXC_R_RAMP;
  if (mixer_control & 0x20000000)
    ret |= MIX_AUXC_S;
  if (mixer_control & 0x40000000)
    ret |= MIX_AUXC_S | MIX_AUXC_S_RAMP;

  return (AXMixControl)ret;
}

void AXWiiUCode::GenerateVolumeRamp(u16* output, u16 vol1, u16 vol2, size_t nvals)
{
  float curr = vol1;
  for (size_t i = 0; i < nvals; ++i)
  {
    curr += (vol2 - vol1) / (float)nvals;
    output[i] = (u16)curr;
  }
}

bool AXWiiUCode::ExtractUpdatesFields(AXPBWii& pb, u16* num_updates, u16* updates,
                                      u32* updates_addr)
{
  auto pb_mem = Common::BitCastToArray<u16>(pb);

  if (!m_old_axwii)
    return false;

  // Copy the num_updates field.
  memcpy(num_updates, &pb_mem[41], 6);

  // Get the address of the updates data
  u16 addr_hi = pb_mem[44];
  u16 addr_lo = pb_mem[45];
  u32 addr = HILO_TO_32(addr);
  u16* ptr = (u16*)HLEMemory_Get_Pointer(addr);

  *updates_addr = addr;

  // Copy the updates data and change the offset to match a PB without
  // updates data.
  u32 updates_count = num_updates[0] + num_updates[1] + num_updates[2];
  for (u32 i = 0; i < updates_count; ++i)
  {
    u16 update_off = Common::swap16(ptr[2 * i]);
    u16 update_val = Common::swap16(ptr[2 * i + 1]);

    if (update_off > 45)
      update_off -= 5;

    updates[2 * i] = update_off;
    updates[2 * i + 1] = update_val;
  }

  // Remove the updates data from the PB
  memmove(&pb_mem[41], &pb_mem[46], sizeof(pb) - 2 * 46);

  Common::BitCastFromArray<u16>(pb_mem, pb);

  return true;
}

void AXWiiUCode::ReinjectUpdatesFields(AXPBWii& pb, u16* num_updates, u32 updates_addr)
{
  auto pb_mem = Common::BitCastToArray<u16>(pb);

  // Make some space
  memmove(&pb_mem[46], &pb_mem[41], sizeof(pb) - 2 * 46);

  // Reinsert previous values
  pb_mem[41] = num_updates[0];
  pb_mem[42] = num_updates[1];
  pb_mem[43] = num_updates[2];
  pb_mem[44] = updates_addr >> 16;
  pb_mem[45] = updates_addr & 0xFFFF;

  Common::BitCastFromArray<u16>(pb_mem, pb);
}

void AXWiiUCode::ProcessPBList(u32 pb_addr)
{
  // Samples per millisecond. In theory DSP sampling rate can be changed from
  // 32KHz to 48KHz, but AX always process at 32KHz.
  constexpr u32 spms = 32;

  AXPBWii pb;

  while (pb_addr)
  {
    AXBuffers buffers = {{m_samples_main_left, m_samples_main_right, m_samples_main_surround,
                          m_samples_auxA_left, m_samples_auxA_right, m_samples_auxA_surround,
                          m_samples_auxB_left, m_samples_auxB_right, m_samples_auxB_surround,
                          m_samples_auxC_left, m_samples_auxC_right, m_samples_auxC_surround,
                          m_samples_wm0,       m_samples_aux0,       m_samples_wm1,
                          m_samples_aux1,      m_samples_wm2,        m_samples_aux2,
                          m_samples_wm3,       m_samples_aux3}};

    ReadPB(pb_addr, pb, m_crc);

    u16 num_updates[3];
    u16 updates[1024];
    u32 updates_addr;
    if (ExtractUpdatesFields(pb, num_updates, updates, &updates_addr))
    {
      for (int curr_ms = 0; curr_ms < 3; ++curr_ms)
      {
        ApplyUpdatesForMs(curr_ms, pb, num_updates, updates);
        ProcessVoice(pb, buffers, spms, ConvertMixerControl(HILO_TO_32(pb.mixer_control)),
                     m_coeffs_checksum ? m_coeffs.data() : nullptr);

        // Forward the buffers
        for (auto& ptr : buffers.ptrs)
          ptr += spms;
      }
      ReinjectUpdatesFields(pb, num_updates, updates_addr);
    }
    else
    {
      ProcessVoice(pb, buffers, 96, ConvertMixerControl(HILO_TO_32(pb.mixer_control)),
                   m_coeffs_checksum ? m_coeffs.data() : nullptr);
    }

    WritePB(pb_addr, pb, m_crc);
    pb_addr = HILO_TO_32(pb.next_pb);
  }
}

void AXWiiUCode::MixAUXSamples(int aux_id, u32 write_addr, u32 read_addr, u16 volume)
{
  std::array<u16, 96> volume_ramp;
  GenerateVolumeRamp(volume_ramp.data(), m_last_aux_volumes[aux_id], volume, volume_ramp.size());
  m_last_aux_volumes[aux_id] = volume;

  std::array<int*, 3> main_buffers{
      m_samples_main_left,
      m_samples_main_right,
      m_samples_main_surround,
  };

  std::array<const int*, 3> buffers{};
  switch (aux_id)
  {
  case 0:
    buffers = {
        m_samples_auxA_left,
        m_samples_auxA_right,
        m_samples_auxA_surround,
    };
    break;

  case 1:
    buffers = {
        m_samples_auxB_left,
        m_samples_auxB_right,
        m_samples_auxB_surround,
    };
    break;

  case 2:
    buffers = {
        m_samples_auxC_left,
        m_samples_auxC_right,
        m_samples_auxC_surround,
    };
    break;
  }

  // Send the content of AUX buffers to the CPU
  if (write_addr)
  {
    int* ptr = (int*)HLEMemory_Get_Pointer(write_addr);
    for (const auto& buffer : buffers)
    {
      for (u32 j = 0; j < 3 * 32; ++j)
        *ptr++ = Common::swap32(buffer[j]);
    }
  }

  // Then read the buffers from the CPU and add to our main buffers.
  const int* ptr = (int*)HLEMemory_Get_Pointer(read_addr);
  for (auto& main_buffer : main_buffers)
  {
    for (u32 j = 0; j < 3 * 32; ++j)
    {
      s64 sample = (s64)(s32)Common::swap32(*ptr++);
      sample *= volume_ramp[j];
      main_buffer[j] += (s32)(sample >> 15);
    }
  }
}

void AXWiiUCode::UploadAUXMixLRSC(int aux_id, u32* addresses, u16 volume)
{
  int* aux_left = aux_id ? m_samples_auxB_left : m_samples_auxA_left;
  int* aux_right = aux_id ? m_samples_auxB_right : m_samples_auxA_right;
  int* aux_surround = aux_id ? m_samples_auxB_surround : m_samples_auxA_surround;
  int* auxc_buffer = aux_id ? m_samples_auxC_surround : m_samples_auxC_right;

  int* upload_ptr = (int*)HLEMemory_Get_Pointer(addresses[0]);
  for (u32 i = 0; i < 96; ++i)
    *upload_ptr++ = Common::swap32(aux_left[i]);
  for (u32 i = 0; i < 96; ++i)
    *upload_ptr++ = Common::swap32(aux_right[i]);
  for (u32 i = 0; i < 96; ++i)
    *upload_ptr++ = Common::swap32(aux_surround[i]);

  upload_ptr = (int*)HLEMemory_Get_Pointer(addresses[1]);
  for (u32 i = 0; i < 96; ++i)
    *upload_ptr++ = Common::swap32(auxc_buffer[i]);

  u16 volume_ramp[96];
  GenerateVolumeRamp(volume_ramp, m_last_aux_volumes[aux_id], volume, 96);
  m_last_aux_volumes[aux_id] = volume;

  int* mix_dest[4] = {m_samples_main_left, m_samples_main_right, m_samples_main_surround,
                      m_samples_auxC_left};
  for (u32 mix_i = 0; mix_i < 4; ++mix_i)
  {
    int* dl_ptr = (int*)HLEMemory_Get_Pointer(addresses[2 + mix_i]);
    for (u32 i = 0; i < 96; ++i)
      aux_left[i] = Common::swap32(dl_ptr[i]);

    for (u32 i = 0; i < 96; ++i)
    {
      s64 sample = (s64)(s32)aux_left[i];
      sample *= volume_ramp[i];
      mix_dest[mix_i][i] += (s32)(sample >> 15);
    }
  }
}

void AXWiiUCode::OutputSamples(u32 lr_addr, u32 surround_addr, u16 volume, bool upload_auxc)
{
  std::array<u16, 96> volume_ramp;
  GenerateVolumeRamp(volume_ramp.data(), m_last_main_volume, volume, volume_ramp.size());
  m_last_main_volume = volume;

  std::array<int, 3 * 32> upload_buffer{};

  for (size_t i = 0; i < upload_buffer.size(); ++i)
    upload_buffer[i] = Common::swap32(m_samples_main_surround[i]);
  memcpy(HLEMemory_Get_Pointer(surround_addr), upload_buffer.data(), sizeof(upload_buffer));

  if (upload_auxc)
  {
    surround_addr += sizeof(upload_buffer);
    for (size_t i = 0; i < upload_buffer.size(); ++i)
      upload_buffer[i] = Common::swap32(m_samples_auxC_left[i]);
    memcpy(HLEMemory_Get_Pointer(surround_addr), upload_buffer.data(), sizeof(upload_buffer));
  }

  // Clamp internal buffers to 16 bits.
  for (size_t i = 0; i < volume_ramp.size(); ++i)
  {
    int left = m_samples_main_left[i];
    int right = m_samples_main_right[i];

    // Apply global volume. Cast to s64 to avoid overflow.
    left = ((s64)left * volume_ramp[i]) >> 15;
    right = ((s64)right * volume_ramp[i]) >> 15;

    m_samples_main_left[i] = std::clamp(left, -32767, 32767);
    m_samples_main_right[i] = std::clamp(right, -32767, 32767);
  }

  std::array<s16, 3 * 32 * 2> buffer;
  for (size_t i = 0; i < 3 * 32; ++i)
  {
    buffer[2 * i] = Common::swap16(m_samples_main_right[i]);
    buffer[2 * i + 1] = Common::swap16(m_samples_main_left[i]);
  }

  memcpy(HLEMemory_Get_Pointer(lr_addr), buffer.data(), sizeof(buffer));
  m_mail_handler.PushMail(DSP_SYNC, true);
}

void AXWiiUCode::OutputWMSamples(u32* addresses)
{
  int* buffers[] = {m_samples_wm0, m_samples_wm1, m_samples_wm2, m_samples_wm3};

  for (u32 i = 0; i < 4; ++i)
  {
    int* in = buffers[i];
    u16* out = (u16*)HLEMemory_Get_Pointer(addresses[i]);
    for (u32 j = 0; j < 3 * 6; ++j)
    {
      int sample = std::clamp(in[j], -32767, 32767);
      out[j] = Common::swap16((u16)sample);
    }
  }
}

void AXWiiUCode::DoState(PointerWrap& p)
{
  DoStateShared(p);
  DoAXState(p);

  p.Do(m_samples_auxC_left);
  p.Do(m_samples_auxC_right);
  p.Do(m_samples_auxC_surround);

  p.Do(m_samples_wm0);
  p.Do(m_samples_wm1);
  p.Do(m_samples_wm2);
  p.Do(m_samples_wm3);

  p.Do(m_samples_aux0);
  p.Do(m_samples_aux1);
  p.Do(m_samples_aux2);
  p.Do(m_samples_aux3);

  p.Do(m_last_main_volume);
  p.Do(m_last_aux_volumes);
}
}  // namespace DSP::HLE
