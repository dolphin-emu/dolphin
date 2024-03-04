// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/HW/DSPHLE/UCodes/AX.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <iterator>

#include "Common/ChunkFile.h"
#include "Common/CommonTypes.h"
#include "Common/FileUtil.h"
#include "Common/Hash.h"
#include "Common/IOFile.h"
#include "Common/Logging/Log.h"
#include "Common/Swap.h"
#include "Core/Core.h"
#include "Core/DolphinAnalytics.h"
#include "Core/HW/DSP.h"
#include "Core/HW/DSPHLE/DSPHLE.h"
#include "Core/HW/DSPHLE/MailHandler.h"
#include "Core/HW/DSPHLE/UCodes/AXStructs.h"

#define AX_GC
#include "Core/HW/DSPHLE/UCodes/AXVoice.h"

namespace DSP::HLE
{
AXUCode::AXUCode(DSPHLE* dsphle, u32 crc, bool dummy) : UCodeInterface(dsphle, crc)
{
}

AXUCode::AXUCode(DSPHLE* dsphle, u32 crc) : AXUCode(dsphle, crc, false)
{
  INFO_LOG_FMT(DSPHLE, "Instantiating AXUCode: crc={:08x}", crc);

  m_accelerator = std::make_unique<HLEAccelerator>(dsphle->GetSystem().GetDSP());
}

AXUCode::~AXUCode() = default;

void AXUCode::Initialize()
{
  InitializeShared();
}

void AXUCode::InitializeShared()
{
  m_mail_handler.PushMail(DSP_INIT, true);

  LoadResamplingCoefficients(false, 0);
}

bool AXUCode::LoadResamplingCoefficients(bool require_same_checksum, u32 desired_checksum)
{
  constexpr size_t raw_coeffs_size = 0x800 * 2;
  m_coeffs_checksum = std::nullopt;

  const std::array<std::string, 2> filenames{
      File::GetUserPath(D_GCUSER_IDX) + "dsp_coef.bin",
      File::GetSysDirectory() + "/GC/dsp_coef.bin",
  };

  for (const std::string& filename : filenames)
  {
    INFO_LOG_FMT(DSPHLE, "Checking for polyphase resampling coeffs at {}", filename);

    if (File::GetSize(filename) != raw_coeffs_size)
      continue;

    File::IOFile fp(filename, "rb");
    std::array<u8, raw_coeffs_size> raw_coeffs;
    fp.ReadBytes(raw_coeffs.data(), raw_coeffs_size);

    u32 checksum = Common::HashAdler32(raw_coeffs.data(), raw_coeffs_size);
    if (require_same_checksum && checksum != desired_checksum)
      continue;

    std::memcpy(m_coeffs.data(), raw_coeffs.data(), raw_coeffs_size);
    for (auto& coef : m_coeffs)
      coef = Common::swap16(coef);

    INFO_LOG_FMT(DSPHLE, "Using polyphase resampling coeffs from {}", filename);
    m_coeffs_checksum = checksum;
    return true;
  }

  return false;
}

void AXUCode::SignalWorkEnd()
{
  // Signal end of processing
  // TODO: figure out how many cycles this is actually supposed to take

  // The Clone Wars hangs upon initial boot if this interrupt happens too quickly after submitting a
  // command list. When played in DSP-LLE, the interrupt lags by about 160,000 cycles, though any
  // value greater than or equal to 814 will work here. In other games, the lag can be as small as
  // 50,000 cycles (in Metroid Prime) and as large as 718,092 cycles (in Tales of Symphonia!).

  // On the PowerPC side, hthh_ discovered that The Clone Wars tracks a "AXCommandListCycles"
  // variable which matches the aforementioned 160,000 cycles. It's initialized to ~2500 cycles for
  // a minimal, empty command list, so that should be a safe number for pretty much anything a game
  // does.

  // For more information, see https://bugs.dolphin-emu.org/issues/10265.
  constexpr int AX_EMPTY_COMMAND_LIST_CYCLES = 2500;

  m_mail_handler.PushMail(DSP_YIELD, true, AX_EMPTY_COMMAND_LIST_CYCLES);
}

void AXUCode::HandleCommandList()
{
  // Temp variables for addresses computation
  u16 addr_hi, addr_lo;
  u16 addr2_hi, addr2_lo;
  u16 size;

  u32 pb_addr = 0;

#if 0
	INFO_LOG_FMT(DSPHLE, "Command list:");
	for (u32 i = 0; m_cmdlist[i] != CMD_END; ++i)
		INFO_LOG_FMT(DSPHLE, "{:04x}", m_cmdlist[i]);
	INFO_LOG_FMT(DSPHLE, "-------------");
#endif

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

    case CMD_DL_AND_VOL_MIX:
    {
      addr_hi = m_cmdlist[curr_idx++];
      addr_lo = m_cmdlist[curr_idx++];
      u16 vol_main = m_cmdlist[curr_idx++];
      u16 vol_auxa = m_cmdlist[curr_idx++];
      u16 vol_auxb = m_cmdlist[curr_idx++];
      DownloadAndMixWithVolume(HILO_TO_32(addr), vol_main, vol_auxa, vol_auxb);
      break;
    }

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
      // These two commands are handled almost the same internally.
      addr_hi = m_cmdlist[curr_idx++];
      addr_lo = m_cmdlist[curr_idx++];
      addr2_hi = m_cmdlist[curr_idx++];
      addr2_lo = m_cmdlist[curr_idx++];
      MixAUXSamples(cmd - CMD_MIX_AUXA, HILO_TO_32(addr), HILO_TO_32(addr2));
      break;

    case CMD_UPLOAD_LRS:
      addr_hi = m_cmdlist[curr_idx++];
      addr_lo = m_cmdlist[curr_idx++];
      UploadLRS(HILO_TO_32(addr));
      break;

    case CMD_SET_LR:
      addr_hi = m_cmdlist[curr_idx++];
      addr_lo = m_cmdlist[curr_idx++];
      SetMainLR(HILO_TO_32(addr));
      break;

    case CMD_UNK_08:
      DolphinAnalytics::Instance().ReportGameQuirk(GameQuirk::USES_UNIMPLEMENTED_AX_COMMAND);
      curr_idx += 10;
      break;  // TODO: check

    case CMD_MIX_AUXB_NOWRITE:
      addr_hi = m_cmdlist[curr_idx++];
      addr_lo = m_cmdlist[curr_idx++];
      MixAUXSamples(1, 0, HILO_TO_32(addr));
      break;

    case CMD_UNK_0A:
    case CMD_UNK_0B:
    case CMD_UNK_0C:
      // nop in all 6 known ucodes we handle here
      break;

    case CMD_MORE:
      addr_hi = m_cmdlist[curr_idx++];
      addr_lo = m_cmdlist[curr_idx++];
      size = m_cmdlist[curr_idx++];

      CopyCmdList(HILO_TO_32(addr), size);
      curr_idx = 0;
      break;

    case CMD_OUTPUT:
      addr_hi = m_cmdlist[curr_idx++];
      addr_lo = m_cmdlist[curr_idx++];
      addr2_hi = m_cmdlist[curr_idx++];
      addr2_lo = m_cmdlist[curr_idx++];
      OutputSamples(HILO_TO_32(addr2), HILO_TO_32(addr));
      break;

    case CMD_END:
      end = true;
      break;

    case CMD_MIX_AUXB_LR:
      addr_hi = m_cmdlist[curr_idx++];
      addr_lo = m_cmdlist[curr_idx++];
      addr2_hi = m_cmdlist[curr_idx++];
      addr2_lo = m_cmdlist[curr_idx++];
      MixAUXBLR(HILO_TO_32(addr), HILO_TO_32(addr2));
      break;

    case CMD_SET_OPPOSITE_LR:
      addr_hi = m_cmdlist[curr_idx++];
      addr_lo = m_cmdlist[curr_idx++];
      SetOppositeLR(HILO_TO_32(addr));
      break;

    case CMD_COMPRESSOR:
    {
      // 0x4e8a8b21 doesn't have this command, but it doesn't range-check
      // the value properly and ends up jumping into a mixer function
      ASSERT(m_crc != 0x4e8a8b21);
      u16 threshold = m_cmdlist[curr_idx++];
      u16 frames = m_cmdlist[curr_idx++];
      addr_hi = m_cmdlist[curr_idx++];
      addr_lo = m_cmdlist[curr_idx++];
      RunCompressor(threshold, frames, HILO_TO_32(addr), 5);
      break;
    }

    // Send the contents of AUXA LRS and AUXB S to RAM, and
    // mix data to MAIN LR and AUXB LR.
    case CMD_SEND_AUX_AND_MIX:
    {
      // Address for AUXA LRS upload
      u16 auxa_lrs_up_hi = m_cmdlist[curr_idx++];
      u16 auxa_lrs_up_lo = m_cmdlist[curr_idx++];

      // Address for AUXB S upload
      u16 auxb_s_up_hi = m_cmdlist[curr_idx++];
      u16 auxb_s_up_lo = m_cmdlist[curr_idx++];

      // Address to read data for Main L
      u16 main_l_dl_hi = m_cmdlist[curr_idx++];
      u16 main_l_dl_lo = m_cmdlist[curr_idx++];

      // Address to read data for Main R
      u16 main_r_dl_hi = m_cmdlist[curr_idx++];
      u16 main_r_dl_lo = m_cmdlist[curr_idx++];

      // Address to read data for AUXB L
      u16 auxb_l_dl_hi = m_cmdlist[curr_idx++];
      u16 auxb_l_dl_lo = m_cmdlist[curr_idx++];

      // Address to read data for AUXB R
      u16 auxb_r_dl_hi = m_cmdlist[curr_idx++];
      u16 auxb_r_dl_lo = m_cmdlist[curr_idx++];

      SendAUXAndMix(HILO_TO_32(auxa_lrs_up), HILO_TO_32(auxb_s_up), HILO_TO_32(main_l_dl),
                    HILO_TO_32(main_r_dl), HILO_TO_32(auxb_l_dl), HILO_TO_32(auxb_r_dl));
      break;
    }

    default:
      ERROR_LOG_FMT(DSPHLE, "Unknown command in AX command list: {:04x}", cmd);
      end = true;
      break;
    }
  }
}

