// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "Common/CommonTypes.h"

namespace DSP
{
namespace HLE
{
struct PBMixer
{
  u16 left;
  u16 left_delta;
  u16 right;
  u16 right_delta;

  u16 auxA_left;
  u16 auxA_left_delta;
  u16 auxA_right;
  u16 auxA_right_delta;

  u16 auxB_left;
  u16 auxB_left_delta;
  u16 auxB_right;
  u16 auxB_right_delta;

  u16 auxB_surround;
  u16 auxB_surround_delta;
  u16 surround;
  u16 surround_delta;
  u16 auxA_surround;
  u16 auxA_surround_delta;
};

struct PBMixerWii
{
  // volume mixing values in .15, 0x8000 = ca. 1.0
  u16 left;
  u16 left_delta;
  u16 right;
  u16 right_delta;

  u16 auxA_left;
  u16 auxA_left_delta;
  u16 auxA_right;
  u16 auxA_right_delta;

  u16 auxB_left;
  u16 auxB_left_delta;
  u16 auxB_right;
  u16 auxB_right_delta;

  // Note: the following elements usage changes a little in DPL2 mode
  // TODO: implement and comment it in the mixer
  u16 auxC_left;
  u16 auxC_left_delta;
  u16 auxC_right;
  u16 auxC_right_delta;

  u16 surround;
  u16 surround_delta;
  u16 auxA_surround;
  u16 auxA_surround_delta;
  u16 auxB_surround;
  u16 auxB_surround_delta;
  u16 auxC_surround;
  u16 auxC_surround_delta;
};

struct PBMixerWM
{
  u16 main0;
  u16 main0_delta;
  u16 aux0;
  u16 aux0_delta;

  u16 main1;
  u16 main1_delta;
  u16 aux1;
  u16 aux1_delta;

  u16 main2;
  u16 main2_delta;
  u16 aux2;
  u16 aux2_delta;

  u16 main3;
  u16 main3_delta;
  u16 aux3;
  u16 aux3_delta;
};

struct PBInitialTimeDelay
{
  u16 on;
  u16 addrMemHigh;
  u16 addrMemLow;
  u16 offsetLeft;
  u16 offsetRight;
  u16 targetLeft;
  u16 targetRight;
};

// Update data - read these each 1ms subframe and use them!
// It seems that to provide higher time precisions for MIDI events, some games
// use this thing to update the parameter blocks per 1ms sub-block (a block is 5ms).
// Using this data should fix games that are missing MIDI notes.
struct PBUpdates
{
  u16 num_updates[5];
  u16 data_hi;  // These point to main RAM. Not sure about the structure of the data.
  u16 data_lo;
};

// The DSP stores the final sample values for each voice after every frame of processing.
// The values are then accumulated for all dropped voices, added to the next frame of audio,
// and ramped down on a per-sample basis to provide a gentle "roll off."
struct PBDpop
{
  s16 left;
  s16 auxA_left;
  s16 auxB_left;

  s16 right;
  s16 auxA_right;
  s16 auxB_right;

  s16 surround;
  s16 auxA_surround;
  s16 auxB_surround;
};

struct PBDpopWii
{
  s16 left;
  s16 auxA_left;
  s16 auxB_left;
  s16 auxC_left;

  s16 right;
  s16 auxA_right;
  s16 auxB_right;
  s16 auxC_right;

  s16 surround;
  s16 auxA_surround;
  s16 auxB_surround;
  s16 auxC_surround;
};

struct PBDpopWM
{
  s16 main0;
  s16 main1;
  s16 main2;
  s16 main3;

