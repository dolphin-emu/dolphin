// Copyright (C) 2003-2009 Dolphin Project.

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

#include "SI_Channel.h"
#include "SI.h"
#include "PeripheralInterface.h"
#include "../PowerPC/PowerPC.h"

#include "SI.cpp"
using namespace SerialInterface;

CSIChannel::CSIChannel()
{
	m_Out.Hex = 0;
	m_InHi.Hex = 0;
	m_InLo.Hex = 0;

	m_pDevice = SIDevice_Create(SI_DUMMY, m_ChannelId);
	_dbg_assert_(SERIALINTERFACE, m_pDevice != NULL);
}

CSIChannel::~CSIChannel()
{
	RemoveDevice();
}

void CSIChannel::AddDevice(const TSIDevices _device, int _iDeviceNumber)
{
	//_dbg_assert_(SERIALINTERFACE, _iSlot < NUM_DEVICES);

	// delete the old device
	RemoveDevice();

	// create the new one
	m_pDevice = SIDevice_Create(_device, _iDeviceNumber);
	_dbg_assert_(SERIALINTERFACE, m_pDevice != NULL);
}

void CSIChannel::RemoveDevice()
{
	if (m_pDevice != NULL)
	{
		delete m_pDevice;
		m_pDevice = NULL;
	}
}

void CSIChannel::Read32(u32& _uReturnValue, const u32 _iAddr)
{
	// registers
	switch (_iAddr)
	{
		//////////////////////////////////////////////////////////////////////////
		// Channel 0
		//////////////////////////////////////////////////////////////////////////
	case SI_CHANNEL_0_OUT:
		_uReturnValue = m_Out.Hex;
		return;

	case SI_CHANNEL_0_IN_HI:
		g_StatusReg.RDST0 = 0;
		UpdateInterrupts();
		_uReturnValue = m_InHi.Hex;
		return;

	case SI_CHANNEL_0_IN_LO:
		g_StatusReg.RDST0 = 0;
		UpdateInterrupts();
		_uReturnValue = m_InLo.Hex;
		return;

		//////////////////////////////////////////////////////////////////////////
		// Channel 1
		//////////////////////////////////////////////////////////////////////////
	case SI_CHANNEL_1_OUT:
		_uReturnValue = m_Out.Hex;
		return;

	case SI_CHANNEL_1_IN_HI:
		g_StatusReg.RDST1 = 0;
		UpdateInterrupts();
		_uReturnValue = m_InHi.Hex;
		return;

	case SI_CHANNEL_1_IN_LO:
		g_StatusReg.RDST1 = 0;
		UpdateInterrupts();
		_uReturnValue = m_InLo.Hex;
		return;

		//////////////////////////////////////////////////////////////////////////
		// Channel 2
		//////////////////////////////////////////////////////////////////////////
	case SI_CHANNEL_2_OUT:
		_uReturnValue = m_Out.Hex;
		return;

	case SI_CHANNEL_2_IN_HI:
		g_StatusReg.RDST2 = 0;
		UpdateInterrupts();
		_uReturnValue = m_InHi.Hex;
		return;

	case SI_CHANNEL_2_IN_LO:
		g_StatusReg.RDST2 = 0;
		UpdateInterrupts();
		_uReturnValue = m_InLo.Hex;
		return;

		//////////////////////////////////////////////////////////////////////////
		// Channel 3
		//////////////////////////////////////////////////////////////////////////
	case SI_CHANNEL_3_OUT:
		_uReturnValue = m_Out.Hex;
		return;

	case SI_CHANNEL_3_IN_HI:
		g_StatusReg.RDST3 = 0;
		UpdateInterrupts();
		_uReturnValue = m_InHi.Hex;
		return;

	case SI_CHANNEL_3_IN_LO:
		g_StatusReg.RDST3 = 0;
		UpdateInterrupts();
		_uReturnValue = m_InLo.Hex;
		return;

	case SI_POLL:				_uReturnValue = g_Poll.Hex; return;
	case SI_COM_CSR:			_uReturnValue = g_ComCSR.Hex; return;
	case SI_STATUS_REG:			_uReturnValue = g_StatusReg.Hex; return;

	case SI_EXI_CLOCK_COUNT:	_uReturnValue = g_EXIClockCount.Hex; return;

	default:
		// (shuffle2) FIX!
		//LOG(SERIALINTERFACE, "(r32-unk): 0x%08x", _iAddress);
		_dbg_assert_(SERIALINTERFACE, 0);
		break;
	}

	// error
	_uReturnValue = 0xdeadbeef;
}