AXMixControl AXUCode::ConvertMixerControl(u32 mixer_control)
{
  u32 ret = 0;

  // TODO: find other UCode versions with different mixer_control values
  if (m_crc == 0x4e8a8b21)
  {
    if (mixer_control & 0x0010)
    {
      // DPL2 mixing
      ret |= MIX_MAIN_L | MIX_MAIN_R;
      if ((mixer_control & 0x0006) == 0)
        ret |= MIX_AUXB_L | MIX_AUXB_R;
      if ((mixer_control & 0x0007) == 1)
        ret |= MIX_AUXA_L | MIX_AUXA_R | MIX_AUXA_S;
    }
    else
    {
      // non-DPL2 mixing
      ret |= MIX_MAIN_L | MIX_MAIN_R;
      if (mixer_control & 0x0001)
        ret |= MIX_AUXA_L | MIX_AUXA_R;
      if (mixer_control & 0x0002)
        ret |= MIX_AUXB_L | MIX_AUXB_R;
      if (mixer_control & 0x0004)
      {
        ret |= MIX_MAIN_S;
        if (ret & MIX_AUXA_L)
          ret |= MIX_AUXA_S;
        if (ret & MIX_AUXB_L)
          ret |= MIX_AUXB_S;
      }
    }
    if (mixer_control & 0x0008)
      ret |= MIX_ALL_RAMPS;
  }
  else
  {
    // newer GameCube ucodes
    if (mixer_control & 0x0001)
      ret |= MIX_MAIN_L;
    if (mixer_control & 0x0002)
      ret |= MIX_MAIN_R;
    if (mixer_control & 0x0004)
      ret |= MIX_MAIN_S;
    if (mixer_control & 0x0008)
      ret |= MIX_MAIN_L_RAMP | MIX_MAIN_R_RAMP | MIX_MAIN_S_RAMP;

    if (mixer_control & 0x0010)
      ret |= MIX_AUXA_L;
    if (mixer_control & 0x0020)
      ret |= MIX_AUXA_R;
    if (mixer_control & 0x0040)
      ret |= MIX_AUXA_L_RAMP | MIX_AUXA_R_RAMP;
    if (mixer_control & 0x0080)
      ret |= MIX_AUXA_S;
    if (mixer_control & 0x0100)
      ret |= MIX_AUXA_S_RAMP;

    if (mixer_control & 0x0200)
      ret |= MIX_AUXB_L;
    if (mixer_control & 0x0400)
      ret |= MIX_AUXB_R;
    if (mixer_control & 0x0800)
      ret |= MIX_AUXB_L_RAMP | MIX_AUXB_R_RAMP;
    if (mixer_control & 0x1000)
      ret |= MIX_AUXB_S;
    if (mixer_control & 0x2000)
      ret |= MIX_AUXB_S_RAMP;

    // TODO: 0x4000 is used for Dolby Pro 2 sound mixing
    // It selects the input surround channel for all AUXB mixing channels.
    // This will only matter once we have ITD support.
  }

  return (AXMixControl)ret;
}

