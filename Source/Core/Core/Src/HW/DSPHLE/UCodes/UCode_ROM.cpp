// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "UCodes.h"
#include "UCode_ROM.h"
#include "Hash.h"
#include "../../Memmap.h"
#include "ConfigManager.h"

CUCode_Rom::CUCode_Rom(DSPHLE *dsp_hle, u32 crc)
	: IUCode(dsp_hle, crc)
	, m_CurrentUCode()
	, m_BootTask_numSteps(0)
	, m_NextParameter(0)
{
	DEBUG_LOG(DSPHLE, "UCode_Rom - initialized");
	m_rMailHandler.Clear();
	m_rMailHandler.PushMail(0x8071FEED);
}

CUCode_Rom::~CUCode_Rom()
{}

void CUCode_Rom::Update(int cycles)
{}

void CUCode_Rom::HandleMail(u32 _uMail)
{
	if (m_NextParameter == 0)
	{
		// wait for beginning of UCode
		if ((_uMail & 0xFFFF0000) != 0x80F30000)
		{
			u32 Message = 0xFEEE0000 | (_uMail & 0xFFFF);
			m_rMailHandler.PushMail(Message);
		}
		else
		{
			m_NextParameter = _uMail;
		}
	}
	else
	{
		switch (m_NextParameter)
		{
			case 0x80F3A001:
				m_CurrentUCode.m_RAMAddress = _uMail;
				break;

			case 0x80F3A002:
				m_CurrentUCode.m_Length = _uMail & 0xffff;
				break;

			case 0x80F3C002:
				m_CurrentUCode.m_IMEMAddress = _uMail & 0xffff;
				break;

			case 0x80F3B002:
				m_CurrentUCode.m_DMEMLength = _uMail & 0xffff;
				if (m_CurrentUCode.m_DMEMLength) {
					NOTICE_LOG(DSPHLE,"m_CurrentUCode.m_DMEMLength = 0x%04x.", m_CurrentUCode.m_DMEMLength);
				}
				break;

			case 0x80F3D001:
			{
				m_CurrentUCode.m_StartPC = _uMail & 0xffff;
				BootUCode();
				return;  // Important! BootUCode indirectly does "delete this;". Must exit immediately.
			}
			break;

			default:
			break;
		}

		// THE GODDAMN OVERWRITE WAS HERE. Without the return above, since BootUCode may delete "this", well ...
		m_NextParameter = 0;
	}
}

void CUCode_Rom::BootUCode()
{
	u32 ector_crc = HashEctor(
		(u8*)HLEMemory_Get_Pointer(m_CurrentUCode.m_RAMAddress),
		m_CurrentUCode.m_Length);

#if defined(_DEBUG) || defined(DEBUGFAST)
	char binFile[MAX_PATH];
	sprintf(binFile, "%sDSP_UC_%08X.bin", File::GetUserPath(D_DUMPDSP_IDX).c_str(), ector_crc);

	File::IOFile pFile(binFile, "wb");
	pFile.WriteArray((u8*)Memory::GetPointer(m_CurrentUCode.m_RAMAddress), m_CurrentUCode.m_Length);
#endif

	DEBUG_LOG(DSPHLE, "CurrentUCode SOURCE Addr: 0x%08x", m_CurrentUCode.m_RAMAddress);
	DEBUG_LOG(DSPHLE, "CurrentUCode Length:      0x%08x", m_CurrentUCode.m_Length);
	DEBUG_LOG(DSPHLE, "CurrentUCode DEST Addr:   0x%08x", m_CurrentUCode.m_IMEMAddress);
	DEBUG_LOG(DSPHLE, "CurrentUCode DMEM Length: 0x%08x", m_CurrentUCode.m_DMEMLength);
	DEBUG_LOG(DSPHLE, "CurrentUCode init_vector: 0x%08x", m_CurrentUCode.m_StartPC);
	DEBUG_LOG(DSPHLE, "CurrentUCode CRC:         0x%08x", ector_crc);
	DEBUG_LOG(DSPHLE, "BootTask - done");

	m_DSPHLE->SetUCode(ector_crc);
}

u32 CUCode_Rom::GetUpdateMs()
{
	return SConfig::GetInstance().m_LocalCoreStartupParameter.bWii ? 3 : 5;
}

void CUCode_Rom::DoState(PointerWrap &p)
{
	p.Do(m_CurrentUCode);
	p.Do(m_BootTask_numSteps);
	p.Do(m_NextParameter);

	DoStateShared(p);
}

