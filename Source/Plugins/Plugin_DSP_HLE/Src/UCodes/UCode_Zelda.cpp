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

// Games that uses this UCode:
// Zelda: The Windwaker, Mario Sunshine, Mario Kart, Twilight Princess

#include "../Globals.h"
#include "UCodes.h"
#include "UCode_Zelda.h"
#include "../MailHandler.h"

#include "../main.h"
#include "Mixer.h"

#include "WaveFile.h"

CUCode_Zelda::CUCode_Zelda(CMailHandler& _rMailHandler, u32 _CRC)
	: 
	IUCode(_rMailHandler),
	m_CRC(_CRC),

	m_bSyncInProgress(0),
	m_MaxVoice(0),

	m_NumVoices(0),

	m_bSyncCmdPending(0),
	m_CurVoice(0),
	m_CurBuffer(0),
	m_NumBuffers(0),

	m_VoicePBsAddr(0),
	m_UnkTableAddr(0),
	m_ReverbPBsAddr(0),

	m_RightBuffersAddr(0),
	m_LeftBuffersAddr(0),
	m_pos(0),

	m_DMABaseAddr(0),

	m_numSteps(0),
	m_bListInProgress(0),
	m_step(0),

	m_readOffset(0),

	m_MailState(WaitForMail),

	m_NumPBs(0),
	m_PBAddress(0),
	m_PBAddress2(0)
{
	DEBUG_LOG(DSPHLE, "UCode_Zelda - add boot mails for handshake");
	m_rMailHandler.PushMail(DSP_INIT);
	g_dspInitialize.pGenerateDSPInterrupt();
	m_rMailHandler.PushMail(0xF3551111); // handshake

	m_TempBuffer = new s32[256 * 1024];
	m_LeftBuffer = new s32[256 * 1024];
	m_RightBuffer = new s32[256 * 1024];

	memset(m_Buffer, 0, sizeof(m_Buffer));
	memset(m_SyncFlags, 0, sizeof(m_SyncFlags));
	memset(m_AFCCoefTable, 0, sizeof(m_AFCCoefTable));
	memset(m_PBMask, 0, sizeof(m_PBMask));
}

CUCode_Zelda::~CUCode_Zelda()
{
	m_rMailHandler.Clear();

	delete [] m_TempBuffer;
	delete [] m_LeftBuffer;
	delete [] m_RightBuffer;
}

void CUCode_Zelda::Update(int cycles)
{
//	if (!m_rMailHandler.IsEmpty())
//		g_dspInitialize.pGenerateDSPInterrupt();
	if (m_bSyncCmdPending && (m_CurBuffer == m_NumBuffers) && (m_rMailHandler.IsEmpty()))
	{
		m_rMailHandler.PushMail(DSP_FRAME_END);
		g_dspInitialize.pGenerateDSPInterrupt();

		soundStream->GetMixer()->SetHLEReady(true);
		DEBUG_LOG(DSPHLE, "Update the SoundThread to be in sync");
		soundStream->Update(); //do it in this thread to avoid sync problems

		m_bSyncCmdPending = false;
	}
}

void CUCode_Zelda::HandleMail(u32 _uMail)
{
	// WARN_LOG(DSPHLE, "Zelda uCode: Handle mail %08X", _uMail);

	if (m_bSyncInProgress)
	{
		if (m_bSyncCmdPending)
		{
			u32 n = (_uMail >> 16) & 0xF;
			m_MaxVoice = (n + 1) << 4;
			m_SyncFlags[n] = _uMail & 0xFFFF;
			m_bSyncInProgress = false;

			// Normally, we should mix to the buffers used by the game.
			// We don't do it currently for a simple reason:
			// if the game runs fast all the time, then it's OK,
			// but if it runs slow, sound can become choppy.
			// This problem won't happen when mixing to the buffer
			// provided by MixAdd(), because the size of this buffer
			// is automatically adjusted if the game runs slow.
#if 0
			if (m_SyncFlags[n] & 0x8000)
			{
				for (; m_CurVoice < m_MaxVoice; m_CurVoice++)
				{
					if (m_CurVoice >= m_NumVoices)
						break;

					MixVoice(m_CurVoice);
				}
			}
			else
#endif
				m_CurVoice = m_MaxVoice;

			if (m_CurVoice >= m_NumVoices)
			{
				m_CurBuffer++;

				m_rMailHandler.PushMail(DSP_SYNC);
				g_dspInitialize.pGenerateDSPInterrupt();
				m_rMailHandler.PushMail(0xF355FF00 | m_CurBuffer);
					
				m_CurVoice = 0;

				if (m_CurBuffer == m_NumBuffers)
				{
					//m_rMailHandler.PushMail(DSP_FRAME_END);
					//g_dspInitialize.pGenerateDSPInterrupt();

				//	soundStream->GetMixer()->SetHLEReady(true);
				//	DEBUG_LOG(DSPHLE, "Update the SoundThread to be in sync");
				//	soundStream->Update(); //do it in this thread to avoid sync problems

					//m_bSyncCmdPending = false;
				}
			}
		}
		else
		{
			m_bSyncInProgress = false;
		}

		return;

    }

	if (m_bListInProgress)
	{
		if (m_step < 0 || m_step >= sizeof(m_Buffer)/4)
			PanicAlert("m_step out of range");

		((u32*)m_Buffer)[m_step] = _uMail;
		m_step++;

		if (m_step >= m_numSteps)
		{
			ExecuteList();
			m_bListInProgress = false;
		}

		return;
	}

	if (_uMail == 0) 
	{
		m_bSyncInProgress = true;
	} 
	else if ((_uMail >> 16) == 0) 
	{
		m_bListInProgress = true;
		m_numSteps = _uMail;
		m_step = 0;
	}
	else if ((_uMail >> 16) == 0xCDD1)			// A 0xCDD1000X mail should come right after we send a DSP_SYNCEND mail
	{
		// The low part of the mail tells the operation to perform
		switch (_uMail & 0xFFFF)
		{
		case 0x0003:		// Do nothing
			return;

		case 0x0000:		// Halt
		case 0x0001:		// Dump memory? and halt
		case 0x0002:		// Do something and halt
			WARN_LOG(DSPHLE, "Zelda uCode: received halting operation %04X", _uMail & 0xFFFF);
			return;

		default:			// Invalid (the real ucode would likely crash)
			WARN_LOG(DSPHLE, "Zelda uCode: received invalid operation %04X", _uMail & 0xFFFF);
			return;
		}
	}
	else 
	{
		WARN_LOG(DSPHLE, "Zelda uCode: unknown mail %08X", _uMail);
	}
}

