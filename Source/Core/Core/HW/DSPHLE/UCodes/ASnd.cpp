// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

// High-level emulation for the libasnd ucode, used by older homebrew
// libasnd is copyright 2008 Hermes <www.entuwii.net> and released under the BSD-3-Clause license

#include "Core/HW/DSPHLE/UCodes/ASnd.h"

#include <algorithm>

#include "Common/ChunkFile.h"
#include "Common/Logging/Log.h"
#include "Common/Swap.h"
#include "Core/HW/DSP.h"
#include "Core/HW/DSPHLE/DSPHLE.h"
#include "Core/HW/DSPHLE/MailHandler.h"
#include "Core/HW/DSPHLE/UCodes/UCodes.h"
#include "Core/System.h"

namespace DSP::HLE
{
// "fill the internal sample buffer and process the voice internally"
constexpr u32 MAIL_INPUT_SAMPLES = 0x0111;
// "get samples from the external buffer to the internal buffer and process the voice mixing the
// samples internally" (not used)
constexpr u32 MAIL_INPUT_SAMPLES_2 = 0x0112;
// "get the address of the voice datas buffer (CHANNEL DATAS)" (actually sets it)
constexpr u32 MAIL_SET_VOICE_DATA_BUFFER = 0x0123;
// "process the voice mixing the samples internally"
constexpr u32 MAIL_INPUT_NEXT_SAMPLES = 0x0222;
// "send the samples for the internal buffer to the external buffer"
constexpr u32 MAIN_SEND_SAMPLES = 0x0666;
// "special: to dump the IROM Datas (remember disable others functions from the interrupt vector to
// use) (CMBH+0x8000) countain the address of IROM" (not used)
constexpr u32 MAIL_ROM_DUMP_WORD = 0x0777;
// "Used for test" (not used)
constexpr u32 MAIL_TEST = 0x0888;
constexpr u32 MAIL_TERMINATE = 0x0999;

// Note that there are additional flags used on the powerpc side (UPDATEADD, UPDATE, and VOLUPDATE)
// that are not relevant to the DSP side.
// The old flags are used in the 2008 and 2009 versions, while the new ones are used in the 2011 and
// 2020 versions.
constexpr u32 OLD_FLAGS_VOICE_PAUSE = 1 << 5;
constexpr u32 OLD_FLAGS_VOICE_LOOP = 1 << 2;
constexpr u32 OLD_FLAGS_SAMPLE_FORMAT_MASK = 3;

constexpr u32 NEW_FLAGS_VOICE_PAUSE = 1 << 9;
constexpr u32 NEW_FLAGS_VOICE_LOOP = 1 << 8;
constexpr u32 NEW_FLAGS_SAMPLE_FORMAT_MASK = 7;
// Used on the PowerPC side as an enabled flag (if 0, then the voice isn't active).
// Used on the DSP side as the step in bytes after each read.
constexpr u32 FLAGS_SAMPLE_FORMAT_BYTES_MASK = 0xffff0000;
constexpr u32 FLAGS_SAMPLE_FORMAT_BYTES_SHIFT = 16;

constexpr u32 SAMPLE_RATE = 48000;

bool ASndUCode::SwapLeftRight() const
{
  return m_crc == HASH_2008 || m_crc == HASH_2009 || m_crc == HASH_2011 || m_crc == HASH_2020 ||
         m_crc == HASH_2020_PAD;
}

bool ASndUCode::UseNewFlagMasks() const
{
  return m_crc == HASH_2011 || m_crc == HASH_2020 || m_crc == HASH_2020_PAD ||
         m_crc == HASH_DESERT_BUS_2011 || m_crc == HASH_DESERT_BUS_2012 || m_crc == HASH_2024 ||
         m_crc == HASH_2024_PAD;
}

ASndUCode::ASndUCode(DSPHLE* dsphle, u32 crc) : UCodeInterface(dsphle, crc)
{
}

void ASndUCode::Initialize()
{
  m_mail_handler.PushMail(DSP_INIT);
}

void ASndUCode::Update()
{
  // This is dubious in general, since we set the interrupt parameter on m_mail_handler.PushMail
  if (m_mail_handler.HasPending())
  {
    m_dsphle->GetSystem().GetDSP().GenerateDSPInterruptFromDSPEmu(DSP::INT_DSP);
  }
}

void ASndUCode::HandleMail(u32 mail)
{
  if (m_upload_setup_in_progress)
  {
    PrepareBootUCode(mail);
  }
  else if (m_next_mail_is_voice_addr)
  {
    m_voice_addr = mail;
    INFO_LOG_FMT(DSPHLE, "ASndUCode - Voice data is at {:08x}", mail);
    m_next_mail_is_voice_addr = false;
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
      WARN_LOG_FMT(DSPHLE, "ASndUCode - unknown 0xcdd1 command: {:08x}", mail);
      break;
    }
    // No mail is sent in response to any of these
  }
  else
  {
    // The ucode only checks the lower word (as long as the upper word is not 0xCDD1).
    switch (mail & 0xffff)
    {
    case MAIL_INPUT_SAMPLES:
      DEBUG_LOG_FMT(DSPHLE, "ASndUCode - MAIL_INPUT_SAMPLES: {:08x}", mail);
      // input_samples
      DMAInVoiceData();
      // loop_get1
      m_output_buffer.fill(0);
      DoMixing(DSP_SYNC);
      // Mail is handled by DoMixing()
      break;
    case MAIL_INPUT_SAMPLES_2:
    {
      WARN_LOG_FMT(DSPHLE, "ASndUCode - MAIL_INPUT_SAMPLES_2: {:08x} - not normally used", mail);
      // input_samples2
      DMAInVoiceData();  // first do_dma call
      // second do_dma call
      auto& memory = m_dsphle->GetSystem().GetMemory();
      for (u32 i = 0; i < NUM_OUTPUT_SAMPLES * 2; i++)
      {
        m_output_buffer[i] = HLEMemory_Read_U16(memory, m_current_voice.out_buf + i * sizeof(u16));
      }
      DoMixing(DSP_SYNC);
      // Mail is handled by DoMixing()
      break;
    }
    case MAIL_SET_VOICE_DATA_BUFFER:
      DEBUG_LOG_FMT(DSPHLE, "ASndUCode - MAIL_SET_VOICE_DATA_BUFFER: {:08x}", mail);
      m_next_mail_is_voice_addr = true;
      // No mail is sent in response
      break;
    case MAIL_INPUT_NEXT_SAMPLES:
      DEBUG_LOG_FMT(DSPHLE, "ASndUCode - MAIL_INPUT_NEXT_SAMPLES: {:08x}", mail);
      // input_next_samples
      DMAInVoiceData();
      // called via fallthrough
      DoMixing(DSP_SYNC);
      // Mail is handled by DoMixing()
      break;
    case MAIN_SEND_SAMPLES:
    {
      DEBUG_LOG_FMT(DSPHLE, "ASndUCode - MAIN_SEND_SAMPLES: {:08x}", mail);
      auto& memory = m_dsphle->GetSystem().GetMemory();
      for (u32 i = 0; i < NUM_OUTPUT_SAMPLES * 2; i++)
      {
        HLEMemory_Write_U16(memory, m_current_voice.out_buf + i * sizeof(u16), m_output_buffer[i]);
      }
      m_mail_handler.PushMail(DSP_SYNC, true);
      break;
    }
    case MAIL_ROM_DUMP_WORD:
      WARN_LOG_FMT(DSPHLE, "ASndUCode - MAIL_ROM_DUMP_WORD: {:08x} - not normally used", mail);
      // Reads instruction at 0x8000 | (mail >> 16), and sends it back in DMBL. DMBH is 0.
      m_mail_handler.PushMail(0x0000'0000, false);  // DIRQ is not set
      break;
    case MAIL_TEST:
      WARN_LOG_FMT(DSPHLE, "ASndUCode - MAIL_TEST: {:08x} - not normally used", mail);
      // Runs `lri $ac0.m, #0x0` and `andf $ac0.m, #0x1`
      // and then sends $sr in DMBH and $ac0.m in DMBL
      // The exact value of $sr will vary, but this isn't used by anything in practice.
      m_mail_handler.PushMail(0x2264'0000, false);  // DIRQ is not set
      break;
    case MAIL_TERMINATE:
      if (m_crc != HASH_2008)
      {
        INFO_LOG_FMT(DSPHLE, "ASndUCode - MAIL_TERMINATE: {:08x}", mail);
        // This doesn't actually change the state of the system.
        m_mail_handler.PushMail(DSP_DONE, true);
      }
      else
      {
        WARN_LOG_FMT(DSPHLE, "ASndUCode - MAIL_TERMINATE is not supported in this version: {:08x}",
                     mail);
        m_mail_handler.PushMail(DSP_SYNC, true);
      }
      break;
    default:
      WARN_LOG_FMT(DSPHLE, "ASndUCode - unknown command: {:08x}", mail);
      m_mail_handler.PushMail(DSP_SYNC, true);
      break;
    }
  }
}

void ASndUCode::DMAInVoiceData()
{
  auto& memory = m_dsphle->GetSystem().GetMemory();
  m_current_voice.out_buf = HLEMemory_Read_U32(memory, m_voice_addr);
  m_current_voice.delay_samples = HLEMemory_Read_U32(memory, m_voice_addr + 4);
  u32 new_flags = HLEMemory_Read_U32(memory, m_voice_addr + 8);
  if (m_current_voice.flags != new_flags)
    DEBUG_LOG_FMT(DSPHLE, "ASndUCode - flags: {:08x}", new_flags);
  m_current_voice.flags = new_flags;
  m_current_voice.start_addr = HLEMemory_Read_U32(memory, m_voice_addr + 12);
  m_current_voice.end_addr = HLEMemory_Read_U32(memory, m_voice_addr + 16);
  m_current_voice.freq = HLEMemory_Read_U32(memory, m_voice_addr + 20);
  m_current_voice.left = HLEMemory_Read_U16(memory, m_voice_addr + 24);
  m_current_voice.right = HLEMemory_Read_U16(memory, m_voice_addr + 26);
  m_current_voice.counter = HLEMemory_Read_U32(memory, m_voice_addr + 28);
  m_current_voice.volume_l = HLEMemory_Read_U16(memory, m_voice_addr + 32);
  m_current_voice.volume_r = HLEMemory_Read_U16(memory, m_voice_addr + 34);
  m_current_voice.start_addr2 = HLEMemory_Read_U32(memory, m_voice_addr + 36);
  m_current_voice.end_addr2 = HLEMemory_Read_U32(memory, m_voice_addr + 40);
  m_current_voice.volume2_l = HLEMemory_Read_U16(memory, m_voice_addr + 44);
  m_current_voice.volume2_r = HLEMemory_Read_U16(memory, m_voice_addr + 46);
  m_current_voice.backup_addr = HLEMemory_Read_U32(memory, m_voice_addr + 48);
  m_current_voice.tick_counter = HLEMemory_Read_U32(memory, m_voice_addr + 52);
  m_current_voice.cb = HLEMemory_Read_U32(memory, m_voice_addr + 56);
  m_current_voice._pad = HLEMemory_Read_U32(memory, m_voice_addr + 60);
}

void ASndUCode::DMAOutVoiceData()
{
  auto& memory = m_dsphle->GetSystem().GetMemory();
  HLEMemory_Write_U32(memory, m_voice_addr, m_current_voice.out_buf);
  HLEMemory_Write_U32(memory, m_voice_addr + 4, m_current_voice.delay_samples);
  HLEMemory_Write_U32(memory, m_voice_addr + 8, m_current_voice.flags);
  HLEMemory_Write_U32(memory, m_voice_addr + 12, m_current_voice.start_addr);
  HLEMemory_Write_U32(memory, m_voice_addr + 16, m_current_voice.end_addr);
  HLEMemory_Write_U32(memory, m_voice_addr + 20, m_current_voice.freq);
  HLEMemory_Write_U16(memory, m_voice_addr + 24, m_current_voice.left);
  HLEMemory_Write_U16(memory, m_voice_addr + 26, m_current_voice.right);
  HLEMemory_Write_U32(memory, m_voice_addr + 28, m_current_voice.counter);
  HLEMemory_Write_U16(memory, m_voice_addr + 32, m_current_voice.volume_l);
  HLEMemory_Write_U16(memory, m_voice_addr + 34, m_current_voice.volume_r);
  HLEMemory_Write_U32(memory, m_voice_addr + 36, m_current_voice.start_addr2);
  HLEMemory_Write_U32(memory, m_voice_addr + 40, m_current_voice.end_addr2);
  HLEMemory_Write_U16(memory, m_voice_addr + 44, m_current_voice.volume2_l);
  HLEMemory_Write_U16(memory, m_voice_addr + 46, m_current_voice.volume2_r);
  HLEMemory_Write_U32(memory, m_voice_addr + 48, m_current_voice.backup_addr);
  HLEMemory_Write_U32(memory, m_voice_addr + 52, m_current_voice.tick_counter);
  HLEMemory_Write_U32(memory, m_voice_addr + 56, m_current_voice.cb);
  HLEMemory_Write_U32(memory, m_voice_addr + 60, m_current_voice._pad);
}

void ASndUCode::DoMixing(u32 return_mail)
{
  // Note: return_mail is set to DSP_SYNC by all callers, but then changed here... to DSP_SYNC.
  // This doesn't really make sense, but I'm keeping that behavior in case it helps with porting
  // older versions.

  // start_main

  const u32 sample_format_mask =
      UseNewFlagMasks() ? NEW_FLAGS_SAMPLE_FORMAT_MASK : OLD_FLAGS_SAMPLE_FORMAT_MASK;
  const u32 sample_format = m_current_voice.flags & sample_format_mask;
  const u32 sample_format_step =
      (m_current_voice.flags & FLAGS_SAMPLE_FORMAT_BYTES_MASK) >> FLAGS_SAMPLE_FORMAT_BYTES_SHIFT;

  // sample_selector jump table
  static constexpr std::array<std::pair<s16, s16> (ASndUCode::*)() const, 8> sample_selector{
      &ASndUCode::ReadSampleMono8Bits,           &ASndUCode::ReadSampleMono16Bits,
      &ASndUCode::ReadSampleStereo8Bits,         &ASndUCode::ReadSampleStereo16Bits,
      &ASndUCode::ReadSampleMono8BitsUnsigned,   &ASndUCode::ReadSampleMono16BitsLittleEndian,
      &ASndUCode::ReadSampleStereo8BitsUnsigned, &ASndUCode::ReadSampleStereo16BitsLittleEndian,
  };
  const auto sample_function = sample_selector[sample_format];

  const u32 pause_mask = UseNewFlagMasks() ? NEW_FLAGS_VOICE_PAUSE : OLD_FLAGS_VOICE_PAUSE;

  if ((m_current_voice.flags & pause_mask) == 0)
  {
    if (m_current_voice.start_addr == 0)
    {
      // "set return as "change of buffer""
      // (which doesn't make sense, since we're changing the return value to what it already was)
      return_mail = DSP_SYNC;
      ChangeBuffer();
      if (m_current_voice.start_addr == 0)
      {
        // Jump to save_datas_end, but since we write to start_addr directly in ChangeBuffer and
        // can't have changed prev_r or prev_l here, there's nothing that needs to be saved.
        // Thus, we can jump to end_main instead. This is still pretty ugly, but I don't think
        // there's any way to tidy it further without making the structure much more convoluted.
        goto end_main;
      }
    }
    // do_not_change1

    u32 buffer_offset = 0;
    // "delay time section"
    u16 remaining_samples = NUM_OUTPUT_SAMPLES;

    // Note: We don't handle masking/sign extension from use of $ac0.h and $ac1.h, as this isn't
    // something that should happen if the PowerPC code is using the library normally.
    // This applies throughout.
    if (m_current_voice.delay_samples != 0)
    {
      m_current_voice.right = 0;
      m_current_voice.left = 0;
      u16 remaining_samples_delay = remaining_samples;
      // l_delay
      while (true)
      {
        buffer_offset += 2;
        remaining_samples_delay--;
        if (remaining_samples_delay == 0)
        {
          // exit_delay1
          remaining_samples = 0;
          break;
        }
        m_current_voice.delay_samples--;
        if (m_current_voice.delay_samples == 0)
        {
          // exit_delay2
          // commented with "load remanent samples to be processed in AXL0",
          // but they use "MRR $AX0.L, $AC1.L" when the remaining samples are in $AC1.M.
          remaining_samples = 0;
          break;
        }
      }
    }
    // no_delay
    // "bucle de generacion de samples", i.e. sample generation loop
    DMAInSampleData();
    // "loops for samples to be processed", corresponding to BLOOP $ax0.l, loop_end
    while (remaining_samples > 0)
    {
      remaining_samples--;
      // "Mix right sample section"
      s32 sample_r = static_cast<s16>(m_output_buffer[buffer_offset]);
      sample_r += m_current_voice.right;
      sample_r = std::clamp(sample_r, -32768, 32767);
      m_output_buffer[buffer_offset++] = sample_r;
      // "Mix left sample section"
      s32 sample_l = static_cast<s16>(m_output_buffer[buffer_offset]);
      sample_l += m_current_voice.left;
      sample_l = std::clamp(sample_l, -32768, 32767);
      m_output_buffer[buffer_offset++] = sample_l;
      // "adds the counter with the voice frequency and test if it >=48000 to get the next sample"
      m_current_voice.counter += m_current_voice.freq;
      if (m_current_voice.counter >= SAMPLE_RATE)
      {
        if (m_current_voice.freq <= SAMPLE_RATE)
        {
          // get_sample: "fast method"
          m_current_voice.counter -= SAMPLE_RATE;
          m_current_voice.start_addr += sample_format_step;
        }
        else
        {
          // get_sample2: "slow method"
          while (m_current_voice.counter >= SAMPLE_RATE)
          {
            m_current_voice.counter -= SAMPLE_RATE;
            m_current_voice.start_addr += sample_format_step;
            if ((m_current_voice.start_addr & INPUT_SAMPLE_BUFFER_BYTE_MASK) == 0)
            {
              DMAInSampleData();
            }
          }
        }

        if (m_current_voice.start_addr >= m_current_voice.end_addr)
        {
          // get_new_buffer (the comparison/jump is performed separately in get_sample and
          // get_sample2, but the same comparison is done for both)
          return_mail = DSP_SYNC;
          ChangeBuffer();
          if (m_current_voice.start_addr == 0)
          {
            // zero_samples
            // This directly jumps to out_samp as well, bypassing the code below.
            // Fortunately we can replicate this behavior with `continue`, and don't need to handle
            // the multiplication logic in out_samp since the samples are zero.
            m_current_voice.right = 0;
            m_current_voice.left = 0;
            continue;
          }
          DMAInSampleData();
        }
        // jump_load_smp_addr
        const u16 input_sample_offset =
            (m_current_voice.start_addr >> 1) & INPUT_SAMPLE_BUFFER_WORD_MASK;
        if (input_sample_offset == 0)
        {
          // jump_load_smp_dma
          DMAInSampleDataAssumeAligned();
        }
        // Both paths jmpr $AR3, which is an index into sample_selector

        auto [new_l, new_r] = (this->*sample_function)();
        if (SwapLeftRight())
        {
          // Most versions of the ASnd ucode have the right channel come before the left channel.
          // The Desert Bus and 2024 versions swapped the left and right input channels so that left
          // comes first, and then right, matching mp3/ogg files.
          std::swap(new_r, new_l);
        }
        // out_samp: "multiply sample x volume" - left is put in $ax0.h, right is put in $ax1.h

        // All functions jumped to from sample_selector jump or fall through here (zero_samples also
        // jumps here, but is handled separately above for structural reasons)
        const s32 temp_l = static_cast<s32>(new_l) * m_current_voice.volume_l;
        m_current_voice.left = static_cast<s16>(temp_l >> 8);
        const s32 temp_r = static_cast<s32>(new_r) * m_current_voice.volume_r;
        m_current_voice.right = static_cast<s16>(temp_r >> 8);
      }
      // loop_end, remember that the labeled instruction is the last instruction in the loop
      // so jumping to the NOP here causes the loop to repeat (and decrement its counter)
    }
    // end_process (not actually jumped to)
    if (m_current_voice.start_addr == 0)
    {
      return_mail = DSP_SYNC;
      ChangeBuffer();
    }
    // save_datas_end (we mutate m_current_voice.start_addr, left, and right in place,
    // so nothing needs to happen here here)
  }
end_main:  // end_main
  DMAOutVoiceData();

  m_mail_handler.PushMail(return_mail, true);
}

void ASndUCode::ChangeBuffer()
{
  // change_buffer
  m_current_voice.volume_l = m_current_voice.volume2_l;
  m_current_voice.volume_r = m_current_voice.volume2_r;
  m_current_voice.end_addr = m_current_voice.end_addr2;
  m_current_voice.start_addr = m_current_voice.start_addr2;
  m_current_voice.backup_addr = m_current_voice.start_addr2;

  const u32 loop_mask = UseNewFlagMasks() ? NEW_FLAGS_VOICE_LOOP : OLD_FLAGS_VOICE_LOOP;

  if ((m_current_voice.flags & loop_mask) == 0)
  {
    m_current_voice.start_addr2 = 0;
    m_current_voice.end_addr2 = 0;
  }
}

void ASndUCode::DMAInSampleData()
{
  // load_smp_addr_align
  // This is its own function, and it contains its own copy of the DMA logic.
  // The only difference is that this one forces the address to be aligned, while when
  // jump_load_smp_dma is used, the address is expected to already be aligned.
  const u32 addr = m_current_voice.start_addr & ~INPUT_SAMPLE_BUFFER_BYTE_MASK;
  auto& memory = m_dsphle->GetSystem().GetMemory();
  for (u16 i = 0; i < INPUT_SAMPLE_BUFFER_SIZE_WORDS; i++)
  {
    m_input_sample_buffer[i] = HLEMemory_Read_U16(memory, addr + i * sizeof(u16));
  }
}

void ASndUCode::DMAInSampleDataAssumeAligned()
{
  // jump_load_smp_dma
  // This is technically not a function, but instead is directly jumped to and then jumps to $ar3
  // (which is set to an address from sample_selector). We can just treat it as a function though.
  const u32 addr = m_current_voice.start_addr;
  auto& memory = m_dsphle->GetSystem().GetMemory();
  for (u16 i = 0; i < INPUT_SAMPLE_BUFFER_SIZE_WORDS; i++)
  {
    m_input_sample_buffer[i] = HLEMemory_Read_U16(memory, addr + i * sizeof(u16));
  }
}

std::pair<s16, s16> ASndUCode::ReadSampleMono8Bits() const
{
  // mono_8bits
  const u32 index = (m_current_voice.start_addr >> 1) & INPUT_SAMPLE_BUFFER_WORD_MASK;
  s16 result = m_input_sample_buffer[index];
  if ((m_current_voice.start_addr & 1) == 0)
    result >>= 8;
  result <<= 8;
  return {result, result};
}

std::pair<s16, s16> ASndUCode::ReadSampleStereo8Bits() const
{
  // stereo_8bits
  const u32 index = (m_current_voice.start_addr >> 1) & INPUT_SAMPLE_BUFFER_WORD_MASK;
  const u16 sample = m_input_sample_buffer[index];
  const s16 right = sample & 0xff00;
  const s16 left = sample << 8;
  return {right, left};
}
std::pair<s16, s16> ASndUCode::ReadSampleMono16Bits() const
{
  // mono_16bits
  const u32 index = (m_current_voice.start_addr >> 1) & INPUT_SAMPLE_BUFFER_WORD_MASK;
  const s16 result = m_input_sample_buffer[index];
  return {result, result};
}

std::pair<s16, s16> ASndUCode::ReadSampleStereo16Bits() const
{
  // stereo_16bits
  const u32 index = (m_current_voice.start_addr >> 1) & INPUT_SAMPLE_BUFFER_WORD_MASK;
  const s16 right = m_input_sample_buffer[index];
  // Note that 1 is added to index after the masking - meaning that theoretically an out-of-bounds
  // index 0x10 can be read (but the buffer is oversized both here and in the actual uCode, so the
  // data at the out-of-bounds index will instead be 0).
  const s16 left = m_input_sample_buffer[index + 1];
  return {right, left};
}

std::pair<s16, s16> ASndUCode::ReadSampleMono8BitsUnsigned() const
{
  // mono_8bits_unsigned
  const u32 index = (m_current_voice.start_addr >> 1) & INPUT_SAMPLE_BUFFER_WORD_MASK;
  s16 result = m_input_sample_buffer[index];
  if ((m_current_voice.start_addr & 1) == 0)
    result >>= 8;
  result <<= 8;
  result ^= 0x8000;  // Flip the sign bit - effectively adding 0x8000
  return {result, result};
}

std::pair<s16, s16> ASndUCode::ReadSampleMono16BitsLittleEndian() const
{
  // mono_16bits_le
  const u32 index = (m_current_voice.start_addr >> 1) & INPUT_SAMPLE_BUFFER_WORD_MASK;
  // Actual implementation is u32 result_l = result | result << 16; result = result_l >> 8;
  const s16 result = Common::swap16(m_input_sample_buffer[index]);
  return {result, result};
}

std::pair<s16, s16> ASndUCode::ReadSampleStereo8BitsUnsigned() const
{
  // stereo_8bits_unsigned
  const u32 index = (m_current_voice.start_addr >> 1) & INPUT_SAMPLE_BUFFER_WORD_MASK;
  u16 sample = m_input_sample_buffer[index] ^ 0x8080;
  const s16 right = sample & 0xff00;
  const s16 left = sample << 8;
  return {right, left};
}

std::pair<s16, s16> ASndUCode::ReadSampleStereo16BitsLittleEndian() const
{
  // stereo_16bits_le
  const u32 index = (m_current_voice.start_addr >> 1) & INPUT_SAMPLE_BUFFER_WORD_MASK;
  const s16 right = Common::swap16(m_input_sample_buffer[index]);
  // Note that 1 is added to index after the masking - meaning that theoretically an out-of-bounds
  // index 0x10 can be read (but the buffer is oversized both here and in the actual uCode, so the
  // data at the out-of-bounds index will instead be 0).
  const s16 left = Common::swap16(m_input_sample_buffer[index + 1]);
  return {right, left};
}

void ASndUCode::DoState(PointerWrap& p)
{
  DoStateShared(p);
  p.Do(m_next_mail_is_voice_addr);
  p.Do(m_voice_addr);
  p.Do(m_current_voice);
  p.Do(m_input_sample_buffer);
  p.Do(m_output_buffer);
}
}  // namespace DSP::HLE
