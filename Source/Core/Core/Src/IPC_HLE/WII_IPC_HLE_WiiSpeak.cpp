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

#include "WII_IPC_HLE_WiiSpeak.h"
#include "StringUtil.h"

CWII_IPC_HLE_Device_usb_oh0::CWII_IPC_HLE_Device_usb_oh0(u32 DeviceID, const std::string& DeviceName)
	: IWII_IPC_HLE_Device(DeviceID, DeviceName)
{

}

CWII_IPC_HLE_Device_usb_oh0::~CWII_IPC_HLE_Device_usb_oh0()
{

}

bool CWII_IPC_HLE_Device_usb_oh0::Open(u32 CommandAddress, u32 Mode)
{
	Memory::Write_U32(GetDeviceID(), CommandAddress + 4);
	m_Active = true;
	return true;
}

bool CWII_IPC_HLE_Device_usb_oh0::Close(u32 CommandAddress, bool Force)
{
	if (!Force)
		Memory::Write_U32(0, CommandAddress + 4);
	m_Active = false;
	return true;
}

bool CWII_IPC_HLE_Device_usb_oh0::IOCtlV(u32 CommandAddress)
{
	bool SendReply = false;

	SIOCtlVBuffer CommandBuffer(CommandAddress);

	switch (CommandBuffer.Parameter)
	{
	case USBV0_IOCTL_DEVINSERTHOOK:
		{
			u16 vid = Memory::Read_U16(CommandBuffer.InBuffer[0].m_Address);
			u16 pid = Memory::Read_U16(CommandBuffer.InBuffer[1].m_Address);
			DEBUG_LOG(OSHLE, "DEVINSERTHOOK %x/%x", vid, pid);
			// It is inserted
			SendReply = true;
		}
		break;

	default:
		_dbg_assert_msg_(OSHLE, 0, "Unknown: %x", CommandBuffer.Parameter);

		DEBUG_LOG(OSHLE, "%s - IOCtlV:", GetDeviceName().c_str());
		DEBUG_LOG(OSHLE, "    Parameter: 0x%x", CommandBuffer.Parameter);
		DEBUG_LOG(OSHLE, "    NumberIn: 0x%08x", CommandBuffer.NumberInBuffer);
		DEBUG_LOG(OSHLE, "    NumberOut: 0x%08x", CommandBuffer.NumberPayloadBuffer);
		DEBUG_LOG(OSHLE, "    BufferVector: 0x%08x", CommandBuffer.BufferVector);
		DumpAsync(CommandBuffer.BufferVector, CommandBuffer.NumberInBuffer, CommandBuffer.NumberPayloadBuffer);
		break;
	}

	Memory::Write_U32(0, CommandAddress + 4);
	return SendReply;
}

bool CWII_IPC_HLE_Device_usb_oh0::IOCtl(u32 CommandAddress)
{
	return IOCtlV(CommandAddress);
}

u32 CWII_IPC_HLE_Device_usb_oh0::Update()
{
	return IWII_IPC_HLE_Device::Update();
}

void CWII_IPC_HLE_Device_usb_oh0::DoState(PointerWrap &p)
{

}


CWII_IPC_HLE_Device_usb_oh0_57e_308::CWII_IPC_HLE_Device_usb_oh0_57e_308(u32 DeviceID, const std::string& DeviceName)
	: IWII_IPC_HLE_Device(DeviceID, DeviceName)
{

}

CWII_IPC_HLE_Device_usb_oh0_57e_308::~CWII_IPC_HLE_Device_usb_oh0_57e_308()
{

}

bool CWII_IPC_HLE_Device_usb_oh0_57e_308::Open(u32 CommandAddress, u32 Mode)
{
	Memory::Write_U32(GetDeviceID(), CommandAddress + 4);
	m_Active = true;

	// set default state
	// TODO should this be elsewhere?
	sampler.sample_on = false;
	sampler.freq = 16000;
	sampler.gain = 36;
	sampler.ec_reset = false;
	sampler.sp_on = true;
	sampler.mute = false;

	return true;
}

bool CWII_IPC_HLE_Device_usb_oh0_57e_308::Close(u32 CommandAddress, bool Force)
{
	if (!Force)
		Memory::Write_U32(0, CommandAddress + 4);
	m_Active = false;
	return true;
}

