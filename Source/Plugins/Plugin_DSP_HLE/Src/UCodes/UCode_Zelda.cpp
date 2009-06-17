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
#include "FileUtil.h"
#include "UCodes.h"
#include "UCode_Zelda.h"
#include "../MailHandler.h"

#include "../main.h"
#include "Mixer.h"

// TODO: replace with table from RAM.
const u16 afccoef[16][2] =
{
	{0,0},          {0x0800,0},     {0,     0x0800},{0x0400,0x0400},
	{0x1000,0xf800},{0x0e00,0xfa00},{0x0c00,0xfc00},{0x1200,0xf600},
	{0x1068,0xf738},{0x12c0,0xf704},{0x1400,0xf400},{0x0800,0xf800},
	{0x0400,0xfc00},{0xfc00,0x0400},{0xfc00,0},     {0xf800,0}
};

// Decoder from in_cube by hcs/destop/etc. Haven't yet found a valid use for it.

// Looking at in_cube, it seems to be 9 bytes of input = 16 samples of output.
// Different from AX ADPCM which is 8 bytes of input = 14 samples of output.
// input = location of encoded source samples
// out = location of destination buffer (16 bits / sample)
void AFCdecodebuffer(const u8 *input, s16 *out, short *histp, short *hist2p)
{
	short nibbles[16];
	u8 *dst = (u8 *)out;
	short hist = *histp;
	short hist2 = *hist2p;
	const u8 *src = input;

	// 9 bytes input - first byte contain delta scaling and coef index.
	const short delta = 1 << (((*src) >> 4) & 0xf);
	const short idx = *src & 0xf;
	src++;

	// denibble the rest of the 8 bytes into 16 values.
	for (int i = 0; i < 16; i += 2)
	{
		int j = *src >> 4;
		nibbles[i] = j;
		j = *src & 0xF;
		nibbles[i + 1] = j;
		src++;
	}

	// make the nibbles signed.
	for (int i = 0; i < 16; i++)
	{
		if (nibbles[i] >= 8) 
			nibbles[i] = nibbles[i] - 16;
	}

	// Perform ADPCM filtering.
	for (int i = 0; i < 16; i++)
	{
		int sample = (delta * nibbles[i]) << 11;
		sample += ((long)hist * afccoef[idx][0]) + ((long)hist2 * afccoef[idx][1]);
		sample = sample >> 11;

		// Clamp sample.
		if (sample > 32767) {
			sample = 32767;
		}
		if (sample < -32768) {
			sample = -32768;
		}

		*(short*)dst = (short)sample;
		dst = dst + 2;

		hist2 = hist;
		hist = (short)sample;
	}

	// Store state.
	*histp = hist;
	*hist2p = hist2;
}


CUCode_Zelda::CUCode_Zelda(CMailHandler& _rMailHandler)
	: IUCode(_rMailHandler)
	, m_bSyncInProgress(false)
	, m_SyncIndex(0)
	, m_SyncStep(0)
	, m_SyncMaxStep(0)
	, m_bSyncCmdPending(false)
	, m_SyncEndSync(0)
	, m_SyncCurStep(0)
	, m_SyncCount(0)
	, m_SyncMax(0)
	, m_numSteps(0)
	, m_bListInProgress(false)
	, m_step(0)
	, m_readOffset(0)
	, num_param_blocks(0)
	, param_blocks_ptr(0)
	, param_blocks2_ptr(0)
{
	DEBUG_LOG(DSPHLE, "UCode_Zelda - add boot mails for handshake");

	m_rMailHandler.PushMail(DSP_INIT);
	g_dspInitialize.pGenerateDSPInterrupt();
	m_rMailHandler.PushMail(0xF3551111); // handshake
	memset(m_Buffer, 0, sizeof(m_Buffer));
	memset(m_SyncValues, 0, sizeof(m_SyncValues));
}


CUCode_Zelda::~CUCode_Zelda()
{
	m_rMailHandler.Clear();
}