void AXUCode::SetupProcessing(u32 init_addr)
{
  const std::array<BufferDesc, 9> buffers = {{
      {m_samples_main_left, 32},
      {m_samples_main_right, 32},
      {m_samples_main_surround, 32},
      {m_samples_auxA_left, 32},
      {m_samples_auxA_right, 32},
      {m_samples_auxA_surround, 32},
      {m_samples_auxB_left, 32},
      {m_samples_auxB_right, 32},
      {m_samples_auxB_surround, 32},
  }};
  InitMixingBuffers<5 /*ms*/>(init_addr, buffers);
}

void AXUCode::DownloadAndMixWithVolume(u32 addr, u16 vol_main, u16 vol_auxa, u16 vol_auxb)
{
  int* buffers_main[3] = {m_samples_main_left, m_samples_main_right, m_samples_main_surround};
  int* buffers_auxa[3] = {m_samples_auxA_left, m_samples_auxA_right, m_samples_auxA_surround};
  int* buffers_auxb[3] = {m_samples_auxB_left, m_samples_auxB_right, m_samples_auxB_surround};
  int** buffers[3] = {buffers_main, buffers_auxa, buffers_auxb};
  u16 volumes[3] = {vol_main, vol_auxa, vol_auxb};

  auto& memory = m_dsphle->GetSystem().GetMemory();
  for (u32 i = 0; i < 3; ++i)
  {
    int* ptr = (int*)HLEMemory_Get_Pointer(memory, addr);
    u16 volume = volumes[i];
    for (u32 j = 0; j < 3; ++j)
    {
      int* buffer = buffers[i][j];
      for (u32 k = 0; k < 5 * 32; ++k)
      {
        s64 sample = (s64)(s32)Common::swap32(*ptr++);
        sample *= volume;
        buffer[k] += (s32)(sample >> 15);
      }
    }
  }
}