  s16 aux0;
  s16 aux1;
  s16 aux2;
  s16 aux3;
};

struct PBVolumeEnvelope
{
  u16 cur_volume;        // Volume at start of frame
  s16 cur_volume_delta;  // Signed per sample delta (96 samples per frame)
};

struct PBUnknown2
{
  u16 unknown_reserved[3];
};

struct PBAudioAddr
{
  u16 looping;
  u16 sample_format;
  u16 loop_addr_hi;  // Start of loop (this will point to a shared "zero" buffer if one-shot mode is
                     // active)
  u16 loop_addr_lo;
  u16 end_addr_hi;  // End of sample (and loop), inclusive
  u16 end_addr_lo;
  u16 cur_addr_hi;
  u16 cur_addr_lo;
};

struct PBADPCMInfo
{
  s16 coefs[16];
  u16 gain;
  u16 pred_scale;
  s16 yn1;
  s16 yn2;
};

struct PBSampleRateConverter
{
  // ratio = (f32)ratio * 0x10000;
  // valid range is 1/512 to 4.0000
  u16 ratio_hi;  // integer part of sampling ratio
  u16 ratio_lo;  // fraction part of sampling ratio
  u16 cur_addr_frac;
  s16 last_samples[4];
};

struct PBSampleRateConverterWM
{
  u16 cur_addr_frac;
  s16 last_samples[4];
};

struct PBADPCMLoopInfo
{
  u16 pred_scale;
  u16 yn1;
  u16 yn2;
};

struct PBLowPassFilter
{
  u16 enabled;
  s16 yn1;
  u16 a0;
  u16 b0;
};

struct AXPB
{
  u16 next_pb_hi;
  u16 next_pb_lo;
  u16 this_pb_hi;
  u16 this_pb_lo;

  u16 src_type;  // Type of sample rate converter (none, ?, linear)
  u16 coef_select;
  u16 mixer_control;

  u16 running;    // 1=RUN 0=STOP
  u16 is_stream;  // 1 = stream, 0 = one shot

  PBMixer mixer;
  PBInitialTimeDelay initial_time_delay;
  PBUpdates updates;
  PBDpop dpop;
  PBVolumeEnvelope vol_env;
  PBUnknown2 unknown3;
  PBAudioAddr audio_addr;
  PBADPCMInfo adpcm;
  PBSampleRateConverter src;
  PBADPCMLoopInfo adpcm_loop_info;
  PBLowPassFilter lpf;  // Skipped when writing to/reading from MRAM/ARAM for certain AX UCodes
  u16 loop_counter;

  u16 padding[24];
};

struct PBBiquadFilter
{
  u16 on;   // on = 2, off = 0
  u16 xn1;  // History data
  u16 xn2;
  u16 yn1;
  u16 yn2;
  u16 b0;  // Filter coefficients
  u16 b1;
  u16 b2;
  u16 a1;
  u16 a2;
};

union PBInfImpulseResponseWM
{
  PBLowPassFilter lpf;
  PBBiquadFilter biquad;
};

struct AXPBWii
{
  u16 next_pb_hi;
  u16 next_pb_lo;
  u16 this_pb_hi;
  u16 this_pb_lo;

  u16 src_type;     // Type of sample rate converter (none, 4-tap, linear)
  u16 coef_select;  // coef for the 4-tap src
  u16 mixer_control_hi;
  u16 mixer_control_lo;

  u16 running;    // 1=RUN   0=STOP
  u16 is_stream;  // 1 = stream, 0 = one shot

  PBMixerWii mixer;
  PBInitialTimeDelay initial_time_delay;
  PBDpopWii dpop;
  PBVolumeEnvelope vol_env;
  PBAudioAddr audio_addr;
  PBADPCMInfo adpcm;
  PBSampleRateConverter src;
  PBADPCMLoopInfo adpcm_loop_info;
  PBLowPassFilter lpf;
  PBBiquadFilter biquad;

  // WIIMOTE :D
  u16 remote;
  u16 remote_mixer_control;

  PBMixerWM remote_mixer;
  PBDpopWM remote_dpop;
  PBSampleRateConverterWM remote_src;
  PBInfImpulseResponseWM remote_iir;

  u16 padding[12]; // align us, captain! (32B)
};

// TODO: All these enums have changed a lot for Wii
enum
{
  AUDIOFORMAT_ADPCM = 0,
  AUDIOFORMAT_PCM8 = 0x19,
  AUDIOFORMAT_PCM16 = 0xA,
};

enum
{
  SRCTYPE_POLYPHASE = 0,
  SRCTYPE_LINEAR = 1,
  SRCTYPE_NEAREST = 2,
};

// Both may be used at once
enum
{
  FILTER_LOWPASS = 1,
  FILTER_BIQUAD = 2,
};
}  // namespace HLE
}  // namespace DSP
