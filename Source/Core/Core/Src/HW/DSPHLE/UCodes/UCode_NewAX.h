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

// Official Git repository and contact information can be found at
// http://code.google.com/p/dolphin-emu/

#ifndef _UCODE_NEWAX_H
#define _UCODE_NEWAX_H

#include "UCodes.h"
#include "UCode_AXStructs.h"

class CUCode_NewAX : public IUCode
{
public:
	CUCode_NewAX(DSPHLE* dsp_hle, u32 crc);
	virtual ~CUCode_NewAX();

	void HandleMail(u32 mail);
	void MixAdd(short* out_buffer, int nsamples);
	void Update(int cycles);
	void DoState(PointerWrap& p);

private:
	enum MailType
	{
		MAIL_RESUME = 0xCDD10000,
		MAIL_NEWUCODE = 0xCDD10001,
		MAIL_RESET = 0xCDD10002,
		MAIL_CONTINUE = 0xCDD10003,

		// CPU sends 0xBABE0000 | cmdlist_size to the DSP
		MAIL_CMDLIST = 0xBABE0000,
		MAIL_CMDLIST_MASK = 0xFFFF0000
	};

	// Volatile because it's set by HandleMail and accessed in
	// HandleCommandList, which are running in two different threads.
	volatile u32 m_cmdlist_addr;

	std::thread m_axthread;

	// Sync objects
	std::mutex m_processing;
	std::condition_variable m_cmdlist_cv;
	std::mutex m_cmdlist_mutex;

	// Send a notification to the AX thread to tell him a new cmdlist addr is
	// available for processing.
	void NotifyAXThread();

	void AXThread();
	void HandleCommandList(u32 addr);
};

#endif // !_UCODE_NEWAX_H
