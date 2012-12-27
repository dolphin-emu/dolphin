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

#include "StringUtil.h"

#include "../MailHandler.h"
#include "Mixer.h"

#include "UCodes.h"
#include "UCode_AX_Structs.h"
#include "UCode_NewAXWii.h"

#define AX_WII
#include "UCode_AX_Voice.h"


CUCode_NewAXWii::CUCode_NewAXWii(DSPHLE *dsp_hle, u32 l_CRC)
	: CUCode_AX(dsp_hle, l_CRC)
{
	WARN_LOG(DSPHLE, "Instantiating CUCode_NewAXWii");
}

CUCode_NewAXWii::~CUCode_NewAXWii()
{
}

void CUCode_NewAXWii::HandleCommandList()
{
	// Temp variables for addresses computation
	u16 addr_hi, addr_lo;
	u16 addr2_hi, addr2_lo;
	u16 volume;

//	WARN_LOG(DSPHLE, "Command list:");
//	for (u32 i = 0; m_cmdlist[i] != CMD_END; ++i)
//		WARN_LOG(DSPHLE, "%04x", m_cmdlist[i]);
//	WARN_LOG(DSPHLE, "-------------");

	u32 curr_idx = 0;
	bool end = false;
	while (!end)
	{
		u16 cmd = m_cmdlist[curr_idx++];

		switch (cmd)
		{
			// Some of these commands are unknown, or unused in this AX HLE.
			// We still need to skip their arguments using "curr_idx += N".

			case CMD_SETUP:
				addr_hi = m_cmdlist[curr_idx++];
				addr_lo = m_cmdlist[curr_idx++];
				SetupProcessing(HILO_TO_32(addr));
				break;

			case CMD_UNK_01: curr_idx += 2; break;
			case CMD_UNK_02: curr_idx += 2; break;
			case CMD_UNK_03: curr_idx += 2; break;

			case CMD_PROCESS:
				addr_hi = m_cmdlist[curr_idx++];
				addr_lo = m_cmdlist[curr_idx++];
				ProcessPBList(HILO_TO_32(addr));
				break;

			case CMD_MIX_AUXA:
			case CMD_MIX_AUXB:
			case CMD_MIX_AUXC:
				volume = m_cmdlist[curr_idx++];
				addr_hi = m_cmdlist[curr_idx++];
				addr_lo = m_cmdlist[curr_idx++];
				addr2_hi = m_cmdlist[curr_idx++];
				addr2_lo = m_cmdlist[curr_idx++];
				MixAUXSamples(cmd - CMD_MIX_AUXA, HILO_TO_32(addr), HILO_TO_32(addr2), volume);
				break;

			// These two go together and manipulate some AUX buffers.
			case CMD_UNK_08: curr_idx += 13; break;
			case CMD_UNK_09: curr_idx += 13; break;

			case CMD_UNK_0A: curr_idx += 4; break;

			case CMD_OUTPUT:
				volume = m_cmdlist[curr_idx++];
				addr_hi = m_cmdlist[curr_idx++];
				addr_lo = m_cmdlist[curr_idx++];
				addr2_hi = m_cmdlist[curr_idx++];
				addr2_lo = m_cmdlist[curr_idx++];
				OutputSamples(HILO_TO_32(addr2), HILO_TO_32(addr), volume);
				break;

			case CMD_UNK_0C: curr_idx += 5; break;

			case CMD_WM_OUTPUT:
			{
				u32 addresses[4] = {
					(u32)(m_cmdlist[curr_idx + 0] << 16) | m_cmdlist[curr_idx + 1],
					(u32)(m_cmdlist[curr_idx + 2] << 16) | m_cmdlist[curr_idx + 3],
					(u32)(m_cmdlist[curr_idx + 4] << 16) | m_cmdlist[curr_idx + 5],
					(u32)(m_cmdlist[curr_idx + 6] << 16) | m_cmdlist[curr_idx + 7],
				};
				curr_idx += 8;
				OutputWMSamples(addresses);
				break;
			}

			case CMD_END:
				end = true;
				break;
		}
	}
}