// zelda debug ..803F6418
void CUCode_Zelda::ExecuteList()
{
	// begin with the list
	m_readOffset = 0;

	u32 CmdMail = Read32();
	u32 Command = (CmdMail >> 24) & 0x7f;
	u32 Sync = CmdMail >> 16;
	u32 ExtraData = CmdMail & 0xFFFF;

	DEBUG_LOG(DSPHLE, "==============================================================================");
	DEBUG_LOG(DSPHLE, "Zelda UCode - execute dlist (cmd: 0x%04x : sync: 0x%04x)", Command, Sync);

	switch (Command)
	{
		// DsetupTable ... zelda ww jumps to 0x0095
	    case 0x01:
	    {
			m_NumVoices = ExtraData;
			m_VoicePBsAddr = Read32() & 0x7FFFFFFF;
			m_UnkTableAddr = Read32() & 0x7FFFFFFF;
			m_AFCCoefTableAddr = Read32() & 0x7FFFFFFF;
			m_ReverbPBsAddr = Read32() & 0x7FFFFFFF;  // WARNING: reverb PBs are very different from voice PBs!

			// Read AFC coef table
			u16 *TempPtr = (u16*) g_dspInitialize.pGetMemoryPointer(m_AFCCoefTableAddr);
			for (int i = 0; i < 32; i++)
				m_AFCCoefTable[i] = (s16)Common::swap16(TempPtr[i]);

		    DEBUG_LOG(DSPHLE, "DsetupTable");
			DEBUG_LOG(DSPHLE, "Num voice param blocks:             %i", m_NumVoices);
			DEBUG_LOG(DSPHLE, "Voice param blocks address:         0x%08x", m_VoicePBsAddr);

			// This points to some strange data table. Don't know yet what it's for. Reverb coefs?
		    DEBUG_LOG(DSPHLE, "DSPRES_FILTER   (size: 0x40):       0x%08x", m_UnkTableAddr);

			// Zelda WW: This points to a 64-byte array of coefficients, which are EXACTLY the same
			// as the AFC ADPCM coef array in decode.c of the in_cube winamp plugin,
			// which can play Zelda audio. So, these should definitely be used when decoding AFC.
		    DEBUG_LOG(DSPHLE, "DSPADPCM_FILTER (size: 0x500):      0x%08x", m_AFCCoefTableAddr);
			DEBUG_LOG(DSPHLE, "Reverb param blocks address:        0x%08x", m_ReverbPBsAddr);
		}
			break;

			// SyncFrame ... zelda ww jumps to 0x0243
		case 0x02:
		{
			//	soundStream->GetMixer()->SetHLEReady(true);
			//	DEBUG_LOG(DSPHLE, "Update the SoundThread to be in sync");
			//soundStream->Update(); //do it in this thread to avoid sync problems

			m_bSyncCmdPending = true;
			m_CurBuffer = 0;
			m_NumBuffers = (CmdMail >> 16) & 0xFF;

			// Addresses for right & left buffers in main memory
			// Each buffer is 160 bytes long. The number of (both left & right) buffers
			// is set by the first mail of the list.
			m_RightBuffersAddr = Read32() & 0x7FFFFFFF;
			m_LeftBuffersAddr = Read32() & 0x7FFFFFFF;

		    DEBUG_LOG(DSPHLE, "DsyncFrame");
			// These alternate between three sets of mixing buffers. They are all three fairly near,
			// but not at, the ADMA read addresses.
		    DEBUG_LOG(DSPHLE, "Right buffer address:               0x%08x", m_RightBuffersAddr);
		    DEBUG_LOG(DSPHLE, "Left buffer address:                0x%08x", m_LeftBuffersAddr);

	    }
		return;


		// Simply sends the sync messages
		case 0x03: break;

/*		case 0x04: break;   // dunno ... zelda ww jmps to 0x0580
		case 0x05: break;   // dunno ... zelda ww jmps to 0x0592
		case 0x06: break;   // dunno ... zelda ww jmps to 0x0469
		case 0x07: break;   // dunno ... zelda ww jmps to 0x044d
		case 0x08: break;   // Mixer ... zelda ww jmps to 0x0485
		case 0x09: break;   // dunno ... zelda ww jmps to 0x044d
 */

		    // DsetDolbyDelay ... zelda ww jumps to 0x00b2
	    case 0x0d:
	    {
		    u32 tmp = Read32();
		    DEBUG_LOG(DSPHLE, "DSetDolbyDelay");
		    DEBUG_LOG(DSPHLE, "DOLBY2_DELAY_BUF (size 0x960):      0x%08x", tmp);
	    }
		    break;

			// This opcode, in the SMG ucode, sets the base address for audio data transfers from main memory (using DMA).
			// In the Zelda ucode, it is dummy, because this ucode uses accelerator for audio data transfers.
	    case 0x0e:
			{
				m_DMABaseAddr = Read32() & 0x7FFFFFFF;
				DEBUG_LOG(DSPHLE, "DsetDMABaseAddr");
				DEBUG_LOG(DSPHLE, "DMA base address:                   0x%08x", m_DMABaseAddr);
			}
		    break;

		// default ... zelda ww jumps to 0x0043
	    default:
		    PanicAlert("Zelda UCode - unknown cmd: %x (size %i)", Command, m_numSteps);
		    break;
	}

	// sync, we are ready
	m_rMailHandler.PushMail(DSP_SYNC);
	g_dspInitialize.pGenerateDSPInterrupt();
	m_rMailHandler.PushMail(0xF3550000 | Sync);
}

