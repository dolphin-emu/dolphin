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

#include "FileUtil.h" // For IsDirectory()
#include "StringUtil.h" // For StringFromFormat()
#include <sstream>

#include "Mixer.h"
#include "../MailHandler.h"
#include "../../DSP.h"
#include "UCodes.h"
#include "UCode_AXStructs.h"
#include "UCode_AX.h"
#include "UCode_AX_Voice.h"

CUCode_AX::CUCode_AX(DSPHLE *dsp_hle, u32 l_CRC)
	: IUCode(dsp_hle, l_CRC)
	, m_addressPBs(0xFFFFFFFF)
{
	// we got loaded
	m_rMailHandler.PushMail(DSP_INIT);

	templbuffer = new int[1024 * 1024];
	temprbuffer = new int[1024 * 1024];
}

CUCode_AX::~CUCode_AX()
{
	m_rMailHandler.Clear();
	delete [] templbuffer;
	delete [] temprbuffer;
}

// Needs A LOT of love!
static void ProcessUpdates(AXPB &PB)
{
	// Make the updates we are told to do. When there are multiple updates for a block they
	// are placed in memory directly following updaddr. They are mostly for initial time
	// delays, sometimes for the FIR filter or channel volumes. We do all of them at once here.
	// If we get both an on and an off update we chose on. Perhaps that makes the RE1 music
	// work better.
	int numupd = PB.updates.num_updates[0]
	+ PB.updates.num_updates[1]
	+ PB.updates.num_updates[2]
	+ PB.updates.num_updates[3]
	+ PB.updates.num_updates[4];
	if (numupd > 64) numupd = 64; // prevent crazy values TODO: LOL WHAT
	const u32 updaddr   = (u32)(PB.updates.data_hi << 16) | PB.updates.data_lo;
	int on = 0, off = 0;
	for (int j = 0; j < numupd; j++)
	{
		const u16 updpar = HLEMemory_Read_U16(updaddr + j*4);
		const u16 upddata = HLEMemory_Read_U16(updaddr + j*4 + 2);
		// some safety checks, I hope it's enough
		if (updaddr > 0x80000000 && updaddr < 0x817fffff
			&& updpar < 63 && updpar > 3 // updpar > 3 because we don't want to change
			// 0-3, those are important
			//&& (upd0 || upd1 || upd2 || upd3 || upd4) // We should use these in some way to I think
			// but I don't know how or when
			)
		{
			((u16*)&PB)[updpar] = upddata; // WTF ABOUNDS!
		}
		if (updpar == 7 && upddata != 0) on++;
		if (updpar == 7 && upddata == 0) off++;
	}
	// hack: if we get both an on and an off select on rather than off
	if (on > 0 && off > 0) PB.running = 1;
}

static void VoiceHacks(AXPB &pb)
{
	// get necessary values
	const u32 sampleEnd = (pb.audio_addr.end_addr_hi << 16) | pb.audio_addr.end_addr_lo;
	const u32 loopPos   = (pb.audio_addr.loop_addr_hi << 16) | pb.audio_addr.loop_addr_lo;
	// 		const u32 updaddr   = (u32)(pb.updates.data_hi << 16) | pb.updates.data_lo;
	// 		const u16 updpar    = HLEMemory_Read_U16(updaddr);
	// 		const u16 upddata   = HLEMemory_Read_U16(updaddr + 2);

	// =======================================================================================
	/* Fix problems introduced with the SSBM fix. Sometimes when a music stream ended sampleEnd
	would end up outside of bounds while the block was still playing resulting in noise 
	a strange noise. This should take care of that.
	*/
	if ((sampleEnd > (0x017fffff * 2) || loopPos > (0x017fffff * 2))) // ARAM bounds in nibbles
	{
		pb.running = 0;

		// also reset all values if it makes any difference
		pb.audio_addr.cur_addr_hi = 0; pb.audio_addr.cur_addr_lo = 0;
		pb.audio_addr.end_addr_hi = 0; pb.audio_addr.end_addr_lo = 0;
		pb.audio_addr.loop_addr_hi = 0; pb.audio_addr.loop_addr_lo = 0;

		pb.src.cur_addr_frac = 0; pb.src.ratio_hi = 0; pb.src.ratio_lo = 0;
		pb.adpcm.pred_scale = 0; pb.adpcm.yn1 = 0; pb.adpcm.yn2 = 0;

		pb.audio_addr.looping = 0;
		pb.adpcm_loop_info.pred_scale = 0;
		pb.adpcm_loop_info.yn1 = 0; pb.adpcm_loop_info.yn2 = 0;
	}

	/*
	// the fact that no settings are reset (except running) after a SSBM type music stream or another
	looping block (for example in Battle Stadium DON) has ended could cause loud garbled sound to be
	played from one or more blocks. Perhaps it was in conjunction with the old sequenced music fix below,
	I'm not sure. This was an attempt to prevent that anyway by resetting all. But I'm not sure if this
	is needed anymore. Please try to play SSBM without it and see if it works anyway.
	*/
	if (
		// detect blocks that have recently been running that we should reset
		pb.running == 0 && pb.audio_addr.looping == 1
		//pb.running == 0 && pb.adpcm_loop_info.pred_scale

		// this prevents us from ruining sequenced music blocks, may not be needed
		/*
		&& !(pb.updates.num_updates[0] || pb.updates.num_updates[1] || pb.updates.num_updates[2]
		|| pb.updates.num_updates[3] || pb.updates.num_updates[4])
		*/	
		//&& !(updpar || upddata)

		&& pb.mixer_control == 0	// only use this in SSBM
		)
	{
		// reset the detection values
		pb.audio_addr.looping = 0;
		pb.adpcm_loop_info.pred_scale = 0;
		pb.adpcm_loop_info.yn1 = 0; pb.adpcm_loop_info.yn2 = 0;

		//pb.audio_addr.cur_addr_hi = 0; pb.audio_addr.cur_addr_lo = 0;
		//pb.audio_addr.end_addr_hi = 0; pb.audio_addr.end_addr_lo = 0;
		//pb.audio_addr.loop_addr_hi = 0; pb.audio_addr.loop_addr_lo = 0;

		//pb.src.cur_addr_frac = 0; PBs[i].src.ratio_hi = 0; PBs[i].src.ratio_lo = 0;
		//pb.adpcm.pred_scale = 0; pb.adpcm.yn1 = 0; pb.adpcm.yn2 = 0;
	}
}

