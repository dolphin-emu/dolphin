// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/HW/DSPHLE/UCodes/INIT.h"

#include "Common/CommonTypes.h"
#include "Common/Logging/Log.h"
#include "Core/HW/DSPHLE/DSPHLE.h"
#include "Core/HW/DSPHLE/MailHandler.h"
#include "Core/HW/DSPHLE/UCodes/UCodes.h"

namespace DSP
{
namespace HLE
{
INITUCode::INITUCode(DSPHLE* dsphle, u32 crc) : UCodeInterface(dsphle, crc)
{
  INFO_LOG(DSPHLE, "INITUCode - initialized");
}

INITUCode::~INITUCode()
{
}

void INITUCode::Initialize()
{
  m_mail_handler.PushMail(0x80544348);
}

void INITUCode::Init()
{
}

void INITUCode::Update()
{
}

void INITUCode::HandleMail(u32 mail)
{
}
}  // namespace HLE
}  // namespace DSP
