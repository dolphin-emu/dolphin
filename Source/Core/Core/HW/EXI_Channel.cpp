// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <memory>

#include "Common/Assert.h"
#include "Common/ChunkFile.h"
#include "Common/CommonTypes.h"
#include "Core/HW/EXI.h"
#include "Core/HW/EXI_Channel.h"
#include "Core/HW/EXI_Device.h"
#include "Core/HW/MMIO.h"

enum
{
	EXI_READ,
	EXI_WRITE,
	EXI_READWRITE
};

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

	for (auto& device : m_pDevices)
		device = EXIDevice_Create(EXIDEVICE_NONE, m_ChannelId);
}

CEXIChannel::~CEXIChannel()
{
	RemoveDevices();
}

void CEXIChannel::RegisterMMIO(MMIO::Mapping* mmio, u32 base)
{
	// Warning: the base is not aligned on a page boundary here. We can't use |
	// to select a register address, instead we need to use +.

	mmio->Register(base + EXI_STATUS,
		MMIO::ComplexRead<u32>([this](u32) {
			// check if external device is present
			// pretty sure it is memcard only, not entirely sure
			if (m_ChannelId == 2)
			{
				m_Status.EXT = 0;
			}
			else
			{
				m_Status.EXT = GetDevice(1)->IsPresent() ? 1 : 0;
			}

			return m_Status.Hex;
		}),
		MMIO::ComplexWrite<u32>([this](u32, u32 val) {
			UEXI_STATUS newStatus(val);

			m_Status.EXIINTMASK = newStatus.EXIINTMASK;
			if (newStatus.EXIINT)
				m_Status.EXIINT = 0;

			m_Status.TCINTMASK = newStatus.TCINTMASK;
			if (newStatus.TCINT)
				m_Status.TCINT = 0;

			m_Status.CLK = newStatus.CLK;

			if (m_ChannelId == 0 || m_ChannelId == 1)
			{
				m_Status.EXTINTMASK = newStatus.EXTINTMASK;

				if (newStatus.EXTINT)
					m_Status.EXTINT = 0;
			}

			if (m_ChannelId == 0)
				m_Status.ROMDIS = newStatus.ROMDIS;

			IEXIDevice* pDevice = GetDevice(m_Status.CHIP_SELECT ^ newStatus.CHIP_SELECT);
			m_Status.CHIP_SELECT = newStatus.CHIP_SELECT;
			if (pDevice != nullptr)
				pDevice->SetCS(m_Status.CHIP_SELECT);

			ExpansionInterface::UpdateInterrupts();
		})
	);

	mmio->Register(base + EXI_DMAADDR,
		MMIO::DirectRead<u32>(&m_DMAMemoryAddress),
		MMIO::DirectWrite<u32>(&m_DMAMemoryAddress)
	);
	mmio->Register(base + EXI_DMALENGTH,
		MMIO::DirectRead<u32>(&m_DMALength),
		MMIO::DirectWrite<u32>(&m_DMALength)
	);
	mmio->Register(base + EXI_DMACONTROL,
		MMIO::DirectRead<u32>(&m_Control.Hex),
		MMIO::ComplexWrite<u32>([this](u32, u32 val) {
			m_Control.Hex = val;

			if (m_Control.TSTART)
			{
				IEXIDevice* pDevice = GetDevice(m_Status.CHIP_SELECT);
				if (pDevice == nullptr)
					return;

				if (m_Control.DMA == 0)
				{
					// immediate data
					switch (m_Control.RW)
					{
						case EXI_READ: m_ImmData = pDevice->ImmRead(m_Control.TLEN + 1); break;
						case EXI_WRITE: pDevice->ImmWrite(m_ImmData, m_Control.TLEN + 1); break;
						case EXI_READWRITE: pDevice->ImmReadWrite(m_ImmData, m_Control.TLEN + 1); break;
						default: _dbg_assert_msg_(EXPANSIONINTERFACE,0,"EXI Imm: Unknown transfer type %i", m_Control.RW);
					}
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
				}

				m_Control.TSTART = 0;

				// Check if device needs specific timing, otherwise just complete transfer immediately
				if (!pDevice->UseDelayedTransferCompletion())
					SendTransferComplete();
			}
		})
	);

	mmio->Register(base + EXI_IMMDATA,
		MMIO::DirectRead<u32>(&m_ImmData),
		MMIO::DirectWrite<u32>(&m_ImmData)
	);
}

