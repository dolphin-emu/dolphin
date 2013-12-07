// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "UCodes.h"
#include "UCode_InitAudioSystem.h"
#include "ConfigManager.h"

CUCode_InitAudioSystem::CUCode_InitAudioSystem(DSPHLE *dsp_hle, u32 crc)
	: IUCode(dsp_hle, crc)
{
	DEBUG_LOG(DSPHLE, "CUCode_InitAudioSystem - initialized");
}


CUCode_InitAudioSystem::~CUCode_InitAudioSystem()
{
}


void CUCode_InitAudioSystem::Init()
{
}


void CUCode_InitAudioSystem::Update(int cycles)
{
	if (m_rMailHandler.IsEmpty())
	{
		m_rMailHandler.PushMail(0x80544348);
		// HALT
	}
}

u32 CUCode_InitAudioSystem::GetUpdateMs()
{
	return SConfig::GetInstance().m_LocalCoreStartupParameter.bWii ? 3 : 5;
}

void CUCode_InitAudioSystem::HandleMail(u32 _uMail)
{
}


