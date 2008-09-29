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
#include "UCode_ROM.h"

CUCode_Rom::CUCode_Rom(CMailHandler& _rMailHandler)
	: IUCode(_rMailHandler)
	, m_BootTask_numSteps(0)
	, m_NextParameter(0)
{
	DebugLog("UCode_Rom - initialized");
	m_rMailHandler.Clear();
	m_rMailHandler.PushMail(0x8071FEED);
}

CUCode_Rom::~CUCode_Rom()
{}

void CUCode_Rom::Update()
{}

void CUCode_Rom::HandleMail(u32 _uMail)
{
	if (m_NextParameter == 0)
	{
		// wait for beginning of UCode
		if ((_uMail & 0xFFFF0000) != 0x80F30000)
		{
			u32 Message = 0xFEEE0000 | (_uMail & 0xFFFF);
			m_rMailHandler.PushMail(Message);
		}
		else
		{
			m_NextParameter = _uMail;
		}
	}
	else
	{
		switch (m_NextParameter)
		{
		    case 0x80F3A001:
			    m_CurrentUCode.m_RAMAddress = _uMail;
			    break;

		    case 0x80F3A002:
			    m_CurrentUCode.m_Length = _uMail;
			    break;

		    case 0x80F3C002:
			    m_CurrentUCode.m_IMEMAddress = _uMail;
			    break;

		    case 0x80F3B002:
			    m_CurrentUCode.m_Unk = _uMail;
			    break;

		    case 0x80F3D001:
		    {
			    m_CurrentUCode.m_StartPC = _uMail;
			    BootUCode();
				return; // FIXES THE OVERWRITE
		    }
		    break;
		}

		// THE GODDAMN OVERWRITE WAS HERE. Without the return above, since BootUCode may delete "this", well ...
		m_NextParameter = 0;
	}
}

void CUCode_Rom::BootUCode()
{
	// simple non-scientific crc invented by ector :P
	// too annoying to change now, and probably good enough anyway
	u32 crc = 0;

	for (u32 i = 0; i < m_CurrentUCode.m_Length; i++)
	{
		crc ^= Memory_Read_U8(m_CurrentUCode.m_RAMAddress + i);
		//let's rol
		crc = (crc << 3) | (crc >> 29);
	}

	DebugLog("CurrentUCode SOURCE Addr: 0x%08x", m_CurrentUCode.m_RAMAddress);
	DebugLog("CurrentUCode Length:      0x%08x", m_CurrentUCode.m_Length);
	DebugLog("CurrentUCode DEST Addr:   0x%08x", m_CurrentUCode.m_IMEMAddress);
	DebugLog("CurrentUCode ???:         0x%08x", m_CurrentUCode.m_Unk);
	DebugLog("CurrentUCode init_vector: 0x%08x", m_CurrentUCode.m_StartPC);
	DebugLog("CurrentUCode CRC:         0x%08x", crc);
	DebugLog("BootTask - done");

	CDSPHandler::GetInstance().SetUCode(crc);
}


