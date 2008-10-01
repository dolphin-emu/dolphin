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

#include "../Globals.h"

#ifdef _WIN32
#include "../PCHW/DSoundStream.h"
#endif
#include "../PCHW/Mixer.h"
#include "../MailHandler.h"

#include "UCodes.h"
#include "UCode_AXStructs.h"
#include "UCode_AX.h"

#include "../Debugger/Debugger.h"
// ---------------------------------------------------------------------------------------
// Externals
// -----------
extern float ratioFactor;
extern CDebugger* m_frame;
// -----------

CUCode_AX::CUCode_AX(CMailHandler& _rMailHandler, bool wii)
	: IUCode(_rMailHandler)
	, m_addressPBs(0xFFFFFFFF)
	, wii_mode(wii)
{
	// we got loaded
	m_rMailHandler.PushMail(0xDCD10000);
	m_rMailHandler.PushMail(0x80000000);  // handshake ??? only (crc == 0xe2136399) needs it ...

	templbuffer = new int[1024 * 1024];
	temprbuffer = new int[1024 * 1024];
}

CUCode_AX::~CUCode_AX()
{
	m_rMailHandler.Clear();
	delete [] templbuffer;
	delete [] temprbuffer;
}

void CUCode_AX::HandleMail(u32 _uMail)
{
	if ((_uMail & 0xFFFF0000) == MAIL_AX_ALIST)
	{
		// a new List
	}
	else
	{
		AXTask(_uMail);
	}
}

s16 CUCode_AX::ADPCM_Step(AXParamBlock& pb, u32& samplePos, u32 newSamplePos, u16 frac)
{
	PBADPCMInfo &adpcm = pb.adpcm;

	while (samplePos < newSamplePos)
	{
		if ((samplePos & 15) == 0)
		{
			adpcm.pred_scale = g_dspInitialize.pARAM_Read_U8((samplePos & ~15) >> 1);
			samplePos    += 2;
			newSamplePos += 2;
		}

		int scale = 1 << (adpcm.pred_scale & 0xF);
		int coef_idx = adpcm.pred_scale >> 4;

		s32 coef1 = adpcm.coefs[coef_idx * 2 + 0];
		s32 coef2 = adpcm.coefs[coef_idx * 2 + 1];

		int temp = (samplePos & 1) ?
			   (g_dspInitialize.pARAM_Read_U8(samplePos >> 1) & 0xF) :
			   (g_dspInitialize.pARAM_Read_U8(samplePos >> 1) >> 4);

		if (temp >= 8)
			temp -= 16;

		// 0x400 = 0.5 in 11-bit fixed point
		int val = (scale * temp) + ((0x400 + coef1 * adpcm.yn1 + coef2 * adpcm.yn2) >> 11);

		if (val > 0x7FFF)
			val = 0x7FFF;
		else if (val < -0x7FFF)
			val = -0x7FFF;
		
		adpcm.yn2 = adpcm.yn1;
		adpcm.yn1 = val;

		samplePos++;
	}

	return adpcm.yn1;
}

void ADPCM_Loop(AXParamBlock& pb)
{
	if (!pb.is_stream)
	{
		pb.adpcm.yn1 = pb.adpcm_loop_info.yn1;
		pb.adpcm.yn2 = pb.adpcm_loop_info.yn2;
		pb.adpcm.pred_scale = pb.adpcm_loop_info.pred_scale;
	}
	//else stream and we should not attempt to replace values
}

