// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/HW/DSPHLE/MailHandler.h"

#include <queue>

#include "Common/ChunkFile.h"
#include "Common/CommonTypes.h"
#include "Common/Logging/Log.h"
#include "Common/MsgHandler.h"
#include "Core/HW/DSP.h"

namespace DSP::HLE
{
CMailHandler::CMailHandler()
{
}

CMailHandler::~CMailHandler()
{
  Clear();
}

void CMailHandler::PushMail(u32 mail, bool interrupt, int cycles_into_future)
{
  if (interrupt)
  {
    if (m_Mails.empty())
    {
      DSP::GenerateDSPInterruptFromDSPEmu(DSP::INT_DSP, cycles_into_future);
    }
    else
    {
      m_Mails.front().second = true;
    }
  }
  m_Mails.emplace(mail, false);
  DEBUG_LOG(DSP_MAIL, "DSP writes 0x%08x", mail);
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
    m_Mails.pop();

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
    PushMail(0x80544348);
  }
}

void CMailHandler::DoState(PointerWrap& p)
{
  if (p.GetMode() == PointerWrap::MODE_READ)
  {
    Clear();
    int sz = 0;
    p.Do(sz);
    for (int i = 0; i < sz; i++)
    {
      u32 mail = 0;
      bool interrupt = false;
      p.Do(mail);
      p.Do(interrupt);
      m_Mails.emplace(mail, interrupt);
    }
  }
  else  // WRITE and MEASURE
  {
    std::queue<std::pair<u32, bool>> temp;
    int sz = (int)m_Mails.size();
    p.Do(sz);
    for (int i = 0; i < sz; i++)
    {
      u32 value = m_Mails.front().first;
      bool interrupt = m_Mails.front().second;
      m_Mails.pop();
      p.Do(value);
      p.Do(interrupt);
      temp.emplace(value, interrupt);
    }
    if (!m_Mails.empty())
      PanicAlert("CMailHandler::DoState - WTF?");

    // Restore queue.
    for (int i = 0; i < sz; i++)
    {
      u32 value = temp.front().first;
      bool interrupt = temp.front().second;
      temp.pop();
      m_Mails.emplace(value, interrupt);
    }
  }
}
}  // namespace DSP::HLE
