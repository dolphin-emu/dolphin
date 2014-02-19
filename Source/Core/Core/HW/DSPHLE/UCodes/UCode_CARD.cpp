// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "Core/ConfigManager.h"
#include "Core/HW/DSP.h"
#include "Core/HW/DSPHLE/DSPHLE.h"
#include "Core/HW/DSPHLE/UCodes/UCode_CARD.h"
#include "Core/HW/DSPHLE/UCodes/UCodes.h"


CUCode_CARD::CUCode_CARD(DSPHLE *dsp_hle, u32 crc)
	: IUCode(dsp_hle, crc)
{
	DEBUG_LOG(DSPHLE, "CUCode_CARD - initialized");
	m_rMailHandler.PushMail(DSP_INIT);
}


CUCode_CARD::~CUCode_CARD()
{
	m_rMailHandler.Clear();
}


void CUCode_CARD::Update(int cycles)
{
	// check if we have to sent something
	if (!m_rMailHandler.IsEmpty())
	{
		DSP::GenerateDSPInterruptFromDSPEmu(DSP::INT_DSP);
	}
}

u32 CUCode_CARD::GetUpdateMs()
{
	return SConfig::GetInstance().m_LocalCoreStartupParameter.bWii ? 3 : 5;
}

void CUCode_CARD::HandleMail(u32 _uMail)
{
	if (_uMail == 0xFF000000) // unlock card
	{
		//	m_Mails.push(0x00000001); // ACK (actually anything != 0)
	}
	else
	{
		DEBUG_LOG(DSPHLE, "CUCode_CARD - unknown command: %x", _uMail);
	}

	m_rMailHandler.PushMail(DSP_DONE);
	m_DSPHLE->SetUCode(UCODE_ROM);
}


