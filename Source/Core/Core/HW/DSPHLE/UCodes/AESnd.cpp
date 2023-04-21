// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

// High-level emulation for the libaesnd ucode, used by homebrew
// libaesnd is part of devkitPro's libogc, released under the Zlib license

#include "Core/HW/DSPHLE/UCodes/AESnd.h"

#include <algorithm>

#include "Common/ChunkFile.h"
#include "Common/Logging/Log.h"
#include "Common/Swap.h"
#include "Core/DSP/DSPAccelerator.h"
#include "Core/HW/DSP.h"
#include "Core/HW/DSPHLE/DSPHLE.h"
#include "Core/HW/DSPHLE/MailHandler.h"
#include "Core/HW/DSPHLE/UCodes/UCodes.h"
#include "Core/System.h"

namespace DSP::HLE
{
constexpr u32 MAIL_PREFIX = 0xface'0000;
constexpr u32 MAIL_PROCESS_FIRST_VOICE = MAIL_PREFIX | 0x0010;
constexpr u32 MAIL_PROCESS_NEXT_VOICE = MAIL_PREFIX | 0x0020;
constexpr u32 MAIL_GET_PB_ADDRESS = MAIL_PREFIX | 0x0080;
constexpr u32 MAIL_SEND_SAMPLES = MAIL_PREFIX | 0x0100;
constexpr u32 MAIL_TERMINATE = MAIL_PREFIX | 0xdead;

constexpr u32 VOICE_MONO8 = 0x00000000;
constexpr u32 VOICE_STEREO8 = 0x00000001;
constexpr u32 VOICE_MONO16 = 0x00000002;
constexpr u32 VOICE_STEREO16 = 0x00000003;
// These are only present in the 2020 release
constexpr u32 VOICE_MONO8_UNSIGNED = 0x00000004;
constexpr u32 VOICE_STEREO8_UNSIGNED = 0x00000005;
constexpr u32 VOICE_MONO16_UNSIGNED = 0x00000006;
constexpr u32 VOICE_STEREO16_UNSIGNED = 0x00000007;

constexpr u32 VOICE_FORMAT_MASK_OLD = 3;
constexpr u32 VOICE_FORMAT_MASK_NEW = 7;
// Note that the above formats have the 2-bit set for 16-bit formats and not set for 8-bit formats
constexpr u32 VOICE_16_BIT_FLAG = 2;

// These are used in the pre-2020 versions version
constexpr u32 VOICE_PAUSE_OLD = 0x00000004;
constexpr u32 VOICE_LOOP_OLD [[maybe_unused]] = 0x00000008;    // not used by the DSP
constexpr u32 VOICE_ONCE_OLD [[maybe_unused]] = 0x00000010;    // not used by the DSP
constexpr u32 VOICE_STREAM_OLD [[maybe_unused]] = 0x00000020;  // not used by the DSP

// These were changed in the 2020 version to account for the different flags
constexpr u32 VOICE_PAUSE_NEW = 0x00000008;
constexpr u32 VOICE_LOOP_NEW [[maybe_unused]] = 0x00000010;    // not used by the DSP
constexpr u32 VOICE_ONCE_NEW [[maybe_unused]] = 0x00000020;    // not used by the DSP
constexpr u32 VOICE_STREAM_NEW [[maybe_unused]] = 0x00000040;  // not used by the DSP

// These did not change between versions
constexpr u32 VOICE_FINISHED = 0x00100000;
constexpr u32 VOICE_STOPPED [[maybe_unused]] = 0x00200000;  // not used by the DSP
constexpr u32 VOICE_RUNNING = 0x40000000;
constexpr u32 VOICE_USED [[maybe_unused]] = 0x80000000;  // not used by the DSP

// 1<<4 = scale gain by 1/1,    2<<2 = PCM decoding from ARAM, 1<<0 = 8-bit reads
constexpr u32 ACCELERATOR_FORMAT_8_BIT = 0x0019;
// 0<<4 = scale gain by 1/2048, 2<<2 = PCM decoding from ARAM, 2<<0 = 16-bit reads
constexpr u32 ACCELERATOR_FORMAT_16_BIT = 0x000a;
// Multiply samples by 0x100/1 = 0x100 (for ACCELERATOR_FORMAT_8_BIT)
constexpr u32 ACCELERATOR_GAIN_8_BIT = 0x0100;
// Multiply samples by 0x800/2048 = 1 (for ACCELERATOR_FORMAT_16_BIT)
constexpr u32 ACCELERATOR_GAIN_16_BIT = 0x0800;

bool AESndUCode::SwapLeftRight() const
{
  return m_crc == HASH_2012 || m_crc == HASH_EDUKE32 || m_crc == HASH_2020 ||
         m_crc == HASH_2020_PAD || m_crc == HASH_2022_PAD;
}

bool AESndUCode::UseNewFlagMasks() const
{
  return m_crc == HASH_EDUKE32 || m_crc == HASH_2020 || m_crc == HASH_2020_PAD ||
         m_crc == HASH_2022_PAD;
}

AESndUCode::AESndUCode(DSPHLE* dsphle, u32 crc) : UCodeInterface(dsphle, crc)
{
}

void AESndUCode::Initialize()
{
  m_mail_handler.PushMail(DSP_INIT);
}

void AESndUCode::Update()
{
  // This is dubious in general, since we set the interrupt parameter on m_mail_handler.PushMail
  if (m_mail_handler.HasPending())
  {
    Core::System::GetInstance().GetDSP().GenerateDSPInterruptFromDSPEmu(DSP::INT_DSP);
  }
}

void AESndUCode::HandleMail(u32 mail)
{
  if (m_upload_setup_in_progress)
  {
    PrepareBootUCode(mail);
  }
  else if (m_next_mail_is_parameter_block_addr)
  {
    // get_pb_address
    m_parameter_block_addr = mail;
    INFO_LOG_FMT(DSPHLE, "AESndUCode - Parameter block is at {:08x}", mail);
    m_next_mail_is_parameter_block_addr = false;
    // No mail is sent in response
  }
  else if ((mail & TASK_MAIL_MASK) == TASK_MAIL_TO_DSP)
  {
    switch (mail)
    {
    case MAIL_NEW_UCODE:
      m_upload_setup_in_progress = true;
      break;
    case MAIL_RESET:
      m_dsphle->SetUCode(UCODE_ROM);
      break;
    default:
      WARN_LOG_FMT(DSPHLE, "AESndUCode - unknown 0xcdd1 command: {:08x}", mail);
      break;
    }
    // No mail is sent in response to any of these.
    // The actual uCode halts on unknown cdd1 commands, which I have not implemented.
  }
  else
  {
    // The uCode checks for a 0xface prefix, which we include in the constants above.
    switch (mail)
    {
    case MAIL_PROCESS_FIRST_VOICE:
      DEBUG_LOG_FMT(DSPHLE, "AESndUCode - MAIL_PROCESS_FIRST_VOICE");
      DMAInParameterBlock();  // dma_pb_block
      m_output_buffer.fill(0);
      DoMixing();  // fall through to dsp_mixer
      // Mail is handled by DoMixing()
      break;
    case MAIL_PROCESS_NEXT_VOICE:
      DEBUG_LOG_FMT(DSPHLE, "AESndUCode - MAIL_PROCESS_NEXT_VOICE");
      DMAInParameterBlock();  // dma_pb_block
      DoMixing();             // jump to dsp_mixer
      // Mail is handled by DoMixing()
      break;
    case MAIL_GET_PB_ADDRESS:
      DEBUG_LOG_FMT(DSPHLE, "AESndUCode - MAIL_GET_PB_ADDRESS");
      m_next_mail_is_parameter_block_addr = true;
      // No mail is sent in response
      break;
    case MAIL_SEND_SAMPLES:
      DEBUG_LOG_FMT(DSPHLE, "AESndUCode - MAIL_SEND_SAMPLES");
      // send_samples
      for (u32 i = 0; i < NUM_OUTPUT_SAMPLES * 2; i++)
      {
        HLEMemory_Write_U16(m_parameter_block.out_buf + i * sizeof(u16), m_output_buffer[i]);
      }
      m_mail_handler.PushMail(DSP_SYNC, true);
      break;
    case MAIL_TERMINATE:
      INFO_LOG_FMT(DSPHLE, "AESndUCode - MAIL_TERMINATE: {:08x}", mail);
      if (m_crc != HASH_2022_PAD)
      {
        // The relevant code looks like this:
        //
        // lrs $acc1.m,@CMBL
        // ...
        // cmpi $acc1.m,#0xdead
        // jeq task_terminate
        //
        // The cmpi instruction always sign-extends, so it will compare $acc1 with 0xff'dead'0000.
        // However, recv_cmd runs in set16 mode, so the load to $acc1 will produce 0x00'dead'0000.
        // This means that the comparison never succeeds, and no mail is sent in response to
        // MAIL_TERMINATE. This means that __dsp_donecallback is never called (since that's
        // normally called in response to DSP_DONE), so __aesnddspinit is never cleared, so
        // AESND_Reset never returns, resulting in a hang. We always send the mail to avoid this
        // hang. (It's possible to exit without calling AESND_Reset, so most homebrew probably
        // isn't affected by this bug in the first place.)
        //
        // A fix exists, but has not yet been added to mainline libogc:
        // https://github.com/extremscorner/libogc2/commit/38edc9db93232faa612f680c91be1eb4d95dd1c6
        WARN_LOG_FMT(DSPHLE, "AESndUCode - MAIL_TERMINATE is broken in this version of the "
                             "uCode; this will hang on real hardware or with DSP LLE");
      }
      // This doesn't actually change the state of the DSP code.
      m_mail_handler.PushMail(DSP_DONE, true);
      break;
    default:
      WARN_LOG_FMT(DSPHLE, "AESndUCode - unknown command: {:08x}", mail);
      // No mail is sent in this case
      break;
    }
  }
}

void AESndUCode::DMAInParameterBlock()
{
  m_parameter_block.out_buf = HLEMemory_Read_U32(m_parameter_block_addr + 0);
  m_parameter_block.buf_start = HLEMemory_Read_U32(m_parameter_block_addr + 4);
  m_parameter_block.buf_end = HLEMemory_Read_U32(m_parameter_block_addr + 8);
  m_parameter_block.buf_curr = HLEMemory_Read_U32(m_parameter_block_addr + 12);
  m_parameter_block.yn1 = HLEMemory_Read_U16(m_parameter_block_addr + 16);
  m_parameter_block.yn2 = HLEMemory_Read_U16(m_parameter_block_addr + 18);
  m_parameter_block.pds = HLEMemory_Read_U16(m_parameter_block_addr + 20);
  m_parameter_block.freq = HLEMemory_Read_U32(m_parameter_block_addr + 22);
  m_parameter_block.counter = HLEMemory_Read_U16(m_parameter_block_addr + 26);
  m_parameter_block.left = HLEMemory_Read_U16(m_parameter_block_addr + 28);
  m_parameter_block.right = HLEMemory_Read_U16(m_parameter_block_addr + 30);
  m_parameter_block.volume_l = HLEMemory_Read_U16(m_parameter_block_addr + 32);
  m_parameter_block.volume_r = HLEMemory_Read_U16(m_parameter_block_addr + 34);
  m_parameter_block.delay = HLEMemory_Read_U32(m_parameter_block_addr + 36);
  m_parameter_block.flags = HLEMemory_Read_U32(m_parameter_block_addr + 40);
}

void AESndUCode::DMAOutParameterBlock()
{
  HLEMemory_Write_U32(m_parameter_block_addr + 0, m_parameter_block.out_buf);
  HLEMemory_Write_U32(m_parameter_block_addr + 4, m_parameter_block.buf_start);
  HLEMemory_Write_U32(m_parameter_block_addr + 8, m_parameter_block.buf_end);
  HLEMemory_Write_U32(m_parameter_block_addr + 12, m_parameter_block.buf_curr);
  HLEMemory_Write_U16(m_parameter_block_addr + 16, m_parameter_block.yn1);
  HLEMemory_Write_U16(m_parameter_block_addr + 18, m_parameter_block.yn2);
  HLEMemory_Write_U16(m_parameter_block_addr + 20, m_parameter_block.pds);
  HLEMemory_Write_U32(m_parameter_block_addr + 22, m_parameter_block.freq);
  HLEMemory_Write_U16(m_parameter_block_addr + 26, m_parameter_block.counter);
  HLEMemory_Write_U16(m_parameter_block_addr + 28, m_parameter_block.left);
  HLEMemory_Write_U16(m_parameter_block_addr + 30, m_parameter_block.right);
  HLEMemory_Write_U16(m_parameter_block_addr + 32, m_parameter_block.volume_l);
  HLEMemory_Write_U16(m_parameter_block_addr + 34, m_parameter_block.volume_r);
  HLEMemory_Write_U32(m_parameter_block_addr + 36, m_parameter_block.delay);
  HLEMemory_Write_U32(m_parameter_block_addr + 40, m_parameter_block.flags);
}

class AESndAccelerator final : public Accelerator
{
protected:
  void OnEndException() override
  {
    // exception5 - this updates internal state
    SetYn1(GetYn1());
    SetYn2(GetYn2());
    SetPredScale(GetPredScale());
  }

