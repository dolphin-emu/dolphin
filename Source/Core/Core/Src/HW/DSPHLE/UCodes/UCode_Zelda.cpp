// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

// Games that uses this UCode:
// Zelda: The Windwaker, Mario Sunshine, Mario Kart, Twilight Princess,
// Super Mario Galaxy

#include "UCodes.h"
#include "UCode_Zelda.h"
#include "../MailHandler.h"

#include "Mixer.h"

#include "WaveFile.h"
#include "../../DSP.h"
#include "ConfigManager.h"


CUCode_Zelda::CUCode_Zelda(DSPHLE *dsp_hle, u32 _CRC)
	: 
	IUCode(dsp_hle, _CRC),

	m_bSyncInProgress(false),
	m_MaxVoice(0),

	m_NumSyncMail(0),

	m_NumVoices(0),

	m_bSyncCmdPending(false),
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
	m_bListInProgress(false),
	m_step(0),

	m_readOffset(0),

	m_MailState(WaitForMail),

	m_NumPBs(0),
	m_PBAddress(0),
	m_PBAddress2(0)
{
	DEBUG_LOG(DSPHLE, "UCode_Zelda - add boot mails for handshake");

	if (IsLightVersion())
	{
		NOTICE_LOG(DSPHLE, "Luigi Stylee!");
		m_rMailHandler.PushMail(0x88881111);	
	}
	else
	{
		m_rMailHandler.PushMail(DSP_INIT);
		DSP::GenerateDSPInterruptFromDSPEmu(DSP::INT_DSP);
		m_rMailHandler.PushMail(0xF3551111); // handshake
	}

	m_VoiceBuffer = new s32[256 * 1024];
	m_ResampleBuffer = new s16[256 * 1024];
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

	delete [] m_VoiceBuffer;
	delete [] m_ResampleBuffer;
	delete [] m_LeftBuffer;
	delete [] m_RightBuffer;
}

u8 *CUCode_Zelda::GetARAMPointer(u32 address)
{
	if (IsDMAVersion())
		return (u8 *)(Memory::GetPointer(m_DMABaseAddr)) + address;
	else
		return (u8 *)(DSP::GetARAMPtr()) + address;
}

void CUCode_Zelda::Update(int cycles)
{
	if (!IsLightVersion())
	{
		if (m_rMailHandler.GetNextMail() == DSP_FRAME_END)
			DSP::GenerateDSPInterruptFromDSPEmu(DSP::INT_DSP);
	}

	if (NeedsResumeMail())
	{
		m_rMailHandler.PushMail(DSP_RESUME);
		DSP::GenerateDSPInterruptFromDSPEmu(DSP::INT_DSP);
	}
}

void CUCode_Zelda::HandleMail(u32 _uMail)
{
	if (IsLightVersion())
		HandleMail_LightVersion(_uMail);
	else if (IsSMSVersion())
		HandleMail_SMSVersion(_uMail);
	else
		HandleMail_NormalVersion(_uMail);
}

void CUCode_Zelda::HandleMail_LightVersion(u32 _uMail)
{
	//ERROR_LOG(DSPHLE, "Light version mail %08X, list in progress: %s, step: %i/%i", 
	//	_uMail, m_bListInProgress ? "yes":"no", m_step, m_numSteps);

	if (m_bSyncCmdPending)
	{
		DSP::GenerateDSPInterruptFromDSPEmu(DSP::INT_DSP);
		m_CurBuffer++;

		if (m_CurBuffer == m_NumBuffers)
		{
			soundStream->GetMixer()->SetHLEReady(true);
			m_bSyncCmdPending = false;
			DEBUG_LOG(DSPHLE, "Update the SoundThread to be in sync");
		}
		return;
	}

	if (!m_bListInProgress)
	{
		switch ((_uMail >> 24) & 0x7F)
		{
		case 0x00: m_numSteps = 1; break; // dummy
		case 0x01: m_numSteps = 5; break; // DsetupTable
		case 0x02: m_numSteps = 3; break; // DsyncFrame

		default:
			{
				m_numSteps = 0;
				PanicAlert("Zelda uCode (light version): unknown/unsupported command %02X", (_uMail >> 24) & 0x7F);
			}
			return;
		}

		m_bListInProgress = true;
		m_step = 0;
	}

	if (m_step >= sizeof(m_Buffer) / 4)
		PanicAlert("m_step out of range");

	((u32*)m_Buffer)[m_step] = _uMail;
	m_step++;

	if (m_step >= m_numSteps)
	{
		ExecuteList();
		m_bListInProgress = false;
	}
}

