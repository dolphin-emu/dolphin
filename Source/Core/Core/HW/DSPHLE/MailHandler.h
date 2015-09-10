// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <queue>
#include <utility>

#include "Common/CommonTypes.h"

class PointerWrap;

class CMailHandler
{
public:
	CMailHandler();
	~CMailHandler();

	void PushMail(u32 _Mail, bool interrupt = false);
	void Clear();
	void Halt(bool _Halt);
	void DoState(PointerWrap &p);
	bool IsEmpty() const;

	u16 ReadDSPMailboxHigh();
	u16 ReadDSPMailboxLow();

private:
	// mail handler
	std::queue<std::pair<u32, bool>> m_Mails;
};
