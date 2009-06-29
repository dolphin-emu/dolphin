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
#include "UCode_Zelda_ADPCM.h"
#include "../MailHandler.h"

#include "../main.h"
#include "Mixer.h"

#include "WaveFile.h"

CUCode_Zelda::CUCode_Zelda(CMailHandler& _rMailHandler, u32 _CRC)
	: IUCode(_rMailHandler)
	, m_CRC(_CRC)
	, m_bSyncInProgress(false)
	, m_MaxVoice(0)
	, m_NumVoices(0)
	, m_bSyncCmdPending(false)
	, m_CurVoice(0)
	, m_CurBuffer(0)
	, m_NumBuffers(0)
	, m_VoicePBsAddr(0)
	, m_UnkTableAddr(0)
	, m_AFCCoefTableAddr(0)
	, m_ReverbPBsAddr(0)
	, m_RightBuffersAddr(0)
	, m_LeftBuffersAddr(0)
	, m_DMABaseAddr(0)
	, m_numSteps(0)
	, m_step(0)
	, m_readOffset(0)
    , m_MailState(WaitForMail)
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

	m_pos = 0;
}

CUCode_Zelda::~CUCode_Zelda()
{
	m_rMailHandler.Clear();

	delete [] m_TempBuffer;
	delete [] m_LeftBuffer;
	delete [] m_RightBuffer;
}