void AXUCode::ProcessPBList(u32 pb_addr)
{
  // Samples per millisecond. In theory DSP sampling rate can be changed from
  // 32KHz to 48KHz, but AX always process at 32KHz.
  constexpr u32 spms = 32;

  AXPB pb;

  auto& memory = m_dsphle->GetSystem().GetMemory();
  while (pb_addr)
  {
    AXBuffers buffers = {{m_samples_main_left, m_samples_main_right, m_samples_main_surround,
                          m_samples_auxA_left, m_samples_auxA_right, m_samples_auxA_surround,
                          m_samples_auxB_left, m_samples_auxB_right, m_samples_auxB_surround}};

    ReadPB(memory, pb_addr, pb, m_crc);

    u32 updates_addr = HILO_TO_32(pb.updates.data);
    u16* updates = (u16*)HLEMemory_Get_Pointer(memory, updates_addr);

    for (int curr_ms = 0; curr_ms < 5; ++curr_ms)
    {
      ApplyUpdatesForMs(curr_ms, pb, pb.updates.num_updates, updates);

      ProcessVoice(static_cast<HLEAccelerator*>(m_accelerator.get()), pb, buffers, spms,
                   ConvertMixerControl(pb.mixer_control),
                   m_coeffs_checksum ? m_coeffs.data() : nullptr);

      // Forward the buffers
      for (auto& ptr : buffers.ptrs)
        ptr += spms;
    }

    WritePB(memory, pb_addr, pb, m_crc);
    pb_addr = HILO_TO_32(pb.next_pb);
  }
}

