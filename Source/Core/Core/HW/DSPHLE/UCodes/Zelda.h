// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <array>

#include "Common/CommonTypes.h"
#include "Common/MathUtil.h"
#include "Core/HW/DSPHLE/UCodes/UCodes.h"

namespace DSP
{
namespace HLE
{
class DSPHLE;

class ZeldaAudioRenderer
{
public:
  void PrepareFrame();
  void AddVoice(u16 voice_id);
  void FinalizeFrame();

  void SetFlags(u32 flags) { m_flags = flags; }
  void SetSineTable(std::array<s16, 0x80>&& sine_table) { m_sine_table = sine_table; }
  void SetConstPatterns(std::array<s16, 0x100>&& patterns) { m_const_patterns = patterns; }
  void SetResamplingCoeffs(std::array<s16, 0x100>&& coeffs) { m_resampling_coeffs = coeffs; }
  void SetAfcCoeffs(std::array<s16, 0x20>&& coeffs) { m_afc_coeffs = coeffs; }
  void SetVPBBaseAddress(u32 addr) { m_vpb_base_addr = addr; }
  void SetReverbPBBaseAddress(u32 addr) { m_reverb_pb_base_addr = addr; }
  void SetOutputVolume(u16 volume) { m_output_volume = volume; }
  void SetOutputLeftBufferAddr(u32 addr) { m_output_lbuf_addr = addr; }
  void SetOutputRightBufferAddr(u32 addr) { m_output_rbuf_addr = addr; }
  void SetARAMBaseAddr(u32 addr) { m_aram_base_addr = addr; }
  void DoState(PointerWrap& p);

private:
  struct VPB;

  // See Zelda.cpp for the list of possible flags.
  u32 m_flags;

  // Utility functions for audio operations.

  // Apply volume to a buffer. The volume is a fixed point integer, usually
  // 1.15 or 4.12 in the DAC UCode.
  template <size_t N, size_t B>
  void ApplyVolumeInPlace(std::array<s16, N>* buf, u16 vol)
  {
    for (size_t i = 0; i < N; ++i)
    {
      s32 tmp = (u32)(*buf)[i] * (u32)vol;
      tmp >>= 16 - B;

      (*buf)[i] = (s16)MathUtil::Clamp(tmp, -0x8000, 0x7FFF);
    }
  }
  template <size_t N>
  void ApplyVolumeInPlace_1_15(std::array<s16, N>* buf, u16 vol)
  {
    ApplyVolumeInPlace<N, 1>(buf, vol);
  }
  template <size_t N>
  void ApplyVolumeInPlace_4_12(std::array<s16, N>* buf, u16 vol)
  {
    ApplyVolumeInPlace<N, 4>(buf, vol);
  }

  // Mixes two buffers together while applying a volume to one of them. The
  // volume ramps up/down in N steps using the provided step delta value.
  //
  // Note: On a real GC, the stepping happens in 32 steps instead. But hey,
  // we can do better here with very low risk. Why not? :)
  template <size_t N>
  s32 AddBuffersWithVolumeRamp(std::array<s16, N>* dst, const std::array<s16, N>& src, s32 vol,
                               s32 step)
  {
    if (!vol && !step)
      return vol;

    for (size_t i = 0; i < N; ++i)
    {
      (*dst)[i] += ((vol >> 16) * src[i]) >> 16;
      vol += step;
    }

    return vol;
  }

  // Does not use std::array because it needs to be able to process partial
  // buffers. Volume is in 1.15 format.
  void AddBuffersWithVolume(s16* dst, const s16* src, size_t count, u16 vol)
  {
    while (count--)
    {
      s32 vol_src = ((s32)*src++ * (s32)vol) >> 15;
      *dst++ += MathUtil::Clamp(vol_src, -0x8000, 0x7FFF);
    }
  }

  // Whether the frame needs to be prepared or not.
  bool m_prepared = false;

  // MRAM addresses where output samples should be copied.
  u32 m_output_lbuf_addr = 0;
  u32 m_output_rbuf_addr = 0;

  // Output volume applied to buffers before being uploaded to RAM.
  u16 m_output_volume = 0;

  // Mixing buffers.
  typedef std::array<s16, 0x50> MixingBuffer;
  MixingBuffer m_buf_front_left{};
  MixingBuffer m_buf_front_right{};
  MixingBuffer m_buf_back_left{};
  MixingBuffer m_buf_back_right{};
  MixingBuffer m_buf_front_left_reverb{};
  MixingBuffer m_buf_front_right_reverb{};
  MixingBuffer m_buf_back_left_reverb{};
  MixingBuffer m_buf_back_right_reverb{};
  MixingBuffer m_buf_unk0_reverb{};
  MixingBuffer m_buf_unk1_reverb{};
  MixingBuffer m_buf_unk0{};
  MixingBuffer m_buf_unk1{};
  MixingBuffer m_buf_unk2{};

  // Maps a buffer "ID" (really, their address in the DSP DRAM...) to our
  // buffers. Returns nullptr if no match is found.
  MixingBuffer* BufferForID(u16 buffer_id);

  // Base address where VPBs are stored linearly in RAM.
  u32 m_vpb_base_addr;
  void FetchVPB(u16 voice_id, VPB* vpb);
  void StoreVPB(u16 voice_id, VPB* vpb);

  // Sine table transferred from MRAM. Contains sin(x) values for x in
  // [0.0;pi/4] (sin(x) in [1.0;0.0]), in 1.15 fixed format.
  std::array<s16, 0x80> m_sine_table{};

  // Const patterns used for some voice samples source. 4 x 0x40 samples.
  std::array<s16, 0x100> m_const_patterns{};

