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
#include "../DSPHandler.h"
#include "UCodes.h"
#include "UCode_CARD.h"


CUCode_CARD::CUCode_CARD(CMailHandler& _rMailHandler)
	: IUCode(_rMailHandler)
{
	DEBUG_LOG(DSPHLE, "CUCode_CARD - initialized");
	m_rMailHandler.PushMail(DSP_INIT);
}


CUCode_CARD::~CUCode_CARD()
{
	m_rMailHandler.Clear();
}


void CUCode_CARD::Update(int cycles)
{
	// check if we have to sent something
	if (!m_rMailHandler.IsEmpty())
	{
		g_dspInitialize.pGenerateDSPInterrupt();
	}
}

void CUCode_CARD::HandleMail(u32 _uMail)
{
	if (_uMail == 0xFF000000) // unlock card
	{
		//	m_Mails.push(0x00000001); // ACK (actualy anything != 0)
	}
	else
	{
		DEBUG_LOG(DSPHLE, "CUCode_CARD - unknown cmd: %x (size %i)", _uMail);
	}

	m_rMailHandler.PushMail(DSP_DONE);
	CDSPHandler::GetInstance().SetUCode(UCODE_ROM);
}


