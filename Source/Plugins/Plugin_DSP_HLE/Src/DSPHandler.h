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

#ifndef _DSPHANDLER_H
#define _DSPHANDLER_H

#include "Common.h"
#include "AudioCommon.h"
#include "MailHandler.h"
#include "UCodes/UCodes.h"

class CDSPHandler
{
public:
	void Update(int cycles);
	unsigned short WriteControlRegister(unsigned short _Value);
	unsigned short ReadControlRegister();
	void SendMailToDSP(u32 _uMail);
	IUCode* GetUCode();
	void SetUCode(u32 _crc);

	CMailHandler& AccessMailHandler() { return m_MailHandler; }

	static CDSPHandler& GetInstance()
	{
		return *m_pInstance;
	}

	static void Destroy()
	{
		delete m_pInstance;
		m_pInstance = NULL;
	}

	static CDSPHandler& CreateInstance()
	{
		if (!m_pInstance)
			m_pInstance = new CDSPHandler();

		return *m_pInstance;
	}

private:
	CDSPHandler();
	~CDSPHandler();

	// singleton instance
	static CDSPHandler* m_pInstance;

	IUCode* m_pUCode;
	UDSPControl m_DSPControl;
	CMailHandler m_MailHandler;

	bool m_bHalt;
	bool m_bAssertInt;
};

#endif