#if 0
void CUCode_Zelda::UpdatePB(ZPB& _rPB, int *templbuffer, int *temprbuffer, u32 _Size)
{
    u16* pTest = (u16*)&_rPB;

	// Checks at 0293
    if (pTest[0x00] == 0) 
        return;

    if (pTest[0x01] != 0)
        return;
    

    if (pTest[0x06] != 0x00)
    {
        // probably pTest[0x06] == 0 -> AFC (and variants)
		// See 02a4
    }
    else
    {
        switch (_rPB.type)  // or Bytes per Sample
        {
        case 0x05:
        case 0x09:
            {
                // initialize "decoder" if the sample is played the first time
                if (pTest[0x04] != 0)
                {
					// This is 0717_ReadOutPBStuff

					// increment 4fb

                    // zelda: 
                    // perhaps init or "has played before"
                    pTest[0x32] = 0x00;
					pTest[0x66] = 0x00;     // history1 
                    pTest[0x67] = 0x00;     // history2 

                    // samplerate? length? num of samples? i dunno...
					// Likely length...
                    pTest[0x3a] = pTest[0x8a];
                    pTest[0x3b] = pTest[0x8b];

                    // Copy ARAM addr from r to rw area.
                    pTest[0x38] = pTest[0x8c];
                    pTest[0x39] = pTest[0x8d];
                }

                if (pTest[0x01] != 0)  // 0747 early out... i dunno if this can happen because we filter it above
                    return;

                u32 ARAMAddr        = (pTest[0x38] << 16) | pTest[0x39];
                u32 NumberOfSamples = (pTest[0x3a] << 16) | pTest[0x3b];

				// round upwards how many samples we need to copy, 0759
                NumberOfSamples = (NumberOfSamples + 0xf) >> 4;   // i think the lower 4 are the fraction
                u32 frac = NumberOfSamples & 0xF;

                u8 inBuffer[9];
                short outbuf[16];
                u32 sampleCount = 0;

				// It must be something like this:

				// The PB contains a small sample buffer of 0x4D decoded samples.
				// If it's empty or "used", decode to it.
				// Then, resample from this buffer to the output as you go. When it needs
				// wrapping, decode more.
				
#define USE_RESAMPLE
#if !defined(USE_RESAMPLE)
                for (int s = 0; s < _Size/16; s++)
                {
                    for (int i = 0; i < 9; i++)    
                    {
                        inBuffer[i] = g_dspInitialize.pARAM_Read_U8(ARAMAddr);
                        ARAMAddr++;
                    }
                                        
                    AFCdecodebuffer((char*)inBuffer, outbuf, (short*)&pTest[0x66], (short*)&pTest[0x67]);

                    for (int i = 0; i < 16; i++)
                    {
                        templbuffer[sampleCount] += outbuf[i];
                        temprbuffer[sampleCount] += outbuf[i];
                        sampleCount++;
                    }

                    NumberOfSamples--;

                    if (NumberOfSamples<=0)
                        break;
                }
#else
                while (NumberOfSamples > 0)
                {
                    for (int i = 0; i < 9; i++)    
                    {
                        inBuffer[i] =  g_dspInitialize.pARAM_Read_U8(ARAMAddr);
                        ARAMAddr++;
                    }

                    AFCdecodebuffer(m_AFCCoefTable, (char*)inBuffer, outbuf, (short*)&pTest[0x66], (short*)&pTest[0x67], 9);
                    CResampler Sampler(outbuf, 16, 48000);

                    while (Sampler.m_queueSize > 0) 
                    {
                        int sample = Sampler.sample_queue.front();
                        Sampler.sample_queue.pop();
                        Sampler.m_queueSize -= 1;

                        templbuffer[sampleCount] += sample;
                        temprbuffer[sampleCount] += sample;
                        sampleCount++;

                        if (sampleCount > _Size)
                            break;
                    }

                    if (sampleCount > _Size)
                        break;

                    NumberOfSamples--;
                }
#endif
                if (NumberOfSamples == 0)
                {
                    pTest[0x01] = 1; // we are done ??
                }

                // write back
                NumberOfSamples = (NumberOfSamples << 4);    // missing fraction

                pTest[0x38] = ARAMAddr >> 16;
                pTest[0x39] = ARAMAddr & 0xFFFF;
                pTest[0x3a] = NumberOfSamples >> 16;
                pTest[0x3b] = NumberOfSamples & 0xFFFF;
                

#if 0
                NumberOfSamples = (NumberOfSamples + 0xf) >> 4;
                
                static u8 Buffer[500000];
                for (int i =0; i<NumberOfSamples*9; i++)
                {
                    Buffer[i] = g_dspInitialize.pARAM_Read_U8(ARAMAddr+i);
                }

                // yes, the dumps are really zelda sound ;)
                DumpAFC(Buffer, NumberOfSamples*9, 0x3d00);

                DumpPB(_rPB);

             //   exit(1);
#endif

                // i think  pTest[0x3a] and pTest[0x3b] got an update after you have decoded some samples...
                // just decrement them with the number of samples you have played
                // and incrrease the ARAM Offset in pTest[0x38], pTest[0x39]


                // end of block (Zelda 03b2)
                if (pTest[0x06] == 0)
                {
					// 02a4
					//

                    pTest[0x04] = 0;
                }
            }
            break;

        default:
			ERROR_LOG(DSPHLE, "Zelda Ucode: Unknown PB type %i", _rPB.type);
			break;
        }
    }
}
#endif

void CUCode_Zelda::Update(int cycles)
{
	if (m_rMailHandler.GetNextMail() == DSP_FRAME_END)
		g_dspInitialize.pGenerateDSPInterrupt();
}

