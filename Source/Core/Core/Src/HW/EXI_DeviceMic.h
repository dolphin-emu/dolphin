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

#ifndef _EXI_DEVICEMIC_H
#define _EXI_DEVICEMIC_H

#if HAVE_PORTAUDIO

#include "StdMutex.h"

class CEXIMic : public IEXIDevice
{
public:
	CEXIMic(const int index);
	virtual ~CEXIMic();
	void SetCS(int cs);
	bool IsInterruptSet();
	bool IsPresent();

private:
	static u8 const exi_id[];
	static int const sample_size = sizeof(s16);
	static int const rate_base = 11025;
	static int const ring_base = 32;

	enum
	{
		cmdID			= 0x00,
		cmdGetStatus	= 0x40,
		cmdSetStatus	= 0x80,
		cmdGetBuffer	= 0x20,
		cmdReset		= 0xFF,
	};

	int slot;

	u32 m_position;
	int command;
	union UStatus
	{
		u16 U16;
		u8 U8[2];
		struct
		{
			u16	out			:4; // MICSet/GetOut...???
			u16 id			:1; // Used for MICGetDeviceID (always 0)
			u16 button_unk	:3; // Button bits which appear unused
			u16 button		:1; // The actual button on the mic
			u16 buff_ovrflw	:1; // Ring buffer wrote over bytes which weren't read by console
			u16 gain		:1; // Gain: 0dB or 15dB
			u16 sample_rate	:2; // Sample rate, 00-11025, 01-22050, 10-44100, 11-??
			u16 buff_size	:2; // Ring buffer size in bytes, 00-32, 01-64, 10-128, 11-???
			u16 is_active	:1; // If we are sampling or not
		};
	};

	// 64 is the max size, can be 16 or 32 as well
	int ring_pos;
	u8 ring_buffer[64 * sample_size];

	// 0 to disable interrupts, else it will be checked against current cpu ticks
	// to determine if interrupt should be raised
	u64 next_int_ticks;
	void UpdateNextInterruptTicks();

	// Streaming input interface
	int pa_error; // PaError
	void *pa_stream; // PaStream

	void StreamLog(const char *msg);
	void StreamInit();
	void StreamTerminate();
	void StreamStart();
	void StreamStop();
	void StreamReadOne();

public:
	UStatus status;

	std::mutex ring_lock;

	// status bits converted to nice numbers
	int sample_rate;
	int buff_size;
	int buff_size_samples;

	// Arbitrarily small ringbuffer used by audio input backend in order to
	// keep delay tolerable
	s16 *stream_buffer;
	int stream_size;
	int stream_wpos;
	int stream_rpos;
	int samples_avail;
	
protected:
	virtual void TransferByte(u8 &byte);
};

#else // HAVE_PORTAUDIO

class CEXIMic : public IEXIDevice
{
public:
	CEXIMic(const int) {}
};

#endif

#endif // _EXI_DEVICEMIC_H