void CUCode_AX::MixAdd(short* _pBuffer, int _iSize)
{
	AXParamBlock PBs[NUMBER_OF_PBS];

	if (_iSize > 1024 * 1024)
		_iSize = 1024 * 1024;

	memset(templbuffer, 0, _iSize * sizeof(int));
	memset(temprbuffer, 0, _iSize * sizeof(int));
	// read out pbs
	int numberOfPBs = ReadOutPBs(PBs, NUMBER_OF_PBS);
#ifdef _WIN32
	ratioFactor = 32000.0f / (float)DSound::DSound_GetSampleRate();
#else
	ratioFactor = 32000.0f / 44100.0f;
#endif

	// write logging data to debugger
	if(m_frame)
	{
		CUCode_AX::Logging(_pBuffer, _iSize, 0);
	}


	for (int i = 0; i < numberOfPBs; i++)
	{
		AXParamBlock& pb = PBs[i];



		// =======================================================================================
		// Sequenced music fix - This seems to work allright. I'm not sure which detection method cause
		// the least side effects, but pred_scale seems to be nice and simple. Please report any side
		// effects.
		// ------------
		if (!pb.running && pb.adpcm_loop_info.pred_scale)	
		/*
		if (!pb.running && 
			(pb.updates.num_updates[0] || pb.updates.num_updates[1] || pb.updates.num_updates[2]
			|| pb.updates.num_updates[3] || pb.updates.num_updates[4])
			)	
		*/
		{
			pb.running = true;
		}
		// =============



		// =======================================================================================
		/*
		Fix a problem introduced with the SSBM fix - Sometimes when a music stream ended sampleEnd
		would become extremely high and the game would play random sound data from ARAM resulting in
		a strange noise. This should take care of that. However, when you leave the Continue menu there's
		some kind of buzing or interference noise in the music. But it goes away, so I guess it's not a
		big issue. Please report any side effects.
		*/
		// ------------
		const u32 sampleEnd = (pb.audio_addr.end_addr_hi << 16) | pb.audio_addr.end_addr_lo;
		if (sampleEnd > 0x10000000)
		{
			pb.running = 0;

			// also reset all values if it makes any difference
			pb.audio_addr.cur_addr_hi = 0;
			pb.audio_addr.cur_addr_lo = 0;
			pb.audio_addr.end_addr_hi = 0;
			pb.audio_addr.end_addr_lo = 0;
			pb.audio_addr.loop_addr_hi = 0;
			pb.audio_addr.loop_addr_lo = 0;

			pb.audio_addr.looping = 0;
			pb.adpcm_loop_info.pred_scale = 0;
			pb.adpcm_loop_info.yn1 = 0;
			pb.adpcm_loop_info.yn2 = 0;
		}
		// =============

		if (pb.running)
		{
			// =======================================================================================
			// Set initial parameters
			// ------------
			//constants
			const u32 loopPos   = (pb.audio_addr.loop_addr_hi << 16) | pb.audio_addr.loop_addr_lo;			
			const u32 ratio     = (u32)(((pb.src.ratio_hi << 16) + pb.src.ratio_lo) * ratioFactor);

			//variables
			u32 samplePos = (pb.audio_addr.cur_addr_hi << 16) | pb.audio_addr.cur_addr_lo;
			u32 frac = pb.src.cur_addr_frac;
			// =============

			

			// =======================================================================================
			// Handle no-src streams - No src streams have pb.src_type == 2 and have pb.src.ratio_hi = 0
			// and pb.src.ratio_lo = 0. We handle that by setting the sampling ratio integer to 1. This
			// makes samplePos update in the correct way.
			// ---------------------------------------------------------------------------------------
			// Stream settings
				// src_type = 2 (most other games have src_type = 0)
			// ------------
			// Affected games:
				// Baten Kaitos - Eternal Wings (2003)
				// Baten Kaitos - Origins (2006)?
				// ?
			// ------------
			if(pb.src_type == 2)
			{
				pb.src.ratio_hi = 1;
			}
			// =============


			// =======================================================================================
			// Games that use looping to play non-looping music streams. SSBM has info in all pb.adpcm_loop_info
			// parameters but has pb.audio_addr.looping = 0. If we treat these streams like any other looping
			// streams the music works.
			// ---------------------------------------------------------------------------------------
			if(pb.adpcm_loop_info.pred_scale || pb.adpcm_loop_info.yn1 || pb.adpcm_loop_info.yn2)
			{
				pb.audio_addr.looping = 1;
			}
			// =======================================================================================


			// =======================================================================================
			// Streaming music and volume - A lot of music in Paper Mario use the exact same settings, namely
			// these:	
				// Base settings
					// is_stream = 1
					// src_type = 0
					// coef (unknown1) = 1
				// PBAudioAddr
					// audio_addr.looping = 1 (adpcm_loop_info.pred_scale = value, .yn1 = 0, .yn2 = 0)			
			// However. Some of the ingame music and seemingly randomly some other music incorrectly get
			// volume = 0 for both left and right. There's also an issue of a hanging very similar to the Baten
			// hanging. The Baten issue fixed itself when the music stream was allowed to play to the end and
			// then stop. However, all five music streams that is playing when the gate locks up in Paper Mario
			// is loooping streams... I don't know what may be wrong.
			// ---------------------------------------------------------------------------------------
			// A game that may be used as a comparison is Starfox Assault also has is_stream = 1, but it
			// has src_type = 1, coef (unknown1) = 0 and its pb.src.ratio_lo (fraction) != 0
			// =======================================================================================


			// =======================================================================================
			// Walk through _iSize
			for (int s = 0; s < _iSize; s++)
			{
				int sample = 0;
				frac += ratio;
				u32 newSamplePos = samplePos + (frac >> 16); //whole number of frac


				// =======================================================================================
				// Process sample format
				// ---------------------------------------------------------------------------------------
				switch (pb.audio_addr.sample_format)
				{
				    case AUDIOFORMAT_PCM8:
					    pb.adpcm.yn2 = pb.adpcm.yn1; //save last sample
					    pb.adpcm.yn1 = ((s8)g_dspInitialize.pARAM_Read_U8(samplePos)) << 8;

					    if (pb.src_type == SRCTYPE_NEAREST)
					    {
						    sample = pb.adpcm.yn1;
					    }
					    else //linear interpolation
					    {
						    sample = (pb.adpcm.yn1 * (u16)frac + pb.adpcm.yn2 * (u16)(0xFFFF - frac)) >> 16;
					    }

					    samplePos = newSamplePos;
					    break;

				    case AUDIOFORMAT_PCM16:
					    pb.adpcm.yn2 = pb.adpcm.yn1; //save last sample
					    pb.adpcm.yn1 = (s16)(u16)((g_dspInitialize.pARAM_Read_U8(samplePos * 2) << 8) | (g_dspInitialize.pARAM_Read_U8((samplePos * 2 + 1))));
					    if (pb.src_type == SRCTYPE_NEAREST)
						    sample = pb.adpcm.yn1;
					    else //linear interpolation
						    sample = (pb.adpcm.yn1 * (u16)frac + pb.adpcm.yn2 * (u16)(0xFFFF - frac)) >> 16;

					    samplePos = newSamplePos;
					    break;

				    case AUDIOFORMAT_ADPCM:
					    sample = ADPCM_Step(pb, samplePos, newSamplePos, frac);
					    break;

				    default:
					    break;
				}
				// =======================================================================================


				// =======================================================================================
				// Volume control
				frac &= 0xffff;

				int vol = pb.vol_env.cur_volume >> 9;
				sample = sample * vol >> 8;

				if (pb.mixer_control & MIXCONTROL_RAMPING)
				{
					int x = pb.vol_env.cur_volume;
					x += pb.vol_env.cur_volume_delta;
					if (x < 0) x = 0;
					if (x >= 0x7fff) x = 0x7fff;
					pb.vol_env.cur_volume = x; // maybe not per sample?? :P
				}

				int leftmix  = pb.mixer.volume_left >> 5;
				int rightmix = pb.mixer.volume_right >> 5;
				// =======================================================================================


				int left  = sample * leftmix >> 8;
				int right = sample * rightmix >> 8;

				//adpcm has to walk from oldSamplePos to samplePos here
				templbuffer[s] += left;
				temprbuffer[s] += right;

				if (samplePos >= sampleEnd)
				{
					if (pb.audio_addr.looping == 1)
					{
						samplePos = loopPos;
						if (pb.audio_addr.sample_format == AUDIOFORMAT_ADPCM)
							ADPCM_Loop(pb);
					}
					else
					{
						pb.running = 0;
						break;
					}
				}
			} // end of the _iSize loop
			// =======================================================================================


			pb.src.cur_addr_frac = (u16)frac;
			pb.audio_addr.cur_addr_hi = samplePos >> 16;
			pb.audio_addr.cur_addr_lo = (u16)samplePos;
		}
	}

	for (int i = 0; i < _iSize; i++)
	{
		// Clamp into 16-bit. Maybe we should add a volume compressor here.
		int left  = templbuffer[i];
		int right = temprbuffer[i];
		if (left < -32767)  left = -32767;
		if (left > 32767)   left = 32767;
		if (right < -32767) right = -32767;
		if (right >  32767) right = 32767;
		*_pBuffer++ += left;
		*_pBuffer++ += right;
	}

	// write back out pbs
	WriteBackPBs(PBs, numberOfPBs);
}

