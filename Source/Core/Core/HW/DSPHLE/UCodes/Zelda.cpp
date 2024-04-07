// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/HW/DSPHLE/UCodes/Zelda.h"

#include <algorithm>
#include <array>
#include <map>

#include "Common/ChunkFile.h"
#include "Common/CommonTypes.h"
#include "Common/Logging/Log.h"
#include "Common/Swap.h"
#include "Core/HW/DSP.h"
#include "Core/HW/DSPHLE/DSPHLE.h"
#include "Core/HW/DSPHLE/MailHandler.h"
#include "Core/HW/DSPHLE/UCodes/GBA.h"
#include "Core/HW/DSPHLE/UCodes/UCodes.h"
#include "Core/System.h"

namespace DSP::HLE
{
// Uncomment this to have a strict version of the HLE implementation, which
// PanicAlerts on recoverable unknown behaviors instead of silently ignoring
// them.  Recommended for development.
// #define STRICT_ZELDA_HLE 1

// These flags modify the behavior of the HLE implementation based on the UCode
// version. When introducing a new flag, please recheck the behavior of each
// UCode version.
enum ZeldaUCodeFlag
{
  // UCode for Wii where no ARAM is present. Instead of using ARAM, DMAs from
  // MRAM are used to transfer sound data.
  NO_ARAM = 0x00000001,

  // Multiply by two the computed Dolby positional volumes. Some UCodes do
  // not do that (Zelda TWW for example), others do (Zelda TP, SMG).
  MAKE_DOLBY_LOUDER = 0x00000002,

  // Light version of the UCode: no Dolby mixing, different synchronization
  // protocol, etc.
  LIGHT_PROTOCOL = 0x00000004,

  // If set, only consider 4 of the 6 non-Dolby mixing outputs. Early
  // versions of the Zelda UCode only had 4.
  FOUR_MIXING_DESTS = 0x00000008,

  // Handle smaller VPBs that are missing their 0x40-0x80 area. Very early
  // versions of the Zelda UCode used 0x80 sized VPBs.
  TINY_VPB = 0x00000010,

  // If set, interpret non-Dolby mixing parameters as step/current volume
  // instead of target/current volume.
  VOLUME_EXPLICIT_STEP = 0x00000020,

  // If set, handle synchronization per-frame instead of per-16-voices.
  SYNC_PER_FRAME = 0x00000040,

  // If set, does not support command 0D. TODO: rename.
  NO_CMD_0D = 0x00000080,

  // If set, command 0C is used for GBA crypto. This was used before the GBA
  // UCode and UCode switching was available.
  SUPPORTS_GBA_CRYPTO = 0x00000100,

  // If set, command 0C is used for an unknown purpose. TODO: rename.
  WEIRD_CMD_0C = 0x00000200,

