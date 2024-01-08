// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

// High-level emulation for the AX GameCube UCode.
//
// TODO:
//  * Depop support
//  * ITD support
//  * Polyphase sample interpolation support (not very useful)
//  * Dolby Pro 2 mixing with recent AX versions

#pragma once

#include <array>
#include <memory>
#include <optional>

#include "Common/BitUtils.h"
#include "Common/CommonTypes.h"
#include "Common/Swap.h"
#include "Core/HW/DSPHLE/DSPHLE.h"
#include "Core/HW/DSPHLE/UCodes/UCodes.h"
#include "Core/HW/Memmap.h"
#include "Core/System.h"

namespace DSP
{
class Accelerator;
}

namespace DSP::HLE
{
class DSPHLE;

// We can't directly use the mixer_control field from the PB because it does
// not mean the same in all AX versions. The AX UCode converts the
// mixer_control value to an AXMixControl bitfield.
enum AXMixControl
{
  // clang-format off
  MIX_MAIN_L      = 0x000001,
  MIX_MAIN_L_RAMP = 0x000002,
  MIX_MAIN_R      = 0x000004,
  MIX_MAIN_R_RAMP = 0x000008,
  MIX_MAIN_S      = 0x000010,
  MIX_MAIN_S_RAMP = 0x000020,

  MIX_AUXA_L      = 0x000040,
  MIX_AUXA_L_RAMP = 0x000080,
  MIX_AUXA_R      = 0x000100,
  MIX_AUXA_R_RAMP = 0x000200,
  MIX_AUXA_S      = 0x000400,
  MIX_AUXA_S_RAMP = 0x000800,

  MIX_AUXB_L      = 0x001000,
  MIX_AUXB_L_RAMP = 0x002000,
  MIX_AUXB_R      = 0x004000,
  MIX_AUXB_R_RAMP = 0x008000,
  MIX_AUXB_S      = 0x010000,
  MIX_AUXB_S_RAMP = 0x020000,

  MIX_AUXC_L      = 0x040000,
  MIX_AUXC_L_RAMP = 0x080000,
  MIX_AUXC_R      = 0x100000,
  MIX_AUXC_R_RAMP = 0x200000,
  MIX_AUXC_S      = 0x400000,
  MIX_AUXC_S_RAMP = 0x800000,

  MIX_ALL_RAMPS   = 0xAAAAAA,
  // clang-format on
};

class AXUCode /* not final: subclassed by AXWiiUCode */ : public UCodeInterface
{
public:
  AXUCode(DSPHLE* dsphle, u32 crc);
  ~AXUCode() override;

  void Initialize() override;
  void HandleMail(u32 mail) override;
  void Update() override;
  void DoState(PointerWrap& p) override;

protected:
  // CPU sends 0xBABE0000 | cmdlist_size to the DSP
  static constexpr u32 MAIL_CMDLIST = 0xBABE0000;
  static constexpr u32 MAIL_CMDLIST_MASK = 0xFFFF0000;

  // 32 * 5 because 32 samples per millisecond, for max 5 milliseconds.
  int m_samples_main_left[32 * 5]{};
  int m_samples_main_right[32 * 5]{};
  int m_samples_main_surround[32 * 5]{};
  int m_samples_auxA_left[32 * 5]{};
  int m_samples_auxA_right[32 * 5]{};
  int m_samples_auxA_surround[32 * 5]{};
  int m_samples_auxB_left[32 * 5]{};
  int m_samples_auxB_right[32 * 5]{};
  int m_samples_auxB_surround[32 * 5]{};

  u16 m_cmdlist[512]{};
  u32 m_cmdlist_size = 0;

  // Table of coefficients for polyphase sample rate conversion.
  // The coefficients aren't always available (they are part of the DSP DROM)
  // so we also need to know if they are valid or not.
  std::optional<u32> m_coeffs_checksum = std::nullopt;
  std::array<s16, 0x800> m_coeffs{};

  u16 m_compressor_pos = 0;

  std::unique_ptr<Accelerator> m_accelerator;

  void InitializeShared();

  bool LoadResamplingCoefficients(bool require_same_checksum, u32 desired_checksum);

  // Copy a command list from memory to our temp buffer
  void CopyCmdList(u32 addr, u16 size);

