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
CMailHandler::CMailHandler(DSP::DSPManager& dsp) : m_dsp(dsp)
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
      m_dsp.GenerateDSPInterruptFromDSPEmu(DSP::INT_DSP, cycles_into_future);
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
  if (!m_halted && !m_pending_mails.empty())
  {
    m_last_mail = m_pending_mails.front().first;
  }
  return u16(m_last_mail >> 0x10);
}

u16 CMailHandler::ReadDSPMailboxLow()
{
  // check if we have a mail for the CPU core
  if (!m_halted && !m_pending_mails.empty())
  {
    m_last_mail = m_pending_mails.front().first;
    const bool generate_interrupt = m_pending_mails.front().second;

    m_pending_mails.pop_front();

    if (generate_interrupt)
    {
      m_dsp.GenerateDSPInterruptFromDSPEmu(DSP::INT_DSP);
    }
  }
  // Clear the top bit of the high mail word after the mail has been read.
  // The remaining bits read back the same as the previous mail, until new mail sent.
  // (The CPU reads the high word first, and then the low word; since this function returns the low
  // word, this means that the next read of the high word will have the top bit cleared.)
  m_last_mail &= ~0x8000'0000;
  return u16(m_last_mail & 0xffff);
}

void CMailHandler::ClearPending()
{
  m_pending_mails.clear();
}

bool CMailHandler::HasPending() const
{
  return !m_pending_mails.empty();
}

void CMailHandler::SetHalted(bool halt)
{
  m_halted = halt;
}

void CMailHandler::DoState(PointerWrap& p)
{
  p.Do(m_pending_mails);
  p.Do(m_last_mail);
  p.Do(m_halted);
}
}  // namespace DSP::HLE