void CSIChannel::Write32(const u32 _iValue, const u32 iAddr)
{
	// registers
	switch (iAddr)
	{
	case SI_CHANNEL_0_OUT:		m_Out.Hex = _iValue; break;
	case SI_CHANNEL_0_IN_HI:	m_InHi.Hex = _iValue; break;
	case SI_CHANNEL_0_IN_LO:	m_InLo.Hex = _iValue; break;
	case SI_CHANNEL_1_OUT:		m_Out.Hex = _iValue; break;
	case SI_CHANNEL_1_IN_HI:	m_InHi.Hex = _iValue; break;
	case SI_CHANNEL_1_IN_LO:	m_InLo.Hex = _iValue; break;
	case SI_CHANNEL_2_OUT:		m_Out.Hex = _iValue; break;
	case SI_CHANNEL_2_IN_HI:	m_InHi.Hex = _iValue; break;
	case SI_CHANNEL_2_IN_LO:	m_InLo.Hex = _iValue; break;
	case SI_CHANNEL_3_OUT:		m_Out.Hex = _iValue; break;
	case SI_CHANNEL_3_IN_HI:	m_InHi.Hex = _iValue; break;
	case SI_CHANNEL_3_IN_LO:	m_InLo.Hex = _iValue; break;

	case SI_POLL:
		g_Poll.Hex = _iValue;
		break;

	case SI_COM_CSR:
		{
			USIComCSR tmpComCSR(_iValue);

			g_ComCSR.CHANNEL	= tmpComCSR.CHANNEL;
			g_ComCSR.INLNGTH	= tmpComCSR.INLNGTH;
			g_ComCSR.OUTLNGTH	= tmpComCSR.OUTLNGTH;
			g_ComCSR.RDSTINTMSK = tmpComCSR.RDSTINTMSK;
			g_ComCSR.TCINTMSK	= tmpComCSR.TCINTMSK;

			g_ComCSR.COMERR		= 0;

			if (tmpComCSR.RDSTINT)	g_ComCSR.RDSTINT = 0;
			if (tmpComCSR.TCINT)	g_ComCSR.TCINT = 0;

			// be careful: RunSIBuffer after updating the INT flags
			if (tmpComCSR.TSTART)	RunSIBuffer();
			UpdateInterrupts();
		}
		break;

	case SI_STATUS_REG:
		{
			USIStatusReg tmpStatus(_iValue);

			// just update the writable bits
			g_StatusReg.NOREP0	= tmpStatus.NOREP0 ? 1 : 0;
			g_StatusReg.COLL0	= tmpStatus.COLL0 ? 1 : 0;
			g_StatusReg.OVRUN0	= tmpStatus.OVRUN0 ? 1 : 0;
			g_StatusReg.UNRUN0	= tmpStatus.UNRUN0 ? 1 : 0;

			g_StatusReg.NOREP1	= tmpStatus.NOREP1 ? 1 : 0;
			g_StatusReg.COLL1	= tmpStatus.COLL1 ? 1 : 0;
			g_StatusReg.OVRUN1	= tmpStatus.OVRUN1 ? 1 : 0;
			g_StatusReg.UNRUN1	= tmpStatus.UNRUN1 ? 1 : 0;

			g_StatusReg.NOREP2	= tmpStatus.NOREP2 ? 1 : 0;
			g_StatusReg.COLL2	= tmpStatus.COLL2 ? 1 : 0;
			g_StatusReg.OVRUN2	= tmpStatus.OVRUN2 ? 1 : 0;
			g_StatusReg.UNRUN2	= tmpStatus.UNRUN2 ? 1 : 0;

			g_StatusReg.NOREP3	= tmpStatus.NOREP3 ? 1 : 0;
			g_StatusReg.COLL3	= tmpStatus.COLL3 ? 1 : 0;
			g_StatusReg.OVRUN3	= tmpStatus.OVRUN3 ? 1 : 0;
			g_StatusReg.UNRUN3	= tmpStatus.UNRUN3 ? 1 : 0;

			// send command to devices
			if (tmpStatus.WR)
			{
				g_StatusReg.WR = 0;
				m_pDevice->SendCommand(m_Out.Hex);
				m_pDevice->SendCommand(m_Out.Hex);
				m_pDevice->SendCommand(m_Out.Hex);
				m_pDevice->SendCommand(m_Out.Hex);

				g_StatusReg.WRST0 = 0;
				g_StatusReg.WRST1 = 0;
				g_StatusReg.WRST2 = 0;
				g_StatusReg.WRST3 = 0;
			}
		}
		break;

	case SI_EXI_CLOCK_COUNT:
		g_EXIClockCount.Hex = _iValue;
		break;

	case 0x80:
		LOG(SERIALINTERFACE, "WII something at 0xCD006480");
		break;

	default:
		_dbg_assert_(SERIALINTERFACE,0);
		break;
	}
}
