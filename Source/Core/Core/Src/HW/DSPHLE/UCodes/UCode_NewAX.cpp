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
	, m_cmdlist_addr(0)
	, m_axthread(&CUCode_NewAX::AXThread, this)
{
	m_rMailHandler.PushMail(DSP_INIT);
	DSP::GenerateDSPInterruptFromDSPEmu(DSP::INT_DSP);
}

CUCode_NewAX::~CUCode_NewAX()
{
	m_cmdlist_addr = (u32)-1;	// Special value to signal end
	NotifyAXThread();
	m_axthread.join();

	m_rMailHandler.Clear();
}

void CUCode_NewAX::AXThread()
{
	while (true)
	{
		{
			std::unique_lock<std::mutex> lk(m_cmdlist_mutex);
			while (m_cmdlist_addr == 0)
				m_cmdlist_cv.wait(lk);
		}

		if (m_cmdlist_addr == (u32)-1)	// End of thread signal
			break;

		m_processing.lock();
		HandleCommandList(m_cmdlist_addr);
		m_cmdlist_addr = 0;

		// Signal end of processing
		m_rMailHandler.PushMail(DSP_YIELD);
		DSP::GenerateDSPInterruptFromDSPEmu(DSP::INT_DSP);
		m_processing.unlock();
	}
}

void CUCode_NewAX::NotifyAXThread()
{
	std::unique_lock<std::mutex> lk(m_cmdlist_mutex);
	m_cmdlist_cv.notify_one();
}

void CUCode_NewAX::HandleCommandList(u32 addr)
{
	WARN_LOG(DSPHLE, "TODO: HandleCommandList(%08x)", addr);
}

void CUCode_NewAX::HandleMail(u32 mail)
{
	// Indicates if the next message is a command list address.
	static bool next_is_cmdlist = false;
	bool set_next_is_cmdlist = false;

	// Wait for DSP processing to be done before answering any mail. This is
	// safe to do because it matches what the DSP does on real hardware: there
	// is no interrupt when a mail from CPU is received.
	m_processing.lock();

	if (next_is_cmdlist)
	{
		m_cmdlist_addr = mail;
		NotifyAXThread();
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

	m_processing.unlock();
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
	std::lock_guard<std::mutex> lk(m_processing);

	DoStateShared(p);
}
