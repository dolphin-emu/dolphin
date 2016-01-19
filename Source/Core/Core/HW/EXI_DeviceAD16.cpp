// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Common/Assert.h"
#include "Common/ChunkFile.h"
#include "Common/CommonTypes.h"
#include "Core/HW/EXI_DeviceAD16.h"

CEXIAD16::CEXIAD16() :
	m_uPosition(0),
	m_uCommand(0)
{
	m_uAD16Register.U32 = 0x00;
}

void CEXIAD16::SetCS(int cs)
{
	if (cs)
		m_uPosition = 0;
}

bool CEXIAD16::IsPresent() const
{
	return true;
}

void CEXIAD16::TransferByte(u8& _byte)
{
	if (m_uPosition == 0)
	{
		m_uCommand = _byte;
	}
	else
	{
		switch (m_uCommand)
		{
		case init:
			{
				m_uAD16Register.U32 = 0x04120000;
				switch (m_uPosition)
				{
				case 1: _dbg_assert_(EXPANSIONINTERFACE, (_byte == 0x00)); break; // just skip
				case 2: _byte = m_uAD16Register.U8[0]; break;
				case 3: _byte = m_uAD16Register.U8[1]; break;
				case 4: _byte = m_uAD16Register.U8[2]; break;
				case 5: _byte = m_uAD16Register.U8[3]; break;
				}
			}
			break;

		case write:
			{
				switch (m_uPosition)
				{
				case 1: m_uAD16Register.U8[0] = _byte; break;
				case 2: m_uAD16Register.U8[1] = _byte; break;
				case 3: m_uAD16Register.U8[2] = _byte; break;
				case 4: m_uAD16Register.U8[3] = _byte; break;
				}
			}
			break;

		case read:
			{
				switch (m_uPosition)
				{
				case 1: _byte = m_uAD16Register.U8[0]; break;
				case 2: _byte = m_uAD16Register.U8[1]; break;
				case 3: _byte = m_uAD16Register.U8[2]; break;
				case 4: _byte = m_uAD16Register.U8[3]; break;
				}
			}
			break;
		}
	}

	m_uPosition++;
}

void CEXIAD16::DoState(PointerWrap &p)
{
	p.Do(m_uPosition);
	p.Do(m_uCommand);
	p.Do(m_uAD16Register);
}
