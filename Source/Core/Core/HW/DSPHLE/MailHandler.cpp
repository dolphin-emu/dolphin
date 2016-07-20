// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/HW/DSPHLE/MailHandler.h"
#include "Common/ChunkFile.h"
#include "Common/CommonTypes.h"
#include "Common/Logging/Log.h"
#include "Common/MsgHandler.h"
#include "Core/HW/DSP.h"

CMailHandler::CMailHandler()
{
}

CMailHandler::~CMailHandler()
{
  Clear();
}

void CMailHandler::PushMail(u32 _Mail, bool interrupt)
{
  if (interrupt)
  {
    if (m_Mails.empty())
    {
      DSP::GenerateDSPInterruptFromDSPEmu(DSP::INT_DSP);
    }
    else
    {
      m_Mails.front().second = true;
    }
  }
  m_Mails.emplace_back(_Mail, false);
  DEBUG_LOG(DSP_MAIL, "DSP writes 0x%08x", _Mail);
}

u16 CMailHandler::ReadDSPMailboxHigh()
{
  // check if we have a mail for the core
  if (!m_Mails.empty())
  {
    u16 result = (m_Mails.front().first >> 16) & 0xFFFF;
    return result;
  }
  return 0x00;
}

u16 CMailHandler::ReadDSPMailboxLow()
{
  // check if we have a mail for the core
  if (!m_Mails.empty())
  {
    u16 result = m_Mails.front().first & 0xFFFF;
    bool generate_interrupt = m_Mails.front().second;
    m_Mails.pop_front();

    if (generate_interrupt)
    {
      DSP::GenerateDSPInterruptFromDSPEmu(DSP::INT_DSP);
    }

    return result;
  }
  return 0x00;
}

void CMailHandler::Clear()
{
  m_Mails.clear();
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
    PushMail(0x80544348);
  }
}

void CMailHandler::DoState(StateLoadStore& p)
{
  p.Do(m_Mails);
}