  // Convert a mixer_control bitfield to our internal representation for that
  // value. Required because that bitfield has a different meaning in some
  // versions of AX.
  AXMixControl ConvertMixerControl(u32 mixer_control);

  // Apply updates to a PB. Generic, used in AX GC and AX Wii.
  template <typename PBType>
  void ApplyUpdatesForMs(int curr_ms, PBType& pb, u16* num_updates, u16* updates)
  {
    auto pb_mem = Common::BitCastToArray<u16>(pb);

    u32 start_idx = 0;
    for (int i = 0; i < curr_ms; ++i)
      start_idx += num_updates[i];

    for (u32 i = start_idx; i < start_idx + num_updates[curr_ms]; ++i)
    {
      u16 update_off = Common::swap16(updates[2 * i]);
      u16 update_val = Common::swap16(updates[2 * i + 1]);

      pb_mem[update_off] = update_val;
    }

    Common::BitCastFromArray<u16>(pb_mem, pb);
  }

  virtual void HandleCommandList();
  void SignalWorkEnd();

  struct BufferDesc
  {
    int* ptr;
    int samples_per_milli;
  };

  template <int Millis, size_t BufCount>
  void InitMixingBuffers(u32 init_addr, const std::array<BufferDesc, BufCount>& buffers)
  {
    auto& system = m_dsphle->GetSystem();
    auto& memory = system.GetMemory();
    std::array<u16, 3 * BufCount> init_array;
    memory.CopyFromEmuSwapped(init_array.data(), init_addr, sizeof(init_array));
    for (size_t i = 0; i < BufCount; ++i)
    {
      const BufferDesc& buf = buffers[i];
      s32 value = s32((u32(init_array[3 * i]) << 16) | init_array[3 * i + 1]);
      s16 delta = init_array[3 * i + 2];
      if (value == 0)
      {
        memset(buf.ptr, 0, Millis * buf.samples_per_milli * sizeof(int));
      }
      else
      {
        for (int j = 0; j < Millis * buf.samples_per_milli; ++j)
          buf.ptr[j] = value + j * delta;
      }
    }
  }
  void SetupProcessing(u32 init_addr);
  void DownloadAndMixWithVolume(u32 addr, u16 vol_main, u16 vol_auxa, u16 vol_auxb);
  void ProcessPBList(u32 pb_addr);
  void MixAUXSamples(int aux_id, u32 write_addr, u32 read_addr);
  void UploadLRS(u32 dst_addr);
  void SetMainLR(u32 src_addr);
  void RunCompressor(u16 threshold, u16 release_stages, u32 table_addr, u32 millis);
  void OutputSamples(u32 out_addr, u32 surround_addr);
  void MixAUXBLR(u32 ul_addr, u32 dl_addr);
  void SetOppositeLR(u32 src_addr);
  void SendAUXAndMix(u32 main_auxa_up, u32 auxb_s_up, u32 main_l_dl, u32 main_r_dl, u32 auxb_l_dl,
                     u32 auxb_r_dl);

  // Handle save states for main AX.
  void DoAXState(PointerWrap& p);

private:
  enum CmdType
  {
    CMD_SETUP = 0x00,
    CMD_DL_AND_VOL_MIX = 0x01,
    CMD_PB_ADDR = 0x02,
    CMD_PROCESS = 0x03,
    CMD_MIX_AUXA = 0x04,
    CMD_MIX_AUXB = 0x05,
    CMD_UPLOAD_LRS = 0x06,
    CMD_SET_LR = 0x07,
    CMD_UNK_08 = 0x08,
    CMD_MIX_AUXB_NOWRITE = 0x09,
    CMD_UNK_0A = 0x0A,
    CMD_UNK_0B = 0x0B,
    CMD_UNK_0C = 0x0C,
    CMD_MORE = 0x0D,
    CMD_OUTPUT = 0x0E,
    CMD_END = 0x0F,
    CMD_MIX_AUXB_LR = 0x10,
    CMD_SET_OPPOSITE_LR = 0x11,
    CMD_COMPRESSOR = 0x12,
    CMD_SEND_AUX_AND_MIX = 0x13,
  };

  enum class MailState
  {
    WaitingForCmdListSize,
    WaitingForCmdListAddress,
    WaitingForNextTask,
  };

  MailState m_mail_state = MailState::WaitingForCmdListSize;
};
}  // namespace DSP::HLE
