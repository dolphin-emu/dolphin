// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/HW/DSPHLE/UCodes/AX.h"

#include <algorithm>
#include <array>
#include <iterator>

#include "Common/ChunkFile.h"
#include "Common/CommonTypes.h"
#include "Common/CommonPaths.h"
#include "Common/File.h"
#include "Common/FileUtil.h"
#include "Common/Logging/Log.h"
#include "Common/Swap.h"
#include "Core/HW/DSP.h"
#include "Core/HW/DSPHLE/DSPHLE.h"
#include "Core/HW/DSPHLE/MailHandler.h"
#include "Core/HW/DSPHLE/UCodes/AXStructs.h"

#define AX_GC
#include "Core/HW/DSPHLE/UCodes/AXVoice.h"

namespace DSP::HLE
{
AXUCode::AXUCode(DSPHLE* dsphle, u32 crc) : UCodeInterface(dsphle, crc), m_cmdlist_size(0)
{
  INFO_LOG(DSPHLE, "Instantiating AXUCode: crc=%08x", crc);
}

AXUCode::~AXUCode()
{
  m_mail_handler.Clear();
}

void AXUCode::Initialize()
{
  m_mail_handler.PushMail(DSP_INIT, true);

  LoadResamplingCoefficients();
}

void AXUCode::LoadResamplingCoefficients()
{
  m_coeffs_available = false;

  const std::array<std::string, 2> filenames{
      File::GetUserPath(D_GCUSER_IDX) + DSP_COEF,
      File::GetSysDirectory() + GC_SYS_DIR DIR_SEP DSP_COEF,
  };

  size_t fidx;
  std::string filename;
  for (fidx = 0; fidx < filenames.size(); ++fidx)
  {
    filename = filenames[fidx];
    if (File::GetSize(filename) != 0x1000)
      continue;

    break;
  }

  if (fidx >= filenames.size())
    return;

  INFO_LOG(DSPHLE, "Loading polyphase resampling coeffs from %s", filename.c_str());

  File::IOFile fp(filename, "rb");
  fp.ReadBytes(m_coeffs, 0x1000);

  for (auto& coef : m_coeffs)
    coef = Common::swap16(coef);

  m_coeffs_available = true;
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
	INFO_LOG(DSPHLE, "Command list:");
	for (u32 i = 0; m_cmdlist[i] != CMD_END; ++i)
		INFO_LOG(DSPHLE, "%04x", m_cmdlist[i]);
	INFO_LOG(DSPHLE, "-------------");
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
      curr_idx += 10;
      break;  // TODO: check

    case CMD_MIX_AUXB_NOWRITE:
      addr_hi = m_cmdlist[curr_idx++];
      addr_lo = m_cmdlist[curr_idx++];
      MixAUXSamples(1, 0, HILO_TO_32(addr));
      break;

    case CMD_COMPRESSOR_TABLE_ADDR:
      curr_idx += 2;
      break;
    case CMD_UNK_0B:
      break;  // TODO: check other versions
    case CMD_UNK_0C:
      break;  // TODO: check other versions

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

    case CMD_UNK_12:
    {
      u16 samp_val = m_cmdlist[curr_idx++];
      u16 idx = m_cmdlist[curr_idx++];
      addr_hi = m_cmdlist[curr_idx++];
      addr_lo = m_cmdlist[curr_idx++];
      // TODO
      // suppress warnings:
      (void)samp_val;
      (void)idx;
      break;
    }

    // Send the contents of MAIN LRS, AUXA LRS and AUXB S to RAM, and
    // mix data to MAIN LR and AUXB LR.
    case CMD_SEND_AUX_AND_MIX:
    {
      // Address for Main + AUXA LRS upload
      u16 main_auxa_up_hi = m_cmdlist[curr_idx++];
      u16 main_auxa_up_lo = m_cmdlist[curr_idx++];

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

      SendAUXAndMix(HILO_TO_32(main_auxa_up), HILO_TO_32(auxb_s_up), HILO_TO_32(main_l_dl),
                    HILO_TO_32(main_r_dl), HILO_TO_32(auxb_l_dl), HILO_TO_32(auxb_r_dl));
      break;
    }

    default:
      ERROR_LOG(DSPHLE, "Unknown command in AX command list: %04x", cmd);
      end = true;
      break;
    }
  }
}

