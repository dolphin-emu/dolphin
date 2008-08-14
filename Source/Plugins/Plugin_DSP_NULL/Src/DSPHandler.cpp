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

#ifdef _WIN32
#include "PCHW/DSoundStream.h"
#endif

#include "DSPHandler.h"

CDSPHandler* CDSPHandler::m_pInstance = NULL;

CDSPHandler::CDSPHandler()
	: m_pUCode(NULL),
	m_bHalt(false),
	m_bAssertInt(false)
{
	SetUCode(UCODE_ROM);
	m_DSPControl.DSPHalt = 1;
	m_DSPControl.DSPInit = 1;
}


CDSPHandler::~CDSPHandler()
{
	delete m_pUCode;
}


void CDSPHandler::Update()
{
	if (m_pUCode != NULL)
	{
		m_pUCode->Update();
	}
}


unsigned short CDSPHandler::WriteControlRegister(unsigned short _Value)
{
	UDSPControl Temp(_Value);

	if (Temp.DSPReset)
	{
		SetUCode(UCODE_ROM);
		Temp.DSPReset = 0;
	}

	if (Temp.DSPInit == 0)
	{
		// copy 128 byte from ARAM 0x000000 to IMEM
		SetUCode(UCODE_INIT_AUDIO_SYSTEM);
		Temp.DSPInitCode = 0;
		// MessageBox(NULL, "UCODE_INIT_AUDIO_SYSTEM", "DSP-HLE", MB_OK);
	}

	m_DSPControl.Hex = Temp.Hex;

	return(m_DSPControl.Hex);
}


unsigned short CDSPHandler::ReadControlRegister()
{
	return(m_DSPControl.Hex);
}


void CDSPHandler::SendMailToDSP(u32 _uMail)
{
	if (m_pUCode != NULL)
	{
		m_pUCode->HandleMail(_uMail);
	}
}


IUCode* CDSPHandler::GetUCode()
{
	return m_pUCode;
}


void CDSPHandler::SetUCode(u32 _crc)
{
	delete m_pUCode;

	m_MailHandler.Clear();
	m_pUCode = UCodeFactory(_crc, m_MailHandler);
}