  // If set, command 0D (unknown purpose) is combined with the render command,
  // which as such takes two more command arguments. TODO: rename.
  COMBINED_CMD_0D = 0x00000400,
};

static const std::map<u32, u32> UCODE_FLAGS = {
    // GameCube IPL/BIOS, NTSC.
    {0x24B22038, LIGHT_PROTOCOL | FOUR_MIXING_DESTS | TINY_VPB | VOLUME_EXPLICIT_STEP | NO_CMD_0D |
                     WEIRD_CMD_0C},
    // GameCube IPL/BIOS, PAL.
    {0x6BA3B3EA, LIGHT_PROTOCOL | FOUR_MIXING_DESTS | NO_CMD_0D},
    // Pikmin 1 GC NTSC Demo.
    {0xDF059F68, LIGHT_PROTOCOL | NO_CMD_0D | SUPPORTS_GBA_CRYPTO},
    // Pikmin 1 GC NTSC.
    // Animal Crossing.
    {0x4BE6A5CB, LIGHT_PROTOCOL | NO_CMD_0D | SUPPORTS_GBA_CRYPTO},
    // Luigi's Mansion.
    {0x42F64AC4, LIGHT_PROTOCOL | NO_CMD_0D | WEIRD_CMD_0C},
    // Pikmin 1 GC PAL.
    {0x267FD05A, SYNC_PER_FRAME | NO_CMD_0D},
    // Super Mario Sunshine.
    {0x56D36052, SYNC_PER_FRAME | NO_CMD_0D},
    // The Legend of Zelda: The Wind Waker.
    {0x86840740, 0},
    // The Legend of Zelda: Collector's Edition (except Wind Waker).
    // The Legend of Zelda: Four Swords Adventures.
    // Mario Kart: Double Dash.
    // Pikmin 2 GC NTSC.
    {0x2FCDF1EC, MAKE_DOLBY_LOUDER},
    // The Legend of Zelda: Twilight Princess / GC.
    // Donkey Kong Jungle Beat GC.
    //
    // TODO: These do additional filtering at frame rendering time. We don't
    // implement this yet.
    {0x6CA33A6D, MAKE_DOLBY_LOUDER | COMBINED_CMD_0D},
    // The Legend of Zelda: Twilight Princess / Wii.
    // Link's Crossbow Training.
    {0x6C3F6F94, NO_ARAM | MAKE_DOLBY_LOUDER | COMBINED_CMD_0D},
    // Super Mario Galaxy.
    // Super Mario Galaxy 2.
    // Donkey Kong Jungle Beat Wii.
    {0xD643001F, NO_ARAM | MAKE_DOLBY_LOUDER | COMBINED_CMD_0D},
    // Pikmin 1 New Play Control.
    {0xB7EB9A9C, NO_ARAM | MAKE_DOLBY_LOUDER | COMBINED_CMD_0D},
    // Pikmin 2 New Play Control.
    {0xEAEB38CC, NO_ARAM | MAKE_DOLBY_LOUDER | COMBINED_CMD_0D},
};

ZeldaUCode::ZeldaUCode(DSPHLE* dsphle, u32 crc)
    : UCodeInterface(dsphle, crc), m_renderer(dsphle->GetSystem())
{
  auto it = UCODE_FLAGS.find(crc);
  if (it == UCODE_FLAGS.end())
    PanicAlertFmt("No flags definition found for Zelda CRC {:08x}", crc);

  m_flags = it->second;
  m_renderer.SetFlags(m_flags);

  INFO_LOG_FMT(DSPHLE, "Zelda UCode loaded, crc={:08x}, flags={:08x}", crc, m_flags);
}

void ZeldaUCode::Initialize()
{
  if (m_flags & LIGHT_PROTOCOL)
  {
    m_mail_handler.PushMail(0x88881111);
  }
  else
  {
    m_mail_handler.PushMail(DSP_INIT, true);
    m_mail_handler.PushMail(0xF3551111);  // handshake
  }
}

void ZeldaUCode::Update()
{
  if (NeedsResumeMail())
  {
    m_mail_handler.PushMail(DSP_RESUME, true);
  }
}

void ZeldaUCode::DoState(PointerWrap& p)
{
  p.Do(m_flags);
  p.Do(m_mail_current_state);
  p.Do(m_mail_expected_cmd_mails);

  p.Do(m_sync_max_voice_id);
  p.Do(m_sync_voice_skip_flags);
  p.Do(m_sync_flags_second_half);

  p.Do(m_cmd_buffer);
  p.Do(m_read_offset);
  p.Do(m_write_offset);
  p.Do(m_pending_commands_count);
  p.Do(m_cmd_can_execute);

  p.Do(m_rendering_requested_frames);
  p.Do(m_rendering_voices_per_frame);
  p.Do(m_rendering_curr_frame);
  p.Do(m_rendering_curr_voice);

  m_renderer.DoState(p);

  DoStateShared(p);
}

void ZeldaUCode::HandleMail(u32 mail)
{
  if (m_upload_setup_in_progress)  // evaluated first!
  {
    PrepareBootUCode(mail);
    return;
  }

  if (m_flags & LIGHT_PROTOCOL)
    HandleMailLight(mail);
  else
    HandleMailDefault(mail);
}

void ZeldaUCode::HandleMailDefault(u32 mail)
{
  switch (m_mail_current_state)
  {
  case MailState::WAITING:
    if (mail & 0x80000000)
    {
      if ((mail & TASK_MAIL_MASK) != TASK_MAIL_TO_DSP)
      {
        WARN_LOG_FMT(DSPHLE, "Received rendering end mail without prefix CDD1: {:08x}", mail);
        mail = TASK_MAIL_TO_DSP | (mail & ~TASK_MAIL_MASK);
        // The actual uCode does not check for the CDD1 prefix.
      }

      switch (mail)
      {
      case MAIL_NEW_UCODE:
        m_cmd_can_execute = true;
        RunPendingCommands();
        NOTICE_LOG_FMT(DSPHLE, "UCode being replaced.");
        m_upload_setup_in_progress = true;
        SetMailState(MailState::WAITING);
        break;

      case MAIL_RESET:
        NOTICE_LOG_FMT(DSPHLE, "UCode being rebooted to ROM.");
        SetMailState(MailState::HALTED);
        m_dsphle->SetUCode(UCODE_ROM);
        break;

      case MAIL_CONTINUE:
        m_cmd_can_execute = true;
        RunPendingCommands();
        break;

      default:
        NOTICE_LOG_FMT(DSPHLE, "Unknown end rendering action. Halting.");
        [[fallthrough]];
      case MAIL_RESUME:
        NOTICE_LOG_FMT(DSPHLE, "UCode asked to halt. Stopping any processing.");
        SetMailState(MailState::HALTED);
        break;
      }
    }
    else if (!(mail & 0xFFFF))
    {
      if (RenderingInProgress())
      {
        SetMailState(MailState::RENDERING);
      }
      else
      {
        NOTICE_LOG_FMT(DSPHLE,
                       "Sync mail ({:08x}) received when rendering was not active. Halting.", mail);
        SetMailState(MailState::HALTED);
      }
    }
    else
    {
      SetMailState(MailState::WRITING_CMD);
      m_mail_expected_cmd_mails = mail & 0xFFFF;
    }
    break;

  case MailState::RENDERING:
    if (m_flags & SYNC_PER_FRAME)
    {
      int base = m_sync_flags_second_half ? 2 : 0;
      m_sync_voice_skip_flags[base] = mail >> 16;
      m_sync_voice_skip_flags[base + 1] = mail & 0xFFFF;

      if (m_sync_flags_second_half)
        m_sync_max_voice_id = 0xFFFF;

      RenderAudio();
      if (m_sync_flags_second_half)
        SetMailState(MailState::WAITING);
      m_sync_flags_second_half = !m_sync_flags_second_half;
    }
    else
    {
      m_sync_max_voice_id = (((mail >> 16) & 0xF) + 1) << 4;
      m_sync_voice_skip_flags[(mail >> 16) & 0xFF] = mail & 0xFFFF;
      RenderAudio();
      SetMailState(MailState::WAITING);
    }
    break;

  case MailState::WRITING_CMD:
    Write32(mail);

    if (--m_mail_expected_cmd_mails == 0)
    {
      m_pending_commands_count += 1;
      SetMailState(MailState::WAITING);
      RunPendingCommands();
    }
    break;

  case MailState::HALTED:
    WARN_LOG_FMT(DSPHLE, "Received mail {:08x} while we're halted.", mail);
    break;
  }
}

void ZeldaUCode::HandleMailLight(u32 mail)
{
  bool add_command = true;

  switch (m_mail_current_state)
  {
  case MailState::WAITING:
    if ((mail & 0x80000000) == 0)
      PanicAlertFmt("Mail received in waiting state has MSB=0: {:08x}", mail);

    // Start of a command. We have to hardcode the number of mails required
    // for each command - the alternative is to rewrite command handling as
    // an asynchronous procedure, and we wouldn't want that, would we?
    Write32(mail);

    switch ((mail >> 24) & 0x7F)
    {
    case 0x00:
      m_mail_expected_cmd_mails = 0;
      break;
    case 0x01:
      m_mail_expected_cmd_mails = 4;
      break;
    case 0x02:
      m_mail_expected_cmd_mails = 2;
      break;
    // Doesn't even register as a command, just rejumps to the dispatcher.
    // TODO: That's true on 0x4BE6A5CB and 0x42F64AC4, what about others?
    case 0x03:
      add_command = false;
      break;
    case 0x0C:
      if (m_flags & SUPPORTS_GBA_CRYPTO)
        m_mail_expected_cmd_mails = 1;
      else if (m_flags & WEIRD_CMD_0C)
        m_mail_expected_cmd_mails = 2;
      else
        m_mail_expected_cmd_mails = 0;
      break;

    default:
      PanicAlertFmt("Received unknown command in light protocol: {:08x}", mail);
      break;
    }
    if (m_mail_expected_cmd_mails)
    {
      SetMailState(MailState::WRITING_CMD);
    }
    else if (add_command)
    {
      m_pending_commands_count += 1;
      RunPendingCommands();
    }
    break;

  case MailState::WRITING_CMD:
    Write32(mail);
    if (--m_mail_expected_cmd_mails == 0)
    {
      m_pending_commands_count += 1;
      SetMailState(MailState::WAITING);
      RunPendingCommands();
    }
    break;

  case MailState::RENDERING:
    if (mail != 0)
      PanicAlertFmt("Sync mail is not zero: {:08x}", mail);

    // No per-voice syncing in the light protocol.
    m_sync_max_voice_id = 0xFFFFFFFF;
    m_sync_voice_skip_flags.fill(0xFFFF);
    RenderAudio();
    m_dsphle->GetSystem().GetDSP().GenerateDSPInterruptFromDSPEmu(DSP::INT_DSP);
    break;

  case MailState::HALTED:
    WARN_LOG_FMT(DSPHLE, "Received mail {:08x} while we're halted.", mail);
    break;
  }
}

void ZeldaUCode::SetMailState(MailState new_state)
{
  m_mail_current_state = new_state;
}

u32 ZeldaUCode::Read32()
{
  if (m_read_offset == m_write_offset)
  {
    ERROR_LOG_FMT(DSPHLE, "Reading too many command params");
    return 0;
  }

  u32 res = m_cmd_buffer[m_read_offset];
  m_read_offset = (m_read_offset + 1) % (sizeof(m_cmd_buffer) / sizeof(u32));
  return res;
}

void ZeldaUCode::Write32(u32 val)
{
  m_cmd_buffer[m_write_offset] = val;
  m_write_offset = (m_write_offset + 1) % (sizeof(m_cmd_buffer) / sizeof(u32));
}

void ZeldaUCode::RunPendingCommands()
{
  if (RenderingInProgress() || !m_cmd_can_execute)
  {
    // No commands can be ran while audio rendering is in progress or
    // waiting for an ACK.
    return;
  }

  while (m_pending_commands_count)
  {
    const u32 cmd_mail = Read32();
    if ((cmd_mail & 0x80000000) == 0)
      continue;

    const u32 command = (cmd_mail >> 24) & 0x7f;
    const u32 sync = cmd_mail >> 16;
    const u32 extra_data = cmd_mail & 0xFFFF;

    m_pending_commands_count--;

    switch (command)
    {
    case 0x00:
    case 0x0A:  // not a NOP in the NTSC IPL ucode but seems unused
    case 0x0B:  // not a NOP in both IPL ucodes but seems unused
    case 0x0F:
      // NOP commands. Log anyway in case we encounter a new version
      // where these are not NOPs anymore.
      NOTICE_LOG_FMT(DSPHLE, "Received a NOP command: {}", command);
      SendCommandAck(CommandAck::STANDARD, sync);
      break;

    case 0x03:
      // NOP on standard protocol but shouldn't ever happen on light protocol
      // since it's going directly back to the dispatcher with no ack.
      if (m_flags & LIGHT_PROTOCOL)
      {
        PanicAlertFmt("Received a 03 command on light protocol.");
        break;
      }
      SendCommandAck(CommandAck::STANDARD, sync);
      break;

    case 0x04:
    case 0x05:
    case 0x06:
    case 0x07:
    case 0x08:
    case 0x09:
      // Commands that crash the DAC UCode on non-light protocols. Log and
      // enter HALTED mode.
      //
      // TODO: These are not crashes on light protocol, however I've never seen
      // them used so far.
      NOTICE_LOG_FMT(DSPHLE, "Received a crashy command: {}", command);
      SetMailState(MailState::HALTED);
      return;

    // Command 01: Setup/initialization command. Provides the address to
    // voice parameter blocks (VPBs) as well as some array of coefficients
    // used for mixing.
    case 0x01:
    {
      m_rendering_voices_per_frame = extra_data;

      m_renderer.SetVPBBaseAddress(Read32());

      auto& memory = m_dsphle->GetSystem().GetMemory();
      u16* data_ptr = (u16*)HLEMemory_Get_Pointer(memory, Read32());

      std::array<s16, 0x100> resampling_coeffs;
      for (size_t i = 0; i < 0x100; ++i)
        resampling_coeffs[i] = Common::swap16(data_ptr[i]);
      m_renderer.SetResamplingCoeffs(std::move(resampling_coeffs));

      std::array<s16, 0x100> const_patterns;
      for (size_t i = 0; i < 0x100; ++i)
        const_patterns[i] = Common::swap16(data_ptr[0x100 + i]);
      m_renderer.SetConstPatterns(std::move(const_patterns));

      // The sine table is only used for Dolby mixing
      // which the light protocol doesn't support.
      if ((m_flags & LIGHT_PROTOCOL) == 0)
      {
        std::array<s16, 0x80> sine_table;
        for (size_t i = 0; i < 0x80; ++i)
          sine_table[i] = Common::swap16(data_ptr[0x200 + i]);
        m_renderer.SetSineTable(std::move(sine_table));
      }

      u16* afc_coeffs_ptr = (u16*)HLEMemory_Get_Pointer(memory, Read32());
      std::array<s16, 0x20> afc_coeffs;
      for (size_t i = 0; i < 0x20; ++i)
        afc_coeffs[i] = Common::swap16(afc_coeffs_ptr[i]);
      m_renderer.SetAfcCoeffs(std::move(afc_coeffs));

      m_renderer.SetReverbPBBaseAddress(Read32());

      SendCommandAck(CommandAck::STANDARD, sync);
      break;
    }

    // Command 02: starts audio processing. NOTE: this handler uses return,
    // not break. This is because it hijacks the mail control flow and
    // stops processing of further commands until audio processing is done.
    case 0x02:
      m_rendering_requested_frames = (cmd_mail >> 16) & 0xFF;
      m_renderer.SetOutputVolume(cmd_mail & 0xFFFF);
      m_renderer.SetOutputLeftBufferAddr(Read32());
      m_renderer.SetOutputRightBufferAddr(Read32());

      if (m_flags & COMBINED_CMD_0D)
      {
        // Ignore the two values which are equivalent to arguments passed to
        // command 0D.
        // Used by Pikmin 1 Wii.
        Read32();
        Read32();
      }

      m_rendering_curr_frame = 0;
      m_rendering_curr_voice = 0;

      if (m_flags & LIGHT_PROTOCOL)
      {
        SendCommandAck(CommandAck::STANDARD, m_rendering_requested_frames);
        SetMailState(MailState::RENDERING);
      }
      else
      {
        RenderAudio();
      }
      return;

    // Command 0C: used for multiple purpose depending on the UCode version:
    // * IPL NTSC, Luigi's Mansion: TODO (unknown as of now).
    // * Pikmin/AC: GBA crypto.
    // * SMS and onwards: NOP.
    case 0x0C:
      if (m_flags & SUPPORTS_GBA_CRYPTO)
      {
        ProcessGBACrypto(m_dsphle->GetSystem().GetMemory(), Read32());
      }
      else if (m_flags & WEIRD_CMD_0C)
      {
        // TODO
        NOTICE_LOG_FMT(DSPHLE, "Received an unhandled 0C command, params: {:08x} {:08x}", Read32(),
                       Read32());
      }
      else
      {
        WARN_LOG_FMT(DSPHLE, "Received a NOP 0C command. Flags={:08x}", m_flags);
      }
      SendCommandAck(CommandAck::STANDARD, sync);
      break;

    // Command 0D: TODO: find a name and implement.
    // Used by Wind Waker.
    case 0x0D:
      if (m_flags & NO_CMD_0D)
      {
        WARN_LOG_FMT(DSPHLE, "Received a 0D command which is NOP'd on this UCode.");
        SendCommandAck(CommandAck::STANDARD, sync);
        break;
      }

      WARN_LOG_FMT(DSPHLE, "CMD0D: {:08x}", Read32());
      SendCommandAck(CommandAck::STANDARD, sync);
      break;

    // Command 0E: Sets the base address of the ARAM for Wii UCodes. Used
    // because the Wii does not have an ARAM, so it simulates it with MRAM
    // and DMAs.
    case 0x0E:
      if ((m_flags & NO_ARAM) == 0)
        PanicAlertFmt("Setting base ARAM addr on non Wii DAC.");
      m_renderer.SetARAMBaseAddr(Read32());
      SendCommandAck(CommandAck::STANDARD, sync);
      break;

    default:
      NOTICE_LOG_FMT(DSPHLE, "Received a non-existing command ({}), halting.", command);
      SetMailState(MailState::HALTED);
      return;
    }
  }
}

void ZeldaUCode::SendCommandAck(CommandAck ack_type, u16 sync_value)
{
  if (m_flags & LIGHT_PROTOCOL)
  {
    // The light protocol uses the address of the command handler in the
    // DSP code instead of the command id... go figure.
    // FIXME: LLE returns a different value
    sync_value = 2 * ((sync_value >> 8) & 0x7F) + 0x62;
    m_mail_handler.PushMail(0x80000000 | sync_value);
  }
  else
  {
    u32 ack_mail = 0;
    switch (ack_type)
    {
    case CommandAck::STANDARD:
      ack_mail = DSP_SYNC;
      break;
    case CommandAck::DONE_RENDERING:
      ack_mail = DSP_FRAME_END;
      break;
    }
    m_mail_handler.PushMail(ack_mail, true);

    if (ack_type == CommandAck::STANDARD)
      m_mail_handler.PushMail(0xF3550000 | sync_value);
  }
}

void ZeldaUCode::RenderAudio()
{
  if (!RenderingInProgress())
  {
    WARN_LOG_FMT(DSPHLE, "Trying to render audio while no rendering should be happening.");
    return;
  }

  while (m_rendering_curr_frame < m_rendering_requested_frames)
  {
    if (m_rendering_curr_voice == 0)
      m_renderer.PrepareFrame();

    while (m_rendering_curr_voice < m_rendering_voices_per_frame)
    {
      // If we are not meant to render this voice yet, go back to message
      // processing.
      if (m_rendering_curr_voice >= m_sync_max_voice_id)
        return;

      // Test the sync flag for this voice, skip it if not set.
      u16 flags = m_sync_voice_skip_flags[m_rendering_curr_voice >> 4];
      u8 bit = 0xF - (m_rendering_curr_voice & 0xF);
      if (flags & (1 << bit))
        m_renderer.AddVoice(m_rendering_curr_voice);

      m_rendering_curr_voice++;
    }

    if (!(m_flags & LIGHT_PROTOCOL))
      SendCommandAck(CommandAck::STANDARD, 0xFF00 | m_rendering_curr_frame);

    m_renderer.FinalizeFrame();

    m_rendering_curr_voice = 0;
    m_sync_max_voice_id = 0;
    m_rendering_curr_frame++;
  }

  if (!(m_flags & LIGHT_PROTOCOL))
  {
    SendCommandAck(CommandAck::DONE_RENDERING, 0);
    m_cmd_can_execute = false;  // Block command execution until ACK is received.
  }
  else
  {
    SetMailState(MailState::WAITING);
  }
}

// Utility to define 32 bit accessors/modifiers methods based on two 16 bit
// fields named _l and _h.
#define DEFINE_32BIT_ACCESSOR(field_name, name)                                                    \
  u32 Get##name() const { return (field_name##_h << 16) | field_name##_l; }                        \
  void Set##name(u32 v)                                                                            \
  {                                                                                                \
    field_name##_h = v >> 16;                                                                      \
    field_name##_l = v & 0xFFFF;                                                                   \
  }

#pragma pack(push, 1)
struct ZeldaAudioRenderer::VPB
{
  // If zero, skip processing this voice.
  u16 enabled;