/*/////////////////////////////////////////////////////////////////////////
#include <portaudio.h>
void CWII_IPC_HLE_Device_usb_oh0_57e_308::StreamLog(const char *msg)
{
	WARN_LOG(OSHLE, "%s: %s",
		msg, Pa_GetErrorText(pa_error));
}

void CWII_IPC_HLE_Device_usb_oh0_57e_308::StreamInit()
{
	// Setup the wonderful c-interfaced lib...
	pa_stream = NULL;

	if ((pa_error = Pa_Initialize()) != paNoError)
		StreamLog("Pa_Initialize");

	stream_buffer = NULL;
	samples_avail = stream_wpos = stream_rpos = 0;
}

void CWII_IPC_HLE_Device_usb_oh0_57e_308::StreamTerminate()
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

	CWII_IPC_HLE_Device_usb_oh0_57e_308 *mic = (CWII_IPC_HLE_Device_usb_oh0_57e_308 *)userData;

	std::lock_guard<std::mutex> lk(mic->ring_lock);

	if (mic->stream_wpos + mic->buff_size_samples > mic->stream_size)
		mic->stream_wpos = 0;

	s16 *buff_in = (s16 *)inputBuffer;
	s16 *buff_out = &mic->stream_buffer[mic->stream_wpos];

	if (buff_in == NULL)
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

void CWII_IPC_HLE_Device_usb_oh0_57e_308::StreamStart()
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

void CWII_IPC_HLE_Device_usb_oh0_57e_308::StreamStop()
{
	if (pa_stream != NULL && Pa_IsStreamActive(pa_stream) >= paNoError)
		Pa_AbortStream(pa_stream);

	delete [] stream_buffer;
	stream_buffer = NULL;
}

void CWII_IPC_HLE_Device_usb_oh0_57e_308::StreamReadOne()
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

*//////////////////////////////////////////////////////////////////////////


bool CWII_IPC_HLE_Device_usb_oh0_57e_308::IOCtlV(u32 CommandAddress)
{
	bool SendReply = false;

	SIOCtlVBuffer CommandBuffer(CommandAddress);

	switch (CommandBuffer.Parameter)
	{
	case USBV0_IOCTL_CTRLMSG:
		{
			USBSetupPacket setup_packet;
			setup_packet.bmRequestType	= *( u8*)Memory::GetPointer(CommandBuffer.InBuffer[0].m_Address);
			setup_packet.bRequest		= *( u8*)Memory::GetPointer(CommandBuffer.InBuffer[1].m_Address);
			setup_packet.wValue			= *(u16*)Memory::GetPointer(CommandBuffer.InBuffer[2].m_Address);
			setup_packet.wIndex			= *(u16*)Memory::GetPointer(CommandBuffer.InBuffer[3].m_Address);
			setup_packet.wLength		= *(u16*)Memory::GetPointer(CommandBuffer.InBuffer[4].m_Address);

			const u32 payload_addr = CommandBuffer.PayloadBuffer[0].m_Address;

			static bool initialized = false;

#define DIR_TO_DEV	0
#define DIR_TO_HOST	1
#define TYPE_STANDARD	0
#define TYPE_VENDOR		2
#define RECP_DEV	0
#define RECP_INT	1
#define RECP_ENDP	2
#define USBHDR(dir, type, recipient, request) \
	((((dir << 7) | (type << 5) | recipient) << 8) | request)

			switch (((u16)setup_packet.bmRequestType << 8) | setup_packet.bRequest)
			{
			case USBHDR(DIR_TO_DEV, TYPE_STANDARD, RECP_INT, 11):
				_dbg_assert_(OSHLE, setup_packet.wValue == 1);
				break;
			case USBHDR(DIR_TO_HOST, TYPE_STANDARD, RECP_INT, 10):
				Memory::Write_U8(1, payload_addr);
				break;
			case USBHDR(DIR_TO_HOST, TYPE_VENDOR, RECP_INT, 6):
				if (!initialized)
					Memory::Write_U8(0, payload_addr), initialized = true;
				else
					Memory::Write_U8(1, payload_addr);
				break;
			case USBHDR(DIR_TO_DEV, TYPE_VENDOR, RECP_INT, 1):
				SetRegister(payload_addr);
				break;
			case USBHDR(DIR_TO_HOST, TYPE_VENDOR, RECP_INT, 2):
				GetRegister(payload_addr);
				break;
			case USBHDR(DIR_TO_DEV, TYPE_VENDOR, RECP_INT, 0):
				initialized = false;
				break;
			default:
				DEBUG_LOG(OSHLE, "UNK %02x %02x %04x %04x",
					setup_packet.bmRequestType, setup_packet.bRequest,
					setup_packet.wLength, setup_packet.wValue);
				break;
			}

			// command finished, send a reply to command
			WII_IPC_HLE_Interface::EnqReply(CommandBuffer.m_Address);
		}
		break;

	case USBV0_IOCTL_BLKMSG:
		{
		u8 Command = Memory::Read_U8(CommandBuffer.InBuffer[0].m_Address);

		switch (Command)
		{
		// used for sending firmware
		case DATA_OUT:
			{
			u16 len = Memory::Read_U16(CommandBuffer.InBuffer[1].m_Address);
			DEBUG_LOG(OSHLE, "SEND DATA %x %x %x", len, CommandBuffer.PayloadBuffer[0].m_Address, CommandBuffer.PayloadBuffer[0].m_Size);
			SendReply = true;
			}
			break;

		default:
			DEBUG_LOG(OSHLE, "UNK BLKMSG %i", Command);
			break;
		}
		}
		break;

	case USBV0_IOCTL_ISOMSG:
		{
		// endp 81 = mic -> console
		// endp 03 = console -> mic
		u8 endpoint = Memory::Read_U8(CommandBuffer.InBuffer[0].m_Address);
		u16 length = Memory::Read_U16(CommandBuffer.InBuffer[1].m_Address);
		u8 num_packets = Memory::Read_U8(CommandBuffer.InBuffer[2].m_Address);
		u16 *packet_sizes = (u16*)Memory::GetPointer(CommandBuffer.PayloadBuffer[0].m_Address);
		u8 *packets = Memory::GetPointer(CommandBuffer.PayloadBuffer[1].m_Address);

		/*
		for (int i = 0; i < num_packets; i++)
		{
			u16 packet_len = Common::swap16(packet_sizes[i]);
			DEBUG_LOG(OSHLE, "packet %i [%i] to endpoint %02x", i, packet_len, endpoint);
			DEBUG_LOG(OSHLE, "%s", ArrayToString(packets, packet_len, 16).c_str());
			packets += packet_len;
		}
		*/
		
		if (endpoint == AUDIO_IN)
			for (u16 *sample = (u16*)packets; sample != (u16*)(packets + length); sample++)
				*sample = 0x8000;
		
		// TODO actual responses should obey some kinda timey thing
		SendReply = true;
		}
		break;

	default:
		_dbg_assert_msg_(OSHLE, 0, "Unknown: %x", CommandBuffer.Parameter);

		DEBUG_LOG(OSHLE, "%s - IOCtlV:", GetDeviceName().c_str());
		DEBUG_LOG(OSHLE, "    Parameter: 0x%x", CommandBuffer.Parameter);
		DEBUG_LOG(OSHLE, "    NumberIn: 0x%08x", CommandBuffer.NumberInBuffer);
		DEBUG_LOG(OSHLE, "    NumberOut: 0x%08x", CommandBuffer.NumberPayloadBuffer);
		DEBUG_LOG(OSHLE, "    BufferVector: 0x%08x", CommandBuffer.BufferVector);
		//DumpAsync(CommandBuffer.BufferVector, CommandBuffer.NumberInBuffer, CommandBuffer.NumberPayloadBuffer, LogTypes::OSHLE, LogTypes::LWARNING);
		break;
	}

	Memory::Write_U32(0, CommandAddress + 4);
	return SendReply;
}