void AXUCode::MixAUXSamples(int aux_id, u32 write_addr, u32 read_addr)
{
  int* buffers[3] = {nullptr};

  switch (aux_id)
  {
  case 0:
    buffers[0] = m_samples_auxA_left;
    buffers[1] = m_samples_auxA_right;
    buffers[2] = m_samples_auxA_surround;
    break;

  case 1:
    buffers[0] = m_samples_auxB_left;
    buffers[1] = m_samples_auxB_right;
    buffers[2] = m_samples_auxB_surround;
    break;
  }

  // First, we need to send the contents of our AUX buffers to the CPU.
  auto& memory = m_dsphle->GetSystem().GetMemory();
  if (write_addr)
  {
    int* ptr = (int*)HLEMemory_Get_Pointer(memory, write_addr);
    for (auto& buffer : buffers)
      for (u32 j = 0; j < 5 * 32; ++j)
        *ptr++ = Common::swap32(buffer[j]);
  }

  // Then, we read the new temp from the CPU and add to our current
  // temp.
  int* ptr = (int*)HLEMemory_Get_Pointer(memory, read_addr);
  for (auto& sample : m_samples_main_left)
    sample += (int)Common::swap32(*ptr++);
  for (auto& sample : m_samples_main_right)
    sample += (int)Common::swap32(*ptr++);
  for (auto& sample : m_samples_main_surround)
    sample += (int)Common::swap32(*ptr++);
}

void AXUCode::UploadLRS(u32 dst_addr)
{
  int buffers[3][5 * 32];

  for (u32 i = 0; i < 5 * 32; ++i)
  {
    buffers[0][i] = Common::swap32(m_samples_main_left[i]);
    buffers[1][i] = Common::swap32(m_samples_main_right[i]);
    buffers[2][i] = Common::swap32(m_samples_main_surround[i]);
  }
  memcpy(HLEMemory_Get_Pointer(m_dsphle->GetSystem().GetMemory(), dst_addr), buffers,
         sizeof(buffers));
}

void AXUCode::SetMainLR(u32 src_addr)
{
  int* ptr = (int*)HLEMemory_Get_Pointer(m_dsphle->GetSystem().GetMemory(), src_addr);
  for (u32 i = 0; i < 5 * 32; ++i)
  {
    int samp = (int)Common::swap32(*ptr++);
    m_samples_main_left[i] = samp;
    m_samples_main_right[i] = samp;
    m_samples_main_surround[i] = 0;
  }
}

void AXUCode::RunCompressor(u16 threshold, u16 release_frames, u32 table_addr, u32 millis)
{
  // check for L/R samples exceeding the threshold
  bool triggered = false;
  for (u32 i = 0; i < 32 * millis; ++i)
  {
    if (std::abs(m_samples_main_left[i]) > int(threshold) ||
        std::abs(m_samples_main_right[i]) > int(threshold))
    {
      triggered = true;
      break;
    }
  }

  const u32 frame_byte_size = 32 * millis * sizeof(s16);
  u32 table_offset = 0;
  if (triggered)
  {
    // one attack frame based on previous frame
    table_offset = m_compressor_pos * frame_byte_size;
    // next frame will start release
    m_compressor_pos = release_frames;
  }
  else if (m_compressor_pos)
  {
    // release
    --m_compressor_pos;
    // the release ramps are located after the attack ramps
    constexpr u32 ATTACK_ENTRY_COUNT = 11;
    table_offset = (ATTACK_ENTRY_COUNT + m_compressor_pos) * frame_byte_size;
  }
  else
  {
    return;
  }

  // apply the selected ramp
  auto& memory = m_dsphle->GetSystem().GetMemory();
  u16* ramp = (u16*)HLEMemory_Get_Pointer(memory, table_addr + table_offset);
  for (u32 i = 0; i < 32 * millis; ++i)
  {
    u16 coef = Common::swap16(*ramp++);
    m_samples_main_left[i] = (s64(m_samples_main_left[i]) * coef) >> 15;
    m_samples_main_right[i] = (s64(m_samples_main_right[i]) * coef) >> 15;
  }
}