  // If non zero, skip processing this voice.
  u16 done;

  // In 4.12 format. 1.0 (0x1000) means 0x50 raw samples from RAM/accelerator
  // will be "resampled" to 0x50 input samples. 2.0 (0x2000) means 2 raw
  // samples for one input samples. 0.5 (0x800) means one raw sample for 2
  // input samples.
  u16 resampling_ratio;

  u16 unk_03;

  // If non zero, reset some value in the VPB when processing it.
  u16 reset_vpb;

  // If non zero, tells PCM8/PCM16 sample sources that the end of the voice
  // has been reached and looping should be considered if enabled.
  u16 end_reached;

  // If non zero, input samples to this VPB will be the fixed value from
  // VPB[33] (constant_sample_value). This is used when a voice is being
  // terminated in order to force silence.
  u16 use_constant_sample;

  // Number of samples that should be saved in the VPB for processing during
  // future frames. Should be at most TODO.
  u16 samples_to_keep_count;

  // Channel mixing information. Each voice can be mixed to 6 different
  // channels, with separate volume information.
  //
  // Used only if VPB[2C] (use_dolby_volume) is not set. Otherwise, the
  // values from VPB[0x20:0x2C] are used to mix to all available channels.
  struct Channel
  {
    // Can be treated as an ID, but in the real world this is actually the
    // address in DRAM of a DSP buffer. The game passes that information to
    // the DSP, which means the game must know the memory layout of the DSP
    // UCode... that's terrible.
    u16 id;

    s16 target_volume;
    s16 current_volume;

    u16 unk;
  };
  Channel channels[6];

  u16 unk_20_28[0x8];

  // When using Dolby voice mixing (see VPB[2C] use_dolby_volume), the X
  // (left/right) and Y (front/back) coordinates of the sound. 0x00 is all
  // right/back, 0x7F is all left/front. Format is 0XXXXXXX0YYYYYYY.
  u16 dolby_voice_position;
  u8 GetDolbyVoiceX() const { return (dolby_voice_position >> 8) & 0x7F; }
  u8 GetDolbyVoiceY() const { return dolby_voice_position & 0x7F; }
  // How much reverbation to apply to the Dolby mixed voice. 0 is none,
  // 0x7FFF is the maximum value.
  s16 dolby_reverb_factor;

