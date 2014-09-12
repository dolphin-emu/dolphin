// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "Core/ConfigManager.h"
#include "Core/CoreTiming.h"
#include "Core/Movie.h"
#include "Core/HW/EXI.h"
#include "Core/HW/EXI_Channel.h"
#include "Core/HW/EXI_Device.h"
#include "Core/HW/MMIO.h"
#include "Core/HW/ProcessorInterface.h"
#include "Core/HW/SystemTimers.h"
#include "Core/PowerPC/PowerPC.h"

#define EXI_READ      0
#define EXI_WRITE     1
#define EXI_READWRITE 2

CEXIChannel::CEXIChannel(u32 ChannelId) :
	m_DMAMemoryAddress(0),
	m_DMALength(0),
	m_ImmData(0),
	m_ChannelId(ChannelId)
{
	struct
	{
		const char* complete;
		const char* interrupts;
	} const eventNames[] = {
		{ "EXIChannel0_TransferComplete", "EXIChannel0_UpdateInterrupts" },
		{ "EXIChannel1_TransferComplete", "EXIChannel1_UpdateInterrupts" },
		{ "EXIChannel2_TransferComplete", "EXIChannel2_UpdateInterrupts" },
	};

	m_Control.Hex = 0;
	m_Status.Hex = 0;

	if (m_ChannelId == 0 || m_ChannelId == 1)
		m_Status.EXTINT = 1;
	if (m_ChannelId == 1)
		m_Status.CHIP_SELECT = 1;

	for (auto& device : m_pDevices)
		device.reset(EXIDevice_Create(EXIDEVICE_NONE, m_ChannelId));

	updateInterrupts = CoreTiming::RegisterEvent(eventNames[m_ChannelId].interrupts, UpdateInterrupts);
	transferComplete = CoreTiming::RegisterEvent(eventNames[m_ChannelId].complete, TransferComplete);
}

CEXIChannel::~CEXIChannel()
{
	CoreTiming::RemoveEvent(updateInterrupts);
	CoreTiming::RemoveEvent(transferComplete);
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

			CoreTiming::ScheduleEvent_Threadsafe_Immediate(updateInterrupts, 0);
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

				u32 transferSize = 0;
				if (m_Control.DMA == 0)
				{
					transferSize = m_Control.TLEN + 1;
					// immediate data
					switch (m_Control.RW)
					{
						case EXI_READ: m_ImmData = pDevice->ImmRead(transferSize); break;
						case EXI_WRITE: pDevice->ImmWrite(m_ImmData, transferSize); break;
						case EXI_READWRITE: pDevice->ImmReadWrite(m_ImmData, transferSize); break;
						default: _dbg_assert_msg_(EXPANSIONINTERFACE,0,"EXI Imm: Unknown transfer type %i", m_Control.RW);
					}
				}
				else
				{
					transferSize = m_DMALength;
					// DMA
					switch (m_Control.RW)
					{
						case EXI_READ: pDevice->DMARead (m_DMAMemoryAddress, transferSize); break;
						case EXI_WRITE: pDevice->DMAWrite(m_DMAMemoryAddress, transferSize); break;
						default: _dbg_assert_msg_(EXPANSIONINTERFACE,0,"EXI DMA: Unknown transfer type %i", m_Control.RW);
					}
				}

				u64 transferTime = 8ULL * transferSize * SystemTimers::GetTicksPerSecond() / GetClockRate();
				CoreTiming::ScheduleEvent_Threadsafe(static_cast<int>(transferTime), transferComplete, m_ChannelId);
				CoreTiming::ForceExceptionCheck(static_cast<int>(transferTime));
			}
		})
	);

	mmio->Register(base + EXI_IMMDATA,
		MMIO::DirectRead<u32>(&m_ImmData),
		MMIO::DirectWrite<u32>(&m_ImmData)
	);
}

void CEXIChannel::TransferComplete(u64 userData, int cyclesLate)
{
	u32 channelID = static_cast<u32>(userData);
	CEXIChannel* channel = ExpansionInterface::FindChannel(channelID);

	if (channel->m_Control.DMA)
	{
		channel->m_Status.TCINT = 1;
		ExpansionInterface::UpdateInterrupts();
	}

	channel->m_Control.TSTART = 0;
}

void CEXIChannel::RemoveDevices()
{
	for (auto& device : m_pDevices)
		device.reset(nullptr);
}

u32 CEXIChannel::GetClockRate() const
{
	return (1 << m_Status.CLK) * 1000000;
}

void CEXIChannel::AddDevice(const TEXIDevices device_type, const int device_num)
{
	IEXIDevice* pNewDevice = EXIDevice_Create(device_type, m_ChannelId);
	AddDevice(pNewDevice, device_num);
}

void CEXIChannel::AddDevice(IEXIDevice* pDevice, const int device_num, bool notifyPresenceChanged)
{
	_dbg_assert_(EXPANSIONINTERFACE, device_num < NUM_DEVICES);

	// replace it with the new one
	m_pDevices[device_num].reset(pDevice);

	if (notifyPresenceChanged)
	{
		// This means "device presence changed", software has to check
		// m_Status.EXT to see if it is now present or not
		if (m_ChannelId != 2)
		{
			m_Status.EXTINT = 1;
			CoreTiming::ScheduleEvent_Threadsafe_Immediate(updateInterrupts, 0);
		}
	}
}

void CEXIChannel::UpdateInterrupts(u64 userdata, int cyclesLate)
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

	for (int d = 0; d < NUM_DEVICES; ++d)
	{
		IEXIDevice* pDevice = m_pDevices[d].get();
		TEXIDevices type = pDevice->m_deviceType;
		p.Do(type);
		IEXIDevice* pSaveDevice = (type == pDevice->m_deviceType) ? pDevice : EXIDevice_Create(type, m_ChannelId);
		pSaveDevice->DoState(p);
		if (pSaveDevice != pDevice)
		{
			// if we had to create a temporary device, discard it if we're not loading.
			// also, if no movie is active, we'll assume the user wants to keep their current devices
			// instead of the ones they had when the savestate was created,
			// unless the device is NONE (since ChangeDevice sets that temporarily).
			if (p.GetMode() != PointerWrap::MODE_READ)
			{
				delete pSaveDevice;
			}
			else
			{
				AddDevice(pSaveDevice, d, false);
			}
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