void CUCode_Zelda::Update(int cycles)
{
	// check if we have to sent something
	//if (!m_rMailHandler.IsEmpty())
	//	g_dspInitialize.pGenerateDSPInterrupt();
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
			m_SyncStep = (n + 1) << 4;
			m_SyncValues[n] = _uMail & 0xFFFF;
			m_bSyncInProgress = false;

			m_SyncCurStep = m_SyncStep;
			if (m_SyncCurStep == m_SyncMaxStep)
			{
				m_SyncCount++;

				m_rMailHandler.PushMail(DSP_SYNC);
				g_dspInitialize.pGenerateDSPInterrupt();
				m_rMailHandler.PushMail(0xF355FF00 | m_SyncCount);

				m_SyncCurStep = 0;

				if (m_SyncCount == (m_SyncMax + 1))
				{
					m_rMailHandler.PushMail(DSP_UNKN);
					g_dspInitialize.pGenerateDSPInterrupt();

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

	if (_uMail != 0)
	{
		DEBUG_LOG(DSPHLE, "Zelda mail 0x%08X, list in progress? %s", _uMail, 
			m_bListInProgress ? "Yes" : "No");
	}

	if (m_bListInProgress)
	{
		if (m_step < 0 || m_step >= sizeof(m_Buffer) / 4)
			PanicAlert("m_step out of range");

		((u32*)m_Buffer)[m_step] = _uMail;
		m_step++;

		if (m_step >= m_numSteps)
		{
			DEBUG_LOG(DSPHLE, "Executing %i-step list.", m_numSteps);
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

		// make sure we never read outside the buffer by mistake.
		// Before deleting extra reads in ExecuteList, we were getting these
		// values.
		memset(m_Buffer, 0xcc, sizeof(m_Buffer));
	} 
	else 
	{
		WARN_LOG(DSPHLE, "Zelda uCode: unknown mail %08X", _uMail);
	}
}

void CUCode_Zelda::ExecuteList()
{
	// begin with the list
	m_readOffset = 0;

	// First figure out what command we're dealing with.
	u32 CmdMail = Read32();
	u32 Command = (CmdMail >> 24) & 0x7f;
	u32 Sync = CmdMail >> 16;
	u16 ExtraData = CmdMail & 0xFFFF;  // not yet used

	DEBUG_LOG(DSPHLE, "==============================================================================");
	DEBUG_LOG(DSPHLE, "Zelda UCode - execute dlist (cmd: 0x%04x : sync: 0x%04x)", Command, Sync);

	switch (Command)
	{
		// DsetupTable ... zelda ww jumps to 0x0095
	    case 0x01:
	    {
			num_param_blocks = ExtraData;
			u32 tmp[4];
		    param_blocks_ptr = tmp[0] = Read32();
		    tmp[1] = Read32();
		    tmp[2] = Read32();
		    param_blocks2_ptr = tmp[3] = Read32();

			m_SyncMaxStep = CmdMail & 0xFFFF;

		    DEBUG_LOG(DSPHLE, "DsetupTable");
			DEBUG_LOG(DSPHLE, "Num param blocks: %i", num_param_blocks);
		    DEBUG_LOG(DSPHLE, "Param blocks 1:            0x%08x", tmp[0]);

			// This points to some strange data table.
		    DEBUG_LOG(DSPHLE, "DSPRES_FILTER   (size: 0x40):  0x%08x", tmp[1]);

			// Zelda WW: This points to a 64-byte array of coefficients, which are EXACTLY the same
			// as the AFC ADPCM coef array in decode.c of the in_cube winamp plugin,
			// which can play Zelda audio.
			// There's also a lot more table-looking data immediately after - maybe alternative
			// tables? I wonder where the parameter blocks are?
		    DEBUG_LOG(DSPHLE, "DSPADPCM_FILTER (size: 0x500): 0x%08x", tmp[2]);
			DEBUG_LOG(DSPHLE, "Param blocks 2:            0x%08x", tmp[3]);
		}
			break;

			// SyncFrame ... zelda ww jumps to 0x0243
		case 0x02:
		{
			u32 tmp[2];
			tmp[0] = Read32();
			tmp[1] = Read32();

			// We're ready to mix
			//	soundStream->GetMixer()->SetHLEReady(true);
			//	DEBUG_LOG(DSPHLE, "Update the SoundThread to be in sync");
			soundStream->Update(); //do it in this thread to avoid sync problems

			m_bSyncCmdPending = true;
			m_SyncEndSync = Sync;
			m_SyncCount = 0;
			m_SyncMax = (CmdMail >> 16) & 0xFF;

			DEBUG_LOG(DSPHLE, "DsyncFrame");

			// These alternate between three sets of mixing buffers. They are all three fairly near,
			// but not at, the ADMA read addresses.
			DEBUG_LOG(DSPHLE, "Left mixing buffer?            0x%08x", tmp[0]);
			DEBUG_LOG(DSPHLE, "Right mixing buffer?           0x%08x", tmp[1]);

			// Let's log the parameter blocks.
			// Copy and byteswap the parameter blocks.

			// For some reason, in Z:WW we get no param blocks until in-game,
			// while Zelda Four Swords happily sets param blocks as soon as the title screen comes up.
			// Looks like it's playing midi music.
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
	    }
		return;
		    break;

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
		    DEBUG_LOG(DSPHLE, "DOLBY2_DELAY_BUF (size 0x960): 0x%08x", tmp);
	    }
		    break;

		    // Set VARAM
			// Luigi__: in the real Zelda ucode, this opcode is dummy
			// however, in the ucode used by SMG it isn't
	    case 0x0e:
			{
				/*
				 00b0 0080 037d lri         $AR0, #0x037d
				 00b2 0e01      lris        $AC0.M, #0x01
				 00b3 02bf 0065 call        0x0065
				 00b5 00de 037d lr          $AC0.M, @0x037d
				 00b7 0240 7fff andi        $AC0.M, #0x7fff
				 00b9 00fe 037d sr          @0x037d, $AC0.M
				 00bb 029f 0041 jmp         0x0041
				*/
				//
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


void CUCode_Zelda::CopyPBsFromRAM()
{
	for (int i = 0; i < num_param_blocks; i++)
	{
		u32 addr = param_blocks_ptr + i * sizeof(ZPB);
		const u16 *src_ptr = (u16 *)g_dspInitialize.pGetMemoryPointer(addr);
		u16 *dst_ptr = (u16 *)&zpbs[i];
		for (size_t p = 0; p < sizeof(ZPB) / 2; p++)
		{
			dst_ptr[p] = Common::swap16(src_ptr[p]);
		}
	}
}

void CUCode_Zelda::CopyPBsToRAM()
{
	for (int i = 0; i < num_param_blocks; i++)
	{
		u32 addr = param_blocks_ptr + i * sizeof(ZPB);
		const u16 *src_ptr = (u16 *)&zpbs[i];
		u16 *dst_ptr = (u16 *)g_dspInitialize.pGetMemoryPointer(addr);
		for (size_t p = 0; p < sizeof(ZPB) / 2; p++)
		{
			dst_ptr[p] = Common::swap16(src_ptr[p]);
		}
	}
}

// size is in stereo samples.
void CUCode_Zelda::MixAdd(short* buffer, int size)
{
//	for (int i = 0; i < size; i++)
//	{
//		buffer[i*2] = rand();
//		buffer[i*2+1] = rand();
//	}
}