void CUCode_NewAXWii::SetupProcessing(u32 init_addr)
{
	// TODO: should be easily factorizable with AX
	s16 init_data[60];

	for (u32 i = 0; i < 60; ++i)
		init_data[i] = HLEMemory_Read_U16(init_addr + 2 * i);

	// List of all buffers we have to initialize
	struct {
		int* ptr;
		u32 samples;
	} buffers[] = {
		{ m_samples_left, 32 },
		{ m_samples_right, 32 },
		{ m_samples_surround, 32 },
		{ m_samples_auxA_left, 32 },
		{ m_samples_auxA_right, 32 },
		{ m_samples_auxA_surround, 32 },
		{ m_samples_auxB_left, 32 },
		{ m_samples_auxB_right, 32 },
		{ m_samples_auxB_surround, 32 },
		{ m_samples_auxC_left, 32 },
		{ m_samples_auxC_right, 32 },
		{ m_samples_auxC_surround, 32 },

		{ m_samples_wm0, 6 },
		{ m_samples_aux0, 6 },
		{ m_samples_wm1, 6 },
		{ m_samples_aux1, 6 },
		{ m_samples_wm2, 6 },
		{ m_samples_aux2, 6 },
		{ m_samples_wm3, 6 },
		{ m_samples_aux3, 6 }
	};

	u32 init_idx = 0;
	for (u32 i = 0; i < sizeof (buffers) / sizeof (buffers[0]); ++i)
	{
		s32 init_val = (s32)((init_data[init_idx] << 16) | init_data[init_idx + 1]);
		s16 delta = (s16)init_data[init_idx + 2];

		init_idx += 3;

		if (!init_val)
			memset(buffers[i].ptr, 0, 3 * buffers[i].samples * sizeof (int));
		else
		{
			for (u32 j = 0; j < 3 * buffers[i].samples; ++j)
			{
				buffers[i].ptr[j] = init_val;
				init_val += delta;
			}
		}
	}
}

AXMixControl CUCode_NewAXWii::ConvertMixerControl(u32 mixer_control)
{
	u32 ret = 0;

	if (mixer_control & 0x00000001) ret |= MIX_L;
	if (mixer_control & 0x00000002) ret |= MIX_R;
	if (mixer_control & 0x00000004) ret |= MIX_L_RAMP | MIX_R_RAMP;
	if (mixer_control & 0x00000008) ret |= MIX_S;
	if (mixer_control & 0x00000010) ret |= MIX_S_RAMP;
	if (mixer_control & 0x00010000) ret |= MIX_AUXA_L;
	if (mixer_control & 0x00020000) ret |= MIX_AUXA_R;
	if (mixer_control & 0x00040000) ret |= MIX_AUXA_L_RAMP | MIX_AUXA_R_RAMP;
	if (mixer_control & 0x00080000) ret |= MIX_AUXA_S;
	if (mixer_control & 0x00100000) ret |= MIX_AUXA_S_RAMP;
	if (mixer_control & 0x00200000) ret |= MIX_AUXB_L;
	if (mixer_control & 0x00400000) ret |= MIX_AUXB_R;
	if (mixer_control & 0x00800000) ret |= MIX_AUXB_L_RAMP | MIX_AUXB_R_RAMP;
	if (mixer_control & 0x01000000) ret |= MIX_AUXB_S;
	if (mixer_control & 0x02000000) ret |= MIX_AUXB_S_RAMP;
	if (mixer_control & 0x04000000) ret |= MIX_AUXC_L;
	if (mixer_control & 0x08000000) ret |= MIX_AUXC_R;
	if (mixer_control & 0x10000000) ret |= MIX_AUXC_L_RAMP | MIX_AUXC_R_RAMP;
	if (mixer_control & 0x20000000) ret |= MIX_AUXC_S;
	if (mixer_control & 0x40000000) ret |= MIX_AUXC_S_RAMP;

	return (AXMixControl)ret;
}

void CUCode_NewAXWii::ProcessPBList(u32 pb_addr)
{
	const u32 spms = 32;

	AXPBWii pb;

	while (pb_addr)
	{
		AXBuffers buffers = {{
			m_samples_left,
			m_samples_right,
			m_samples_surround,
			m_samples_auxA_left,
			m_samples_auxA_right,
			m_samples_auxA_surround,
			m_samples_auxB_left,
			m_samples_auxB_right,
			m_samples_auxB_surround,
			m_samples_auxC_left,
			m_samples_auxC_right,
			m_samples_auxC_surround
		}};

		if (!ReadPB(pb_addr, pb))
			break;

		for (int curr_ms = 0; curr_ms < 3; ++curr_ms)
		{
			Process1ms(pb, buffers, ConvertMixerControl(HILO_TO_32(pb.mixer_control)));

			// Forward the buffers
			for (u32 i = 0; i < sizeof (buffers.ptrs) / sizeof (buffers.ptrs[0]); ++i)
				buffers.ptrs[i] += spms;
		}

		WritePB(pb_addr, pb);
		pb_addr = HILO_TO_32(pb.next_pb);
	}
}

