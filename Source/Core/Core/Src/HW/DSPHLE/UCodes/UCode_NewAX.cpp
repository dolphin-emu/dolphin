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
	, m_cmdlist_size(0)
	, m_axthread(&CUCode_NewAX::AXThread, this)
{
	m_rMailHandler.PushMail(DSP_INIT);
	DSP::GenerateDSPInterruptFromDSPEmu(DSP::INT_DSP);
}

CUCode_NewAX::~CUCode_NewAX()
{
	m_cmdlist_size = (u16)-1;	// Special value to signal end
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
			while (m_cmdlist_size == 0)
				m_cmdlist_cv.wait(lk);
		}

		if (m_cmdlist_size == (u16)-1)	// End of thread signal
			break;

		m_processing.lock();
		HandleCommandList();
		m_cmdlist_size = 0;

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

void CUCode_NewAX::HandleCommandList()
{
	u16 pb_addr_hi, pb_addr_lo;
	u32 pb_addr = 0;

	u32 curr_idx = 0;
	bool end = false;
	while (!end)
	{
		u16 cmd = m_cmdlist[curr_idx++];

		switch (cmd)
		{

			// A lot of these commands are unknown, or unused in this AX HLE.
			// We still need to skip their arguments using "curr_idx += N".

			case CMD_STUDIO_ADDR: curr_idx += 2; break;
			case CMD_UNK_01: curr_idx += 5; break;

			case CMD_PB_ADDR:
				pb_addr_hi = m_cmdlist[curr_idx++];
				pb_addr_lo = m_cmdlist[curr_idx++];
				pb_addr = (pb_addr_hi << 16) | pb_addr_lo;

				WARN_LOG(DSPHLE, "PB addr: %08x", pb_addr);
				break;

			case CMD_PROCESS:
				ProcessPB(pb_addr);
				break;

			case CMD_UNK_04: curr_idx += 4; break;
			case CMD_UNK_05: curr_idx += 4; break;
			case CMD_UNK_06: curr_idx += 2; break;
			case CMD_SBUFFER_ADDR: curr_idx += 2; break;
			case CMD_UNK_08: curr_idx += 10; break;	// TODO: check
			case CMD_UNK_09: curr_idx += 2; break;
			case CMD_COMPRESSOR_TABLE_ADDR: curr_idx += 2; break;
			case CMD_UNK_0B: break; // TODO: check other versions
			case CMD_UNK_0C: break; // TODO: check other versions
			case CMD_UNK_0D: curr_idx += 2; break;
			case CMD_UNK_0E: curr_idx += 4; break;

			case CMD_END:
				end = true;
				break;

			case CMD_UNK_10: curr_idx += 4; break;
			case CMD_UNK_11: curr_idx += 2; break;
			case CMD_UNK_12: curr_idx += 1; break;
			case CMD_UNK_13: curr_idx += 12; break;

			default:
				ERROR_LOG(DSPHLE, "Unknown command in AX cmdlist: %04x", cmd);
				end = true;
				break;
		}
	}
}

void CUCode_NewAX::ProcessPB(u32 pb_addr)
{
	NOTICE_LOG(DSPHLE, "TODO: process pb %08x", pb_addr);
}

void CUCode_NewAX::HandleMail(u32 mail)
{
	// Indicates if the next message is a command list address.
	static bool next_is_cmdlist = false;
	static u16 cmdlist_size = 0;

	bool set_next_is_cmdlist = false;

	// Wait for DSP processing to be done before answering any mail. This is
	// safe to do because it matches what the DSP does on real hardware: there
	// is no interrupt when a mail from CPU is received.
	m_processing.lock();

	if (next_is_cmdlist)
	{
		CopyCmdList(mail, cmdlist_size);
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
	else if (mail == MAIL_NEW_UCODE)
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
		cmdlist_size = (u16)(mail & ~MAIL_CMDLIST_MASK);
	}
	else
	{
		ERROR_LOG(DSPHLE, "Unknown mail sent to AX::HandleMail: %08x", mail);
	}

	m_processing.unlock();
	next_is_cmdlist = set_next_is_cmdlist;
}

void CUCode_NewAX::CopyCmdList(u32 addr, u16 size)
{
	if (size >= (sizeof (m_cmdlist) / sizeof (u16)))
	{
		ERROR_LOG(DSPHLE, "Command list at %08x is too large: size=%d", addr, size);
		return;
	}

	for (u32 i = 0; i < size; ++i, addr += 2)
		m_cmdlist[i] = HLEMemory_Read_U16(addr);
	m_cmdlist_size = size;
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