void CUCode_AX::MixAdd(short* _pBuffer, int _iSize)
{
	if (_iSize > 1024 * 1024)
		_iSize = 1024 * 1024;

	memset(templbuffer, 0, _iSize * sizeof(int));
	memset(temprbuffer, 0, _iSize * sizeof(int));

	AXPB PB;

	for (int x = 0; x < numPBaddr; x++) 
	{
		//u32 blockAddr = m_addressPBs;
		u32 blockAddr = PBaddr[x];

		if (!blockAddr)
			return;

		for (int i = 0; i < NUMBER_OF_PBS; i++)
		{
			if (!ReadPB(blockAddr, PB))
				break;

			if (m_CRC != 0x3389a79e)
				VoiceHacks(PB);

			MixAddVoice(PB, templbuffer, temprbuffer, _iSize);

			if (!WritePB(blockAddr, PB))
				break;

			// next PB, or done
			blockAddr = (PB.next_pb_hi << 16) | PB.next_pb_lo;
			if (!blockAddr)
				break;
		}
	}

	if (_pBuffer)
	{
		for (int i = 0; i < _iSize; i++)
		{
			// Clamp into 16-bit. Maybe we should add a volume compressor here.
			int left  = templbuffer[i] + _pBuffer[0];
			int right = temprbuffer[i] + _pBuffer[1];
			if (left < -32767)  left = -32767;
			if (left > 32767)   left = 32767;
			if (right < -32767) right = -32767;
			if (right >  32767) right = 32767;
			*_pBuffer++ = left;
			*_pBuffer++ = right;
		}
	}
}


// ------------------------------------------------------------------------------
// Handle incoming mail
void CUCode_AX::HandleMail(u32 _uMail)
{
	if (m_UploadSetupInProgress) 
	{
		PrepareBootUCode(_uMail);
		return;
	}
	else {	
		if ((_uMail & 0xFFFF0000) == MAIL_AX_ALIST)
		{
			// We are expected to get a new CmdBlock
			DEBUG_LOG(DSPHLE, "GetNextCmdBlock (%ibytes)", (u16)_uMail);
		}
		else if (_uMail == 0xCDD10000) // Action 0 - AX_ResumeTask();
		{
			m_rMailHandler.PushMail(DSP_RESUME);
		}
		else if (_uMail == 0xCDD10001) // Action 1 - new ucode upload ( GC: BayBlade S.T.B,...)
		{
			DEBUG_LOG(DSPHLE,"DSP IROM - New Ucode!");
			// TODO find a better way to protect from HLEMixer?
			soundStream->GetMixer()->SetHLEReady(false);
			m_UploadSetupInProgress = true;
		}
		else if (_uMail == 0xCDD10002) // Action 2 - IROM_Reset(); ( GC: NFS Carbon, FF Crystal Chronicles,...)
		{
			DEBUG_LOG(DSPHLE,"DSP IROM - Reset!");
			m_DSPHLE->SetUCode(UCODE_ROM);
			return;
		}
		else if (_uMail == 0xCDD10003) // Action 3 - AX_GetNextCmdBlock();
		{
		}
		else
		{
			DEBUG_LOG(DSPHLE, " >>>> u32 MAIL : AXTask Mail (%08x)", _uMail);
			AXTask(_uMail);
		}
	}
}


