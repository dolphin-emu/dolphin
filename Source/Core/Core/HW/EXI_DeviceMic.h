// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include "Core/HW/EXI_Device.h"

#if HAVE_PORTAUDIO

#include <mutex>

class CEXIMic : public IEXIDevice
{
public:
	CEXIMic(const int index);
	virtual ~CEXIMic();
	void SetCS(int cs) override;
	bool IsInterruptSet() override;
	bool IsPresent() override;

private:
	static u8 const exi_id[];
	static int const sample_size = sizeof(s16);
	static int const rate_base = 11025;
	static int const ring_base = 32;

	enum
	{
		cmdID        = 0x00,
		cmdGetStatus = 0x40,
		cmdSetStatus = 0x80,
		cmdGetBuffer = 0x20,
		cmdReset     = 0xFF,
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
			u16 out         : 4; // MICSet/GetOut...???
			u16 id          : 1; // Used for MICGetDeviceID (always 0)
			u16 button_unk  : 3; // Button bits which appear unused
			u16 button      : 1; // The actual button on the mic
			u16 buff_ovrflw : 1; // Ring buffer wrote over bytes which weren't read by console
			u16 gain        : 1; // Gain: 0dB or 15dB
			u16 sample_rate : 2; // Sample rate, 00-11025, 01-22050, 10-44100, 11-??
			u16 buff_size   : 2; // Ring buffer size in bytes, 00-32, 01-64, 10-128, 11-???
			u16 is_active   : 1; // If we are sampling or not
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
	virtual void TransferByte(u8 &byte) override;
};

#else // HAVE_PORTAUDIO

class CEXIMic : public IEXIDevice
{
public:
	CEXIMic(const int) {}
};

#endif
