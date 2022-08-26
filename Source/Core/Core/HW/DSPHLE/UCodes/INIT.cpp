// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/HW/DSPHLE/UCodes/INIT.h"

#include "Common/CommonTypes.h"
#include "Common/Logging/Log.h"
#include "Core/HW/DSPHLE/DSPHLE.h"
#include "Core/HW/DSPHLE/MailHandler.h"
#include "Core/HW/DSPHLE/UCodes/UCodes.h"

namespace DSP::HLE
{
INITUCode::INITUCode(DSPHLE* dsphle, u32 crc) : UCodeInterface(dsphle, crc)
{
  INFO_LOG_FMT(DSPHLE, "INITUCode - initialized");
}

void INITUCode::Initialize()
{
  m_mail_handler.PushMail(0x80544348);
}

void INITUCode::Update()
{
}

void INITUCode::HandleMail(u32 mail)
{
}

void INITUCode::DoState(PointerWrap& p)
{
  // We don't need to call DoStateShared() as the init uCode doesn't support launching new uCode
}
}  // namespace DSP::HLE
