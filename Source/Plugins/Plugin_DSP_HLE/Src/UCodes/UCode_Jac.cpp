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
#include "UCodes.h"
#include "UCode_Jac.h"
#include "../MailHandler.h"

CUCode_Jac::CUCode_Jac(CMailHandler& _rMailHandler)
	: IUCode(_rMailHandler)
	, m_bListInProgress(false)
{
	DebugLog("CUCode_Jac: init");
	m_rMailHandler.PushMail(0xDCD10000);
	m_rMailHandler.PushMail(0x80000000);
}


CUCode_Jac::~CUCode_Jac()
{
	m_rMailHandler.Clear();
}


void CUCode_Jac::HandleMail(u32 _uMail)
{
	// this is prolly totally bullshit and should work like the zelda one...
	// but i am to lazy to change it atm

	if (m_bListInProgress == false)
	{
		// get the command to find out how much steps it has
		switch (_uMail & 0xFFFF)
		{
			// release halt
		    case 0x00:
			    // m_Mails.push(0x80000000);
			    g_dspInitialize.pGenerateDSPInterrupt();
			    break;

		    case 0x40:
			    m_step = 0;
			    ((u32*)m_Buffer)[m_step++] = _uMail;
			    m_bListInProgress = true;
			    m_numSteps = 5;
			    break;

		    case 0x2000:
		    case 0x4000:
			    m_step = 0;
			    ((u32*)m_Buffer)[m_step++] = _uMail;
			    m_bListInProgress = true;
			    m_numSteps = 3;
			    break;

		    default:
			    PanicAlert("UCode Jac");
			    DebugLog("UCode Jac - unknown cmd: %x", _uMail & 0xFFFF);
			    break;
		}
	}
	else
	{
		((u32*)m_Buffer)[m_step] = _uMail;
		m_step++;

		if (m_step == m_numSteps)
		{
			ExecuteList();
			m_bListInProgress = false;
		}
	}
}


void CUCode_Jac::Update()
{
	// check if we have to sent something
/*	if (!g_MailHandler.empty())
    {
    g_dspInitialize.pGenerateDSPInterrupt();
    }*/
}


void CUCode_Jac::ExecuteList()
{
	// begin with the list
	m_readOffset = 0;

	u16 cmd  = Read16();
	u16 sync = Read16();

	DebugLog("==============================================================================");
	DebugLog("UCode Jac - execute dlist (cmd: 0x%04x : sync: 0x%04x)", cmd, sync);

	switch (cmd)
	{
		// ==============================================================================
		// DsetupTable
		// ==============================================================================
	    case 0x40:
	    {
		    u32 tmp[4];
		    tmp[0] = Read32();
		    tmp[1] = Read32();
		    tmp[2] = Read32();
		    tmp[3] = Read32();

		    DebugLog("DsetupTable");
		    DebugLog("???:                           0x%08x", tmp[0]);
		    DebugLog("DSPRES_FILTER   (size: 0x40):  0x%08x",    tmp[1]);
		    DebugLog("DSPADPCM_FILTER (size: 0x500): 0x%08x", tmp[2]);
		    DebugLog("???:                           0x%08x", tmp[3]);
	    }
		    break;

		    // ==============================================================================
		    // UpdateDSPChannel
		    // ==============================================================================
	    case 0x2000:
	    case 0x4000: // animal crossing
	    {
		    u32 tmp[3];
		    tmp[0] = Read32();
		    tmp[1] = Read32();
		    tmp[2] = Read32();

		    DebugLog("UpdateDSPChannel");
		    DebugLog("audiomemory:                   0x%08x", tmp[0]);
		    DebugLog("audiomemory:                   0x%08x", tmp[1]);
		    DebugLog("DSPADPCM_FILTER (size: 0x40):  0x%08x", tmp[2]);
	    }
		    break;

	    default:
		    PanicAlert("UCode Jac unknown cmd: %s (size %)", cmd, m_numSteps);
		    DebugLog("Jac UCode - unknown cmd: %x (size %i)", cmd, m_numSteps);
		    break;
	}

	// sync, we are rdy
	m_rMailHandler.PushMail(DSP_SYNC);
	m_rMailHandler.PushMail(0xF3550000 | sync);
}


