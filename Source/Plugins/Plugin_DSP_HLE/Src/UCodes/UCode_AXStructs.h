// Copyright (C) 2003-2008 Dolphin Project.

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.0.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License 2.0 for more details.

// A copy of the GPL 2.0 should have been included with the program.
// If not, see http://www.gnu.org/licenses/

// Official SVN repository and contact information can be found at
// http://code.google.com/p/dolphin-emu/

#ifndef UCODE_AX_STRUCTS
#define UCODE_AX_STRUCTS

struct PBMixer
{
	u16 volume_left;
	u16 unknown;
	u16 volume_right;
	u16 unknown2;

	u16 unknown3[8];
	u16 unknown4[6];
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

struct PBUnknown
{
	s16 unknown[9];
};

struct PBVolumeEnvelope
{
	u16 cur_volume;
	s16 cur_volume_delta;
};

struct PBUnknown2
{
	u16 unknown_reserved[3];
};

struct PBAudioAddr
{
	u16 looping;
	u16 sample_format;
	u16 loop_addr_hi;  // Start of loop (this will point to a shared "zero" buffer if one-shot mode is active)
	u16 loop_addr_lo;
	u16 end_addr_hi;   // End of sample (and loop), inclusive
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
	u16 ratio_hi;
	u16 ratio_lo;
	u16 cur_addr_frac;
	u16 last_samples[4];
};

struct PBADPCMLoopInfo
{
	u16 pred_scale;
	u16 yn1;
	u16 yn2;
};

struct AXParamBlock
{
	u16 next_pb_hi;
	u16 next_pb_lo;

	u16 this_pb_hi;
	u16 this_pb_lo;

	u16 src_type;     // Type of sample rate converter (none, ?, linear)
	u16 unknown1;

	u16 mixer_control;
	u16 running;       // 1=RUN 0=STOP
	u16 is_stream;     // 1 = stream, 0 = one shot

/*  9 */	PBMixer mixer;
/* 27 */	PBInitialTimeDelay initial_time_delay;  
/* 34 */	PBUpdates updates;
/* 41 */	PBUnknown unknown2;
/* 50 */	PBVolumeEnvelope vol_env;
/* 52 */	PBUnknown2 unknown3;
/* 55 */	PBAudioAddr audio_addr;
/* 63 */	PBADPCMInfo adpcm;
/* 83 */    PBSampleRateConverter src;
/* 90 */	PBADPCMLoopInfo adpcm_loop_info;
/* 93 */	u16 unknown_maybe_padding[3];
};

enum {
    AUDIOFORMAT_ADPCM = 0,
    AUDIOFORMAT_PCM8  = 0x19,
	AUDIOFORMAT_PCM16 = 0xA,
};

enum {
	SRCTYPE_LINEAR  = 1,
	SRCTYPE_NEAREST = 2,
	MIXCONTROL_RAMPING = 8,
};


#endif