void CUCode_NewAXWii::MixAUXSamples(int aux_id, u32 write_addr, u32 read_addr, u16 volume)
{
	int* buffers[3] = { 0 };
	int* main_buffers[3] = {
		m_samples_left,
		m_samples_right,
		m_samples_surround
	};

	switch (aux_id)
	{
	case 0:
		buffers[0] = m_samples_auxA_left;
		buffers[1] = m_samples_auxA_right;
		buffers[2] = m_samples_auxA_surround;
		break;

	case 1:
		buffers[0] = m_samples_auxB_left;
		buffers[1] = m_samples_auxB_right;
		buffers[2] = m_samples_auxB_surround;
		break;

	case 2:
		buffers[0] = m_samples_auxC_left;
		buffers[1] = m_samples_auxC_right;
		buffers[2] = m_samples_auxC_surround;
		break;
	}

	// Send the content of AUX buffers to the CPU
	if (write_addr)
	{
		int* ptr = (int*)HLEMemory_Get_Pointer(write_addr);
		for (u32 i = 0; i < 3; ++i)
			for (u32 j = 0; j < 3 * 32; ++j)
				*ptr++ = Common::swap32(buffers[i][j]);
	}

	// Then read the buffers from the CPU and add to our main buffers.
	int* ptr = (int*)HLEMemory_Get_Pointer(read_addr);
	for (u32 i = 0; i < 3; ++i)
		for (u32 j = 0; j < 3 * 32; ++j)
		{
			s64 new_val = main_buffers[i][j] + Common::swap32(*ptr++);
			main_buffers[i][j] = (new_val * volume) >> 15;
		}
}

void CUCode_NewAXWii::OutputSamples(u32 lr_addr, u32 surround_addr, u16 volume)
{
	int surround_buffer[3 * 32] = { 0 };

	for (u32 i = 0; i < 3 * 32; ++i)
		surround_buffer[i] = Common::swap32(m_samples_surround[i]);
	memcpy(HLEMemory_Get_Pointer(surround_addr), surround_buffer, sizeof (surround_buffer));

	short buffer[3 * 32 * 2];

	// Clamp internal buffers to 16 bits.
	for (u32 i = 0; i < 3 * 32; ++i)
	{
		int left  = m_samples_left[i];
		int right = m_samples_right[i];

		// Apply global volume. Cast to s64 to avoid overflow.
		left = ((s64)left * volume) >> 15;
		right = ((s64)right * volume) >> 15;

		if (left < -32767)  left = -32767;
		if (left > 32767)   left = 32767;
		if (right < -32767) right = -32767;
		if (right >  32767) right = 32767;

		m_samples_left[i] = left;
		m_samples_right[i] = right;
	}

	for (u32 i = 0; i < 3 * 32; ++i)
	{
		buffer[2 * i] = Common::swap16(m_samples_left[i]);
		buffer[2 * i + 1] = Common::swap16(m_samples_right[i]);
	}

	memcpy(HLEMemory_Get_Pointer(lr_addr), buffer, sizeof (buffer));
}

void CUCode_NewAXWii::OutputWMSamples(u32* addresses)
{
	int* buffers[] = {
		m_samples_wm0,
		m_samples_wm1,
		m_samples_wm2,
		m_samples_wm3
	};

	for (u32 i = 0; i < 4; ++i)
	{
		int* in = buffers[i];
		u16* out = (u16*)HLEMemory_Get_Pointer(addresses[i]);
		for (u32 j = 0; j < 3 * 6; ++j)
		{
			int sample = in[j];
			if (sample < -32767) sample = -32767;
			if (sample > 32767) sample = 32767;
			out[j] = Common::swap16((u16)sample);
		}
	}
}

void CUCode_NewAXWii::DoState(PointerWrap &p)
{
	std::lock_guard<std::mutex> lk(m_processing);

	DoStateShared(p);
	DoAXState(p);

	p.Do(m_samples_auxC_left);
	p.Do(m_samples_auxC_right);
	p.Do(m_samples_auxC_surround);

	p.Do(m_samples_wm0);
	p.Do(m_samples_wm1);
	p.Do(m_samples_wm2);
	p.Do(m_samples_wm3);

	p.Do(m_samples_aux0);
	p.Do(m_samples_aux1);
	p.Do(m_samples_aux2);
	p.Do(m_samples_aux3);
}
