// Copyright 2019 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <span>

#include "Common/ChunkFile.h"
#include "Common/CommonTypes.h"
#include "Core/HW/WiimoteEmu/I2CBus.h"
#include "InputCommon/ControllerEmu/Setting/NumericSetting.h"

namespace WiimoteEmu
{
struct ADPCMState
{
  s32 predictor = 0;
  s32 step = 127;
};

class Wiimote;

class SpeakerLogic : public I2CSlave
{
  friend class Wiimote;

public:
  static const u8 I2C_ADDR = 0x51;

  static constexpr u8 SPEAKER_DATA_OFFSET = 0x00;

  void Reset();
  void DoState(PointerWrap& p);

  void SetEnabled(bool enabled);
  void SetMuted(bool muted);

private:
  void ProcessSpeakerData(std::span<const u8>);

  u32 GetCurrentSampleRate() const;

  // Much of the information here comes from reverse engineering of an original (non-TR) Wii remote.
  // -TR Wii remotes, for whatever reason, seem to perform better buffering of data,
  // resulting in fewer dropped packets and notably better audio playback.
  // However, the situation is still not ideal unless "sniff mode" is enabled.

  // Despite wiibrew claims, this dividend seems to be constant, regardless of the PCM/ADPCM mode.
  static constexpr u32 SAMPLE_RATE_DIVIDEND = 12'000'000;

  // There are 2 somewhat understood bits at addr:0x01.

  // Setting bit:0x01 seems to stop basically everything.
  // Written audio data is dropped (not buffered).
  // Already buffered audio data does not play.
  // The speaker audibly "clicks" off/on.
  static constexpr u8 SPEAKER_STOP_BIT = 0x01;

  // Setting bit 0x80 seems to halt playback.
  // Written audio data is buffered, but it will not decode/play.
  // The effect is perhaps indirect, because this also seems to clear the state at addr:0x08.
  // Restoring playback after this bit is written requires *first* clearing it,
  //  then afterward setting bit:0x01 at addr:0x08.
  static constexpr u8 SPEAKER_DISABLE_PLAYBACK_BIT = 0x80;

  // FYI: There seems to be a whopping 127 byte buffer for undecoded data regardless of format.
  // This buffering isn't at all implemented by us.

  // There are 4 somewhat understood bits at addr:0x07.

  // Unlike an original remote, reading this byte on a -TR remote seems to always produce 0x00.
  // Therefore, much of this is not applicable to -TR (and maybe other versions) of remotes.
  // The audio decoder IC has changed at least once from a ROHM BU7849 to a BU8849.

  // Seems to set/unset itself when the buffer has room for data.
  // Writing exactly 64 bytes of speaker data without output enabled will clear the bit.
  // Letting some samples play will set the bit again.
  // I think when the bit is observed, 64 more bytes of speaker data may be safely written.
  static constexpr u8 DECODER_READY_FOR_DATA_BIT = 0x01;

  // Writing this bit resets the ADPCM decoder.
  // This, expectedly, does work on a -TR remote.
  // It also sets the above "ready" bit so I assume it resets the buffer.
  // And, if any data was buffered, it sets bit 0x08 (processed data).
  // And, if 64 or more bytes were buffered, it sets bit 0x04 (dropped? data). Odd..
  // That happens even when also writting those bits (to clear them),
  //  so actually clearing all the bits can take two writes.
  static constexpr u8 DECODER_RESET_BIT = 0x02;

  // Seems to latch on after data was dropped, or something like that?
  // Writing 128 bytes or more of speaker data without output enabled will set the bit.
  //  This also sets the above "ready" bit, allowing overwrite of existing data I suppose?
  // Resetting the decoder with 64 bytes or more buffered will set this bit. Odd..
  // Enabling output with with 64 bytes or more buffered will set this bit. Odd..
  //  Those two are unusual and make me think this isn't really a "dropped data" flag.
  // This bit can be cleared by writing to it.
  static constexpr u8 DECODER_DROPPED_DATA_BIT = 0x04;

  // Seems to latch on after any playback.
  // This bit can be cleared by writing to it.
  static constexpr u8 DECODER_PROCESSED_DATA_BIT = 0x08;

#pragma pack(push, 1)
  struct Register
  {
    // Speaker reports result in a write of samples to addr 0x00 (which also plays sound)
    u8 speaker_data;

    // Note: Even though games always write the entire configuration as 7-bytes,
    //  it seems all of the parameters can be individually altered on the fly.

    // Games write 0x80 here when enabling speaker and 0x01 when disabling.
    // They also write 0x00 here in their 7-byte "configuration" of this and the following bytes.
    // In testing, it seems 0x80 and 0x01 are the only bits here that stop sound.
    u8 speaker_flags;

    // While the audio hardware itself appers to support 16bit samples, it's not practical
    //  because of the Wii remote's pitiful Bluetooth communication throughput.
    // One could maybe approach 32Kb/s (20bytes every 5ms) with "sniff mode" properly enabled.
    // Wii games only ever use 4bit Yamaha ADPCM at 24Kb/s (20bytes every ~6.66ms).
    //
    // If the 0x01 bit is set then no audio is produced,
    //  but it seems to be otherwise processed normally based on the decoder flags.
    //
    // The unknown format bit sizes are calculated from buffer "readiness" timing.
    // All these guesses come from testing on an original (non-TR) Wii remote.
    //
    // 0x00-0x1e = 4bit Yamaha ADPCM
    // 0x20-0x3e = 8bit? Plays static atop signal when given PCM ?
    // 0x40-0x5e = 8bit signed PCM
    // 0x60-0x7e = Seems to be 16bit signed little-endian PCM.
    // 0x80-0x9e = 8bit? PCM-like but quiet and different ?
    // 0xa0-0xbe = 8bit? Sounds overdriven when given PCM ?
    // 0xc0-0xde = 8bit?
    // 0xe0-0xfe = Seems to also be s16le PCM ?
    u8 audio_format;

    // Little-endian. 12,000,000 dividend.
    u16 sample_rate_divisor;

    // Games never set a value higher than 0x7f.
    u8 volume;

    // Games write 0x0c here when enabling the speaker.
    // Purpose is entirely unknown. Real hardware seems unaffected by this.
    u8 unknown_flags;

    // Games write 0x0e here when enabling the speaker to clear the decoder state.
    u8 decoder_flags;

    // Games write 0x01 here to enable playback.
    // When this bit is cleared, data can be buffered, but decoding/playback is paused.
    u8 audio_playback_enable;

    // Games write 0x01 to enable speaker and 0x00 to disable.
    // When this bit is cleared, written audio data is ignored.
    u8 audio_input_enable;

    u8 unknown[0xf6];
  };
#pragma pack(pop)

  static_assert(0x100 == sizeof(Register));

  int BusRead(u8 slave_addr, u8 addr, int count, u8* data_out) override;
  int BusWrite(u8 slave_addr, u8 addr, int count, const u8* data_in) override;

  Register m_register_data{};

  ADPCMState m_adpcm_state{};

  ControllerEmu::SettingValue<double> m_speaker_pan_setting;

  // FYI: Real hardware seems to be not-enabled and not-muted on power up.
  bool m_is_enabled = false;
  bool m_is_muted = false;
};

}  // namespace WiimoteEmu