// size is in stereo samples.
void CUCode_Zelda::MixAdd(short* _Buffer, int _Size)
{
	if (_Size > 256 * 1024)
		_Size = 256 * 1024;

	memset(m_LeftBuffer, 0, _Size * sizeof(s32));
	memset(m_RightBuffer, 0, _Size * sizeof(s32));

	for (u32 i = 0; i < m_NumVoices; i++)
	{
		u32 flags = m_SyncFlags[(i >> 4) & 0xF];
		if (!(flags & 1 << (15 - (i & 0xF))))
			continue;

		ZeldaVoicePB pb;
		ReadVoicePB(m_VoicePBsAddr + (i * 0x180), pb);

		if (pb.Status == 0)
			continue;
		if (pb.KeyOff != 0)
			continue;

		RenderAddVoice(pb, m_LeftBuffer, m_RightBuffer, _Size);
		WritebackVoicePB(m_VoicePBsAddr + (i * 0x180), pb);
	}

	if (_Buffer)
	{
		for (u32 i = 0; i < _Size; i++)
		{
			s32 left  = (s32)_Buffer[0] + m_LeftBuffer[i];
			s32 right = (s32)_Buffer[1] + m_RightBuffer[i];

			if (left < -32768) left = -32768;
			if (left > 32767)  left = 32767;
			_Buffer[0] = (short)left;

			if (right < -32768) right = -32768;
			if (right > 32767)  right = 32767;
			_Buffer[1] = (short)right;

			_Buffer += 2;
		}
	}
}

void CUCode_Zelda::DoState(PointerWrap &p) {
	p.Do(m_CRC);

	p.Do(m_bSyncInProgress);
	p.Do(m_MaxVoice);
	p.Do(m_SyncFlags);

	p.Do(m_NumVoices);

	p.Do(m_bSyncCmdPending);
	p.Do(m_CurVoice);
	p.Do(m_CurBuffer);
	p.Do(m_NumBuffers);

	p.Do(m_VoicePBsAddr);
	p.Do(m_UnkTableAddr);
	p.Do(m_AFCCoefTableAddr);
	p.Do(m_ReverbPBsAddr);

	p.Do(m_RightBuffersAddr);
	p.Do(m_LeftBuffersAddr);
	p.Do(m_pos);

	p.Do(m_DMABaseAddr);

	p.Do(m_numSteps);
	p.Do(m_bListInProgress);
	p.Do(m_step);
	p.Do(m_Buffer);

	p.Do(m_readOffset);

	p.Do(m_MailState);
	p.Do(m_PBMask);

	p.Do(m_NumPBs);
	p.Do(m_PBAddress);
	p.Do(m_PBAddress2);
}

