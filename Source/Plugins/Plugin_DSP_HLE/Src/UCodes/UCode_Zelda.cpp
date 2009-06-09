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
	DEBUG_LOG(DSPHLE, "Zelda mail 0x%08X, list in progress? %s, sync in progress? %s", _uMail, 
		m_bListInProgress ? "Yes" : "No", m_bSyncInProgress ? "Yes" : "No");
	// SetupTable
	// in WW we get SetDolbyDelay
	// SyncFrame
	// The last mails we get before the audio goes bye-bye
	// 0
	// 0x00000
	// 0
	// 0x10000
	// 0
	// 0x20000
	// 0
	// 0x30000
	// And then silence...

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
	else 
	{
		DEBUG_LOG(DSPHLE, "Zelda uCode: unknown mail %08X", _uMail);
	}
}

void CUCode_Zelda::MixAdd(short* buffer, int size)
{
	//TODO(XK): Zelda UCode MixAdd?
}

void CUCode_Zelda::ExecuteList()
{
	// begin with the list
	m_readOffset = 0;

	u32 CmdMail = Read32();
	u32 Command = (CmdMail >> 24) & 0x7f;
	u32 Sync = CmdMail >> 16;

	DEBUG_LOG(DSPHLE, "==============================================================================");
	DEBUG_LOG(DSPHLE, "Zelda UCode - execute dlist (cmd: 0x%04x : sync: 0x%04x)", Command, Sync);

	switch (Command)
	{
		// DsetupTable ... zelda ww jumps to 0x0095
	    case 0x01:
	    {
		    u32 tmp[4];
		    tmp[0] = Read32();
		    tmp[1] = Read32();
		    tmp[2] = Read32();
		    tmp[3] = Read32();

			m_SyncMaxStep = CmdMail & 0xFFFF;

		    DEBUG_LOG(DSPHLE, "DsetupTable");
		    DEBUG_LOG(DSPHLE, "???:                           0x%08x", tmp[0]);
		    DEBUG_LOG(DSPHLE, "DSPRES_FILTER   (size: 0x40):  0x%08x", tmp[1]);
		    DEBUG_LOG(DSPHLE, "DSPADPCM_FILTER (size: 0x500): 0x%08x", tmp[2]);
		    DEBUG_LOG(DSPHLE, "???:                           0x%08x", tmp[3]);
	    }
		    break;

		    // SyncFrame ... zelda ww jumps to 0x0243
	    case 0x02:
	    {
		    u32 tmp[3];
		    tmp[0] = Read32();
		    tmp[1] = Read32();
		    tmp[2] = Read32();

			// We're ready to mix
		//	soundStream->GetMixer()->SetHLEReady(true);
		//	DEBUG_LOG(DSPHLE, "Update the SoundThread to be in sync");
		//	soundStream->Update(); //do it in this thread to avoid sync problems

			m_bSyncCmdPending = true;
			m_SyncEndSync = Sync;
			m_SyncCount = 0;
			m_SyncMax = (CmdMail >> 16) & 0xFF;

		    DEBUG_LOG(DSPHLE, "DsyncFrame");
		    DEBUG_LOG(DSPHLE, "???:                           0x%08x", tmp[0]);
		    DEBUG_LOG(DSPHLE, "???:                           0x%08x", tmp[1]);
		    DEBUG_LOG(DSPHLE, "DSPADPCM_FILTER (size: 0x500): 0x%08x", tmp[2]);
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
		    u32 tmp[2];
		    tmp[0] = Read32();
		    tmp[1] = Read32();

		    DEBUG_LOG(DSPHLE, "DSetDolbyDelay");
		    DEBUG_LOG(DSPHLE, "DOLBY2_DELAY_BUF (size 0x960): 0x%08x", tmp[0]);
		    DEBUG_LOG(DSPHLE, "DSPRES_FILTER    (size 0x500): 0x%08x", tmp[1]);
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

	// sync, we are rdy
	m_rMailHandler.PushMail(DSP_SYNC);
	g_dspInitialize.pGenerateDSPInterrupt();
	m_rMailHandler.PushMail(0xF3550000 | Sync);
}


