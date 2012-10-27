// Copyright (C) 2003 Dolphin Project.

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.0.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License 2.0 for more details.

// A copy of the GPL 2.0 should have been included with the program.
// If not, see http://www.gnu.org/licenses/

// Official SVN repository and contact information can be found at
// http://code.google.com/p/dolphin-emu/

#include "UCodes.h"
#include "UCode_ROM.h"
#include "Hash.h"
#include "../../Memmap.h"

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

void CUCode_Rom::DoState(PointerWrap &p)
{
	p.Do(m_CurrentUCode);
	p.Do(m_BootTask_numSteps);
	p.Do(m_NextParameter);

	DoStateShared(p);
}