void CWII_IPC_HLE_Device_usb_oh0_57e_308::SetRegister(const u32 cmd_ptr)
{
	DEBUG_LOG(OSHLE, "cmd -> %s",
		ArrayToString(Memory::GetPointer(cmd_ptr), 10, 16).c_str());

	const u8 reg = Memory::Read_U8(cmd_ptr + 1) & ~1;
	const u16 arg1 = Memory::Read_U16(cmd_ptr + 2);
	const u16 arg2 = Memory::Read_U16(cmd_ptr + 4);
	//u16 arg3 = Memory::Read_U16(cmd_ptr + 6);
	//u16 count = Memory::Read_U16(cmd_ptr + 8);

	_dbg_assert_(OSHLE, Memory::Read_U8(cmd_ptr) == 0x9a);
	_dbg_assert_(OSHLE, (Memory::Read_U8(cmd_ptr + 1) & 1) == 0);

	switch (reg)
	{
	case SAMPLER_STATE:
		sampler.sample_on = !!arg1;
		break;
	case SAMPLER_FREQ:
		switch (arg1)
		{
		case FREQ_8KHZ:
			sampler.freq = 8000;
			break;
		case FREQ_11KHZ:
			sampler.freq = 11025;
			break;
		case FREQ_RESERVED:
		case FREQ_16KHZ:
		default:
			sampler.freq = 16000;
			break;
		}
		break;
	case SAMPLER_GAIN:
		switch (arg1 & ~0x300)
		{
		case GAIN_00dB:
			sampler.gain = 0;
			break;
		case GAIN_15dB:
			sampler.gain = 15;
			break;
		case GAIN_30dB:
			sampler.gain = 30;
			break;
		case GAIN_36dB:
		default:
			sampler.gain = 36;
			break;
		}
		break;
	case EC_STATE:
		sampler.ec_reset = !!arg1;
		break;
	case SP_STATE:
		switch (arg1)
		{
		case SP_ENABLE:
			sampler.sp_on = arg2 == 0;
			break;
		case SP_SIN:
			break;
		case SP_SOUT:
			break;
		case SP_RIN:
			break;
		}
		break;
	case SAMPLER_MUTE:
		sampler.mute = !!arg1;
		break;
	}
}

