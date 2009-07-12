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

#ifndef _MAILHANDLER_H
#define _MAILHANDLER_H

#include <queue>

#include "Common.h"

class CMailHandler
{
public:
	CMailHandler();
	~CMailHandler();

	void PushMail(u32 _Mail);
	void Clear();
	void Halt(bool _Halt);
	bool IsEmpty();

	u16 ReadDSPMailboxHigh();
	u16 ReadDSPMailboxLow();
	void Update();

	u32 GetNextMail()
	{ 
		if (m_Mails.size())
			return m_Mails.front();
		else
		{
			WARN_LOG(DSPHLE, "GetNextMail: No mails");
			return 0;  // 
		}
	}

private:
	// mail handler
	std::queue<u32> m_Mails;
};

#endif