void CUCode_Zelda::HandleMail(u32 _uMail)
{
	// When we used to lose sync, the last mails we get before the audio goes bye-bye
	// 0
	// 0x00000
	// 0
	// 0x10000
	// 0
	// 0x20000
	// 0
	// 0x30000
	// And then silence... Looks like some reverse countdown :)
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
					m_rMailHandler.PushMail(DSP_FRAME_END);
					//g_dspInitialize.pGenerateDSPInterrupt();

					soundStream->GetMixer()->SetHLEReady(true);
					DEBUG_LOG(DSPHLE, "Update the SoundThread to be in sync");
					soundStream->Update(); //do it in this thread to avoid sync problems

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

#if 0
void CUCode_Zelda::MixAdd(short* _pBuffer, int _iSize)
{
    if (m_NumberOfFramesToRender > 0)
    {
        if (m_NumPBs <= m_MaxSyncedPB)  // we just render if all PBs are synced...Zelda does it in steps of 0x10 PBs but hey this is HLE
        {
            if (_iSize > 1024 * 1024)
                _iSize = 1024 * 1024;

            memset(templbuffer, 0, _iSize * sizeof(int));
            memset(temprbuffer, 0, _iSize * sizeof(int));

            CopyPBsFromRAM();

            // render frame...
            for (u32 i = 0; i < m_NumPBs; i++)
            {
                // masking of PBs is done in zelda 0272... skip it for the moment
                /* int Slot = i >> 4;
                int Mask = i & 0x0F;
                if (m_PBMask[Slot] & Mask)) */
                {
                    UpdatePB(m_PBs[i], templbuffer, temprbuffer, _iSize);
                }                
            }
            CopyPBsToRAM();
            m_MaxSyncedPB = 0;

            if (_pBuffer) {
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
        else
        {
            return;
        }

        m_CurrentFrameToRender++;

		// make sure we never read outside the buffer by mistake.
		// Before deleting extra reads in ExecuteList, we were getting these
		// values.
		memset(m_Buffer, 0xcc, sizeof(m_Buffer));
	}
}
#endif

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

			// Let's log the parameter blocks.
			// Copy and byteswap the parameter blocks.

			// Both Z:TP, Z:WW and Zelda Four Swords happily sets param blocks as soon as the title screen comes up.
			// Although in Z:WW, it won't set any param blocks until in-game if the item hack is on.
#if 0
			DEBUG_LOG(DSPHLE, "Param block at %08x:", param_blocks_ptr);
			CopyPBsFromRAM();
			for (int i = 0; i < num_param_blocks; i++)
			{
				const ZPB &pb = zpbs[i];
				// The only thing that consistently looks like a pointer in the param blocks. 
				u32 addr = (pb.addr_high << 16) | pb.addr_low;
				if (addr)
				{
					DEBUG_LOG(DSPHLE, "Param block:  ==== %i ( %08x ) ====", i, GetParamBlockAddr(i));
					DEBUG_LOG(DSPHLE, "Addr: %08x  Type: %i", addr, pb.type);

					// Got one! Read from ARAM, dump to file.
					// I can't get the below to produce anything resembling sane audio :(
					//addr *= 2;
					/*
					int size = 0x10000;
					u8 *temp = new u8[size];
					for (int i = 0; i < size; i++) {
						temp[i] = g_dspInitialize.pARAM_Read_U8(addr + i);
					}
					s16 *audio = new s16[size * 4];
					int aoff = 0;
					short hist1 = 0, hist2 = 0;
					for (int i = 0; i < size; i+=9)
					{
						AFCdecodebuffer(temp + i, audio + aoff, &hist1, &hist2);
						aoff += 16;
					}
					char fname[256];
					sprintf(fname, "%08x.bin", addr);
					if (File::Exists(fname))
						continue;

					FILE *f = fopen(fname, "wb");
					fwrite(audio, 1, size*4, f);
					fclose(f);

					sprintf(fname, "%08x_raw.bin", addr);

					f = fopen(fname, "wb");
					fwrite(temp, 1, size, f);
					fclose(f);
					*/
				}
			}
			CopyPBsToRAM();
#endif
	    }
		return;

/*
		case 0x03: break;   // dunno ... zelda ww jmps to 0x0073
		case 0x04: break;   // dunno ... zelda ww jmps to 0x0580
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

		MixAddVoice(pb, m_LeftBuffer, m_RightBuffer, _Size);
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
	//p.Do(m_MailState);
	//p.Do(m_PBMask);
	//p.Do(m_NumPBs);
	//p.Do(m_PBAddress);
	//p.Do(m_MaxSyncedPB);
	//p.Do(m_PBs);
	p.Do(m_readOffset);
	//p.Do(m_NumberOfFramesToRender);
	//p.Do(m_CurrentFrameToRender);
	p.Do(m_numSteps);
	p.Do(m_step);
	p.Do(m_Buffer);
}