void AXUCode::OutputSamples(u32 lr_addr, u32 surround_addr)
{
  int surround_buffer[5 * 32];

  for (u32 i = 0; i < 5 * 32; ++i)
    surround_buffer[i] = Common::swap32(m_samples_main_surround[i]);
  auto& memory = m_dsphle->GetSystem().GetMemory();
  memcpy(HLEMemory_Get_Pointer(memory, surround_addr), surround_buffer, sizeof(surround_buffer));

  // 32 samples per ms, 5 ms, 2 channels
  short buffer[5 * 32 * 2];

  // Output samples clamped to 16 bits and interlaced RLRLRLRLRL...
  for (u32 i = 0; i < 5 * 32; ++i)
  {
    int left = std::clamp(m_samples_main_left[i], -32767, 32767);
    int right = std::clamp(m_samples_main_right[i], -32767, 32767);

    buffer[2 * i + 0] = Common::swap16(right);
    buffer[2 * i + 1] = Common::swap16(left);
  }

  memcpy(HLEMemory_Get_Pointer(memory, lr_addr), buffer, sizeof(buffer));
}

void AXUCode::MixAUXBLR(u32 ul_addr, u32 dl_addr)
{
  // Upload AUXB L/R
  auto& memory = m_dsphle->GetSystem().GetMemory();
  int* ptr = (int*)HLEMemory_Get_Pointer(memory, ul_addr);
  for (auto& sample : m_samples_auxB_left)
    *ptr++ = Common::swap32(sample);
  for (auto& sample : m_samples_auxB_right)
    *ptr++ = Common::swap32(sample);

  // Mix AUXB L/R to MAIN L/R, and replace AUXB L/R
  ptr = (int*)HLEMemory_Get_Pointer(memory, dl_addr);
  for (u32 i = 0; i < 5 * 32; ++i)
  {
    int samp = Common::swap32(*ptr++);
    m_samples_auxB_left[i] = samp;
    m_samples_main_left[i] += samp;
  }
  for (u32 i = 0; i < 5 * 32; ++i)
  {
    int samp = Common::swap32(*ptr++);
    m_samples_auxB_right[i] = samp;
    m_samples_main_right[i] += samp;
  }
}

void AXUCode::SetOppositeLR(u32 src_addr)
{
  auto& memory = m_dsphle->GetSystem().GetMemory();
  int* ptr = (int*)HLEMemory_Get_Pointer(memory, src_addr);
  for (u32 i = 0; i < 5 * 32; ++i)
  {
    int inp = Common::swap32(*ptr++);
    m_samples_main_left[i] = -inp;
    m_samples_main_right[i] = inp;
    m_samples_main_surround[i] = 0;
  }
}

void AXUCode::SendAUXAndMix(u32 auxa_lrs_up, u32 auxb_s_up, u32 main_l_dl, u32 main_r_dl,
                            u32 auxb_l_dl, u32 auxb_r_dl)
{
  // Buffers to upload first
  const std::array<const int*, 3> up_buffers{
      m_samples_auxA_left,
      m_samples_auxA_right,
      m_samples_auxA_surround,
  };

  // Upload AUXA LRS
  auto& memory = m_dsphle->GetSystem().GetMemory();
  int* ptr = (int*)HLEMemory_Get_Pointer(memory, auxa_lrs_up);
  for (const auto& up_buffer : up_buffers)
  {
    for (u32 j = 0; j < 32 * 5; ++j)
      *ptr++ = Common::swap32(up_buffer[j]);
  }

  // Upload AUXB S
  ptr = (int*)HLEMemory_Get_Pointer(memory, auxb_s_up);
  for (auto& sample : m_samples_auxB_surround)
    *ptr++ = Common::swap32(sample);

  // Download buffers and addresses
  const std::array<int*, 4> dl_buffers{
      m_samples_main_left,
      m_samples_main_right,
      m_samples_auxB_left,
      m_samples_auxB_right,
  };
  const std::array<u32, 4> dl_addrs{
      main_l_dl,
      main_r_dl,
      auxb_l_dl,
      auxb_r_dl,
  };

  // Download and mix
  for (size_t i = 0; i < dl_buffers.size(); ++i)
  {
    const int* dl_src = (int*)HLEMemory_Get_Pointer(memory, dl_addrs[i]);
    for (size_t j = 0; j < 32 * 5; ++j)
      dl_buffers[i][j] += (int)Common::swap32(*dl_src++);
  }
}