  // Fills up a buffer with the input samples for a voice, represented by its
  // VPB.
  void LoadInputSamples(MixingBuffer* buffer, VPB* vpb);

  // Raw samples (pre-resampling) that need to be generated to result in 0x50
  // post-resampling input samples.
  u16 NeededRawSamplesCount(const VPB& vpb);

  // Resamples raw samples to 0x50 input samples, using the resampling ratio
  // and current position information from the VPB.
  void Resample(VPB* vpb, const s16* src, MixingBuffer* dst);

  // Coefficients used for resampling.
  std::array<s16, 0x100> m_resampling_coeffs{};

  // If non zero, base MRAM address for sound data transfers from ARAM. On
  // the Wii, this points to some MRAM location since there is no ARAM to be
  // used. If zero, use the top of ARAM.
  u32 m_aram_base_addr = 0;
  void* GetARAMPtr() const;

  // Downloads PCM encoded samples from ARAM. Handles looping and other
  // parameters appropriately.
  template <typename T>
  void DownloadPCMSamplesFromARAM(s16* dst, VPB* vpb, u16 requested_samples_count);

  // Downloads AFC encoded samples from ARAM and decode them. Handles looping
  // and other parameters appropriately.
  void DownloadAFCSamplesFromARAM(s16* dst, VPB* vpb, u16 requested_samples_count);
  void DecodeAFC(VPB* vpb, s16* dst, size_t block_count);
  std::array<s16, 0x20> m_afc_coeffs{};

  // Downloads samples from MRAM while handling appropriate length / looping
  // behavior.
  void DownloadRawSamplesFromMRAM(s16* dst, VPB* vpb, u16 requested_samples_count);

  // Applies the reverb effect to Dolby mixed voices based on a set of
  // per-buffer parameters. Is called twice: once before frame rendering and
  // once after.
  void ApplyReverb(bool post_rendering);
  std::array<u16, 4> m_reverb_pb_frames_count{};
  std::array<s16, 8> m_buf_unk0_reverb_last8{};
  std::array<s16, 8> m_buf_unk1_reverb_last8{};
  std::array<s16, 8> m_buf_front_left_reverb_last8{};
  std::array<s16, 8> m_buf_front_right_reverb_last8{};
  u32 m_reverb_pb_base_addr = 0;
};

class ZeldaUCode : public UCodeInterface
{
public:
  ZeldaUCode(DSPHLE* dsphle, u32 crc);
  virtual ~ZeldaUCode();

  void Initialize() override;
  void HandleMail(u32 mail) override;
  void Update() override;

  void DoState(PointerWrap& p) override;

private:
  // Flags that alter the behavior of the UCode. See Zelda.cpp for complete
  // list and explanation.
  u32 m_flags;

  // Different mail handlers for different protocols.
  void HandleMailDefault(u32 mail);
  void HandleMailLight(u32 mail);

  // UCode state machine. The control flow in the Zelda UCode family is quite
  // complex, using interrupt handlers heavily to handle incoming messages
  // which, depending on the type, get handled immediately or are queued in a
  // command buffer. In this implementation, the synchronous+interrupts flow
  // of the original DSP implementation is rewritten in an asynchronous/coro
  // + state machine style. It is less readable, but the best we can do given
  // our constraints.
  enum class MailState : u32
  {
    WAITING,
    RENDERING,
    WRITING_CMD,
    HALTED,
  };
  MailState m_mail_current_state = MailState::WAITING;
  u32 m_mail_expected_cmd_mails = 0;

  // Utility function to set the current state. Useful for debugging and
  // logging as a hook point.
  void SetMailState(MailState new_state);

  // Voice synchronization / audio rendering flow control. When rendering an
  // audio frame, only voices up to max_voice_id will be rendered until a
  // sync mail arrives, increasing the value of max_voice_id. Additionally,
  // these sync mails contain 16 bit values that are used as bitfields to
  // control voice skipping on a voice per voice level.
  u32 m_sync_max_voice_id = 0;
  std::array<u16, 256> m_sync_voice_skip_flags{};
  bool m_sync_flags_second_half = false;

  // Command buffer (circular queue with r/w indices). Filled by HandleMail
  // when the state machine is in WRITING_CMD state. Commands get executed
  // when entering WAITING state and we are not rendering audio.
  std::array<u32, 64> m_cmd_buffer{};
  u32 m_read_offset = 0;
  u32 m_write_offset = 0;
  u32 m_pending_commands_count = 0;
  bool m_cmd_can_execute = true;

  // Reads a 32 bit value from the command buffer. Advances the read pointer.
  u32 Read32();

  // Writes a 32 bit value to the command buffer. Advances the write pointer.
  void Write32(u32 val);

  // Tries to run as many commands as possible until either the command
  // buffer is empty (pending_commands == 0) or we reached a long lived
  // command that needs to hijack the mail control flow.
  //
  // Might change the current state to indicate crashy commands.
  void RunPendingCommands();

  // Sends the two mails from DSP to CPU to ack the command execution.
  enum class CommandAck : u32
  {
    STANDARD,
    DONE_RENDERING,
  };
  void SendCommandAck(CommandAck ack_type, u16 sync_value);

  // Audio rendering flow control state.
  u32 m_rendering_requested_frames = 0;
  u16 m_rendering_voices_per_frame = 0;
  u32 m_rendering_curr_frame = 0;
  u32 m_rendering_curr_voice = 0;

  bool RenderingInProgress() const
  {
    return m_rendering_curr_frame != m_rendering_requested_frames;
  }
  void RenderAudio();

  // Main object handling audio rendering logic and state.
  ZeldaAudioRenderer m_renderer;
};
}  // namespace HLE
}  // namespace DSP
