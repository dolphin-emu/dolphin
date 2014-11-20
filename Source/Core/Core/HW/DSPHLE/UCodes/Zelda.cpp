// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

// Games that use this UCode (exhaustive list):
// * Animal Crossing (type ????, CRC ????)
// * Donkey Kong Jungle Beat (type ????, CRC ????)
// * IPL (type ????, CRC ????)
// * Luigi's Mansion (type ????, CRC ????)
// * Mario Kart: Double Dash!! (type ????, CRC ????)
// * Pikmin (type ????, CRC ????)
// * Pikmin 2 (type ????, CRC ????)
// * Super Mario Galaxy (type ????, CRC ????)
// * Super Mario Galaxy 2 (type ????, CRC ????)
// * Super Mario Sunshine (type ????, CRC ????)
// * The Legend of Zelda: Four Swords Adventures (type ????, CRC ????)
// * The Legend of Zelda: The Wind Waker (type DAC, CRC 86840740)
// * The Legend of Zelda: Twilight Princess (type ????, CRC ????)

#include "Core/ConfigManager.h"
#include "Core/HW/DSPHLE/MailHandler.h"
#include "Core/HW/DSPHLE/UCodes/UCodes.h"
#include "Core/HW/DSPHLE/UCodes/Zelda.h"

ZeldaUCode::ZeldaUCode(DSPHLE *dsphle, u32 crc)
	: UCodeInterface(dsphle, crc)
{
	m_mail_handler.PushMail(DSP_INIT, true);
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
		m_mail_handler.PushMail(DSP_RESUME, true);
	}
}

u32 ZeldaUCode::GetUpdateMs()
{
	return SConfig::GetInstance().bWii ? 3 : 5;
}

void ZeldaUCode::DoState(PointerWrap &p)
{
	p.Do(m_mail_current_state);
	p.Do(m_mail_expected_cmd_mails);

	p.Do(m_sync_max_voice_id);
	p.Do(m_sync_flags);

	p.Do(m_cmd_buffer);
	p.Do(m_read_offset);
	p.Do(m_write_offset);
	p.Do(m_pending_commands_count);
	p.Do(m_cmd_can_execute);

	p.Do(m_rendering_requested_frames);
	p.Do(m_rendering_voices_per_frame);
	p.Do(m_rendering_mram_lbuf_addr);
	p.Do(m_rendering_mram_rbuf_addr);
	p.Do(m_rendering_curr_frame);
	p.Do(m_rendering_curr_voice);

	DoStateShared(p);
}

void ZeldaUCode::HandleMail(u32 mail)
{
	if (m_upload_setup_in_progress) // evaluated first!
	{
		PrepareBootUCode(mail);
		return;
	}

	switch (m_mail_current_state)
	{
	case MailState::WAITING:
		if (mail & 0x80000000)
		{
			if ((mail >> 16) != 0xCDD1)
			{
				ERROR_LOG(DSPHLE, "Rendering end mail without prefix CDD1: %08x",
				          mail);
			}

			switch (mail & 0xFFFF)
			{
			case 1:
				NOTICE_LOG(DSPHLE, "UCode being replaced.");
				m_upload_setup_in_progress = true;
				SetMailState(MailState::HALTED);
				break;

			case 2:
				NOTICE_LOG(DSPHLE, "UCode being rebooted to ROM.");
				SetMailState(MailState::HALTED);
				m_dsphle->SetUCode(UCODE_ROM);
				break;

			case 3:
				m_cmd_can_execute = true;
				RunPendingCommands();
				break;

			default:
				NOTICE_LOG(DSPHLE, "Unknown end rendering action. Halting.");
			case 0:
				NOTICE_LOG(DSPHLE, "UCode asked to halt. Stopping any processing.");
				SetMailState(MailState::HALTED);
				break;
			}
		}
		else if (!(mail & 0xFFFF))
		{
			if (RenderingInProgress())
			{
				SetMailState(MailState::RENDERING);
			}
			else
			{
				NOTICE_LOG(DSPHLE, "Sync mail (%08x) received when rendering was not active. Halting.",
				           mail);
				SetMailState(MailState::HALTED);
			}
		}
		else
		{
			SetMailState(MailState::WRITING_CMD);
			m_mail_expected_cmd_mails = mail & 0xFFFF;
		}
		break;

	case MailState::RENDERING:
		m_sync_max_voice_id = (((mail >> 16) & 0xF) + 1) << 4;
		m_sync_flags[(mail >> 16) & 0xFF] = mail & 0xFFFF;

		RenderAudio();
		SetMailState(MailState::WAITING);
		break;

	case MailState::WRITING_CMD:
		Write32(mail);

		if (--m_mail_expected_cmd_mails == 0)
		{
			m_pending_commands_count += 1;
			SetMailState(MailState::WAITING);
			RunPendingCommands();
		}
		break;

	case MailState::HALTED:
		WARN_LOG(DSPHLE, "Received mail %08x while we're halted.", mail);
		break;
	}
}