  u8 ReadMemory(u32 address) override
  {
    return Core::System::GetInstance().GetDSP().ReadARAM(address);
  }
  void WriteMemory(u32 address, u8 value) override
  {
    Core::System::GetInstance().GetDSP().WriteARAM(value, address);
  }
};

static std::unique_ptr<Accelerator> s_accelerator = std::make_unique<AESndAccelerator>();
static constexpr std::array<s16, 16> ACCELERATOR_COEFS = {};  // all zeros

void AESndUCode::SetUpAccelerator(u16 format, [[maybe_unused]] u16 gain)
{
  // setup_accl
  s_accelerator->SetSampleFormat(format);
  // not currently implemented, but it doesn't matter since the gain is configured to be a no-op
  // s_accelerator->SetGain(gain);
  s_accelerator->SetStartAddress(m_parameter_block.buf_start);
  s_accelerator->SetEndAddress(m_parameter_block.buf_end);
  s_accelerator->SetCurrentAddress(m_parameter_block.buf_curr);
  s_accelerator->SetYn1(m_parameter_block.yn1);
  s_accelerator->SetYn2(m_parameter_block.yn2);
  s_accelerator->SetPredScale(m_parameter_block.pds);
  // All of the coefficients (COEF_A1_0 at ffa0 - COEF_A2_7 at ffaf) are set to 0
}

void AESndUCode::DoMixing()
{
  const u32 pause_flag = UseNewFlagMasks() ? VOICE_PAUSE_NEW : VOICE_PAUSE_OLD;
  const u32 format_mask = UseNewFlagMasks() ? VOICE_FORMAT_MASK_NEW : VOICE_FORMAT_MASK_OLD;
  // dsp_mixer
  const bool paused = (m_parameter_block.flags & pause_flag) != 0;
  const bool running = (m_parameter_block.flags & VOICE_RUNNING) != 0;
  const bool has_buf = m_parameter_block.buf_start != 0;
  if (!paused && running && has_buf)
  {
    // no_change_buffer
    const u32 voice_format = m_parameter_block.flags & format_mask;
    const bool is_16_bit = (voice_format & VOICE_16_BIT_FLAG) != 0;
    if (m_crc == HASH_EDUKE32)
    {
      if (voice_format != VOICE_STEREO8 && voice_format != VOICE_STEREO16 &&
          voice_format != VOICE_STEREO8_UNSIGNED)
      {
        // The EDuke32 Wii version does not support 16-but unsigned stereo, and also has broken
        // handling of all mono formats.
        if (!m_has_shown_unsupported_sample_format_warning)
        {
          m_has_shown_unsupported_sample_format_warning = true;
          PanicAlertFmt("EDuke32 Wii aesndlib uCode does not correctly handle this sample format: "
                        "{} (flags: {:08x})",
                        voice_format, m_parameter_block.flags);
        }
      }
    }
    // select_format table
    const u16 accelerator_format = is_16_bit ? ACCELERATOR_FORMAT_16_BIT : ACCELERATOR_FORMAT_8_BIT;
    const u16 accelerator_gain = is_16_bit ? ACCELERATOR_GAIN_16_BIT : ACCELERATOR_GAIN_8_BIT;
    SetUpAccelerator(accelerator_format, accelerator_gain);
    // read from select_mixer skipped

    // Note: We don't handle masking/sign extension from use of $ac0.h and $ac1.h, as this isn't
    // something that should happen if the PowerPC code is using the library normally.
    // This applies throughout.
    u32 delay = m_parameter_block.delay;
    u32 sample_index = 0;
    u32 remaining_samples = NUM_OUTPUT_SAMPLES;

    if (delay != 0)
    {
      // delay_loop
      // This is implemented with a BLOOP and then a read from the loop counter stack register in
      // the actual uCode. I'm not 100% sure how it works/if it works correctly there.
      while (remaining_samples != 0)
      {
        remaining_samples--;
        delay--;
        if (delay == 0)
          break;
        sample_index++;
      }
      // exit_delay
      m_parameter_block.delay = delay;
    }
    // no_delay - operates in set40 mode
    while (remaining_samples != 0)  // BLOOP dspmixer_loop_end
    {
      remaining_samples--;
      s32 right = m_output_buffer[sample_index * 2 + 0];
      s32 left = m_output_buffer[sample_index * 2 + 1];
      right += m_parameter_block.right;
      left += m_parameter_block.left;
      // Clamping from set40 mode
      right = std::clamp(right, -32768, 32767);
      left = std::clamp(left, -32768, 32767);
      m_output_buffer[sample_index * 2 + 0] = right;
      m_output_buffer[sample_index * 2 + 1] = left;
      sample_index++;

      const u32 counter = static_cast<u32>(m_parameter_block.counter) + m_parameter_block.freq;
      m_parameter_block.counter = static_cast<u16>(counter);
      u16 counter_h = counter >> 16;
      if (counter_h >= 1)
      {
        s16 new_l = 0, new_r = 0;

        // jrge $ar3 (using select_mixer table)
        // We already masked voice_format with the right mask for the version, so nothing special
        // needs to be done for this switch statement to exclude unsupported formats
        switch (voice_format)
        {
        case VOICE_MONO8:
        case VOICE_MONO16:
          // mono_mix
          while (counter_h >= 1)
          {
            counter_h--;
            new_r = s_accelerator->Read(ACCELERATOR_COEFS.data());
            new_l = new_r;
          }
          break;

        case VOICE_STEREO8:
        case VOICE_STEREO16:
          // stereo_mix
          while (counter_h >= 1)
          {
            counter_h--;
            new_r = s_accelerator->Read(ACCELERATOR_COEFS.data());
            new_l = s_accelerator->Read(ACCELERATOR_COEFS.data());
          }
          break;  // falls through to mix_samples normally

        case VOICE_MONO8_UNSIGNED:
        case VOICE_MONO16_UNSIGNED:
          // mono_unsigned_mix
          while (counter_h >= 1)
          {
            counter_h--;
            new_r = s_accelerator->Read(ACCELERATOR_COEFS.data());
            new_l = new_r;
          }
          new_r ^= 0x8000;
          new_l ^= 0x8000;
          break;

        case VOICE_STEREO8_UNSIGNED:
        case VOICE_STEREO16_UNSIGNED:
          // stereo_unsigned_mix
          while (counter_h >= 1)
          {
            counter_h--;
            new_r = s_accelerator->Read(ACCELERATOR_COEFS.data());
            new_l = s_accelerator->Read(ACCELERATOR_COEFS.data());
          }
          new_r ^= 0x8000;
          new_l ^= 0x8000;
          break;
        }
        if (SwapLeftRight())
        {
          // The 2012 version swapped the left and right input channels so that left comes first,
          // and then right. Before, right came before left. The 2012 version didn't update comments
          // for it, though.
          std::swap(new_r, new_l);
        }
        // mix_samples
        const s32 mixed_l = (static_cast<s32>(new_l) * m_parameter_block.volume_l) >> 8;
        const s32 mixed_r = (static_cast<s32>(new_r) * m_parameter_block.volume_r) >> 8;
        // Clamping from set40 mode
        m_parameter_block.left = std::clamp(mixed_l, -32768, 32767);
        m_parameter_block.right = std::clamp(mixed_r, -32768, 32767);
      }
      // no_mix - we don't need to do anything as we modify m_parameter_block.left/right in place
    }
    // mixer_end - back to set16 mode
    m_parameter_block.pds = s_accelerator->GetPredScale();
    m_parameter_block.yn2 = s_accelerator->GetYn2();
    m_parameter_block.yn1 = s_accelerator->GetYn1();
    m_parameter_block.buf_curr = s_accelerator->GetCurrentAddress();
  }
  // finish_voice
  m_parameter_block.flags |= VOICE_FINISHED;
  DMAOutParameterBlock();  // dma_pb_block
  m_mail_handler.PushMail(DSP_SYNC, true);
}

void AESndUCode::DoState(PointerWrap& p)
{
  DoStateShared(p);
  p.Do(m_next_mail_is_parameter_block_addr);
  p.Do(m_parameter_block_addr);
  p.Do(m_parameter_block);
  p.Do(m_output_buffer);
  s_accelerator->DoState(p);
}
}  // namespace DSP::HLE
