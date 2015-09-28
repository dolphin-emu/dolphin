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
	m_mail_handler.PushMail(0x80544348);
}

INITUCode::~INITUCode()
{
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