// ------------------------------------------------------------------------------
// Update with DSP Interrupt
void CUCode_AX::Update(int cycles)
{
	if (NeedsResumeMail())
	{
		m_rMailHandler.PushMail(DSP_RESUME);
		DSP::GenerateDSPInterruptFromDSPEmu(DSP::INT_DSP);
	}
	// check if we have to send something
	else if (!m_rMailHandler.IsEmpty())
	{
		DSP::GenerateDSPInterruptFromDSPEmu(DSP::INT_DSP);
	}
}

// ============================================
// AX seems to bootup one task only and waits for resume-callbacks
// everytime the DSP has "spare time" it sends a resume-mail to the CPU
// and the __DSPHandler calls a AX-Callback which generates a new AXFrame
bool CUCode_AX::AXTask(u32& _uMail)
{
	u32 uAddress = _uMail;
	DEBUG_LOG(DSPHLE, "Begin");
	DEBUG_LOG(DSPHLE, "=====================================================================");
	DEBUG_LOG(DSPHLE, "%08x : AXTask - AXCommandList-Addr:", uAddress);

	u32 Addr__AXStudio;
	u32 Addr__AXOutSBuffer;
	u32 Addr__AXOutSBuffer_1;
	u32 Addr__AXOutSBuffer_2;
	u32 Addr__A;
	//u32 Addr__12;
	u32 Addr__4_1;
	u32 Addr__4_2;
	//u32 Addr__4_3;
	//u32 Addr__4_4;
	u32 Addr__5_1;
	u32 Addr__5_2;
	u32 Addr__6;
	u32 Addr__9;

	bool bExecuteList = true;

	numPBaddr = 0;

	while (bExecuteList)
	{
		static int last_valid_command = 0;
		u16 iCommand = HLEMemory_Read_U16(uAddress);
		uAddress += 2;

		switch (iCommand)
		{
		case AXLIST_STUDIOADDR: //00
			Addr__AXStudio = HLEMemory_Read_U32(uAddress);
			uAddress += 4;
			DEBUG_LOG(DSPHLE, "%08x : AXLIST studio address: %08x", uAddress, Addr__AXStudio);
			break;

		case 0x001: // 2byte x 10
			{
				u32 address = HLEMemory_Read_U32(uAddress);
				uAddress += 4;
				u16 param1 = HLEMemory_Read_U16(uAddress);
				uAddress += 2;
				u16 param2 = HLEMemory_Read_U16(uAddress);
				uAddress += 2;
				u16 param3 = HLEMemory_Read_U16(uAddress);
				uAddress += 2;
				DEBUG_LOG(DSPHLE, "%08x : AXLIST 1: %08x, %04x, %04x, %04x", uAddress, address, param1, param2, param3);
			}
			break;

			//
			// Somewhere we should be getting a bitmask of AX_SYNC values
			// that tells us what has been updated
			// Dunno if important
			//
		case AXLIST_PBADDR: //02
			{
				PBaddr[numPBaddr] = HLEMemory_Read_U32(uAddress);
				numPBaddr++;

				// HACK: process updates right now instead of waiting until
				// Premix is called. Some games using sequenced music (Tales of
				// Symphonia for example) thought PBs were unused because we
				// were too slow to update them and set them as running. This
				// happens because Premix is basically completely desync-ed
				// from the emulation core (it's running in the audio thread).
				// Fixing this would require rewriting most of the AX HLE.
				u32 block_addr = uAddress;
				AXPB pb;
				for (int i = 0; block_addr && i < NUMBER_OF_PBS; i++)
				{
					if (!ReadPB(block_addr, pb))
						break;
					ProcessUpdates(pb);
					WritePB(block_addr, pb);
					block_addr = (pb.next_pb_hi << 16) | pb.next_pb_lo;
				}

				m_addressPBs = HLEMemory_Read_U32(uAddress); // left in for now
				uAddress += 4;
				soundStream->GetMixer()->SetHLEReady(true);
				DEBUG_LOG(DSPHLE, "%08x : AXLIST PB address: %08x", uAddress, m_addressPBs);
			}
			break;

		case 0x0003:
			DEBUG_LOG(DSPHLE, "%08x : AXLIST command 0x0003 ????", uAddress);
			break;

		case 0x0004:  // AUX?
			Addr__4_1 = HLEMemory_Read_U32(uAddress);
			uAddress += 4;
			Addr__4_2 = HLEMemory_Read_U32(uAddress);
			uAddress += 4;
			DEBUG_LOG(DSPHLE, "%08x : AXLIST 4_1 4_2 addresses: %08x %08x", uAddress, Addr__4_1, Addr__4_2);
			break;

		case 0x0005:
			Addr__5_1 = HLEMemory_Read_U32(uAddress);
			uAddress += 4;
			Addr__5_2 = HLEMemory_Read_U32(uAddress);
			uAddress += 4;
			DEBUG_LOG(DSPHLE, "%08x : AXLIST 5_1 5_2 addresses: %08x %08x", uAddress, Addr__5_1, Addr__5_2);
			break;

		case 0x0006:
			Addr__6   = HLEMemory_Read_U32(uAddress);
			uAddress += 4;
			DEBUG_LOG(DSPHLE, "%08x : AXLIST 6 address: %08x", uAddress, Addr__6);
			break;

		case AXLIST_SBUFFER:
			Addr__AXOutSBuffer = HLEMemory_Read_U32(uAddress);
			uAddress += 4;
			DEBUG_LOG(DSPHLE, "%08x : AXLIST OutSBuffer address: %08x", uAddress, Addr__AXOutSBuffer);
			break;

		case 0x0009:
			Addr__9   = HLEMemory_Read_U32(uAddress);
			uAddress += 4;
			DEBUG_LOG(DSPHLE, "%08x : AXLIST 6 address: %08x", uAddress, Addr__9);
			break;

		case AXLIST_COMPRESSORTABLE:  // 0xa
			Addr__A   = HLEMemory_Read_U32(uAddress);
			uAddress += 4;
			DEBUG_LOG(DSPHLE, "%08x : AXLIST CompressorTable address: %08x", uAddress, Addr__A);
			break;

		case 0x000e:
			Addr__AXOutSBuffer_1 = HLEMemory_Read_U32(uAddress);
			uAddress += 4;

			// Addr__AXOutSBuffer_2 is the address in RAM that we are supposed to mix to.
			// Although we don't, currently.
			Addr__AXOutSBuffer_2 = HLEMemory_Read_U32(uAddress);
			uAddress += 4;
			DEBUG_LOG(DSPHLE, "%08x : AXLIST sbuf2 addresses: %08x %08x", uAddress, Addr__AXOutSBuffer_1, Addr__AXOutSBuffer_2);
			break;

		case AXLIST_END:
			bExecuteList = false;
			DEBUG_LOG(DSPHLE, "%08x : AXLIST end", uAddress);
			break;

		case 0x0010:  //Super Monkey Ball 2
			DEBUG_LOG(DSPHLE, "%08x : AXLIST 0x0010", uAddress);
			//should probably read/skip stuff here
			uAddress += 8;
			break;

		case 0x0011:
			uAddress += 4;
			break;

		case 0x0012:
			//Addr__12  = HLEMemory_Read_U16(uAddress);
			uAddress += 2;
			break;

		case 0x0013:
			uAddress += 6 * 4; // 6 Addresses.
			break;

		default:
			{
				static bool bFirst = true;
				if (bFirst)
				{
					char szTemp[2048];
					sprintf(szTemp, "Unknown AX-Command 0x%x (address: 0x%08x). Last valid: %02x\n",
						iCommand, uAddress - 2, last_valid_command);
					int num = -32;
					while (num < 64+32)
					{
						char szTemp2[128] = "";
						sprintf(szTemp2, "%s0x%04x\n", num == 0 ? ">>" : "  ", HLEMemory_Read_U16(uAddress + num));
						strcat(szTemp, szTemp2);
						num += 2;
					}

					PanicAlert("%s", szTemp);
					// bFirst = false;
				}

				// unknown command so stop the execution of this TaskList
				bExecuteList = false;
			}
			break;
		}
		if (bExecuteList)
			last_valid_command = iCommand;
	}
	DEBUG_LOG(DSPHLE, "AXTask - done, send resume");
	DEBUG_LOG(DSPHLE, "=====================================================================");
	DEBUG_LOG(DSPHLE, "End");

	m_rMailHandler.PushMail(DSP_YIELD);
	return true;
}

void CUCode_AX::DoState(PointerWrap &p)
{
	std::lock_guard<std::mutex> lk(m_csMix);

	p.Do(numPBaddr);
	p.Do(m_addressPBs);
	p.Do(PBaddr);

	DoStateShared(p);
}