void AXUCode::ApplyUpdatesForMs(int curr_ms, u16* pb, u16* num_updates, u16* updates)
{
  u32 start_idx = 0;
  for (int i = 0; i < curr_ms; ++i)
    start_idx += num_updates[i];

  for (u32 i = start_idx; i < start_idx + num_updates[curr_ms]; ++i)
  {
    u16 update_off = Common::swap16(updates[2 * i]);
    u16 update_val = Common::swap16(updates[2 * i + 1]);

    pb[update_off] = update_val;
  }
}

AXMixControl AXUCode::ConvertMixerControl(u32 mixer_control)
{
  u32 ret = 0;

  // TODO: find other UCode versions with different mixer_control values
  if (m_crc == 0x4e8a8b21)
  {
    ret |= MIX_L | MIX_R;
    if (mixer_control & 0x0001)
      ret |= MIX_AUXA_L | MIX_AUXA_R;
    if (mixer_control & 0x0002)
      ret |= MIX_AUXB_L | MIX_AUXB_R;
    if (mixer_control & 0x0004)
    {
      ret |= MIX_S;
      if (ret & MIX_AUXA_L)
        ret |= MIX_AUXA_S;
      if (ret & MIX_AUXB_L)
        ret |= MIX_AUXB_S;
    }
    if (mixer_control & 0x0008)
    {
      ret |= MIX_L_RAMP | MIX_R_RAMP;
      if (ret & MIX_AUXA_L)
        ret |= MIX_AUXA_L_RAMP | MIX_AUXA_R_RAMP;
      if (ret & MIX_AUXB_L)
        ret |= MIX_AUXB_L_RAMP | MIX_AUXB_R_RAMP;
      if (ret & MIX_AUXA_S)
        ret |= MIX_AUXA_S_RAMP;
      if (ret & MIX_AUXB_S)
        ret |= MIX_AUXB_S_RAMP;
    }
  }
  else
  {
    if (mixer_control & 0x0001)
      ret |= MIX_L;
    if (mixer_control & 0x0002)
      ret |= MIX_R;
    if (mixer_control & 0x0004)
      ret |= MIX_S;
    if (mixer_control & 0x0008)
      ret |= MIX_L_RAMP | MIX_R_RAMP | MIX_S_RAMP;
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
  }

  return (AXMixControl)ret;
}

void AXUCode::SetupProcessing(u32 init_addr)
{
  u16 init_data[0x20];

  for (u32 i = 0; i < 0x20; ++i)
    init_data[i] = HLEMemory_Read_U16(init_addr + 2 * i);

  // List of all buffers we have to initialize
  int* buffers[] = {m_samples_left,      m_samples_right,      m_samples_surround,
                    m_samples_auxA_left, m_samples_auxA_right, m_samples_auxA_surround,
                    m_samples_auxB_left, m_samples_auxB_right, m_samples_auxB_surround};

  u32 init_idx = 0;
  for (auto& buffer : buffers)
  {
    s32 init_val = (s32)((init_data[init_idx] << 16) | init_data[init_idx + 1]);
    s16 delta = (s16)init_data[init_idx + 2];

    init_idx += 3;

    if (!init_val)
    {
      memset(buffer, 0, 5 * 32 * sizeof(int));
    }
    else
    {
      for (u32 j = 0; j < 32 * 5; ++j)
      {
        buffer[j] = init_val;
        init_val += delta;
      }
    }
  }
}

