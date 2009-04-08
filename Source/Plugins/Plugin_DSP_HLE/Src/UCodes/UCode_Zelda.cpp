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
	, m_numSteps(0)
	, m_bListInProgress(false)
	, m_step(0)
	, m_readOffset(0)
{
	DEBUG_LOG(DSPHLE, "UCode_Zelda - add boot mails for handshake");
	m_rMailHandler.PushMail(DSP_INIT);
	m_rMailHandler.PushMail(0x80000000); // handshake
	memset(m_Buffer, 0, sizeof(m_Buffer));
}


CUCode_Zelda::~CUCode_Zelda()
{
	m_rMailHandler.Clear();
}


void CUCode_Zelda::Update(int cycles)
{
	// check if we have to sent something
	if (!m_rMailHandler.IsEmpty())
		g_dspInitialize.pGenerateDSPInterrupt();
}


void CUCode_Zelda::HandleMail(u32 _uMail)
{
	//PanicAlert("Zelda mail 0x%08X, list in progress? %s", _uMail, 
	//	m_bListInProgress ? "Yes" : "No");
	// SetupTable
	// in WW we get SetDolbyDelay
	// SyncFrame
	// The last mails we get before the audio goes bye-bye
	// 0, 0, 0
	// 0x10000
	// 0
	// 0x20000
	// 0
	// 0x30000
	// And then silence...
	if (m_bListInProgress == false)
	{
		if(_uMail == 0) {
			g_dspInitialize.pGenerateDSPInterrupt();
		} else if((_uMail >> 16) == 0) {
			m_bListInProgress = true;
			m_numSteps = _uMail;
			m_step = 0;
		} else {
			// Release halt
			#if 0
			// The _uMail seems to be 1,2,3 when bitshifted right 16
			// after 3, we get some unknown values, probably supposed to be in the list
			if((_uMail >> 16) == 1)
				m_rMailHandler.PushMail(DSP_RESUME);
			else if((_uMail >> 16) == 2)
				m_rMailHandler.PushMail(DSP_YIELD);
			else if((_uMail >> 16) == 3)
				m_rMailHandler.PushMail(DSP_DONE);
			else
				{ /*Unknown values here*/}
			#else
			m_rMailHandler.PushMail(DSP_RESUME);
			#endif
			g_dspInitialize.pGenerateDSPInterrupt();
		}
	}
	else
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

	u32 Temp = Read32();
	u32 Command = (Temp >> 24) & 0x7f;
	u32 Sync = Temp >> 16;

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
			soundStream->GetMixer()->SetHLEReady(true);
			DEBUG_LOG(DSPHLE, "Update the SoundThread to be in sync");
			soundStream->Update(); //do it in this thread to avoid sync problems


		    DEBUG_LOG(DSPHLE, "DsyncFrame");
		    DEBUG_LOG(DSPHLE, "???:                           0x%08x", tmp[0]);
		    DEBUG_LOG(DSPHLE, "???:                           0x%08x", tmp[1]);
		    DEBUG_LOG(DSPHLE, "DSPADPCM_FILTER (size: 0x500): 0x%08x", tmp[2]);
	    }
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
	    case 0x0e:
//        MessageBox(NULL, "Zelda VARAM", "cmd", MB_OK);
		    break;

		    // default ... zelda ww jumps to 0x0043
	    default:
		    PanicAlert("Zelda UCode - unknown cmd: %x (size %i)", Command, m_numSteps);
		    break;
	}

	// sync, we are rdy
	m_rMailHandler.PushMail(DSP_SYNC);
	m_rMailHandler.PushMail(0xF3550000 | Sync);
}