void ZeldaUCode::RunPendingCommands()
{
	if (RenderingInProgress() || !m_cmd_can_execute)
	{
		// No commands can be ran while audio rendering is in progress or
		// waiting for an ACK.
		return;
	}

	while (m_pending_commands_count)
	{
		m_pending_commands_count--;

		u32 cmd_mail = Read32();
		u32 command = (cmd_mail >> 24) & 0x7f;
		u32 sync = cmd_mail >> 16;
		u32 extra_data = cmd_mail & 0xFFFF;

		switch (command)
		{
		case 0x00:
		case 0x03:
		case 0x0A:
		case 0x0B:
		case 0x0C:
		case 0x0E:
		case 0x0F:
			// NOP commands. Log anyway in case we encounter a new version
			// where these are not NOPs anymore.
			NOTICE_LOG(DSPHLE, "Received a NOP command: %d", command);
			SendCommandAck(CommandAck::STANDARD, sync);
			break;

		case 0x04:
		case 0x05:
		case 0x06:
		case 0x07:
		case 0x08:
		case 0x09:
			// Commands that crash the DAC UCode. Log and enter HALTED mode.
			NOTICE_LOG(DSPHLE, "Received a crashy command: %d", command);
			SetMailState(MailState::HALTED);
			return;

		// Command 01: TODO: find a name and implement.
		case 0x01:
			WARN_LOG(DSPHLE, "CMD01: %08x %08x %08x %08x",
			         Read32(), Read32(), Read32(), Read32());
			m_rendering_voices_per_frame = extra_data;
			SendCommandAck(CommandAck::STANDARD, sync);
			break;

		// Command 02: starts audio processing. NOTE: this handler uses return,
		// not break. This is because it hijacks the mail control flow and
		// stops processing of further commands until audio processing is done.
		case 0x02:
			m_rendering_requested_frames = (cmd_mail >> 16) & 0xFF;
			m_rendering_mram_lbuf_addr = Read32();
			m_rendering_mram_rbuf_addr = Read32();

			m_rendering_curr_frame = 0;
			m_rendering_curr_voice = 0;
			RenderAudio();
			return;

		// Command 0D: TODO: find a name and implement.
		case 0x0D:
			WARN_LOG(DSPHLE, "CMD0D: %08x", Read32());
			SendCommandAck(CommandAck::STANDARD, sync);
			break;

		default:
			NOTICE_LOG(DSPHLE, "Received a non-existing command (%d), halting.", command);
			SetMailState(MailState::HALTED);
			return;
		}
	}
}

void ZeldaUCode::SendCommandAck(CommandAck ack_type, u16 sync_value)
{
	u32 ack_mail;
	switch (ack_type)
	{
	case CommandAck::STANDARD: ack_mail = DSP_SYNC; break;
	case CommandAck::DONE_RENDERING: ack_mail = DSP_FRAME_END; break;
	}
	m_mail_handler.PushMail(ack_mail, true);

	if (ack_type == CommandAck::STANDARD)
	{
		m_mail_handler.PushMail(0xF3550000 | sync_value);
	}
}

void ZeldaUCode::RenderAudio()
{
	WARN_LOG(DSPHLE, "RenderAudio() frame %d/%d voice %d/%d (sync to %d)",
	         m_rendering_curr_frame, m_rendering_requested_frames,
	         m_rendering_curr_voice, m_rendering_voices_per_frame,
	         m_sync_max_voice_id);

	if (!RenderingInProgress())
	{
		WARN_LOG(DSPHLE, "Trying to render audio while no rendering should be happening.");
		return;
	}

	while (m_rendering_curr_frame < m_rendering_requested_frames)
	{
		while (m_rendering_curr_voice < m_rendering_voices_per_frame)
		{
			// If we are not meant to render this voice yet, go back to message
			// processing.
			if (m_rendering_curr_voice >= m_sync_max_voice_id)
				return;

			// TODO(delroth): render.

			m_rendering_curr_voice++;
		}

		SendCommandAck(CommandAck::STANDARD, 0xFF00 | m_rendering_curr_frame);

		m_rendering_curr_voice = 0;
		m_sync_max_voice_id = 0;
		m_rendering_curr_frame++;
	}

	SendCommandAck(CommandAck::DONE_RENDERING, 0);
	m_cmd_can_execute = false;  // Block command execution until ACK is received.
}
