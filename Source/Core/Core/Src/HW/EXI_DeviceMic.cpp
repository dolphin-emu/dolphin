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

#include "Common.h"

#if HAVE_PORTAUDIO

#include "FileUtil.h"
#include "StringUtil.h"
#include "../Core.h"
#include "../CoreTiming.h"
#include "SystemTimers.h"

#include "EXI_Device.h"
#include "EXI_DeviceMic.h"

#include <portaudio.h>

static bool pa_init = false;

void CEXIMic::StreamLog(const char *msg)
{
	DEBUG_LOG(EXPANSIONINTERFACE, "%s: %s",
		msg, Pa_GetErrorText(pa_error));
}

void CEXIMic::StreamInit()
{
	// Setup the wonderful c-interfaced lib...
	pa_error = paNoError;
	if (!pa_init)
	{
		pa_error = Pa_Initialize();

		if (pa_error != paNoError)
		{
			StreamLog("Pa_Initialize");
		}
		else
			pa_init = true;
	}

	mic_count++;
}

void CEXIMic::StreamTerminate()
{
	if (pa_init && --mic_count <= 0)
		pa_error = Pa_Terminate();

	if (pa_error != paNoError)
	{
		StreamLog("Pa_Terminate");
	}
	else
		pa_init = false;
}

void CEXIMic::StreamStart()
{
	// Open stream with current parameters
	if (pa_init)
	{
		pa_error = Pa_OpenDefaultStream(&pa_stream, 1, 0, paInt16,
			sample_rate, buff_size_samples, NULL, NULL);
		StreamLog("Pa_OpenDefaultStream");
		pa_error = Pa_StartStream(pa_stream);
		StreamLog("Pa_StartStream");
	}
}

void CEXIMic::StreamStop()
{
	// Acts as if Pa_AbortStream was called
	pa_error = Pa_CloseStream(pa_stream);
	StreamLog("Pa_CloseStream");
}

void CEXIMic::StreamReadOne()
{
	// Returns num samples or error
	pa_error = Pa_GetStreamReadAvailable(pa_stream);
	if (pa_error >= buff_size_samples)
	{
		pa_error = Pa_ReadStream(pa_stream, ring_buffer, buff_size_samples);

		if (pa_error != paNoError)
		{
			status.buff_ovrflw = 1;
			// Input overflowed - is re-setting the stream the only to recover?
			StreamLog("Pa_ReadStream");

			StreamStop();
			StreamStart();
		}
	}
}

// EXI Mic Device
// This works by opening and starting a portaudio input stream when the is_active
// bit is set. The interrupt is scheduled in the future based on sample rate and
// buffer size settings. When the console handles the interrupt, it will send
// cmdGetBuffer, which is when we actually read data from a buffer portaudio fills
// in the background (ie on demand instead of realtime). Because of this we need
// to clear portaudio's buffer if emulation speed drops below realtime, or else
// a bad audio lag develops. It's actually kind of convenient because it gives
// us a way to detect if buff_ovrflw should be set.

u8 const CEXIMic::exi_id[] = { 0, 0x0a, 0, 0, 0 };
int CEXIMic::mic_count = 0;

CEXIMic::CEXIMic()
{
	status.U16 = 0;
	command = 0;
	m_position = 0;
	ring_pos = 0;
	next_int_ticks = 0;

	StreamInit();
}

CEXIMic::~CEXIMic()
{
	StreamTerminate();
}

bool CEXIMic::IsPresent()
{
	return true;
}

void CEXIMic::SetCS(int cs)
{
	if (cs) // not-selected to selected
		m_position = 0;
	// Doesn't appear to do anything we care about
	//else if (command == cmdReset)
}

void CEXIMic::UpdateNextInterruptTicks()
{
	next_int_ticks = CoreTiming::GetTicks() +
		(SystemTimers::GetTicksPerSecond() / sample_rate) *	buff_size_samples;
}

bool CEXIMic::IsInterruptSet()
{
	if (next_int_ticks && CoreTiming::GetTicks() >= next_int_ticks)
	{
		if (status.is_active)
			UpdateNextInterruptTicks();
		else
			next_int_ticks = 0;

		return true;
	}
	else
	{
		return false;
	}
}

void CEXIMic::TransferByte(u8 &byte)
{
	if (m_position == 0)
	{
		command = byte;	// first byte is command
		byte = 0xFF;	// would be tristate, but we don't care.
		m_position++;
		return;
	}

	int pos = m_position - 1;

	switch (command)
	{
	case cmdID:
		byte = exi_id[pos];
		break;

	case cmdGetStatus:
		byte = status.U8[pos ^ 1];
		if (pos == 1 && status.buff_ovrflw)
			status.buff_ovrflw = 0;
		break;

	case cmdSetStatus:
		{
		bool wasactive = status.is_active;
		status.U8[pos ^ 1] = byte;

		// safe to do since these can only be entered if both bytes of status have been written
		if (!wasactive && status.is_active)
		{
			sample_rate = rate_base << status.sample_rate;
			buff_size = ring_base << status.buff_size;
			buff_size_samples = buff_size / sample_size;

			UpdateNextInterruptTicks();
			
			StreamStart();
		}
		else if (wasactive && !status.is_active)
		{
			StreamStop();
		}
		}
		break;

	case cmdGetBuffer:
		if (ring_pos == 0)
		{
			// Can set buff_ovrflw
			StreamReadOne();
		}
		byte = ring_buffer[ring_pos ^ 1];
		ring_pos = (ring_pos + 1) % buff_size;
		break;

	default:
		ERROR_LOG(EXPANSIONINTERFACE,  "EXI MIC: unknown command byte %02x", command);
		break;
	}

	m_position++;
}
#endif
