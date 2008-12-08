// Copyright (C) 2003-2008 Dolphin Project.

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

#include "../Core.h"

#include "EXI_Device.h"
#include "EXI_DeviceAD16.h"

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

bool CEXIAD16::IsPresent()
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
		switch(m_uCommand)
		{
		case init:
			{
				m_uAD16Register.U32 = 0x04120000;
				switch(m_uPosition)
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
				switch(m_uPosition)
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
				switch(m_uPosition)
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
