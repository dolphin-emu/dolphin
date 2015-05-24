// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Common/ChunkFile.h"
#include "Core/HW/DSPHLE/MailHandler.h"

CMailHandler::CMailHandler()
{
}

CMailHandler::~CMailHandler()
{
	Clear();
}

void CMailHandler::PushMail(u32 _Mail)
{
	m_Mails.push(_Mail);
	DEBUG_LOG(DSP_MAIL, "DSP writes 0x%08x", _Mail);
}

u16 CMailHandler::ReadDSPMailboxHigh()
{
	// check if we have a mail for the core
	if (!m_Mails.empty())
	{
		u16 result = (m_Mails.front() >> 16) & 0xFFFF;
		return result;
	}
	return 0x00;
}

u16 CMailHandler::ReadDSPMailboxLow()
{
	// check if we have a mail for the core
	if (!m_Mails.empty())
	{
		u16 result = m_Mails.front() & 0xFFFF;
		m_Mails.pop();
		return result;
	}
	return 0x00;
}

void CMailHandler::Clear()
{
	while (!m_Mails.empty())
		m_Mails.pop();
}

bool CMailHandler::IsEmpty() const
{
	return m_Mails.empty();
}

void CMailHandler::Halt(bool _Halt)
{
	if (_Halt)
	{
		Clear();
		m_Mails.push(0x80544348);
	}
}

void CMailHandler::DoState(PointerWrap &p)
{
	if (p.GetMode() == PointerWrap::MODE_READ)
	{
		Clear();
		int sz = 0;
		p.Do(sz);
		for (int i = 0; i < sz; i++)
		{
			u32 mail = 0;
			p.Do(mail);
			m_Mails.push(mail);
		}
	}
	else  // WRITE and MEASURE
	{
		std::queue<u32> temp;
		int sz = (int)m_Mails.size();
		p.Do(sz);
		for (int i = 0; i < sz; i++)
		{
			u32 value = m_Mails.front();
			m_Mails.pop();
			p.Do(value);
			temp.push(value);
		}
		if (!m_Mails.empty())
			PanicAlert("CMailHandler::DoState - WTF?");

		// Restore queue.
		for (int i = 0; i < sz; i++)
		{
			u32 value = temp.front();
			temp.pop();
			m_Mails.push(value);
		}
	}
}
