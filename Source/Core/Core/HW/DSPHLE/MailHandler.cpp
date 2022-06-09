// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/HW/DSPHLE/MailHandler.h"

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
}

void CMailHandler::PushMail(u32 mail, bool interrupt, int cycles_into_future)
{
  if (interrupt)
  {
    if (m_pending_mails.empty())
    {
      DSP::GenerateDSPInterruptFromDSPEmu(DSP::INT_DSP, cycles_into_future);
    }
    else
    {
      m_pending_mails.front().second = true;
    }
  }
  m_pending_mails.emplace_back(mail, false);
  DEBUG_LOG_FMT(DSP_MAIL, "DSP writes {:#010x}", mail);
}

u16 CMailHandler::ReadDSPMailboxHigh()
{
  // check if we have a mail for the CPU core
  if (!m_pending_mails.empty())
  {
    u16 result = (m_pending_mails.front().first >> 16) & 0xFFFF;
    return result;
  }
  return 0x00;
}

u16 CMailHandler::ReadDSPMailboxLow()
{
  // check if we have a mail for the CPU core
  if (!m_pending_mails.empty())
  {
    u16 result = m_pending_mails.front().first & 0xFFFF;
    const bool generate_interrupt = m_pending_mails.front().second;
    m_pending_mails.pop_front();

    if (generate_interrupt)
    {
      DSP::GenerateDSPInterruptFromDSPEmu(DSP::INT_DSP);
    }

    return result;
  }
  return 0x00;
}

void CMailHandler::ClearPending()
{
  m_pending_mails.clear();
}

bool CMailHandler::HasPending() const
{
  return !m_pending_mails.empty();
}

void CMailHandler::Halt(bool _Halt)
{
  if (_Halt)
  {
    ClearPending();
    PushMail(0x80544348);
  }
}

void CMailHandler::DoState(PointerWrap& p)
{
  p.Do(m_pending_mails);
}
}  // namespace DSP::HLE
