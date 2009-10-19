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

#include "EXI_Channel.h"
#include "EXI_Device.h"
#include "EXI.h"

#define EXI_READ		0
#define EXI_WRITE		1
#define EXI_READWRITE	2


#include "ProcessorInterface.h"
#include "../PowerPC/PowerPC.h"

CEXIChannel::CEXIChannel(u32 ChannelId) :
	m_DMAMemoryAddress(0),
	m_DMALength(0),
	m_ImmData(0),
	m_ChannelId(ChannelId)
{
	m_Control.Hex = 0;
	m_Status.Hex = 0;

	if (m_ChannelId == 0 || m_ChannelId == 1)
		m_Status.EXTINT = 1;
	if (m_ChannelId == 1)
		m_Status.CHIP_SELECT = 1;

	for (int i = 0; i < NUM_DEVICES; i++)
		m_pDevices[i] = EXIDevice_Create(EXIDEVICE_NONE);
}

CEXIChannel::~CEXIChannel()
{
	RemoveDevices();
}

void CEXIChannel::RemoveDevices()
{
	for (int i = 0; i < NUM_DEVICES; i++)
	{
		delete m_pDevices[i];
		m_pDevices[i] = NULL;
	}
}

void CEXIChannel::AddDevice(const TEXIDevices _device, const unsigned int _iSlot)
{
	_dbg_assert_(EXPANSIONINTERFACE, _iSlot < NUM_DEVICES);

	// delete the old device
	if (m_pDevices[_iSlot] != NULL)
	{
		delete m_pDevices[_iSlot];
		m_pDevices[_iSlot] = NULL;
	}

	// create the new one
	m_pDevices[_iSlot] = EXIDevice_Create(_device);

	// This means "device presence changed", software has to check
	// m_Status.EXT to see if it is now present or not
	if (m_ChannelId != 2)
	{
		m_Status.EXTINT = 1;
		UpdateInterrupts();
	}
}

void CEXIChannel::UpdateInterrupts()
{
	ExpansionInterface::UpdateInterrupts();
}

bool CEXIChannel::IsCausingInterrupt()
{
	if (m_ChannelId != 2 && GetDevice(1)->IsInterruptSet())
		m_Status.EXIINT = 1; // Always check memcard slots
	else if (GetDevice(m_Status.CHIP_SELECT))
		if (GetDevice(m_Status.CHIP_SELECT)->IsInterruptSet())
			m_Status.EXIINT = 1;

	if ((m_Status.EXIINT	& m_Status.EXIINTMASK) ||
		(m_Status.TCINT		& m_Status.TCINTMASK) ||
		(m_Status.EXTINT	& m_Status.EXTINTMASK))
	{
		return true;
	}
	else
	{
		return false;
	}
}

IEXIDevice* CEXIChannel::GetDevice(u8 _CHIP_SELECT)
{
	switch(_CHIP_SELECT)
	{
	case 1: return m_pDevices[0];
	case 2: return m_pDevices[1];
	case 4: return m_pDevices[2];
	}
	return NULL;
}

void CEXIChannel::Update()
{
	// start the transfer
	for (int i = 0; i < NUM_DEVICES; i++)
	{
		m_pDevices[i]->Update();
	}
}

void CEXIChannel::Read32(u32& _uReturnValue, const u32 _iRegister)
{
	switch (_iRegister)
	{
	case EXI_STATUS:
		{
			// check if external device is present
			// pretty sure it is memcard only, not entirely sure
			if (m_ChannelId == 2)
				m_Status.EXT = 0;
			else
				m_Status.EXT = GetDevice(1)->IsPresent() ? 1 : 0;

			_uReturnValue = m_Status.Hex;
			break;
		}

	case EXI_DMAADDR:
		_uReturnValue = m_DMAMemoryAddress;
		break;

	case EXI_DMALENGTH:
		_uReturnValue = m_DMALength;
		break;

	case EXI_DMACONTROL:
		_uReturnValue = m_Control.Hex;
		break;

	case EXI_IMMDATA:
		_uReturnValue = m_ImmData;
		break;

	default:
		_dbg_assert_(EXPANSIONINTERFACE, 0);
		_uReturnValue = 0xDEADBEEF;
	}

	DEBUG_LOG(EXPANSIONINTERFACE, "(r32) 0x%08x channel: %i  reg: %s",
		_uReturnValue, m_ChannelId, Debug_GetRegisterName(_iRegister));
}