void CUCode_AX::Update()
{
	// check if we have to sent something
	if (!m_rMailHandler.IsEmpty())
	{
		g_dspInitialize.pGenerateDSPInterrupt();
	}
}

// AX seems to bootup one task only and waits for resume-callbacks
// everytime the DSP has "spare time" it sends a resume-mail to the CPU
// and the __DSPHandler calls a AX-Callback which generates a new AXFrame
bool CUCode_AX::AXTask(u32& _uMail)
{
	u32 uAddress = _uMail;
	DebugLog("AXTask - AXCommandList-Addr: 0x%08x", uAddress);

	u32 Addr__AXStudio;
	u32 Addr__AXOutSBuffer;
	u32 Addr__AXOutSBuffer_1;
	u32 Addr__AXOutSBuffer_2;
	u32 Addr__A;
	u32 Addr__12;
	u32 Addr__4_1;
	u32 Addr__4_2;
	u32 Addr__5_1;
	u32 Addr__5_2;
	u32 Addr__6;
	u32 Addr__9;

	bool bExecuteList = true;

	while (bExecuteList)
	{
		static int last_valid_command = 0;
		u16 iCommand = Memory_Read_U16(uAddress);
		uAddress += 2;
		switch (iCommand)
		{
	    case AXLIST_STUDIOADDR: //00
		    Addr__AXStudio = Memory_Read_U32(uAddress);
		    uAddress += 4;
			if (wii_mode)
				uAddress += 6;
		    DebugLog("AXLIST studio address: %08x", Addr__AXStudio);
		    break;

	    case 0x001:
	    {
		    u32 address = Memory_Read_U32(uAddress);
		    uAddress += 4;
		    u16 param1 = Memory_Read_U16(uAddress);
		    uAddress += 2;
		    u16 param2 = Memory_Read_U16(uAddress);
		    uAddress += 2;
		    u16 param3 = Memory_Read_U16(uAddress);
		    uAddress += 2;
		    DebugLog("AXLIST 1: %08x, %04x, %04x, %04x", address, param1, param2, param3);
	    }
		    break;

	    //
	    // Somewhere we should be getting a bitmask of AX_SYNC values
	    // that tells us what has been updated
	    // Dunno if important
	    //
	    case AXLIST_PBADDR: //02
		    {
		    m_addressPBs = Memory_Read_U32(uAddress);
		    uAddress += 4;

		    mixer_HLEready = true;
		    DebugLog("AXLIST PB address: %08x", m_addressPBs);
#ifdef _WIN32
		    DebugLog("Update the SoundThread to be in sync");
		    DSound::DSound_UpdateSound(); //do it in this thread to avoid sync problems
#endif
		    }
		    break;

	    case 0x0003:
		    DebugLog("AXLIST command 0x0003 ????");
		    break;

	    case 0x0004:
		    Addr__4_1 = Memory_Read_U32(uAddress);
		    uAddress += 4;
		    Addr__4_2 = Memory_Read_U32(uAddress);
		    uAddress += 4;
		    DebugLog("AXLIST 4_1 4_2 addresses: %08x %08x", Addr__4_1, Addr__4_2);
		    break;

	    case 0x0005:
		    Addr__5_1 = Memory_Read_U32(uAddress);
		    uAddress += 4;
		    Addr__5_2 = Memory_Read_U32(uAddress);
		    uAddress += 4;
		    DebugLog("AXLIST 5_1 5_2 addresses: %08x %08x", Addr__5_1, Addr__5_2);
		    break;

	    case 0x0006:
		    Addr__6   = Memory_Read_U32(uAddress);
		    uAddress += 4;
		    DebugLog("AXLIST 6 address: %08x", Addr__6);
		    break;

	    case AXLIST_SBUFFER:
			// Hopefully this is where in main ram to write.
		    Addr__AXOutSBuffer = Memory_Read_U32(uAddress);
		    uAddress += 4;
			if (wii_mode) {
				uAddress += 12;
			}
		    DebugLog("AXLIST OutSBuffer address: %08x", Addr__AXOutSBuffer);
		    break;

	    case 0x0009:
		    Addr__9   = Memory_Read_U32(uAddress);
		    uAddress += 4;
		    DebugLog("AXLIST 6 address: %08x", Addr__9);
		    break;

	    case AXLIST_COMPRESSORTABLE:  // 0xa
		    Addr__A   = Memory_Read_U32(uAddress);
		    uAddress += 4;
			if (wii_mode) {
				// There's one more here.
//				uAddress += 4;
			}
		    DebugLog("AXLIST CompressorTable address: %08x", Addr__A);
		    break;

	    case 0x000e:
		    Addr__AXOutSBuffer_1 = Memory_Read_U32(uAddress);
		    uAddress += 4;
		    Addr__AXOutSBuffer_2 = Memory_Read_U32(uAddress);
		    uAddress += 4;
		    DebugLog("AXLIST sbuf2 addresses: %08x %08x", Addr__AXOutSBuffer_1, Addr__AXOutSBuffer_2);
		    break;

	    case AXLIST_END:
		    bExecuteList = false;
		    DebugLog("AXLIST end");
		    break;

	    case 0x0010:  //Super Monkey Ball 2
		    DebugLog("AXLIST unknown");
		    //should probably read/skip stuff here
		    uAddress += 8;
		    break;

	    case 0x0011:
		    uAddress += 4;
		    break;

	    case 0x0012:
		    Addr__12  = Memory_Read_U16(uAddress);
		    uAddress += 2;
		    break;

	    case 0x0013:
		    uAddress += 6 * 4; // 6 Addresses.
		    break;

		case 0x000d:
			if (wii_mode) {
				uAddress += 4 * 4;  // 4 addresses. another aux?
				break;
			}
			// non-wii : fall through

		case 0x000b:
			if (wii_mode) {
				uAddress += 2;  // one 0x8000 in rabbids
				uAddress += 4 * 2; // then two RAM addressses
				break;
			}
			// non-wii : fall through

	    default:
			{
		    static bool bFirst = true;
		    if (bFirst == true)
		    {
			    char szTemp[2048];
				sprintf(szTemp, "Unknown AX-Command 0x%x (address: 0x%08x). Last valid: %02x\n",
					    iCommand, uAddress - 2, last_valid_command);
			    int num = -32;
			    while (num < 64+32)
			    {
				    char szTemp2[128] = "";
					sprintf(szTemp2, "%s0x%04x\n", num == 0 ? ">>" : "  ", Memory_Read_U16(uAddress + num));
				    strcat(szTemp, szTemp2);
				    num += 2;
			    }

			    PanicAlert(szTemp);
			    bFirst = false;
		    }

		    // unknown command so stop the execution of this TaskList
		    bExecuteList = false;
			}
			break;
		}
		if (bExecuteList)
			last_valid_command = iCommand;
	}
	DebugLog("AXTask - done, send resume");

	// i hope resume is okay AX
	m_rMailHandler.PushMail(0xDCD10001);
	return true;
}