void AXUCode::DownloadAndMixWithVolume(u32 addr, u16 vol_main, u16 vol_auxa, u16 vol_auxb)
{
  int* buffers_main[3] = {m_samples_left, m_samples_right, m_samples_surround};
  int* buffers_auxa[3] = {m_samples_auxA_left, m_samples_auxA_right, m_samples_auxA_surround};
  int* buffers_auxb[3] = {m_samples_auxB_left, m_samples_auxB_right, m_samples_auxB_surround};
  int** buffers[3] = {buffers_main, buffers_auxa, buffers_auxb};
  u16 volumes[3] = {vol_main, vol_auxa, vol_auxb};

  for (u32 i = 0; i < 3; ++i)
  {
    int* ptr = (int*)HLEMemory_Get_Pointer(addr);
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
  const u32 spms = 32;

  AXPB pb;

  while (pb_addr)
  {
    AXBuffers buffers = {{m_samples_left, m_samples_right, m_samples_surround, m_samples_auxA_left,
                          m_samples_auxA_right, m_samples_auxA_surround, m_samples_auxB_left,
                          m_samples_auxB_right, m_samples_auxB_surround}};

    ReadPB(pb_addr, pb, m_crc);

    u32 updates_addr = HILO_TO_32(pb.updates.data);
    u16* updates = (u16*)HLEMemory_Get_Pointer(updates_addr);

    for (int curr_ms = 0; curr_ms < 5; ++curr_ms)
    {
      ApplyUpdatesForMs(curr_ms, (u16*)&pb, pb.updates.num_updates, updates);

      ProcessVoice(pb, buffers, spms, ConvertMixerControl(pb.mixer_control),
                   m_coeffs_available ? m_coeffs : nullptr);

      // Forward the buffers
      for (auto& ptr : buffers.ptrs)
        ptr += spms;
    }

    WritePB(pb_addr, pb, m_crc);
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
  if (write_addr)
  {
    int* ptr = (int*)HLEMemory_Get_Pointer(write_addr);
    for (auto& buffer : buffers)
      for (u32 j = 0; j < 5 * 32; ++j)
        *ptr++ = Common::swap32(buffer[j]);
  }

  // Then, we read the new temp from the CPU and add to our current
  // temp.
  int* ptr = (int*)HLEMemory_Get_Pointer(read_addr);
  for (auto& sample : m_samples_left)
    sample += (int)Common::swap32(*ptr++);
  for (auto& sample : m_samples_right)
    sample += (int)Common::swap32(*ptr++);
  for (auto& sample : m_samples_surround)
    sample += (int)Common::swap32(*ptr++);
}

void AXUCode::UploadLRS(u32 dst_addr)
{
  int buffers[3][5 * 32];

  for (u32 i = 0; i < 5 * 32; ++i)
  {
    buffers[0][i] = Common::swap32(m_samples_left[i]);
    buffers[1][i] = Common::swap32(m_samples_right[i]);
    buffers[2][i] = Common::swap32(m_samples_surround[i]);
  }
  memcpy(HLEMemory_Get_Pointer(dst_addr), buffers, sizeof(buffers));
}

void AXUCode::SetMainLR(u32 src_addr)
{
  int* ptr = (int*)HLEMemory_Get_Pointer(src_addr);
  for (u32 i = 0; i < 5 * 32; ++i)
  {
    int samp = (int)Common::swap32(*ptr++);
    m_samples_left[i] = samp;
    m_samples_right[i] = samp;
    m_samples_surround[i] = 0;
  }
}

void AXUCode::OutputSamples(u32 lr_addr, u32 surround_addr)
{
  int surround_buffer[5 * 32];

  for (u32 i = 0; i < 5 * 32; ++i)
    surround_buffer[i] = Common::swap32(m_samples_surround[i]);
  memcpy(HLEMemory_Get_Pointer(surround_addr), surround_buffer, sizeof(surround_buffer));

  // 32 samples per ms, 5 ms, 2 channels
  short buffer[5 * 32 * 2];

  // Output samples clamped to 16 bits and interlaced RLRLRLRLRL...
  for (u32 i = 0; i < 5 * 32; ++i)
  {
    int left = std::clamp(m_samples_left[i], -32767, 32767);
    int right = std::clamp(m_samples_right[i], -32767, 32767);

    buffer[2 * i + 0] = Common::swap16(right);
    buffer[2 * i + 1] = Common::swap16(left);
  }

  memcpy(HLEMemory_Get_Pointer(lr_addr), buffer, sizeof(buffer));
}

void AXUCode::MixAUXBLR(u32 ul_addr, u32 dl_addr)
{
  // Upload AUXB L/R
  int* ptr = (int*)HLEMemory_Get_Pointer(ul_addr);
  for (auto& sample : m_samples_auxB_left)
    *ptr++ = Common::swap32(sample);
  for (auto& sample : m_samples_auxB_right)
    *ptr++ = Common::swap32(sample);

  // Mix AUXB L/R to MAIN L/R, and replace AUXB L/R
  ptr = (int*)HLEMemory_Get_Pointer(dl_addr);
  for (u32 i = 0; i < 5 * 32; ++i)
  {
    int samp = Common::swap32(*ptr++);
    m_samples_auxB_left[i] = samp;
    m_samples_left[i] += samp;
  }
  for (u32 i = 0; i < 5 * 32; ++i)
  {
    int samp = Common::swap32(*ptr++);
    m_samples_auxB_right[i] = samp;
    m_samples_right[i] += samp;
  }
}

void AXUCode::SetOppositeLR(u32 src_addr)
{
  int* ptr = (int*)HLEMemory_Get_Pointer(src_addr);
  for (u32 i = 0; i < 5 * 32; ++i)
  {
    int inp = Common::swap32(*ptr++);
    m_samples_left[i] = -inp;
    m_samples_right[i] = inp;
    m_samples_surround[i] = 0;
  }
}

void AXUCode::SendAUXAndMix(u32 main_auxa_up, u32 auxb_s_up, u32 main_l_dl, u32 main_r_dl,
                            u32 auxb_l_dl, u32 auxb_r_dl)
{
  // Buffers to upload first
  const std::array<const int*, 3> up_buffers{
      m_samples_auxA_left,
      m_samples_auxA_right,
      m_samples_auxA_surround,
  };

  // Upload AUXA LRS
  int* ptr = (int*)HLEMemory_Get_Pointer(main_auxa_up);
  for (const auto& up_buffer : up_buffers)
  {
    for (u32 j = 0; j < 32 * 5; ++j)
      *ptr++ = Common::swap32(up_buffer[j]);
  }

  // Upload AUXB S
  ptr = (int*)HLEMemory_Get_Pointer(auxb_s_up);
  for (auto& sample : m_samples_auxB_surround)
    *ptr++ = Common::swap32(sample);

  // Download buffers and addresses
  const std::array<int*, 4> dl_buffers{
      m_samples_left,
      m_samples_right,
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
    const int* dl_src = (int*)HLEMemory_Get_Pointer(dl_addrs[i]);
    for (size_t j = 0; j < 32 * 5; ++j)
      dl_buffers[i][j] += (int)Common::swap32(*dl_src++);
  }
}

void AXUCode::HandleMail(u32 mail)
{
  // Indicates if the next message is a command list address.
  static bool next_is_cmdlist = false;
  static u16 cmdlist_size = 0;

  bool set_next_is_cmdlist = false;

  if (next_is_cmdlist)
  {
    CopyCmdList(mail, cmdlist_size);
    HandleCommandList();
    m_cmdlist_size = 0;
    SignalWorkEnd();
  }
  else if (m_upload_setup_in_progress)
  {
    PrepareBootUCode(mail);
  }
  else if (mail == MAIL_RESUME)
  {
    // Acknowledge the resume request
    m_mail_handler.PushMail(DSP_RESUME, true);
  }
  else if (mail == MAIL_NEW_UCODE)
  {
    m_upload_setup_in_progress = true;
  }
  else if (mail == MAIL_RESET)
  {
    m_dsphle->SetUCode(UCODE_ROM);
  }
  else if (mail == MAIL_CONTINUE)
  {
    // We don't have to do anything here - the CPU does not wait for a ACK
    // and sends a cmdlist mail just after.
  }
  else if ((mail & MAIL_CMDLIST_MASK) == MAIL_CMDLIST)
  {
    // A command list address is going to be sent next.
    set_next_is_cmdlist = true;
    cmdlist_size = (u16)(mail & ~MAIL_CMDLIST_MASK);
  }
  else
  {
    ERROR_LOG(DSPHLE, "Unknown mail sent to AX::HandleMail: %08x", mail);
  }

  next_is_cmdlist = set_next_is_cmdlist;
}

void AXUCode::CopyCmdList(u32 addr, u16 size)
{
  if (size >= std::size(m_cmdlist))
  {
    ERROR_LOG(DSPHLE, "Command list at %08x is too large: size=%d", addr, size);
    return;
  }

  for (u32 i = 0; i < size; ++i, addr += 2)
    m_cmdlist[i] = HLEMemory_Read_U16(addr);
  m_cmdlist_size = size;
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

  p.Do(m_samples_left);
  p.Do(m_samples_right);
  p.Do(m_samples_surround);
  p.Do(m_samples_auxA_left);
  p.Do(m_samples_auxA_right);
  p.Do(m_samples_auxA_surround);
  p.Do(m_samples_auxB_left);
  p.Do(m_samples_auxB_right);
  p.Do(m_samples_auxB_surround);
}

void AXUCode::DoState(PointerWrap& p)
{
  DoStateShared(p);
  DoAXState(p);
}
}  // namespace DSP::HLE