  // The volume for the 0x50 samples being mixed will ramp between current
  // and target. After the ramping is done, the current value is updated (to
  // match target, usually).
  s16 dolby_volume_current;
  s16 dolby_volume_target;

  // If non zero, use positional audio mixing. Instead of using the channels
  // information, use the 4 Dolby related VPB fields defined above.
  u16 use_dolby_volume;

  u16 unk_2D;
  u16 unk_2E;
  u16 unk_2F;

  // Fractional part of the current sample position, in 0.12 format (all
  // decimal part, 0x0800 = 0.5). The 4 top bits are unused.
  u16 current_pos_frac;

  u16 unk_31;

  // Number of remaining decoded AFC samples in the VPB internal buffer (see
  // VPB[0x58]) that haven't been output yet.
  u16 afc_remaining_decoded_samples;

  // Value used as the constant sample value if VPB[6] (use_constant_sample)
  // is set. Reset to the last sample value after each round of resampling.
  s16 constant_sample;

  // Current position in the voice. Not needed for accelerator based voice
  // types since the accelerator exposes a streaming based interface, but DMA
  // based voice types (PCM16_FROM_MRAM for example) require it to know where
  // to seek in the MRAM buffer.
  u16 current_position_h;
  u16 current_position_l;
  DEFINE_32BIT_ACCESSOR(current_position, CurrentPosition)

  // Number of samples that will be processed before the loop point of the
  // voice is reached. Maintained by the UCode and used by the game to
  // schedule some parameters updates.
  u16 samples_before_loop;

  u16 unk_37;

  // Current address used to stream samples for the ARAM sample source types.
  u16 current_aram_addr_h;
  u16 current_aram_addr_l;
  DEFINE_32BIT_ACCESSOR(current_aram_addr, CurrentARAMAddr)

  // Remaining number of samples to load before considering the voice
  // rendering complete and setting the done flag. Note that this is an
  // absolute value that does not take into account loops. If a loop of 100
  // samples is played 4 times, remaining_length will have decreased by 400.
  u16 remaining_length_h;
  u16 remaining_length_l;
  DEFINE_32BIT_ACCESSOR(remaining_length, RemainingLength)

  // Stores the last 4 resampled input samples after each frame, so that they
  // can be used for future linear interpolation.
  s16 resample_buffer[4];

  // TODO: document and implement.
  s16 prev_input_samples[0x18];

  // Values from the last decoded AFC block. The last two values are
  // especially important since AFC decoding - as a variant of ADPCM -
  // requires the two latest sample values to be able to decode future
  // samples.
  s16 afc_remaining_samples[0x10];
  s16* AFCYN2() { return &afc_remaining_samples[0xE]; }
  s16* AFCYN1() { return &afc_remaining_samples[0xF]; }
  u16 unk_68_80[0x80 - 0x68];

  enum SamplesSourceType
  {
    // Simple square wave at 50% amplitude and frequency controlled via the
    // resampling ratio.
    SRC_SQUARE_WAVE = 0,
    // Simple saw wave at 100% amplitude and frequency controlled via the
    // resampling ratio.
    SRC_SAW_WAVE = 1,
    // Same "square" wave as SRC_SQUARE_WAVE but using a 0.25 duty cycle
    // instead of 0.5.
    SRC_SQUARE_WAVE_25PCT = 3,
    // Breaking the numerical ordering for these, but they are all related.
    // Simple pattern stored in the data downloaded by command 01. Playback
    // frequency is controlled by the resampling ratio.
    SRC_CONST_PATTERN_0 = 7,
    SRC_CONST_PATTERN_0_VARIABLE_STEP = 10,
    SRC_CONST_PATTERN_1 = 4,
    SRC_CONST_PATTERN_2 = 11,
    SRC_CONST_PATTERN_3 = 12,
    // Samples stored in ARAM at a rate of 16 samples/5 bytes, AFC encoded,
    // at an arbitrary sample rate (resampling is applied).
    SRC_AFC_LQ_FROM_ARAM = 5,
    // Samples stored in ARAM in PCM8 format, at an arbitrary sampling rate
    // (resampling is applied).
    SRC_PCM8_FROM_ARAM = 8,
    // Samples stored in ARAM at a rate of 16 samples/9 bytes, AFC encoded,
    // at an arbitrary sample rate (resampling is applied).
    SRC_AFC_HQ_FROM_ARAM = 9,
    // Samples stored in ARAM in PCM16 format, at an arbitrary sampling
    // rate (resampling is applied).
    SRC_PCM16_FROM_ARAM = 16,
    // Samples stored in MRAM at an arbitrary sample rate (resampling is
    // applied, unlike PCM16_FROM_MRAM_RAW).
    SRC_PCM16_FROM_MRAM = 33,
    // TODO: 2, 6
  };
  u16 samples_source_type;

  // If non zero, indicates that the sound should loop.
  u16 is_looping;

  // For AFC looping voices, the values of the last 2 samples before the
  // start of the loop, in order to be able to decode samples after looping.
  s16 loop_yn1;
  s16 loop_yn2;

  u16 unk_84;

  // If true, ramp down quickly to a volume of zero, and end the voice (by
  // setting VPB[1] done) when it reaches zero.
  u16 end_requested;

  u16 unk_86;
  u16 unk_87;

  // Base address used to download samples data after the loop point of the
  // voice has been reached.
  u16 loop_address_h;
  u16 loop_address_l;
  DEFINE_32BIT_ACCESSOR(loop_address, LoopAddress)

  // Offset (in number of raw samples) of the start of the loop area in the
  // voice. Note: some sample sources only use the _h part of this.
  //
  // TODO: rename to length? confusion with remaining_length...
  u16 loop_start_position_h;
  u16 loop_start_position_l;
  DEFINE_32BIT_ACCESSOR(loop_start_position, LoopStartPosition)

  // Base address used to download samples data before the loop point of the
  // voice has been reached.
  u16 base_address_h;
  u16 base_address_l;
  DEFINE_32BIT_ACCESSOR(base_address, BaseAddress)

  u16 padding[0xC0];

  // These next two functions are terrible hacks used in order to support two
  // different VPB sizes.

  // Transforms from an NTSC-IPL type 0x80-sized VPB to a full size VPB.
  void Uncompress()
  {
    u16* words = (u16*)this;
    // RO part of the VPB is from 0x40-0x80 instead of 0x80-0xC0.
    for (int i = 0; i < 0x40; ++i)
    {
      words[0x80 + i] = words[0x40 + i];
      words[0x40 + i] = 0;
    }
    // AFC decoded samples are offset by 0x28.
    for (int i = 0; i < 0x10; ++i)
    {
      words[0x58 + i] = words[0x30 + i];
      words[0x30 + i] = 0;
    }
    // Most things are offset by 0x18 because no Dolby mixing.
    for (int i = 0; i < 0x18; ++i)
    {
      words[0x30 + i] = words[0x18 + i];
      words[0x18 + i] = 0;
    }
  }

  // Transforms from a full size VPB to an NTSC-IPL 0x80-sized VPB.
  void Compress()
  {
    u16* words = (u16*)this;
    for (int i = 0; i < 0x18; ++i)
    {
      words[0x18 + i] = words[0x30 + i];
      words[0x30 + i] = 0;
    }
    for (int i = 0; i < 0x10; ++i)
    {
      words[0x30 + i] = words[0x58 + i];
      words[0x58 + i] = 0;
    }
    for (int i = 0; i < 0x40; ++i)
    {
      words[0x40 + i] = words[0x80 + i];
      words[0x80 + i] = 0;
    }
  }
};

struct ReverbPB
{
  // If zero, skip this reverb PB.
  u16 enabled;
  // Size of the circular buffer in MRAM, expressed in number of 0x50 samples
  // blocks (0xA0 bytes).
  u16 circular_buffer_size;
  // Base address of the circular buffer in MRAM.
  u16 circular_buffer_base_h;
  u16 circular_buffer_base_l;
  DEFINE_32BIT_ACCESSOR(circular_buffer_base, CircularBufferBase)

  struct Destination
  {
    u16 buffer_id;  // See VPB::Channel::id.
    u16 volume;     // 1.15 format.
  };
  Destination dest[2];

