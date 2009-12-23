// Copyright (C) 2003 Dolphin Project.

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

#ifndef _UCODE_AX_STRUCTS_H
#define _UCODE_AX_STRUCTS_H

struct PBMixer
{
	u16 volume_left;
	u16 unknown;
	u16 volume_right;
	u16 unknown2;

	u16 unknown3[8];
	u16 unknown4[6];
};

struct PBMixerWii
{
	u16 volume_left;
	u16 unknown;
	u16 volume_right;
	u16 unknown2;

	u16 unknown3[12];
	u16 unknown4[8];
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

struct PBUpdatesWii
{
	u16 num_updates[3];
	u16 data_hi;  // These point to main RAM. Not sure about the structure of the data.
	u16 data_lo;
};

struct PBDpop
{
	s16 unknown[9];
};

	struct PBDpopWii
	{
		s16 unknown[12];
	};

	struct PBDpopWii_ // new CRC version
	{
		s16 unknown[7];
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
	u16 coef_select;

	u16 mixer_control;
	u16 running;       // 1=RUN 0=STOP
	u16 is_stream;     // 1 = stream, 0 = one shot

/*  9 */	PBMixer mixer;
/* 27 */	PBInitialTimeDelay initial_time_delay;  
/* 34 */	PBUpdates updates;
/* 41 */	PBDpop dpop;
/* 50 */	PBVolumeEnvelope vol_env;
/* 52 */	PBUnknown2 unknown3;
/* 55 */	PBAudioAddr audio_addr;
/* 63 */	PBADPCMInfo adpcm;
/* 83 */    PBSampleRateConverter src;
/* 90 */	PBADPCMLoopInfo adpcm_loop_info;
/* 93 */	//u16 unknown_maybe_padding[3];		// Comment this out to get some speedup
};

struct PBLpf
{
	u16 enabled;
	u16 yn1;
	u16 a0;
	u16 b0;
};

struct PBHpf
{
	u16 enabled;
	u16 yn1;
	u16 a0;
	u16 b0;
};

struct AXParamBlockWii
{
	u16 next_pb_hi;
	u16 next_pb_lo;

	u16 this_pb_hi;
	u16 this_pb_lo;

	u16 src_type;     // Type of sample rate converter (none, ?, linear)
	u16 coef_select;
	u32 mixer_control;

	u16 running;       // 1=RUN   0=STOP
	u16 is_stream;     // 1 = stream, 0 = one shot

/*  10 */	PBMixerWii mixer;
/*  34 */	PBInitialTimeDelay initial_time_delay;  
/*  41 */	PBUpdatesWii updates;
/*  46 */	PBDpopWii dpop;
/*  58 */	PBVolumeEnvelope vol_env;
/*  60 */	PBAudioAddr audio_addr;
/*  68 */	PBADPCMInfo adpcm;
/*  88 */	PBSampleRateConverter src;
/*  95 */	PBADPCMLoopInfo adpcm_loop_info;
/*  98 */	PBLpf lpf;
/* 102 */	PBHpf hpf;
/* 106 */	//u16 pad[22];	// Comment this out to get some speedup
};

struct AXParamBlockWii_ // new CRC version
{
/* 0x000 */	u16 next_pb_hi;
/* 0x002 */	u16 next_pb_lo;
/* 0x004 */	u16 this_pb_hi;
/* 0x006 */	u16 this_pb_lo;
/* 0x008 */	u16 src_type;     // Type of sample rate converter (none, ?, linear)
/* 0x00A */	u16 coef_select;
/* 0x00C */	u32 mixer_control;
/* 0x010 */	u16 running;       // 1=RUN   0=STOP
/* 0x012 */	u16 is_stream;     // 1 = stream, 0 = one shot
/* 0x014 */	PBMixerWii mixer;
/* 0x044 */	PBInitialTimeDelay initial_time_delay;
/* 0x052 */	PBUpdatesWii updates;
/* 0x05C */	PBDpopWii_ dpop;
/* 0x06A */	PBVolumeEnvelope vol_env;
/* 0x06E */	PBAudioAddr audio_addr;
/* 0x07E */	PBADPCMInfo adpcm;
/* 0x0A6 */	PBSampleRateConverter src;
/* 0x0B4 */	PBADPCMLoopInfo adpcm_loop_info;
/* 0x0BA */	PBLpf lpf;
/* 0x0C2 */	PBHpf hpf;
/* 0x0CA */	//u16 pad[27];	// Comment this out to get some speedup
/* 0x100 */
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

#endif  // _UCODE_AX_STRUCTS_H
