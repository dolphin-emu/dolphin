// Copyright 2019 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "Common/ChunkFile.h"
#include "Common/CommonTypes.h"
#include "Core/HW/WiimoteEmu/I2CBus.h"
#include "InputCommon/ControllerEmu/Setting/NumericSetting.h"

namespace WiimoteEmu
{
struct ADPCMState
{
  s32 predictor, step;
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

  void SetSpeakerEnabled(bool enabled);

private:
  // Pan is -1.0 to +1.0
  void SpeakerData(const u8* data, int length, float speaker_pan);

  // TODO: enum class
  static const u8 DATA_FORMAT_ADPCM = 0x00;
  static const u8 DATA_FORMAT_PCM = 0x40;

  // TODO: It seems reading address 0x00 should always return 0xff.
#pragma pack(push, 1)
  struct Register
  {
    // Speaker reports result in a write of samples to addr 0x00 (which also plays sound)
    u8 speaker_data;
    u8 unk_1;
    u8 format;
    // seems to always play at 6khz no matter what this is set to?
    // or maybe it only applies to pcm input
    // Little-endian:
    u16 sample_rate;
    u8 volume;
    u8 unk_5;
    u8 unk_6;
    // Reading this byte on real hardware seems to return 0x09:
    u8 unk_7;
    u8 unk_8;
    u8 unknown[0xf6];
  };
#pragma pack(pop)

  static_assert(0x100 == sizeof(Register));

  int BusRead(u8 slave_addr, u8 addr, int count, u8* data_out) override;
  int BusWrite(u8 slave_addr, u8 addr, int count, const u8* data_in) override;

  Register reg_data{};

  // TODO: What actions reset this state?
  // Is this actually in the register somewhere?
  ADPCMState adpcm_state{};

  ControllerEmu::SettingValue<double> m_speaker_pan_setting;

  bool m_speaker_enabled = false;
};

}  // namespace WiimoteEmu