void CEXIChannel::Write32(const u32 _iValue, const u32 _iRegister)
{
	DEBUG_LOG(EXPANSIONINTERFACE, "(w32) 0x%08x channel: %i  reg: %s",
		_iValue, m_ChannelId, Debug_GetRegisterName(_iRegister));

	switch (_iRegister)
	{
	case EXI_STATUS:
		{
			UEXI_STATUS newStatus(_iValue);

			m_Status.EXIINTMASK		= newStatus.EXIINTMASK;
			if (newStatus.EXIINT)	m_Status.EXIINT = 0;

			m_Status.TCINTMASK		= newStatus.TCINTMASK;
			if (newStatus.TCINT)	m_Status.TCINT = 0;

			m_Status.CLK			= newStatus.CLK;

			if (m_ChannelId == 0 || m_ChannelId == 1)
			{
				m_Status.EXTINTMASK	= newStatus.EXTINTMASK;
				if (newStatus.EXTINT)	m_Status.EXTINT = 0;
			}

			if (m_ChannelId == 0)
				m_Status.ROMDIS		= newStatus.ROMDIS;

			IEXIDevice* pDevice = GetDevice(m_Status.CHIP_SELECT ^ newStatus.CHIP_SELECT);
			m_Status.CHIP_SELECT	= newStatus.CHIP_SELECT;
			if (pDevice != NULL)
				pDevice->SetCS(m_Status.CHIP_SELECT);

			UpdateInterrupts();
		}
		break;

	case EXI_DMAADDR:
		INFO_LOG(EXPANSIONINTERFACE, "Wrote DMAAddr, chan %i", m_ChannelId);
		m_DMAMemoryAddress = _iValue;
		break;

	case EXI_DMALENGTH:
		INFO_LOG(EXPANSIONINTERFACE, "Wrote DMALength, chan %i", m_ChannelId);
		m_DMALength = _iValue;
		break;

	case EXI_DMACONTROL:
		INFO_LOG(EXPANSIONINTERFACE, "Wrote DMAControl, chan %i", m_ChannelId);
		m_Control.Hex = _iValue;

		if (m_Control.TSTART)
		{
			IEXIDevice* pDevice = GetDevice(m_Status.CHIP_SELECT);
			if (pDevice == NULL)
				return;

			if (m_Control.DMA == 0)
			{
				// immediate data
				switch (m_Control.RW)
				{
					case EXI_READ: m_ImmData = pDevice->ImmRead(m_Control.TLEN + 1); break;
					case EXI_WRITE: pDevice->ImmWrite(m_ImmData, m_Control.TLEN + 1); break;
					case EXI_READWRITE:
/*						Only used by USBGecko?
						pDevice->ImmWrite(m_ImmData, m_Control.TLEN + 1);
						m_ImmData = pDevice->ImmRead(m_Control.TLEN + 1); */
						break;

					default: _dbg_assert_msg_(EXPANSIONINTERFACE,0,"EXI Imm: Unknown transfer type %i", m_Control.RW);
				}
				m_Control.TSTART = 0;
			}
			else
			{
				// DMA
				switch (m_Control.RW)
				{
					case EXI_READ: pDevice->DMARead (m_DMAMemoryAddress, m_DMALength); break;
					case EXI_WRITE: pDevice->DMAWrite(m_DMAMemoryAddress, m_DMALength); break;
					default: _dbg_assert_msg_(EXPANSIONINTERFACE,0,"EXI DMA: Unknown transfer type %i", m_Control.RW);
				}
				m_Control.TSTART = 0;
			}

			if(!m_Control.TSTART) // completed !
			{
				m_Status.TCINT = 1;
				UpdateInterrupts();
			}
		}
		break;

	case EXI_IMMDATA:
		INFO_LOG(EXPANSIONINTERFACE, "Wrote IMMData, chan %i", m_ChannelId);
		m_ImmData = _iValue;
		break;
	}
}
