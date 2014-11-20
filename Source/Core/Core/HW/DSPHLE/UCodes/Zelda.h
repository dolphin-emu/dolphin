// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "Common/CommonTypes.h"
#include "Core/HW/DSPHLE/UCodes/UCodes.h"

class ZeldaUCode : public UCodeInterface
{
public:
	ZeldaUCode(DSPHLE *dsphle, u32 crc);
	virtual ~ZeldaUCode();
	u32 GetUpdateMs() override;

	void HandleMail(u32 mail) override;
	void Update() override;

	void DoState(PointerWrap &p) override;

private:
	// UCode state machine. The control flow in the Zelda UCode family is quite
	// complex, using interrupt handlers heavily to handle incoming messages
	// which, depending on the type, get handled immediately or are queued in a
	// command buffer. In this implementation, the synchronous+interrupts flow
	// of the original DSP implementation is rewritten in an asynchronous/coro
	// + state machine style. It is less readable, but the best we can do given
	// our constraints.
	enum class MailState : u32
	{
		WAITING,
		RENDERING,
		WRITING_CMD,
		HALTED,
	};
	MailState m_mail_current_state = MailState::WAITING;
	u32 m_mail_expected_cmd_mails = 0;

	// Utility function to set the current state. Useful for debugging and
	// logging as a hook point.
	void SetMailState(MailState new_state)
	{
		// WARN_LOG(DSPHLE, "MailState %d -> %d", m_mail_current_state, new_state);
		m_mail_current_state = new_state;
	}

	// Voice synchronization / audio rendering flow control. When rendering an
	// audio frame, only voices up to max_voice_id will be rendered until a
	// sync mail arrives, increasing the value of max_voice_id. Additionally,
	// these sync mails contain 16 bit values that are used for TODO.
	u32 m_sync_max_voice_id = 0;
	std::array<u32, 256> m_sync_flags;

	// Command buffer (circular queue with r/w indices). Filled by HandleMail
	// when the state machine is in WRITING_CMD state. Commands get executed
	// when entering WAITING state and we are not rendering audio.
	std::array<u32, 64> m_cmd_buffer;
	u32 m_read_offset = 0;
	u32 m_write_offset = 0;
	u32 m_pending_commands_count = 0;
	bool m_cmd_can_execute = true;

	// Reads a 32 bit value from the command buffer. Advances the read pointer.
	u32 Read32()
	{
		if (m_read_offset == m_write_offset)
		{
			ERROR_LOG(DSPHLE, "Reading too many command params");
			return 0;
		}

		u32 res = m_cmd_buffer[m_read_offset];
		m_read_offset = (m_read_offset + 1) % (sizeof (m_cmd_buffer) / sizeof (u32));
		return res;
	}

	// Writes a 32 bit value to the command buffer. Advances the write pointer.
	void Write32(u32 val)
	{
		m_cmd_buffer[m_write_offset] = val;
		m_write_offset = (m_write_offset + 1) % (sizeof (m_cmd_buffer) / sizeof (u32));
	}

	// Tries to run as many commands as possible until either the command
	// buffer is empty (pending_commands == 0) or we reached a long lived
	// command that needs to hijack the mail control flow.
	//
	// Might change the current state to indicate crashy commands.
	void RunPendingCommands();

	// Sends the two mails from DSP to CPU to ack the command execution.
	enum class CommandAck : u32
	{
		STANDARD,
		DONE_RENDERING,
	};
	void SendCommandAck(CommandAck ack_type, u16 sync_value);

	// Audio rendering state.
	u32 m_rendering_requested_frames = 0;
	u16 m_rendering_voices_per_frame = 0;
	u32 m_rendering_mram_lbuf_addr = 0;
	u32 m_rendering_mram_rbuf_addr = 0;
	u32 m_rendering_curr_frame = 0;
	u32 m_rendering_curr_voice = 0;

	bool RenderingInProgress() const { return m_rendering_curr_frame != m_rendering_requested_frames; }
	void RenderAudio();
};
