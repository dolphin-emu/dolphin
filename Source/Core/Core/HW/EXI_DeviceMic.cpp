// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "Common/CommonTypes.h"

#if HAVE_PORTAUDIO

#include "Core/CoreTiming.h"
#include "Core/HW/EXI.h"
#include "Core/HW/EXI_Device.h"
#include "Core/HW/EXI_DeviceMic.h"
#include "Core/HW/GCPad.h"
#include "Core/HW/SystemTimers.h"

#include <portaudio.h>

void CEXIMic::StreamLog(const char *msg)
{
	DEBUG_LOG(EXPANSIONINTERFACE, "%s: %s",
		msg, Pa_GetErrorText(pa_error));
}

void CEXIMic::StreamInit()
{
	// Setup the wonderful c-interfaced lib...
	pa_stream = nullptr;

	if ((pa_error = Pa_Initialize()) != paNoError)
		StreamLog("Pa_Initialize");

	stream_buffer = nullptr;
	samples_avail = stream_wpos = stream_rpos = 0;
}

void CEXIMic::StreamTerminate()
{
	StreamStop();

	if ((pa_error = Pa_Terminate()) != paNoError)
		StreamLog("Pa_Terminate");
}

static int Pa_Callback(const void *inputBuffer, void *outputBuffer,
	unsigned long framesPerBuffer,
	const PaStreamCallbackTimeInfo *timeInfo,
	PaStreamCallbackFlags statusFlags,
	void *userData)
{
	(void)outputBuffer;
	(void)timeInfo;
	(void)statusFlags;

	CEXIMic *mic = (CEXIMic *)userData;

	std::lock_guard<std::mutex> lk(mic->ring_lock);

	if (mic->stream_wpos + mic->buff_size_samples > mic->stream_size)
		mic->stream_wpos = 0;

	s16 *buff_in = (s16 *)inputBuffer;
	s16 *buff_out = &mic->stream_buffer[mic->stream_wpos];

	if (buff_in == nullptr)
	{
		for (int i = 0; i < mic->buff_size_samples; i++)
		{
			buff_out[i] = 0;
		}
	}
	else
	{
		for (int i = 0; i < mic->buff_size_samples; i++)
		{
			buff_out[i] = buff_in[i];
		}
	}

	mic->samples_avail += mic->buff_size_samples;
	if (mic->samples_avail > mic->stream_size)
	{
		mic->samples_avail = 0;
		mic->status.buff_ovrflw = 1;
	}

	mic->stream_wpos += mic->buff_size_samples;
	mic->stream_wpos %= mic->stream_size;

	return paContinue;
}

void CEXIMic::StreamStart()
{
	// Open stream with current parameters
	stream_size = buff_size_samples * 500;
	stream_buffer = new s16[stream_size];

	pa_error = Pa_OpenDefaultStream(&pa_stream, 1, 0, paInt16,
		sample_rate, buff_size_samples, Pa_Callback, this);
	StreamLog("Pa_OpenDefaultStream");
	pa_error = Pa_StartStream(pa_stream);
	StreamLog("Pa_StartStream");
}

void CEXIMic::StreamStop()
{
	if (pa_stream != nullptr && Pa_IsStreamActive(pa_stream) >= paNoError)
		Pa_AbortStream(pa_stream);

	samples_avail = stream_wpos = stream_rpos = 0;

	delete [] stream_buffer;
	stream_buffer = nullptr;
}

void CEXIMic::StreamReadOne()
{
	std::lock_guard<std::mutex> lk(ring_lock);

	if (samples_avail >= buff_size_samples)
	{
		s16 *last_buffer = &stream_buffer[stream_rpos];
		memcpy(ring_buffer, last_buffer, buff_size);

		samples_avail -= buff_size_samples;

		stream_rpos += buff_size_samples;
		stream_rpos %= stream_size;
	}
}

// EXI Mic Device
// This works by opening and starting a portaudio input stream when the is_active
// bit is set. The interrupt is scheduled in the future based on sample rate and
// buffer size settings. When the console handles the interrupt, it will send
// cmdGetBuffer, which is when we actually read data from a buffer filled
// in the background by Pa_Callback.

u8 const CEXIMic::exi_id[] = { 0, 0x0a, 0, 0, 0 };

CEXIMic::CEXIMic(int index)
	: slot(index)
{
	m_position = 0;
	command = 0;
	status.U16 = 0;

	sample_rate = rate_base;
	buff_size = ring_base;
	buff_size_samples = buff_size / sample_size;

	ring_pos = 0;
	memset(ring_buffer, 0, sizeof(ring_buffer));

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
	int diff = (SystemTimers::GetTicksPerSecond() / sample_rate) * buff_size_samples;
	next_int_ticks = CoreTiming::GetTicks() + diff;
	ExpansionInterface::ScheduleUpdateInterrupts(diff);
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
		command = byte; // first byte is command
		byte = 0xFF;    // would be tristate, but we don't care.
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
		if (pos == 0)
			status.button = Pad::GetMicButton(slot);

		byte = status.U8[pos ^ 1];

		if (pos == 1)
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
		{
		if (ring_pos == 0)
			StreamReadOne();

		byte = ring_buffer[ring_pos ^ 1];
		ring_pos = (ring_pos + 1) % buff_size;
		}
		break;

	default:
		ERROR_LOG(EXPANSIONINTERFACE,  "EXI MIC: unknown command byte %02x", command);
		break;
	}

	m_position++;
}
#endif
