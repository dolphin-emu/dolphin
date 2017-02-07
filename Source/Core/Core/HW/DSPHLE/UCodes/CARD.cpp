// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/HW/DSPHLE/UCodes/CARD.h"

#include "Common/CommonTypes.h"
#include "Common/Logging/Log.h"
#include "Core/HW/DSP.h"
#include "Core/HW/DSPHLE/DSPHLE.h"
#include "Core/HW/DSPHLE/UCodes/UCodes.h"

namespace DSP
{
namespace HLE
{
CARDUCode::CARDUCode(DSPHLE* dsphle, u32 crc) : UCodeInterface(dsphle, crc)
{
  INFO_LOG(DSPHLE, "CARDUCode - initialized");
}

CARDUCode::~CARDUCode()
{
  m_mail_handler.Clear();
}

void CARDUCode::Initialize()
{
  m_mail_handler.PushMail(DSP_INIT);
}

void CARDUCode::Update()
{
  // check if we have to sent something
  if (!m_mail_handler.IsEmpty())
  {
    DSP::GenerateDSPInterruptFromDSPEmu(DSP::INT_DSP);
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
    WARN_LOG(DSPHLE, "CARDUCode - unknown command: %x", mail);
  }

  m_mail_handler.PushMail(DSP_DONE);
  m_dsphle->SetUCode(UCODE_ROM);
}
}  // namespace HLE
}  // namespace DSP
