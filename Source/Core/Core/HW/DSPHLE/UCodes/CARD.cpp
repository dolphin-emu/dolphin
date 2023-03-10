// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/HW/DSPHLE/UCodes/CARD.h"

#include "Common/ChunkFile.h"
#include "Common/CommonTypes.h"
#include "Common/Logging/Log.h"
#include "Core/HW/DSP.h"
#include "Core/HW/DSPHLE/DSPHLE.h"
#include "Core/HW/DSPHLE/UCodes/UCodes.h"
#include "Core/System.h"

namespace DSP::HLE
{
CARDUCode::CARDUCode(DSPHLE* dsphle, u32 crc) : UCodeInterface(dsphle, crc)
{
  INFO_LOG_FMT(DSPHLE, "CARDUCode - initialized");
}

void CARDUCode::Initialize()
{
  m_mail_handler.PushMail(DSP_INIT);
}

void CARDUCode::Update()
{
  // check if we have something to send
  if (m_mail_handler.HasPending())
  {
    Core::System::GetInstance().GetDSP().GenerateDSPInterruptFromDSPEmu(DSP::INT_DSP);
  }
}

void CARDUCode::HandleMail(u32 mail)
{
  if (mail == 0xFF000000)  // unlock card
  {
    //	m_Mails.push(0x00000001); // ACK (actually anything != 0)
  }
  else
  {
    WARN_LOG_FMT(DSPHLE, "CARDUCode - unknown command: {:x}", mail);
  }

  m_mail_handler.PushMail(DSP_DONE);
  m_dsphle->SetUCode(UCODE_ROM);
}

void CARDUCode::DoState(PointerWrap& p)
{
  DoStateShared(p);
  p.Do(m_state);
}
}  // namespace DSP::HLE