int CUCode_AX::ReadOutPBs(AXParamBlock* _pPBs, int _num)
{
	int count = 0;
	u32 blockAddr = m_addressPBs;

	// reading and 'halfword' swap
	for (int i = 0; i < _num; i++)
	{
		const short *pSrc = (const short *)g_dspInitialize.pGetMemoryPointer(blockAddr);
		if (pSrc != NULL)
		{
			short *pDest = (short *)&_pPBs[i];
			for (size_t p = 0; p < sizeof(AXParamBlock) / 2; p++)
			{
				pDest[p] = Common::swap16(pSrc[p]);
			}
			blockAddr = (_pPBs[i].next_pb_hi << 16) | _pPBs[i].next_pb_lo;
			count++;
		}
		else
			break;
	}

	// return the number of readed PBs
	return count;
}

void CUCode_AX::WriteBackPBs(AXParamBlock* _pPBs, int _num)
{
	u32 blockAddr = m_addressPBs;

	// write back and 'halfword'swap
	for (int i = 0; i < _num; i++)
	{
		short* pSrc  = (short*)&_pPBs[i];
		short* pDest = (short*)g_dspInitialize.pGetMemoryPointer(blockAddr);
		for (size_t p = 0; p < sizeof(AXParamBlock) / 2; p++)
		{
			pDest[p] = Common::swap16(pSrc[p]);
		}

		// next block
		blockAddr = (_pPBs[i].next_pb_hi << 16) | _pPBs[i].next_pb_lo;
	}
}