void CWII_IPC_HLE_Device_usb_oh0_57e_308::GetRegister(const u32 cmd_ptr) const
{
	const u8 reg = Memory::Read_U8(cmd_ptr + 1) & ~1;
	const u32 arg1 = cmd_ptr + 2;
	const u32 arg2 = cmd_ptr + 4;

	_dbg_assert_(OSHLE, Memory::Read_U8(cmd_ptr) == 0x9a);
	_dbg_assert_(OSHLE, (Memory::Read_U8(cmd_ptr + 1) & 1) == 1);

	switch (reg)
	{
	case SAMPLER_STATE:
		Memory::Write_U16(sampler.sample_on ? 1 : 0, arg1);
		break;
	case SAMPLER_FREQ:
		switch (sampler.freq)
		{
		case 8000:
			Memory::Write_U16(FREQ_8KHZ, arg1);
			break;
		case 11025:
			Memory::Write_U16(FREQ_11KHZ, arg1);
			break;
		case 16000:
		default:
			Memory::Write_U16(FREQ_16KHZ, arg1);
			break;
		}
		break;
	case SAMPLER_GAIN:
		switch (sampler.gain)
		{
		case 0:
			Memory::Write_U16(0x300 | GAIN_00dB, arg1);
			break;
		case 15:
			Memory::Write_U16(0x300 | GAIN_15dB, arg1);
			break;
		case 30:
			Memory::Write_U16(0x300 | GAIN_30dB, arg1);
			break;
		case 36:
		default:
			Memory::Write_U16(0x300 | GAIN_36dB, arg1);
			break;
		}
		break;
	case EC_STATE:
		Memory::Write_U16(sampler.ec_reset ? 1 : 0, arg1);
		break;
	case SP_STATE:
		switch (Memory::Read_U16(arg1))
		{
		case SP_ENABLE:
			Memory::Write_U16(sampler.sp_on ? 0 : 1, arg2);
			break;
		case SP_SIN:
			break;
		case SP_SOUT:
			Memory::Write_U16(0x39B0, arg2); // 6dB TODO actually calc?
			break;
		case SP_RIN:
			break;
		}
		break;
	case SAMPLER_MUTE:
		Memory::Write_U16(sampler.mute ? 1 : 0, arg1);
		break;
	}

	DEBUG_LOG(OSHLE, "cmd <- %s",
		ArrayToString(Memory::GetPointer(cmd_ptr), 10, 16).c_str());
}

bool CWII_IPC_HLE_Device_usb_oh0_57e_308::IOCtl(u32 CommandAddress)
{
	bool SendReply = false;

	SIOCtlVBuffer CommandBuffer(CommandAddress);

	switch (CommandBuffer.Parameter)
	{
	case USBV0_IOCTL_DEVREMOVALHOOK:
		// Reply is sent when device is removed
		//SendReply = true;
		break;

	default:
		_dbg_assert_msg_(OSHLE, 0, "Unknown: %x", CommandBuffer.Parameter);

		DEBUG_LOG(OSHLE, "%s - IOCtl:", GetDeviceName().c_str());
		DEBUG_LOG(OSHLE, "    Parameter: 0x%x", CommandBuffer.Parameter);
		DEBUG_LOG(OSHLE, "    NumberIn: 0x%08x", CommandBuffer.NumberInBuffer);
		DEBUG_LOG(OSHLE, "    NumberOut: 0x%08x", CommandBuffer.NumberPayloadBuffer);
		DEBUG_LOG(OSHLE, "    BufferVector: 0x%08x", CommandBuffer.BufferVector);
		DumpAsync(CommandBuffer.BufferVector, CommandBuffer.NumberInBuffer, CommandBuffer.NumberPayloadBuffer);
		break;
	}

	Memory::Write_U32(0, CommandAddress + 4);
	return SendReply;
}

u32 CWII_IPC_HLE_Device_usb_oh0_57e_308::Update()
{
	return IWII_IPC_HLE_Device::Update();
}

void CWII_IPC_HLE_Device_usb_oh0_57e_308::DoState(PointerWrap &p)
{

}