void AXUCode::HandleMail(u32 mail)
{
  if (m_upload_setup_in_progress)
  {
    PrepareBootUCode(mail);
    return;
  }

  switch (m_mail_state)
  {
  case MailState::WaitingForCmdListSize:
    if ((mail & MAIL_CMDLIST_MASK) == MAIL_CMDLIST)
    {
      // A command list address is going to be sent next.
      m_cmdlist_size = static_cast<u16>(mail & ~MAIL_CMDLIST_MASK);
      m_mail_state = MailState::WaitingForCmdListAddress;
    }
    else
    {
      ERROR_LOG_FMT(DSPHLE, "Unknown mail sent to AX::HandleMail; expected command list: {:08x}",
                    mail);
    }
    break;

  case MailState::WaitingForCmdListAddress:
    CopyCmdList(mail, m_cmdlist_size);
    HandleCommandList();
    m_cmdlist_size = 0;
    SignalWorkEnd();
    m_mail_state = MailState::WaitingForNextTask;
    break;

  case MailState::WaitingForNextTask:
    if ((mail & TASK_MAIL_MASK) != TASK_MAIL_TO_DSP)
    {
      WARN_LOG_FMT(DSPHLE, "Rendering task without prefix CDD1: {:08x}", mail);
      mail = TASK_MAIL_TO_DSP | (mail & ~TASK_MAIL_MASK);
      // The actual uCode does not check for the CDD1 prefix.
    }

    switch (mail)
    {
    case MAIL_RESUME:
      // Acknowledge the resume request
      m_mail_handler.PushMail(DSP_RESUME, true);
      m_mail_state = MailState::WaitingForCmdListSize;
      break;

    case MAIL_NEW_UCODE:
      m_upload_setup_in_progress = true;
      // Relevant when this uCode is resumed after switching.
      // The resume mail is sent via the NeedsResumeMail() check above
      // (and the flag corresponding to it is set by PrepareBootUCode).
      m_mail_state = MailState::WaitingForCmdListSize;
      break;

    case MAIL_RESET:
      m_dsphle->SetUCode(UCODE_ROM);
      break;

    case MAIL_CONTINUE:
      // We don't have to do anything here - the CPU does not wait for a ACK
      // and sends a cmdlist mail just after.
      m_mail_state = MailState::WaitingForCmdListSize;
      break;

    default:
      WARN_LOG_FMT(DSPHLE, "Unknown task mail: {:08x}", mail);
      break;
    }
    break;
  }
}

void AXUCode::CopyCmdList(u32 addr, u16 size)
{
  if (size >= std::size(m_cmdlist))
  {
    ERROR_LOG_FMT(DSPHLE, "Command list at {:08x} is too large: size={}", addr, size);
    return;
  }

  auto& memory = m_dsphle->GetSystem().GetMemory();
  for (u32 i = 0; i < size; ++i, addr += 2)
    m_cmdlist[i] = HLEMemory_Read_U16(memory, addr);
}

void AXUCode::Update()
{
  // Used for UCode switching.
  if (NeedsResumeMail())
  {
    m_mail_handler.PushMail(DSP_RESUME, true);
  }
}

void AXUCode::DoAXState(PointerWrap& p)
{
  p.Do(m_cmdlist);
  p.Do(m_cmdlist_size);
  p.Do(m_mail_state);

  p.Do(m_samples_main_left);
  p.Do(m_samples_main_right);
  p.Do(m_samples_main_surround);
  p.Do(m_samples_auxA_left);
  p.Do(m_samples_auxA_right);
  p.Do(m_samples_auxA_surround);
  p.Do(m_samples_auxB_left);
  p.Do(m_samples_auxB_right);
  p.Do(m_samples_auxB_surround);

  auto old_checksum = m_coeffs_checksum;
  p.Do(m_coeffs_checksum);

  if (p.IsReadMode() && m_coeffs_checksum && old_checksum != m_coeffs_checksum)
  {
    if (!LoadResamplingCoefficients(true, *m_coeffs_checksum))
    {
      Core::DisplayMessage("Could not find the DSP polyphase resampling coefficients used by the "
                           "savestate. Aborting load state.",
                           3000);
      p.SetVerifyMode();
      return;
    }
  }

  p.Do(m_compressor_pos);

  m_accelerator->DoState(p);
}

void AXUCode::DoState(PointerWrap& p)
{
  DoStateShared(p);
  DoAXState(p);
}
}  // namespace DSP::HLE