void CEXIChannel::SendTransferComplete()
{
	m_Status.TCINT = 1;
	ExpansionInterface::UpdateInterrupts();
}

void CEXIChannel::RemoveDevices()
{
	for (auto& device : m_pDevices)
		device.reset(nullptr);
}

void CEXIChannel::AddDevice(const TEXIDevices device_type, const int device_num)
{
	AddDevice(EXIDevice_Create(device_type, m_ChannelId), device_num);
}

void CEXIChannel::AddDevice(std::unique_ptr<IEXIDevice> device, const int device_num, bool notify_presence_changed)
{
	_dbg_assert_(EXPANSIONINTERFACE, device_num < NUM_DEVICES);

	// Replace it with the new one
	m_pDevices[device_num] = std::move(device);

	if (notify_presence_changed)
	{
		// This means "device presence changed", software has to check
		// m_Status.EXT to see if it is now present or not
		if (m_ChannelId != 2)
		{
			m_Status.EXTINT = 1;
			ExpansionInterface::UpdateInterrupts();
		}
	}
}

bool CEXIChannel::IsCausingInterrupt()
{
	if (m_ChannelId != 2 && GetDevice(1)->IsInterruptSet())
		m_Status.EXIINT = 1; // Always check memcard slots
	else if (GetDevice(m_Status.CHIP_SELECT))
		if (GetDevice(m_Status.CHIP_SELECT)->IsInterruptSet())
			m_Status.EXIINT = 1;

	if ((m_Status.EXIINT & m_Status.EXIINTMASK) ||
		(m_Status.TCINT  & m_Status.TCINTMASK) ||
		(m_Status.EXTINT & m_Status.EXTINTMASK))
	{
		return true;
	}
	else
	{
		return false;
	}
}

IEXIDevice* CEXIChannel::GetDevice(const u8 chip_select)
{
	switch (chip_select)
	{
	case 1: return m_pDevices[0].get();
	case 2: return m_pDevices[1].get();
	case 4: return m_pDevices[2].get();
	}
	return nullptr;
}

void CEXIChannel::DoState(PointerWrap &p)
{
	p.DoPOD(m_Status);
	p.Do(m_DMAMemoryAddress);
	p.Do(m_DMALength);
	p.Do(m_Control);
	p.Do(m_ImmData);

	for (int device_index = 0; device_index < NUM_DEVICES; ++device_index)
	{
		std::unique_ptr<IEXIDevice>& device = m_pDevices[device_index];
		TEXIDevices type = device->m_deviceType;
		p.Do(type);

		if (type == device->m_deviceType)
		{
			device->DoState(p);
		}
		else
		{
			std::unique_ptr<IEXIDevice> save_device = EXIDevice_Create(type, m_ChannelId);
			save_device->DoState(p);
			AddDevice(std::move(save_device), device_index, false);
		}
	}
}

void CEXIChannel::PauseAndLock(bool doLock, bool unpauseOnUnlock)
{
	for (auto& device : m_pDevices)
		device->PauseAndLock(doLock, unpauseOnUnlock);
}

IEXIDevice* CEXIChannel::FindDevice(TEXIDevices device_type, int customIndex)
{
	for (auto& sup : m_pDevices)
	{
		IEXIDevice* device = sup->FindDevice(device_type, customIndex);
		if (device)
			return device;
	}
	return nullptr;
}