  // Coefficients for an 8-tap filter applied to each reverb buffer before
  // adding its data to the destination.
  s16 filter_coeffs[8];
};
#pragma pack(pop)

ZeldaAudioRenderer::ZeldaAudioRenderer(Core::System& system) : m_system(system)
{
}

ZeldaAudioRenderer::~ZeldaAudioRenderer() = default;

void ZeldaAudioRenderer::PrepareFrame()
{
  if (m_prepared)
    return;

  m_buf_front_left.fill(0);
  m_buf_front_right.fill(0);

  ApplyVolumeInPlace_1_15(&m_buf_back_left, 0x6784);
  ApplyVolumeInPlace_1_15(&m_buf_back_right, 0x6784);

// TODO: Back left and back right should have a filter applied to them,
// then get mixed into front left and front right. In practice, TWW never
// uses this AFAICT. PanicAlert to help me find places that use this.
#ifdef STRICT_ZELDA_HLE
  if (!(m_flags & LIGHT_PROTOCOL) && (m_buf_back_left[0] != 0 || m_buf_back_right[0] != 0))
    PanicAlertFmt("Zelda HLE using back mixing buffers");
#endif

  // Add reverb data from previous frame.
  ApplyReverb(false);
  AddBuffersWithVolume(m_buf_front_left_reverb.data(), m_buf_back_left_reverb.data(), 0x50, 0x7FFF);
  AddBuffersWithVolume(m_buf_front_right_reverb.data(), m_buf_back_left_reverb.data(), 0x50,
                       0xB820);
  AddBuffersWithVolume(m_buf_front_left_reverb.data(), m_buf_back_right_reverb.data() + 0x28, 0x28,
                       0xB820);
  AddBuffersWithVolume(m_buf_front_right_reverb.data(), m_buf_back_right_reverb.data() + 0x28, 0x28,
                       0x7FFF);
  m_buf_back_left_reverb.fill(0);
  m_buf_back_right_reverb.fill(0);

  // Prepare patterns 2/3 - they are not constant unlike 0/1.
  s16* pattern2 = m_const_patterns.data() + 2 * 0x40;
  s32 yn2 = pattern2[0x40 - 2], yn1 = pattern2[0x40 - 1], v;
  for (int i = 0; i < 0x40; i += 2)
  {
    v = yn2 * yn1 - (pattern2[i] << 16);
    yn2 = yn1;
    yn1 = pattern2[i];
    pattern2[i] = v >> 16;

    v = 2 * (yn2 * yn1 + (pattern2[i + 1] << 16));
    yn2 = yn1;
    yn1 = pattern2[i + 1];
    pattern2[i + 1] = v >> 16;
  }
  s16* pattern3 = m_const_patterns.data() + 3 * 0x40;
  yn2 = pattern3[0x40 - 2];
  yn1 = pattern3[0x40 - 1];
  s16 acc = yn1;
  s16 step = pattern3[0] + ((yn1 * yn2 + ((yn2 << 16) + yn1)) >> 16);
  step = (step & 0x1FF) | 0x2000;
  for (s32 i = 0; i < 0x40; ++i)
    pattern3[i] = acc + (i + 1) * step;

  m_prepared = true;
}

void ZeldaAudioRenderer::ApplyReverb(bool post_rendering)
{
  if (!m_reverb_pb_base_addr)
  {
#ifdef STRICT_ZELDA_HLE
    PanicAlertFmt("Trying to apply reverb without available parameters.");
#endif
    return;
  }

  // Each of the 4 RPBs maps to one of these buffers.
  MixingBuffer* reverb_buffers[4] = {
      &m_buf_unk0_reverb,
      &m_buf_unk1_reverb,
      &m_buf_front_left_reverb,
      &m_buf_front_right_reverb,
  };
  std::array<s16, 8>* last8_samples_buffers[4] = {
      &m_buf_unk0_reverb_last8,
      &m_buf_unk1_reverb_last8,
      &m_buf_front_left_reverb_last8,
      &m_buf_front_right_reverb_last8,
  };

  auto& memory = m_system.GetMemory();
  u16* rpb_base_ptr = (u16*)HLEMemory_Get_Pointer(memory, m_reverb_pb_base_addr);
  for (u16 rpb_idx = 0; rpb_idx < 4; ++rpb_idx)
  {
    ReverbPB rpb;
    u16* rpb_raw_ptr = reinterpret_cast<u16*>(&rpb);
    for (size_t i = 0; i < sizeof(ReverbPB) / 2; ++i)
      rpb_raw_ptr[i] = Common::swap16(rpb_base_ptr[rpb_idx * sizeof(ReverbPB) / 2 + i]);

    if (!rpb.enabled)
      continue;

    u16 mram_buffer_idx = m_reverb_pb_frames_count[rpb_idx];

    u32 mram_addr = rpb.GetCircularBufferBase() + mram_buffer_idx * 0x50 * sizeof(s16);
    s16* mram_ptr = (s16*)HLEMemory_Get_Pointer(memory, mram_addr);

    if (!post_rendering)
    {
      // 8 more samples because of the filter order. The first 8 samples
      // are the last 8 samples of the previous frame.
      std::array<s16, 0x58> buffer;
      for (u16 i = 0; i < 8; ++i)
        buffer[i] = (*last8_samples_buffers[rpb_idx])[i];

      for (u16 i = 0; i < 0x50; ++i)
        buffer[8 + i] = Common::swap16(mram_ptr[i]);

      for (u16 i = 0; i < 8; ++i)
        (*last8_samples_buffers[rpb_idx])[i] = buffer[0x50 + i];

      auto ApplyFilter = [&]() {
        // Filter the buffer using provided coefficients.
        for (u16 i = 0; i < 0x50; ++i)
        {
          s32 sample = 0;
          for (u16 j = 0; j < 8; ++j)
            sample += (s32)buffer[i + j] * rpb.filter_coeffs[j];
          sample >>= 15;
          buffer[i] = std::clamp(sample, -0x8000, 0x7FFF);
        }
      };

      // LSB set -> pre-filtering.
      if (rpb.enabled & 1)
        ApplyFilter();

      for (const auto& dest : rpb.dest)
      {
        if (dest.buffer_id == 0)
          continue;

        MixingBuffer* dest_buffer = BufferForID(dest.buffer_id);
        if (!dest_buffer)
        {
#ifdef STRICT_ZELDA_HLE
          PanicAlertFmt("RPB mixing to an unknown buffer: {:04x}", dest.buffer_id);
#endif
          continue;
        }
        AddBuffersWithVolume(dest_buffer->data(), buffer.data(), 0x50, dest.volume);
      }

      // LSB not set, bit 1 set -> post-filtering.
      if (rpb.enabled & 2)
        ApplyFilter();

      for (u16 i = 0; i < 0x50; ++i)
        (*reverb_buffers[rpb_idx])[i] = buffer[i];
    }
    else
    {
      MixingBuffer* buffer = reverb_buffers[rpb_idx];

      // Upload the reverb data to RAM.
      for (auto sample : *buffer)
        *mram_ptr++ = Common::swap16(sample);

      mram_buffer_idx = (mram_buffer_idx + 1) % rpb.circular_buffer_size;
      m_reverb_pb_frames_count[rpb_idx] = mram_buffer_idx;
    }
  }
}

ZeldaAudioRenderer::MixingBuffer* ZeldaAudioRenderer::BufferForID(u16 buffer_id)
{
  switch (buffer_id)
  {
  case 0x0D00:
    return &m_buf_front_left;
  case 0x0D60:
    return &m_buf_front_right;
  case 0x0F40:
    return &m_buf_back_left;
  case 0x0CA0:
    return &m_buf_back_right;
  case 0x0E80:
    return &m_buf_front_left_reverb;
  case 0x0EE0:
    return &m_buf_front_right_reverb;
  case 0x0C00:
    return &m_buf_back_left_reverb;
  case 0x0C50:
    return &m_buf_back_right_reverb;
  case 0x0DC0:
    return &m_buf_unk0_reverb;
  case 0x0E20:
    return &m_buf_unk1_reverb;
  case 0x09A0:
    return &m_buf_unk0;  // Used by the GC IPL as a reverb dest.
  case 0x0FA0:
    return &m_buf_unk1;  // Used by the GC IPL as a mixing dest.
  case 0x0B00:
    return &m_buf_unk2;  // Used by Pikmin 1 as a mixing dest.
  default:
    return nullptr;
  }
}

void ZeldaAudioRenderer::AddVoice(u16 voice_id)
{
  VPB vpb;
  FetchVPB(voice_id, &vpb);

  if (!vpb.enabled || vpb.done)
    return;

  MixingBuffer input_samples;
  LoadInputSamples(&input_samples, &vpb);

  // TODO: In place effects.

  // TODO: IIR filter.

  if (vpb.use_dolby_volume)
  {
    if (vpb.end_requested)
    {
      vpb.dolby_volume_target = vpb.dolby_volume_current / 2;
      if (vpb.dolby_volume_target == 0)
        vpb.done = true;
    }

    // Each of these volumes is in 1.15 fixed format.
    s16 right_volume = m_sine_table[vpb.GetDolbyVoiceX()];
    s16 back_volume = m_sine_table[vpb.GetDolbyVoiceY()];
    s16 left_volume = m_sine_table[vpb.GetDolbyVoiceX() ^ 0x7F];
    s16 front_volume = m_sine_table[vpb.GetDolbyVoiceY() ^ 0x7F];

    // Compute volume for each quadrant.
    u16 shift_factor = (m_flags & MAKE_DOLBY_LOUDER) ? 15 : 16;
    s16 quadrant_volumes[4] = {
        (s16)((left_volume * front_volume) >> shift_factor),
        (s16)((left_volume * back_volume) >> shift_factor),
        (s16)((right_volume * front_volume) >> shift_factor),
        (s16)((right_volume * back_volume) >> shift_factor),
    };

    // Compute the volume delta for each sample to match the difference
    // between current and target volume.
    s16 delta = vpb.dolby_volume_target - vpb.dolby_volume_current;
    s16 volume_deltas[4];
    for (size_t i = 0; i < 4; ++i)
      volume_deltas[i] = ((u16)quadrant_volumes[i] * delta) >> shift_factor;

    // Apply master volume to each quadrant.
    for (s16& quadrant_volume : quadrant_volumes)
      quadrant_volume = (quadrant_volume * vpb.dolby_volume_current) >> shift_factor;

    // Compute reverb volume and ramp deltas.
    s16 reverb_volumes[4], reverb_volume_deltas[4];
    for (size_t i = 0; i < 4; ++i)
    {
      reverb_volumes[i] = (quadrant_volumes[i] * vpb.dolby_reverb_factor) >> shift_factor;
      reverb_volume_deltas[i] = (volume_deltas[i] * vpb.dolby_reverb_factor) >> shift_factor;
    }

    struct
    {
      MixingBuffer* buffer;
      s16 volume;
      s16 volume_delta;
    } buffers[8] = {
        {&m_buf_front_left, quadrant_volumes[0], volume_deltas[0]},
        {&m_buf_back_left, quadrant_volumes[1], volume_deltas[1]},
        {&m_buf_front_right, quadrant_volumes[2], volume_deltas[2]},
        {&m_buf_back_right, quadrant_volumes[3], volume_deltas[3]},

        {&m_buf_front_left_reverb, reverb_volumes[0], reverb_volume_deltas[0]},
        {&m_buf_back_left_reverb, reverb_volumes[1], reverb_volume_deltas[1]},
        {&m_buf_front_right_reverb, reverb_volumes[2], reverb_volume_deltas[2]},
        {&m_buf_back_right_reverb, reverb_volumes[3], reverb_volume_deltas[3]},
    };
    for (const auto& buffer : buffers)
    {
      AddBuffersWithVolumeRamp(buffer.buffer, input_samples, buffer.volume << 16,
                               (buffer.volume_delta << 16) / (s32)buffer.buffer->size());
    }

    vpb.dolby_volume_current = vpb.dolby_volume_target;
  }
  else
  {
    // TODO: Store input samples if requested by the VPB.

    int num_channels = (m_flags & FOUR_MIXING_DESTS) ? 4 : 6;
    if (vpb.end_requested)
    {
      bool all_mute = true;
      for (int i = 0; i < num_channels; ++i)
      {
        vpb.channels[i].target_volume = vpb.channels[i].current_volume / 2;
        all_mute &= (vpb.channels[i].target_volume == 0);
      }
      if (all_mute)
        vpb.done = true;
    }

    for (int i = 0; i < num_channels; ++i)
    {
      if (!vpb.channels[i].id)
        continue;

      // Some UCode versions provide the delta directly instead of
      // providing a target volume.
      s16 volume_delta;
      if (m_flags & VOLUME_EXPLICIT_STEP)
        volume_delta = vpb.channels[i].target_volume;
      else
        volume_delta = vpb.channels[i].target_volume - vpb.channels[i].current_volume;

      s32 volume_step = (volume_delta << 16) / (s32)input_samples.size();  // In 1.31 format.

      // TODO: The last value of each channel structure is used to
      // determine whether a channel should be skipped or not. Not
      // implemented yet.

      if (!vpb.channels[i].current_volume && !volume_step)
        continue;

      MixingBuffer* dst_buffer = BufferForID(vpb.channels[i].id);
      if (!dst_buffer)
      {
#ifdef STRICT_ZELDA_HLE
        PanicAlertFmt("Mixing to an unmapped buffer: {:04x}", vpb.channels[i].id);
#endif
        continue;
      }

      s32 new_volume = AddBuffersWithVolumeRamp(dst_buffer, input_samples,
                                                vpb.channels[i].current_volume << 16, volume_step);
      vpb.channels[i].current_volume = new_volume >> 16;
    }
  }

  // By then the VPB has been reset, unless we're in the "constant sample" /
  // silence mode.
  if (!vpb.use_constant_sample)
    vpb.reset_vpb = false;

  StoreVPB(voice_id, &vpb);
}

void ZeldaAudioRenderer::FinalizeFrame()
{
  // TODO: Dolby mixing.

  ApplyVolumeInPlace_4_12(&m_buf_front_left, m_output_volume);
  ApplyVolumeInPlace_4_12(&m_buf_front_right, m_output_volume);

  auto& memory = m_system.GetMemory();
  u16* ram_left_buffer = (u16*)HLEMemory_Get_Pointer(memory, m_output_lbuf_addr);
  u16* ram_right_buffer = (u16*)HLEMemory_Get_Pointer(memory, m_output_rbuf_addr);
  for (size_t i = 0; i < m_buf_front_left.size(); ++i)
  {
    ram_left_buffer[i] = Common::swap16(m_buf_front_left[i]);
    ram_right_buffer[i] = Common::swap16(m_buf_front_right[i]);
  }
  m_output_lbuf_addr += sizeof(u16) * (u32)m_buf_front_left.size();
  m_output_rbuf_addr += sizeof(u16) * (u32)m_buf_front_right.size();

  // TODO: Some more Dolby mixing.

  ApplyReverb(true);

  m_prepared = false;
}

void ZeldaAudioRenderer::FetchVPB(u16 voice_id, VPB* vpb)
{
  auto& memory = m_system.GetMemory();
  u16* vpb_words = (u16*)vpb;
  u16* ram_vpbs = (u16*)HLEMemory_Get_Pointer(memory, m_vpb_base_addr);

  // A few versions of the UCode have VPB of size 0x80 (vs. the standard
  // 0xC0). The whole 0x40-0x80 part is gone. Handle that by moving things
  // around.
  size_t vpb_size = (m_flags & TINY_VPB) ? 0x80 : 0xC0;

  size_t base_idx = voice_id * vpb_size;
  for (size_t i = 0; i < vpb_size; ++i)
    vpb_words[i] = Common::swap16(ram_vpbs[base_idx + i]);

  if (m_flags & TINY_VPB)
    vpb->Uncompress();
}

void ZeldaAudioRenderer::StoreVPB(u16 voice_id, VPB* vpb)
{
  auto& memory = m_system.GetMemory();
  u16* vpb_words = (u16*)vpb;
  u16* ram_vpbs = (u16*)HLEMemory_Get_Pointer(memory, m_vpb_base_addr);

  size_t vpb_size = (m_flags & TINY_VPB) ? 0x80 : 0xC0;
  size_t base_idx = voice_id * vpb_size;

  if (m_flags & TINY_VPB)
    vpb->Compress();

  // Only the first 0x80 words are transferred back - the rest is read-only.
  for (size_t i = 0; i < vpb_size - 0x40; ++i)
    ram_vpbs[base_idx + i] = Common::swap16(vpb_words[i]);
}

void ZeldaAudioRenderer::LoadInputSamples(MixingBuffer* buffer, VPB* vpb)
{
  // Input data pre-resampling. Resampled into the mixing buffer parameter at
  // the end of processing, if needed.
  //
  // Maximum of 0x500 samples here - see NeededRawSamplesCount to understand
  // this practical limit (resampling_ratio = 0xFFFF -> 0x500 samples). Add a
  // margin of 4 that is needed for samples source that do resampling.
  std::array<s16, 0x500 + 4> raw_input_samples;
  for (size_t i = 0; i < 4; ++i)
    raw_input_samples[i] = vpb->resample_buffer[i];

  if (vpb->use_constant_sample)
  {
    buffer->fill(vpb->constant_sample);
    return;
  }

  switch (vpb->samples_source_type)
  {
  case VPB::SRC_SQUARE_WAVE:
  case VPB::SRC_SQUARE_WAVE_25PCT:
  {
    u32 shift;
    if (vpb->samples_source_type == VPB::SRC_SQUARE_WAVE)
      shift = 1;
    else
      shift = 2;
    u32 mask = (1 << shift) - 1;

    u32 pos = vpb->current_pos_frac << shift;
    for (s16& sample : *buffer)
    {
      sample = ((pos >> 16) & mask) ? 0xC000 : 0x4000;
      pos += vpb->resampling_ratio;
    }
    vpb->current_pos_frac = (pos >> shift) & 0xFFFF;
    break;
  }

  case VPB::SRC_SAW_WAVE:
  {
    u32 pos = vpb->current_pos_frac;
    for (s16& sample : *buffer)
    {
      sample = pos & 0xFFFF;
      pos += (vpb->resampling_ratio) >> 1;
    }
    vpb->current_pos_frac = pos & 0xFFFF;
    break;
  }

  case VPB::SRC_CONST_PATTERN_0:
  case VPB::SRC_CONST_PATTERN_0_VARIABLE_STEP:
  case VPB::SRC_CONST_PATTERN_1:
  case VPB::SRC_CONST_PATTERN_2:
  case VPB::SRC_CONST_PATTERN_3:
  {
    const u16 PATTERN_SIZE = 0x40;

    struct PatternInfo
    {
      u16 idx;
      bool variable_step;
    };
    std::map<u16, PatternInfo> samples_source_to_pattern = {
        {VPB::SRC_CONST_PATTERN_0, {0, false}}, {VPB::SRC_CONST_PATTERN_0_VARIABLE_STEP, {0, true}},
        {VPB::SRC_CONST_PATTERN_1, {1, false}}, {VPB::SRC_CONST_PATTERN_2, {2, false}},
        {VPB::SRC_CONST_PATTERN_3, {3, false}},
    };
    auto& pattern_info = samples_source_to_pattern[vpb->samples_source_type];
    u16 pattern_offset = pattern_info.idx * PATTERN_SIZE;
    s16* pattern = m_const_patterns.data() + pattern_offset;

    u32 pos = vpb->current_pos_frac << 6;   // log2(PATTERN_SIZE)
    u32 step = vpb->resampling_ratio << 5;  // FIXME: ucode 24B22038 shifts by 6 (?)

    for (size_t i = 0; i < buffer->size(); ++i)
    {
      (*buffer)[i] = pattern[pos >> 16];
      pos = (pos + step) % (PATTERN_SIZE << 16);
      if (pattern_info.variable_step)
        pos = ((pos << 10) + m_buf_back_right[i] * vpb->resampling_ratio) >> 10;
    }

    vpb->current_pos_frac = pos >> 6;
    break;
  }

  case VPB::SRC_PCM8_FROM_ARAM:
    DownloadPCMSamplesFromARAM<s8>(raw_input_samples.data() + 4, vpb, NeededRawSamplesCount(*vpb));
    Resample(vpb, raw_input_samples.data(), buffer);
    break;

  case VPB::SRC_AFC_HQ_FROM_ARAM:
  case VPB::SRC_AFC_LQ_FROM_ARAM:
    DownloadAFCSamplesFromARAM(raw_input_samples.data() + 4, vpb, NeededRawSamplesCount(*vpb));
    Resample(vpb, raw_input_samples.data(), buffer);
    break;

  case VPB::SRC_PCM16_FROM_ARAM:
    DownloadPCMSamplesFromARAM<s16>(raw_input_samples.data() + 4, vpb, NeededRawSamplesCount(*vpb));
    Resample(vpb, raw_input_samples.data(), buffer);
    break;

  case VPB::SRC_PCM16_FROM_MRAM:
    DownloadRawSamplesFromMRAM(raw_input_samples.data() + 4, vpb, NeededRawSamplesCount(*vpb));
    Resample(vpb, raw_input_samples.data(), buffer);
    break;

  default:
    PanicAlertFmt("Using an unknown/unimplemented sample source: {:04x}", vpb->samples_source_type);
    buffer->fill(0);
    return;
  }
}

u16 ZeldaAudioRenderer::NeededRawSamplesCount(const VPB& vpb)
{
  // Both of these are 4.12 fixed point, so shift by 12 to get the int part.
  return (vpb.current_pos_frac + 0x50 * vpb.resampling_ratio) >> 12;
}

void ZeldaAudioRenderer::Resample(VPB* vpb, const s16* src, MixingBuffer* dst)
{
  // Both in 20.12 format.
  u32 ratio = vpb->resampling_ratio;
  u32 pos = vpb->current_pos_frac;

  // Check if we need to do some interpolation. If the resampling ratio is
  // more than 4:1, it's not worth it.
  if ((ratio >> 12) >= 4)
  {
    for (s16& dst_sample : *dst)
    {
      pos += ratio;
      dst_sample = src[pos >> 12];
    }
  }
  else
  {
    for (auto& dst_sample : *dst)
    {
      // We have 0x40 * 4 coeffs that need to be selected based on the
      // most significant bits of the fractional part of the position. 12
      // bits >> 6 = 6 bits = 0x40. Multiply by 4 since there are 4
      // consecutive coeffs.
      u32 coeffs_idx = ((pos & 0xFFF) >> 6) * 4;
      const s16* coeffs = &m_resampling_coeffs[coeffs_idx];
      const s16* input = &src[pos >> 12];

      s64 dst_sample_unclamped = 0;
      for (size_t i = 0; i < 4; ++i)
        dst_sample_unclamped += (s64)2 * coeffs[i] * input[i];
      dst_sample_unclamped >>= 16;

      dst_sample = (s16)std::clamp<s64>(dst_sample_unclamped, -0x8000, 0x7FFF);

      pos += ratio;
    }
  }

  for (u32 i = 0; i < 4; ++i)
    vpb->resample_buffer[i] = src[(pos >> 12) + i];
  vpb->constant_sample = (*dst)[dst->size() - 1];
  vpb->current_pos_frac = pos & 0xFFF;
}

void* ZeldaAudioRenderer::GetARAMPtr(u32 offset) const
{
  if (m_system.IsWii())
    return HLEMemory_Get_Pointer(m_system.GetMemory(), m_aram_base_addr + offset);
  else
    return reinterpret_cast<u8*>(m_system.GetDSP().GetARAMPtr()) + offset;
}

template <typename T>
void ZeldaAudioRenderer::DownloadPCMSamplesFromARAM(s16* dst, VPB* vpb, u16 requested_samples_count)
{
  if (vpb->done)
  {
    for (u16 i = 0; i < requested_samples_count; ++i)
      dst[i] = 0;
    return;
  }

  if (vpb->reset_vpb)
  {
    vpb->SetRemainingLength(vpb->GetLoopStartPosition() - vpb->GetCurrentPosition());
    vpb->SetCurrentARAMAddr(vpb->GetBaseAddress() + vpb->GetCurrentPosition() * sizeof(T));
  }

  vpb->end_reached = false;
  while (requested_samples_count)
  {
    if (vpb->end_reached)
    {
      vpb->end_reached = false;
      if (!vpb->is_looping)
      {
        for (u16 i = 0; i < requested_samples_count; ++i)
          dst[i] = 0;
        vpb->done = true;
        break;
      }
      vpb->SetCurrentPosition(vpb->GetLoopAddress());
      vpb->SetRemainingLength(vpb->GetLoopStartPosition() - vpb->GetCurrentPosition());
      vpb->SetCurrentARAMAddr(vpb->GetBaseAddress() + vpb->GetCurrentPosition() * sizeof(T));
    }

    T* src_ptr = (T*)GetARAMPtr(vpb->GetCurrentARAMAddr());
    u16 samples_to_download = std::min(vpb->GetRemainingLength(), (u32)requested_samples_count);

    for (u16 i = 0; i < samples_to_download; ++i)
      *dst++ = Common::FromBigEndian<T>(*src_ptr++) << (16 - 8 * sizeof(T));

    vpb->SetRemainingLength(vpb->GetRemainingLength() - samples_to_download);
    vpb->SetCurrentARAMAddr(vpb->GetCurrentARAMAddr() + samples_to_download * sizeof(T));
    requested_samples_count -= samples_to_download;
    if (!vpb->GetRemainingLength())
      vpb->end_reached = true;
  }
}

void ZeldaAudioRenderer::DownloadAFCSamplesFromARAM(s16* dst, VPB* vpb, u16 requested_samples_count)
{
  if (vpb->reset_vpb)
  {
    *vpb->AFCYN1() = 0;
    *vpb->AFCYN2() = 0;
    vpb->afc_remaining_decoded_samples = 0;
    vpb->SetRemainingLength(vpb->GetLoopStartPosition());
    vpb->SetCurrentARAMAddr(vpb->GetBaseAddress());
  }

  if (vpb->done)
  {
    for (u16 i = 0; i < requested_samples_count; ++i)
      dst[i] = 0;
    return;
  }

  // Try several things until we have output enough samples.
  while (true)
  {
    // Try to push currently cached/already decoded samples.
    u16 remaining_to_output = std::min(vpb->afc_remaining_decoded_samples, requested_samples_count);
    s16* base = &vpb->afc_remaining_samples[0x10 - vpb->afc_remaining_decoded_samples];
    for (size_t i = 0; i < remaining_to_output; ++i)
      *dst++ = base[i];

    vpb->afc_remaining_decoded_samples -= remaining_to_output;
    requested_samples_count -= remaining_to_output;

    if (requested_samples_count == 0)
    {
      return;  // We have output everything we needed.
    }
    else if (requested_samples_count <= vpb->GetRemainingLength())
    {
      // Each AFC block is 16 samples.
      u16 requested_blocks_count = (requested_samples_count + 0xF) >> 4;
      u16 decoded_samples_count = requested_blocks_count << 4;

      if (decoded_samples_count < vpb->GetRemainingLength())
      {
        vpb->afc_remaining_decoded_samples = decoded_samples_count - requested_samples_count;
        vpb->SetRemainingLength(vpb->GetRemainingLength() - decoded_samples_count);
      }
      else
      {
        vpb->afc_remaining_decoded_samples = vpb->GetRemainingLength() - requested_samples_count;
        vpb->SetRemainingLength(0);
      }

      DecodeAFC(vpb, dst, requested_blocks_count);

      if (vpb->afc_remaining_decoded_samples)
      {
        for (size_t i = 0; i < 0x10; ++i)
          vpb->afc_remaining_samples[i] = dst[decoded_samples_count - 0x10 + i];

        if (!vpb->GetRemainingLength() && vpb->GetLoopStartPosition())
        {
          // Adjust remaining samples to account for the future loop iteration.
          base = vpb->afc_remaining_samples + ((vpb->GetLoopStartPosition() + 0xF) & 0xF);
          for (size_t i = 0; i < vpb->afc_remaining_decoded_samples; ++i)
            vpb->afc_remaining_samples[0x10 - i - 1] = *base--;
        }
      }

      return;
    }
    else
    {
      // More samples asked than available. Either complete the sound, or
      // start looping.
      if (vpb->GetRemainingLength())  // Skip if we cannot load anything.
      {
        requested_samples_count -= vpb->GetRemainingLength();
        u16 requested_blocks_count = (vpb->GetRemainingLength() + 0xF) >> 4;
        DecodeAFC(vpb, dst, requested_blocks_count);
        dst += vpb->GetRemainingLength();
      }

      if (!vpb->is_looping)
      {
        vpb->done = true;
        for (size_t i = 0; i < requested_samples_count; ++i)
          *dst++ = 0;
        return;
      }
      else
      {
        // We need to loop. Compute the new position, decode a block,
        // and loop back to the beginning of the download logic.

        // Use the fact that the sample source number also represents
        // the number of bytes per 16 samples.
        u32 loop_off_in_bytes = (vpb->GetLoopAddress() >> 4) * vpb->samples_source_type;
        u32 loop_start_addr = vpb->GetBaseAddress() + loop_off_in_bytes;
        vpb->SetCurrentARAMAddr(loop_start_addr);

        *vpb->AFCYN2() = vpb->loop_yn2;
        *vpb->AFCYN1() = vpb->loop_yn1;

        DecodeAFC(vpb, vpb->afc_remaining_samples, 1);

        // Realign and recompute the number of internally cached
        // samples and the current position.
        vpb->afc_remaining_decoded_samples = 0x10 - (vpb->GetLoopAddress() & 0xF);

        u32 remaining_length = vpb->GetLoopStartPosition();
        remaining_length -= vpb->afc_remaining_decoded_samples;
        remaining_length -= vpb->GetLoopAddress();
        vpb->SetRemainingLength(remaining_length);
        continue;
      }
    }
  }
}

void ZeldaAudioRenderer::DecodeAFC(VPB* vpb, s16* dst, size_t block_count)
{
  u32 addr = vpb->GetCurrentARAMAddr();
  u8* src = (u8*)GetARAMPtr(addr);
  vpb->SetCurrentARAMAddr(addr + (u32)block_count * vpb->samples_source_type);

  for (size_t b = 0; b < block_count; ++b)
  {
    s16 nibbles[16];
    s16 delta = 1 << ((*src >> 4) & 0xF);
    s16 idx = (*src & 0xF);
    src++;

    if (vpb->samples_source_type == VPB::SRC_AFC_HQ_FROM_ARAM)
    {
      // 4-bit samples
      for (size_t i = 0; i < 16; i += 2)
      {
        nibbles[i + 0] = *src >> 4;
        nibbles[i + 1] = *src & 0xF;
        src++;
      }
      for (auto& nibble : nibbles)
        nibble = s16(nibble << 12) >> 1;
    }
    else
    {
      // 2-bit samples
      for (size_t i = 0; i < 16; i += 4)
      {
        nibbles[i + 0] = (*src >> 6) & 3;
        nibbles[i + 1] = (*src >> 4) & 3;
        nibbles[i + 2] = (*src >> 2) & 3;
        nibbles[i + 3] = (*src >> 0) & 3;
        src++;
      }
      for (auto& nibble : nibbles)
        nibble = s16(nibble << 14) >> 1;
    }

    s32 yn1 = *vpb->AFCYN1(), yn2 = *vpb->AFCYN2();
    for (s16 nibble : nibbles)
    {
      s32 sample = delta * nibble + yn1 * m_afc_coeffs[idx * 2] + yn2 * m_afc_coeffs[idx * 2 + 1];
      sample >>= 11;
      sample = std::clamp(sample, -0x8000, 0x7fff);
      *dst++ = (s16)sample;
      yn2 = yn1;
      yn1 = sample;
    }

    *vpb->AFCYN2() = yn2;
    *vpb->AFCYN1() = yn1;
  }
}

void ZeldaAudioRenderer::DownloadRawSamplesFromMRAM(s16* dst, VPB* vpb, u16 requested_samples_count)
{
  auto& memory = m_system.GetMemory();
  u32 addr = vpb->GetBaseAddress() + vpb->current_position_h * sizeof(u16);
  s16* src_ptr = (s16*)HLEMemory_Get_Pointer(memory, addr);

  if (requested_samples_count > vpb->GetRemainingLength())
  {
    s16 last_sample = 0;
    for (u16 i = 0; i < vpb->GetRemainingLength(); ++i)
      *dst++ = last_sample = Common::swap16(*src_ptr++);
    for (u16 i = vpb->GetRemainingLength(); i < requested_samples_count; ++i)
      *dst++ = last_sample;

    vpb->current_position_h += vpb->GetRemainingLength();
    vpb->SetRemainingLength(0);
    vpb->done = true;
  }
  else
  {
    vpb->SetRemainingLength(vpb->GetRemainingLength() - requested_samples_count);
    vpb->samples_before_loop = vpb->loop_start_position_h - vpb->current_position_h;
    if (requested_samples_count <= vpb->samples_before_loop)
    {
      for (u16 i = 0; i < requested_samples_count; ++i)
        *dst++ = Common::swap16(*src_ptr++);
      vpb->current_position_h += requested_samples_count;
    }
    else
    {
      for (u16 i = 0; i < vpb->samples_before_loop; ++i)
        *dst++ = Common::swap16(*src_ptr++);
      vpb->SetBaseAddress(vpb->GetLoopAddress());
      src_ptr = (s16*)HLEMemory_Get_Pointer(memory, vpb->GetLoopAddress());
      for (u16 i = vpb->samples_before_loop; i < requested_samples_count; ++i)
        *dst++ = Common::swap16(*src_ptr++);
      vpb->current_position_h = requested_samples_count - vpb->samples_before_loop;
    }
  }
}

void ZeldaAudioRenderer::DoState(PointerWrap& p)
{
  p.Do(m_flags);
  p.Do(m_prepared);

  p.Do(m_output_lbuf_addr);
  p.Do(m_output_rbuf_addr);
  p.Do(m_output_volume);

  p.Do(m_buf_front_left);
  p.Do(m_buf_front_right);
  p.Do(m_buf_back_left);
  p.Do(m_buf_back_right);
  p.Do(m_buf_front_left_reverb);
  p.Do(m_buf_front_right_reverb);
  p.Do(m_buf_back_left_reverb);
  p.Do(m_buf_back_right_reverb);
  p.Do(m_buf_unk0_reverb);
  p.Do(m_buf_unk1_reverb);
  p.Do(m_buf_unk0);
  p.Do(m_buf_unk1);
  p.Do(m_buf_unk2);

  p.Do(m_resampling_coeffs);
  p.Do(m_const_patterns);
  p.Do(m_sine_table);
  p.Do(m_afc_coeffs);

  p.Do(m_aram_base_addr);
  p.Do(m_vpb_base_addr);
  p.Do(m_reverb_pb_base_addr);

  p.Do(m_reverb_pb_frames_count);
  p.Do(m_buf_unk0_reverb_last8);
  p.Do(m_buf_unk1_reverb_last8);
  p.Do(m_buf_front_left_reverb_last8);
  p.Do(m_buf_front_right_reverb_last8);
}
}  // namespace DSP::HLE
