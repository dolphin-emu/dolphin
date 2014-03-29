// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "Core/ConfigManager.h"
#include "Core/HW/DSP.h"
#include "Core/HW/DSPHLE/DSPHLE.h"
#include "Core/HW/DSPHLE/UCodes/CARD.h"
#include "Core/HW/DSPHLE/UCodes/UCodes.h"


CARDUCode::CARDUCode(DSPHLE *dsp_hle, u32 crc)
	: UCodeInterface(dsp_hle, crc)
{
	DEBUG_LOG(DSPHLE, "CARDUCode - initialized");
	m_rMailHandler.PushMail(DSP_INIT);
}


CARDUCode::~CARDUCode()
{
	m_rMailHandler.Clear();
}


void CARDUCode::Update(int cycles)
{
	// check if we have to sent something
	if (!m_rMailHandler.IsEmpty())
	{
		DSP::GenerateDSPInterruptFromDSPEmu(DSP::INT_DSP);
	}
}

u32 CARDUCode::GetUpdateMs()
{
	return SConfig::GetInstance().m_LocalCoreStartupParameter.bWii ? 3 : 5;
}

void CARDUCode::HandleMail(u32 _uMail)
{
	if (_uMail == 0xFF000000) // unlock card
	{
		//	m_Mails.push(0x00000001); // ACK (actually anything != 0)
	}
	else
	{
		DEBUG_LOG(DSPHLE, "CARDUCode - unknown command: %x", _uMail);
	}

	m_rMailHandler.PushMail(DSP_DONE);
	m_DSPHLE->SetUCode(UCODE_ROM);
}


