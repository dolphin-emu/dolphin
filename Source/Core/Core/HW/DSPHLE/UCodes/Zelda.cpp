// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

// Games that uses this UCode (exhaustive list):
// * Animal Crossing (type ????, CRC ????)
// * Donkey Kong Jungle Beat (type ????, CRC ????)
// * IPL (type ????, CRC ????)
// * Luigi's Mansion (type ????, CRC ????)
// * Mario Kary: Double Dash!! (type ????, CRC ????)
// * Pikmin (type ????, CRC ????)
// * Pikmin 2 (type ????, CRC ????)
// * Super Mario Galaxy (type ????, CRC ????)
// * Super Mario Galaxy 2 (type ????, CRC ????)
// * Super Mario Sunshine (type ????, CRC ????)
// * The Legend of Zelda: Four Swords Adventures (type ????, CRC ????)
// * The Legend of Zelda: The Wind Waker (type Normal, CRC 86840740)
// * The Legend of Zelda: Twilight Princess (type ????, CRC ????)

#include "Core/ConfigManager.h"
#include "Core/HW/DSP.h"
#include "Core/HW/DSPHLE/MailHandler.h"
#include "Core/HW/DSPHLE/UCodes/UCodes.h"
#include "Core/HW/DSPHLE/UCodes/Zelda.h"

ZeldaUCode::ZeldaUCode(DSPHLE *dsphle, u32 crc)
	: UCodeInterface(dsphle, crc),
	  m_sync_in_progress(false),
	  m_max_voice(0),
	  m_num_sync_mail(0),
	  m_num_voices(0),
	  m_sync_cmd_pending(false),
	  m_current_voice(0),
	  m_current_buffer(0),
	  m_num_buffers(0),
	  m_num_steps(0),
	  m_list_in_progress(false),
	  m_step(0),
	  m_read_offset(0)
{
	m_mail_handler.PushMail(DSP_INIT);
	DSP::GenerateDSPInterruptFromDSPEmu(DSP::INT_DSP);
	m_mail_handler.PushMail(0xF3551111); // handshake
}

ZeldaUCode::~ZeldaUCode()
{
	m_mail_handler.Clear();
}

void ZeldaUCode::Update()
{
	if (NeedsResumeMail())
	{
		m_mail_handler.PushMail(DSP_RESUME);
		DSP::GenerateDSPInterruptFromDSPEmu(DSP::INT_DSP);
	}
}

void ZeldaUCode::HandleMail(u32 mail)
{
	if (m_upload_setup_in_progress) // evaluated first!
	{
		PrepareBootUCode(mail);
		return;
	}

	if (m_sync_in_progress)
	{
		if (m_sync_cmd_pending)
		{
			u32 n = (mail >> 16) & 0xF;
			m_max_voice = (n + 1) << 4;
			m_sync_flags[n] = mail & 0xFFFF;
			m_sync_in_progress = false;

			m_current_voice = m_max_voice;

			if (m_current_voice >= m_num_voices)
			{
				// TODO(delroth): Mix audio.

				m_current_buffer++;

				m_mail_handler.PushMail(DSP_SYNC);
				DSP::GenerateDSPInterruptFromDSPEmu(DSP::INT_DSP);
				m_mail_handler.PushMail(0xF355FF00 | m_current_buffer);

				m_current_voice = 0;

				if (m_current_buffer == m_num_buffers)
				{
					m_mail_handler.PushMail(DSP_FRAME_END);
					m_sync_cmd_pending = false;
				}
			}
		}
		else
		{
			m_sync_in_progress = false;
		}

		return;
	}

	if (m_list_in_progress)
	{
		if (m_step >= sizeof(m_buffer) / 4)
			PanicAlert("m_step out of range");

		((u32*)m_buffer)[m_step] = mail;
		m_step++;

		if (m_step >= m_num_steps)
		{
			ExecuteList();
			m_list_in_progress = false;
		}

		return;
	}

	// Here holds: m_sync_in_progress == false && m_list_in_progress == false

	// Zelda-only mails:
	// - 0000XXXX - Begin list
	// - 00000000, 000X0000 - Sync mails
	// - CDD1XXXX - comes after DsyncFrame completed, seems to be debugging stuff

	if (mail == 0)
	{
		m_sync_in_progress = true;
	}
	else if ((mail >> 16) == 0)
	{
		m_list_in_progress = true;
		m_num_steps = mail;
		m_step = 0;
	}
	else if ((mail >> 16) == 0xCDD1) // A 0xCDD1000X mail should come right after we send a DSP_FRAME_END mail
	{
		// The low part of the mail tells the operation to perform
		// Seeing as every possible operation number halts the uCode,
		// except 3, that thing seems to be intended for debugging
		switch (mail & 0xFFFF)
		{
		case 0x0003: // Do nothing - continue normally
			return;

		case 0x0001: // accepts params to either DMA to iram and/or DRAM (used for hotbooting a new ucode)
			// TODO find a better way to protect from HLEMixer?
			m_upload_setup_in_progress = true;
			return;

		case 0x0002: // Let IROM play us off
			m_dsphle->SetUCode(UCODE_ROM);
			return;

		case 0x0000: // Halt
			WARN_LOG(DSPHLE, "Zelda uCode: received halting operation %04X", mail & 0xFFFF);
			return;

		default:     // Invalid (the real ucode would likely crash)
			WARN_LOG(DSPHLE, "Zelda uCode: received invalid operation %04X", mail & 0xFFFF);
			return;
		}
	}
	else
	{
		WARN_LOG(DSPHLE, "Zelda uCode: unknown mail %08X", mail);
	}
}

void ZeldaUCode::ExecuteList()
{
	m_read_offset = 0;

	u32 cmd_mail = Read32();
	u32 command = (cmd_mail >> 24) & 0x7f;
	u32 sync;
	u32 extra_data = cmd_mail & 0xFFFF;

	sync = cmd_mail >> 16;

	switch (command)
	{
		case 0x00: break;

		case 0x01:
			Read32(); Read32(); Read32(); Read32();
			break;

		case 0x02:
			Read32(); Read32();
			return;

		case 0x03: break;

		case 0x0d:
			Read32();
			break;

		case 0x0e:
			Read32();
			break;

		default:
			PanicAlert("Zelda UCode - unknown command: %x (size %i)", command, m_num_steps);
			break;
	}

	m_mail_handler.PushMail(DSP_SYNC);
	DSP::GenerateDSPInterruptFromDSPEmu(DSP::INT_DSP);
	m_mail_handler.PushMail(0xF3550000 | sync);
}

u32 ZeldaUCode::GetUpdateMs()
{
	return SConfig::GetInstance().bWii ? 3 : 5;
}

void ZeldaUCode::DoState(PointerWrap &p)
{
	p.Do(m_sync_in_progress);
	p.Do(m_max_voice);
	p.Do(m_sync_flags);

	p.Do(m_num_sync_mail);

	p.Do(m_num_voices);

	p.Do(m_sync_cmd_pending);
	p.Do(m_current_voice);
	p.Do(m_current_buffer);
	p.Do(m_num_buffers);

	p.Do(m_num_steps);
	p.Do(m_list_in_progress);
	p.Do(m_step);
	p.Do(m_buffer);

	p.Do(m_read_offset);

	DoStateShared(p);
}
