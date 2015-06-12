// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/ConfigManager.h"
#include "Core/HW/DSPHLE/UCodes/INIT.h"
#include "Core/HW/DSPHLE/UCodes/UCodes.h"

INITUCode::INITUCode(DSPHLE *dsphle, u32 crc)
	: UCodeInterface(dsphle, crc)
{
	DEBUG_LOG(DSPHLE, "INITUCode - initialized");
}

INITUCode::~INITUCode()
{
}

void INITUCode::Init()
{
}

void INITUCode::Update()
{
	if (m_mail_handler.IsEmpty())
	{
		m_mail_handler.PushMail(0x80544348);
		// HALT
	}
}

u32 INITUCode::GetUpdateMs()
{
	return SConfig::GetInstance().bWii ? 3 : 5;
}

void INITUCode::HandleMail(u32 mail)
{
}