void CUCode_Zelda::HandleMail_SMSVersion(u32 _uMail)
{
	if (m_bSyncInProgress)
	{
		if (m_bSyncCmdPending)
		{
			m_SyncFlags[(m_NumSyncMail << 1)    ] = _uMail >> 16;
			m_SyncFlags[(m_NumSyncMail << 1) + 1] = _uMail & 0xFFFF;

			m_NumSyncMail++;
			if (m_NumSyncMail == 2)
			{
				m_NumSyncMail = 0;
				m_bSyncInProgress = false;

				m_CurBuffer++;

				m_rMailHandler.PushMail(DSP_SYNC);
				DSP::GenerateDSPInterruptFromDSPEmu(DSP::INT_DSP);
				m_rMailHandler.PushMail(0xF355FF00 | m_CurBuffer);

				if (m_CurBuffer == m_NumBuffers)
				{
					m_rMailHandler.PushMail(DSP_FRAME_END);
					//	DSP::GenerateDSPInterruptFromDSPEmu(DSP::INT_DSP);

					soundStream->GetMixer()->SetHLEReady(true);
					DEBUG_LOG(DSPHLE, "Update the SoundThread to be in sync");
//					soundStream->Update(); //do it in this thread to avoid sync problems

					m_bSyncCmdPending = false;
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
		if (m_step >= sizeof(m_Buffer) / 4)
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

	// Here holds: m_bSyncInProgress == false && m_bListInProgress == false

	if (_uMail == 0)
	{
		m_bSyncInProgress = true;
		m_NumSyncMail = 0;
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
		// Seeing as every possible operation number halts the uCode,
		// except 3, that thing seems to be intended for debugging
		switch (_uMail & 0xFFFF)
		{
		case 0x0003:		// Do nothing
			return;

		case 0x0000:		// Halt
		case 0x0001:		// Dump memory? and halt
		case 0x0002:		// Do something and halt
			WARN_LOG(DSPHLE, "Zelda uCode(SMS version): received halting operation %04X", _uMail & 0xFFFF);
			return;

		default:			// Invalid (the real ucode would likely crash)
			WARN_LOG(DSPHLE, "Zelda uCode(SMS version): received invalid operation %04X", _uMail & 0xFFFF);
			return;
		}
	}
	else
	{
		WARN_LOG(DSPHLE, "Zelda uCode (SMS version): unknown mail %08X", _uMail);
	}
}

void CUCode_Zelda::HandleMail_NormalVersion(u32 _uMail)
{
	// WARN_LOG(DSPHLE, "Zelda uCode: Handle mail %08X", _uMail);

	if (m_UploadSetupInProgress) // evaluated first!
	{
		PrepareBootUCode(_uMail);
		return;
	}

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
				DSP::GenerateDSPInterruptFromDSPEmu(DSP::INT_DSP);
				m_rMailHandler.PushMail(0xF355FF00 | m_CurBuffer);
					
				m_CurVoice = 0;

				if (m_CurBuffer == m_NumBuffers)
				{
					if (!IsDMAVersion()) // this is a hack... without it Pikmin 1 Wii/ Zelda TP Wii mail-s stopped
						m_rMailHandler.PushMail(DSP_FRAME_END);
					//g_dspInitialize.pGenerateDSPInterrupt();

					soundStream->GetMixer()->SetHLEReady(true);
					DEBUG_LOG(DSPHLE, "Update the SoundThread to be in sync");
//					soundStream->Update(); //do it in this thread to avoid sync problems

					m_bSyncCmdPending = false;
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
		if (m_step >= sizeof(m_Buffer) / 4)
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

	// Here holds: m_bSyncInProgress == false && m_bListInProgress == false

	// Zelda-only mails:
	// - 0000XXXX - Begin list
	// - 00000000, 000X0000 - Sync mails
	// - CDD1XXXX - comes after DsyncFrame completed, seems to be debugging stuff

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
	else if ((_uMail >> 16) == 0xCDD1)			// A 0xCDD1000X mail should come right after we send a DSP_FRAME_END mail
	{
		// The low part of the mail tells the operation to perform
		// Seeing as every possible operation number halts the uCode,
		// except 3, that thing seems to be intended for debugging
		switch (_uMail & 0xFFFF)
		{
		case 0x0003:	// Do nothing - continue normally
			return;

		case 0x0001:	// accepts params to either dma to iram and/or dram (used for hotbooting a new ucode)
			// TODO find a better way to protect from HLEMixer?
			soundStream->GetMixer()->SetHLEReady(false);
			m_UploadSetupInProgress = true;
			return;

		case 0x0002:	// Let IROM play us off
			m_DSPHLE->SetUCode(UCODE_ROM);
			return;

		case 0x0000:	// Halt
			WARN_LOG(DSPHLE, "Zelda uCode: received halting operation %04X", _uMail & 0xFFFF);
			return;

		default:		// Invalid (the real ucode would likely crash)
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
	u32 Sync;
	u32 ExtraData = CmdMail & 0xFFFF;

	if (IsLightVersion())
		Sync = 0x62 + (Command << 1); // seen in DSP_UC_Luigi.txt
	else
		Sync = CmdMail >> 16;

	DEBUG_LOG(DSPHLE, "==============================================================================");
	DEBUG_LOG(DSPHLE, "Zelda UCode - execute dlist (command: 0x%04x : sync: 0x%04x)", Command, Sync);

	switch (Command)
	{
		// dummy
		case 0x00: break;

		// DsetupTable ... zelda ww jumps to 0x0095
		case 0x01:
		{
			m_NumVoices = ExtraData;
			m_VoicePBsAddr = Read32() & 0x7FFFFFFF;
			m_UnkTableAddr = Read32() & 0x7FFFFFFF;
			m_AFCCoefTableAddr = Read32() & 0x7FFFFFFF;
			m_ReverbPBsAddr = Read32() & 0x7FFFFFFF;  // WARNING: reverb PBs are very different from voice PBs!

			// Read the other table
			u16 *TempPtr = (u16*)Memory::GetPointer(m_UnkTableAddr);
			for (int i = 0; i < 0x280; i++)
				m_MiscTable[i] = (s16)Common::swap16(TempPtr[i]);

			// Read AFC coef table
			TempPtr = (u16*)Memory::GetPointer(m_AFCCoefTableAddr);
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

			if (IsLightVersion())
				break;
			else
				return;
		}


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
			PanicAlert("Zelda UCode - unknown command: %x (size %i)", Command, m_numSteps);
			break;
	}

	// sync, we are ready
	if (IsLightVersion())
	{
		if (m_bSyncCmdPending)
			m_rMailHandler.PushMail(0x80000000 | m_NumBuffers); // after CMD_2
		else	
			m_rMailHandler.PushMail(0x80000000 | Sync); // after CMD_0, CMD_1
	}
	else
	{
		m_rMailHandler.PushMail(DSP_SYNC);
		DSP::GenerateDSPInterruptFromDSPEmu(DSP::INT_DSP);
		m_rMailHandler.PushMail(0xF3550000 | Sync);
	}
}

u32 CUCode_Zelda::GetUpdateMs()
{
	return SConfig::GetInstance().m_LocalCoreStartupParameter.bWii ? 3 : 5;
}

void CUCode_Zelda::DoState(PointerWrap &p)
{
	// It's bad if we try to save during Mix()
	std::lock_guard<std::mutex> lk(m_csMix);

	p.Do(m_AFCCoefTable);
	p.Do(m_MiscTable);

	p.Do(m_bSyncInProgress);
	p.Do(m_MaxVoice);
	p.Do(m_SyncFlags);

	p.Do(m_NumSyncMail);

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

	DoStateShared(p);
}
