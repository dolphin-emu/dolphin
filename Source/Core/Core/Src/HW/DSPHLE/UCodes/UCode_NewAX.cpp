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

#include "UCode_NewAX.h"
#include "../../DSP.h"

CUCode_NewAX::CUCode_NewAX(DSPHLE* dsp_hle, u32 crc)
	: IUCode(dsp_hle, crc)
{
	m_rMailHandler.PushMail(DSP_INIT);
	DSP::GenerateDSPInterruptFromDSPEmu(DSP::INT_DSP);
}

CUCode_NewAX::~CUCode_NewAX()
{
	m_rMailHandler.Clear();
}

void CUCode_NewAX::HandleCommandList(u32 addr)
{
	// Signal end of processing
	m_rMailHandler.PushMail(DSP_YIELD);
	DSP::GenerateDSPInterruptFromDSPEmu(DSP::INT_DSP);
}

void CUCode_NewAX::HandleMail(u32 mail)
{
	// Indicates if the next message is a command list address.
	static bool next_is_cmdlist = false;
	bool set_next_is_cmdlist = false;

	if (next_is_cmdlist)
	{
		HandleCommandList(mail);
	}
	else if (m_UploadSetupInProgress)
	{
		PrepareBootUCode(mail);
	}
	else if (mail == MAIL_RESUME)
	{
		// Acknowledge the resume request
		m_rMailHandler.PushMail(DSP_RESUME);
		DSP::GenerateDSPInterruptFromDSPEmu(DSP::INT_DSP);
	}
	else if (mail == MAIL_NEWUCODE)
	{
		soundStream->GetMixer()->SetHLEReady(false);
		m_UploadSetupInProgress = true;
	}
	else if (mail == MAIL_RESET)
	{
		m_DSPHLE->SetUCode(UCODE_ROM);
	}
	else if (mail == MAIL_CONTINUE)
	{
		// We don't have to do anything here - the CPU does not wait for a ACK
		// and sends a cmdlist mail just after.
	}
	else if ((mail & MAIL_CMDLIST_MASK) == MAIL_CMDLIST)
	{
		// A command list address is going to be sent next.
		set_next_is_cmdlist = true;
	}
	else
	{
		ERROR_LOG(DSPHLE, "Unknown mail sent to AX::HandleMail: %08x", mail);
	}

	next_is_cmdlist = set_next_is_cmdlist;
}

void CUCode_NewAX::MixAdd(short* out_buffer, int nsamples)
{
	// nsamples * 2 for left and right audio channel
	memset(out_buffer, 0, nsamples * 2 * sizeof (short));
}

void CUCode_NewAX::Update(int cycles)
{
	// Used for UCode switching.
	if (NeedsResumeMail())
	{
		m_rMailHandler.PushMail(DSP_RESUME);
		DSP::GenerateDSPInterruptFromDSPEmu(DSP::INT_DSP);
	}
}

void CUCode_NewAX::DoState(PointerWrap& p)
{
	DoStateShared(p);
}
