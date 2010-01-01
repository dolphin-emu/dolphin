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



// Include
#include "../Core.h" // Local core functions
#include "../Debugger/Debugger_SymbolMap.h"
#include "../Host.h"
#include "../PluginManager.h"
#include "../HW/WII_IPC.h"
#include "WII_IPC_HLE.h"
#include "WII_IPC_HLE_Device_usb.h"

// The device class
CWII_IPC_HLE_Device_usb_oh1_57e_305::CWII_IPC_HLE_Device_usb_oh1_57e_305(u32 _DeviceID, const std::string& _rDeviceName)
	: IWII_IPC_HLE_Device(_DeviceID, _rDeviceName)
	, m_PINType(0)
	, m_ScanEnable(0)
	, m_EventFilterType(0)
	, m_EventFilterCondition(0)
	, m_HostMaxACLSize(0)
	, m_HostMaxSCOSize(0)
	, m_HostNumACLPackets(0)
	, m_HostNumSCOPackets(0)
	, m_HCIBuffer(NULL)
	, m_HCIPool(0)
	, m_ACLBuffer(NULL)
	, m_ACLPool(0)
	, m_LastCmd(0)
	, m_FreqDividerSync(0)
{
	// Activate only first Wiimote by default
	m_WiiMotes.push_back(CWII_IPC_HLE_WiiMote(this, 0, true));
	m_WiiMotes.push_back(CWII_IPC_HLE_WiiMote(this, 1));
	m_WiiMotes.push_back(CWII_IPC_HLE_WiiMote(this, 2));
	m_WiiMotes.push_back(CWII_IPC_HLE_WiiMote(this, 3));

	// The BCM2045's btaddr:
	m_ControllerBD.b[0] = 0x11;
	m_ControllerBD.b[1] = 0x02;
	m_ControllerBD.b[2] = 0x19;
	m_ControllerBD.b[3] = 0x79;
	m_ControllerBD.b[4] = 0x00;
	m_ControllerBD.b[5] = 0xFF;

	// Class and name are written via HCI
	m_ClassOfDevice[0] = 0x00;
	m_ClassOfDevice[1] = 0x00;
	m_ClassOfDevice[2] = 0x00;

	memset(m_LocalName, 0, HCI_UNIT_NAME_SIZE);
	memset(m_PacketCount, 0, sizeof(m_PacketCount));
	memset(m_FreqDividerMote, 0, sizeof(m_FreqDividerMote));

	Host_SetWiiMoteConnectionState(0);
}

CWII_IPC_HLE_Device_usb_oh1_57e_305::~CWII_IPC_HLE_Device_usb_oh1_57e_305()
{
	m_WiiMotes.clear();
}

void CWII_IPC_HLE_Device_usb_oh1_57e_305::DoState(PointerWrap &p)
{
	p.Do(m_LastCmd);
	p.Do(m_CtrlSetup);
	p.Do(m_ACLSetup);
	p.Do(m_HCIBuffer);
	p.Do(m_HCIPool);
	p.Do(m_ACLBuffer);
	p.Do(m_ACLPool);
	p.Do(m_FreqDividerSync);

	for (int i = 0; i < 4; i++)
	{
		p.Do(m_PacketCount[i]);
		p.Do(m_FreqDividerMote[i]);
	}

	for (unsigned int i = 0; i < m_WiiMotes.size(); i++)
		m_WiiMotes[i].DoState(p);
}

bool CWII_IPC_HLE_Device_usb_oh1_57e_305::RemoteDisconnect(u16 _connectionHandle)
{
	return SendEventDisconnect(_connectionHandle, 0x13);
}

// ===================================================
// Open
bool CWII_IPC_HLE_Device_usb_oh1_57e_305::Open(u32 _CommandAddress, u32 _Mode)
{
	Memory::Write_U32(GetDeviceID(), _CommandAddress+4);
	m_Active = true;
	return true;
}

// ===================================================
// Close
bool CWII_IPC_HLE_Device_usb_oh1_57e_305::Close(u32 _CommandAddress, bool _bForce)
{
	m_PINType = 0;
	m_ScanEnable = 0;
	m_EventFilterType = 0;
	m_EventFilterCondition = 0;
	m_HostMaxACLSize = 0;
	m_HostMaxSCOSize = 0;
	m_HostNumACLPackets = 0;
	m_HostNumSCOPackets = 0;

	m_LastCmd = 0;
	m_FreqDividerSync = 0;
	memset(m_FreqDividerMote, 0, sizeof(m_FreqDividerMote));
	memset(m_PacketCount, 0, sizeof(m_PacketCount));

	m_HCIBuffer.m_address = NULL;
	m_HCIPool.m_number = 0;
	m_ACLBuffer.m_address = NULL;
	m_ACLPool.m_number = 0;

	if (!_bForce)
		Memory::Write_U32(0, _CommandAddress + 4);
	m_Active = false;
	return true;
}


// ===================================================
// IOCtl
bool CWII_IPC_HLE_Device_usb_oh1_57e_305::IOCtl(u32 _CommandAddress)
{
	return IOCtlV(_CommandAddress);	//hack
}


// ===================================================
// IOCtlV
bool CWII_IPC_HLE_Device_usb_oh1_57e_305::IOCtlV(u32 _CommandAddress)
{
/*
	Memory::Write_U8(255, 0x80149950);  // BTM LOG  // 3 logs L2Cap  // 4 logs l2_csm$
	Memory::Write_U8(255, 0x80149949);  // Security Manager
	Memory::Write_U8(255, 0x80149048);  // HID
	Memory::Write_U8(3, 0x80152058);    // low ??   // >= 4 and you will get a lot of event messages of the same type
	Memory::Write_U8(1, 0x80152018);    // WUD
	Memory::Write_U8(1, 0x80151FC8);    // DEBUGPrint
	Memory::Write_U8(1, 0x80151488);    // WPAD_LOG
	Memory::Write_U8(1, 0x801514A8);    // USB_LOG
	Memory::Write_U8(1, 0x801514D8);    // WUD_DEBUGPrint
	Memory::Write_U8(1, 0x80148E09);    // HID LOG
*/

	bool _SendReply = false;

	SIOCtlVBuffer CommandBuffer(_CommandAddress);

	switch(CommandBuffer.Parameter)
	{
	case USB_IOCTL_HCI_COMMAND_MESSAGE:
		{
			// This is the HCI datapath from CPU to Wiimote, the USB stuff is little endian..
			m_CtrlSetup.bRequestType = *(u8*)Memory::GetPointer(CommandBuffer.InBuffer[0].m_Address);
			m_CtrlSetup.bRequest = *(u8*)Memory::GetPointer(CommandBuffer.InBuffer[1].m_Address);
			m_CtrlSetup.wValue = *(u16*)Memory::GetPointer(CommandBuffer.InBuffer[2].m_Address);
			m_CtrlSetup.wIndex = *(u16*)Memory::GetPointer(CommandBuffer.InBuffer[3].m_Address);
			m_CtrlSetup.wLength = *(u16*)Memory::GetPointer(CommandBuffer.InBuffer[4].m_Address);
			m_CtrlSetup.m_PayLoadAddr = CommandBuffer.PayloadBuffer[0].m_Address;
			m_CtrlSetup.m_PayLoadSize = CommandBuffer.PayloadBuffer[0].m_Size;
			m_CtrlSetup.m_Address = CommandBuffer.m_Address;

			// check termination
			_dbg_assert_msg_(WII_IPC_WIIMOTE, *(u8*)Memory::GetPointer(CommandBuffer.InBuffer[5].m_Address) == 0,
													"WIIMOTE: Termination != 0");
			#if defined(_DEBUG) || defined(DEBUGFAST)
			DEBUG_LOG(WII_IPC_WIIMOTE, "USB_IOCTL_CTRLMSG (0x%08x) - execute command", _CommandAddress);
			DEBUG_LOG(WII_IPC_WIIMOTE, "    bRequestType: 0x%x", m_CtrlSetup.bRequestType);
			DEBUG_LOG(WII_IPC_WIIMOTE, "    bRequest: 0x%x", m_CtrlSetup.bRequest);
			DEBUG_LOG(WII_IPC_WIIMOTE, "    wValue: 0x%x", m_CtrlSetup.wValue);
			DEBUG_LOG(WII_IPC_WIIMOTE, "    wIndex: 0x%x", m_CtrlSetup.wIndex);
			DEBUG_LOG(WII_IPC_WIIMOTE, "    wLength: 0x%x", m_CtrlSetup.wLength);
			DEBUG_LOG(WII_IPC_WIIMOTE, "    m_PayLoadAddr:  0x%x", m_CtrlSetup.m_PayLoadAddr);
			DEBUG_LOG(WII_IPC_WIIMOTE, "    m_PayLoadSize:  0x%x", m_CtrlSetup.m_PayLoadSize);
			#endif

			ExecuteHCICommandMessage(m_CtrlSetup);
			// Replies are generated inside
		}
		break;

	case USB_IOCTL_BLKMSG:
		{
			u8 Command = Memory::Read_U8(CommandBuffer.InBuffer[0].m_Address);
			switch (Command)
			{
			case ACL_DATA_BLK_OUT:
				{
					// This is the ACL datapath from CPU to Wiimote
					// Here we only need to record the command address in case we need to delay the reply
					m_ACLSetup = CommandBuffer.m_Address;

					#if defined(_DEBUG) || defined(DEBUGFAST)
					DumpAsync(CommandBuffer.BufferVector, _CommandAddress, CommandBuffer.NumberInBuffer, CommandBuffer.NumberPayloadBuffer);
					#endif

					CtrlBuffer BulkBuffer(_CommandAddress);
					UACLHeader* pACLHeader = (UACLHeader*)Memory::GetPointer(BulkBuffer.m_buffer);

					_dbg_assert_(WII_IPC_WIIMOTE, pACLHeader->BCFlag == 0);
					_dbg_assert_(WII_IPC_WIIMOTE, pACLHeader->PBFlag == 2);

					SendToDevice(pACLHeader->ConnectionHandle, Memory::GetPointer(BulkBuffer.m_buffer + 4), pACLHeader->Size);
					m_PacketCount[pACLHeader->ConnectionHandle & 0xFF]++;

					// If ACLPool is not used, we can send a reply immediately
					// or else we have to delay this reply
					if (m_ACLPool.m_number == 0)
						_SendReply = true;
				}
				break;

			case ACL_DATA_ENDPOINT:
				{
					CtrlBuffer _TempCtrlBuffer(_CommandAddress);
					m_ACLBuffer = _TempCtrlBuffer;
					// Reply should not be sent here but when this buffer is filled

					INFO_LOG(WII_IPC_WIIMOTE, "ACL_DATA_ENDPOINT: 0x%08x ", _CommandAddress);
				}
				break;

			default:
				{
					_dbg_assert_msg_(WII_IPC_WIIMOTE, 0, "Unknown USB_IOCTL_BLKMSG: %x", Command);
				}
				break;
			}
		}
		break;


	case USB_IOCTL_INTRMSG:
		{
			u8 Command = Memory::Read_U8(CommandBuffer.InBuffer[0].m_Address);
			switch (Command)
			{
			case HCI_EVENT_ENDPOINT:
				{
					CtrlBuffer _TempCtrlBuffer(_CommandAddress);
					m_HCIBuffer = _TempCtrlBuffer;
					// Reply should not be sent here but when this buffer is filled

					INFO_LOG(WII_IPC_WIIMOTE, "HCI_EVENT_ENDPOINT: 0x%08x ", _CommandAddress);
				}
				break;

			default:
				{
					_dbg_assert_msg_(WII_IPC_WIIMOTE, 0, "Unknown USB_IOCTL_INTRMSG: %x", Command);		
				}
				break;
			}
		}
		break;

	default:
		{
			_dbg_assert_msg_(WII_IPC_WIIMOTE, 0, "Unknown CWII_IPC_HLE_Device_usb_oh1_57e_305: %x", CommandBuffer.Parameter);

			DEBUG_LOG(WII_IPC_WIIMOTE, "%s - IOCtlV:", GetDeviceName().c_str());
			DEBUG_LOG(WII_IPC_WIIMOTE, "    Parameter: 0x%x", CommandBuffer.Parameter);
			DEBUG_LOG(WII_IPC_WIIMOTE, "    NumberIn: 0x%08x", CommandBuffer.NumberInBuffer);
			DEBUG_LOG(WII_IPC_WIIMOTE, "    NumberOut: 0x%08x", CommandBuffer.NumberPayloadBuffer);
			DEBUG_LOG(WII_IPC_WIIMOTE, "    BufferVector: 0x%08x", CommandBuffer.BufferVector);
			DEBUG_LOG(WII_IPC_WIIMOTE, "    PayloadAddr: 0x%08x", CommandBuffer.PayloadBuffer[0].m_Address);
			DEBUG_LOG(WII_IPC_WIIMOTE, "    PayloadSize: 0x%08x", CommandBuffer.PayloadBuffer[0].m_Size);
			#if defined(_DEBUG) || defined(DEBUGFAST)
			DumpAsync(CommandBuffer.BufferVector, _CommandAddress, CommandBuffer.NumberInBuffer, CommandBuffer.NumberPayloadBuffer);
			#endif
		}
		break;
	}

	// write return value
	Memory::Write_U32(0, _CommandAddress + 0x4);
	return (_SendReply);
}
// ================



// ===================================================
/* Here we handle the USB_IOCTL_BLKMSG Ioctlv */
// ----------------

void CWII_IPC_HLE_Device_usb_oh1_57e_305::SendToDevice(u16 _ConnectionHandle, u8* _pData, u32 _Size)
{
	CWII_IPC_HLE_WiiMote* pWiiMote = AccessWiiMote(_ConnectionHandle);
	if (pWiiMote == NULL)
		return;

	INFO_LOG(WII_IPC_WIIMOTE, "++++++++++++++++++++++++++++++++++++++");
	INFO_LOG(WII_IPC_WIIMOTE, "Execute ACL Command: ConnectionHandle 0x%04x", _ConnectionHandle);
	pWiiMote->ExecuteL2capCmd(_pData, _Size);
}

// ================


// ===================================================
// Here we send ACL pakcets to CPU. They will consist of header + data.
// The header is for example 07 00 41 00 which means size 0x0007 and channel 0x0041.
// ---------------------------------------------------

// AyuanX: Basically, our WII_IPC_HLE is efficient enough to send the packet immediately
// rather than enqueue it to some other memory 
// But...the only exception comes from the Wiimote_Plugin
//
void CWII_IPC_HLE_Device_usb_oh1_57e_305::SendACLPacket(u16 _ConnectionHandle, u8* _pData, u32 _Size)
{
	if(m_ACLBuffer.m_address != NULL)
	{
		INFO_LOG(WII_IPC_WIIMOTE, "Sending ACL Packet: 0x%08x .... (ConnectionHandle 0x%04x)", m_ACLBuffer.m_address, _ConnectionHandle);

		UACLHeader* pHeader = (UACLHeader*)Memory::GetPointer(m_ACLBuffer.m_buffer);
		pHeader->ConnectionHandle = _ConnectionHandle;
		pHeader->BCFlag = 0;
		pHeader->PBFlag = 2;
		pHeader->Size = _Size;

		// Write the packet to the buffer
		memcpy((u8*)pHeader + sizeof(UACLHeader), _pData, _Size);

		// Write the packet size as return value
		Memory::Write_U32(sizeof(UACLHeader) + _Size, m_ACLBuffer.m_address + 0x4);

		// Send a reply to indicate ACL buffer is sent
		WII_IPCInterface::EnqReply(m_ACLBuffer.m_address);

		// Invalidate ACL buffer
		m_ACLBuffer.m_address = NULL;
		m_ACLBuffer.m_buffer = NULL;
	}
	else if ((sizeof(UACLHeader) + _Size) > ACL_MAX_SIZE )
	{
		ERROR_LOG(WII_IPC_HLE, "ACL Packet size is too big!");
		PanicAlert("WII_IPC_HLE: ACL Packet size is too big!");
	}
	else if (m_ACLPool.m_number >= 16)
	{
		ERROR_LOG(WII_IPC_HLE, "ACL Pool is full, something must be wrong!");
		PanicAlert("WII_IPC_HLE: ACL Pool is full, something must be wrong!");
	}
	else
	{
		UACLHeader* pHeader = (UACLHeader*)(m_ACLPool.m_data + m_ACLPool.m_number * ACL_MAX_SIZE);
		pHeader->ConnectionHandle = _ConnectionHandle;
		pHeader->BCFlag = 0;
		pHeader->PBFlag = 2;
		pHeader->Size = _Size;

		memcpy((u8*)pHeader + sizeof(UACLHeader), _pData, _Size);
		m_ACLPool.m_number++;
	}
}

// The normal hardware behavior is like this:
// e.g. if you have 3 packets to send you have to send them one by one in 3 cycles
// and this is the mechanism how our IPC works
// but current implementation of WiiMote_Plugin doesn't comply with this rule
// It acts like sending all the 3 packets in one cycle and idling around in the other two cycles
// that's why we need this contingent ACL pool
//
void CWII_IPC_HLE_Device_usb_oh1_57e_305::PurgeACLPool()
{
	if(m_ACLBuffer.m_address == NULL)
		return;

		INFO_LOG(WII_IPC_WIIMOTE, "Purging ACL Pool: 0x%08x ....", m_ACLBuffer.m_address);

		if(m_ACLPool.m_number > 0)
		{
			m_ACLPool.m_number--;
			// Fill the buffer
			u8* _Address = m_ACLPool.m_data + m_ACLPool.m_number * ACL_MAX_SIZE;
			memcpy(Memory::GetPointer(m_ACLBuffer.m_buffer), _Address, ACL_MAX_SIZE);
			// Write the packet size as return value
			Memory::Write_U32(sizeof(UACLHeader) + ((UACLHeader*)_Address)->Size, m_ACLBuffer.m_address + 0x4);
			// Send a reply to indicate ACL buffer is sent
			WII_IPCInterface::EnqReply(m_ACLBuffer.m_address);
			// Invalidate ACL buffer
			m_ACLBuffer.m_address = NULL;
			m_ACLBuffer.m_buffer = NULL;
		}
}

// ===================================================
/* See IPC_HLE_PERIOD in SystemTimers.cpp for a documentation of this update. */
// ----------------
u32 CWII_IPC_HLE_Device_usb_oh1_57e_305::Update()
{
	// Check if HCI Pool is not purged
	if (m_HCIPool.m_number > 0)
	{
		PurgeHCIPool();
		if (m_HCIPool.m_number == 0)
			WII_IPCInterface::EnqReply(m_CtrlSetup.m_Address);
		return true;
	}

	// Check if last command needs more work
	if (m_HCIBuffer.m_address && m_LastCmd)
	{
		ExecuteHCICommandMessage(m_CtrlSetup);
		return true;
	}

	// Check if ACL Pool is not purged
	if (m_ACLPool.m_number > 0)
	{
		PurgeACLPool();
		if (m_ACLPool.m_number == 0)
			WII_IPCInterface::EnqReply(m_ACLSetup);
		return true;
	}

	// --------------------------------------------------------------------
	/* We wait for ScanEnable to be sent from the game through HCI_CMD_WRITE_SCAN_ENABLE
	   before we initiate the connection.

	   FiRES: TODO find a good solution to do this

	/* Why do we need this? 0 worked with the emulated wiimote in all games I tried. Do we have to
	   wait for wiiuse_init() and wiiuse_find() for a real Wiimote here? I'm testing
	   this new method of not waiting at all if there are no real Wiimotes. Please let me know
	   if it doesn't work. */

	// AyuanX: I don't know the Real Wiimote behavior, so I'll leave it here untouched
	//
	// Initiate ACL connection 
	static int counter = (Core::GetRealWiimote() ? 1000 : 0);
	if (m_HCIBuffer.m_address && (m_ScanEnable & 0x2))
	{
		counter--;
		if (counter < 0)
		{
			for (unsigned int i = 0; i < m_WiiMotes.size(); i++)
				if (m_WiiMotes[i].EventPagingChanged(m_ScanEnable))
				{
					Host_SetWiiMoteConnectionState(1);
					// Create ACL connection
					SendEventRequestConnection(m_WiiMotes[i]);
					return true;
				}
		}
	}

	// AyuanX: Actually we don't need to link channels so early
	// We can wait until HCI command: CommandReadRemoteFeatures is finished
	// Because at this moment, CPU is busy handling HCI commands
	// and have no time to respond ACL requests shortly
	// But ... whatever, either way works
	//
	// Link channels when connected
	if (m_ACLBuffer.m_address && !m_LastCmd && !WII_IPCInterface::GetAddress())
	{
		for (size_t i = 0; i < m_WiiMotes.size(); i++)
		{
			if (m_WiiMotes[i].LinkChannel())
				return true;
		}
	}

	// The Real Wiimote sends report at a fixed frequency of 100Hz
	// So let's make it also 100Hz here
	// Calculation: 15000Hz (IPC_HLE) / 100Hz (WiiMote) = 150
	if (m_ACLBuffer.m_address && !m_LastCmd)
	{
		for (unsigned int i = 0; i < m_WiiMotes.size(); i++)
		{
			m_FreqDividerMote[i]++;
			if (m_WiiMotes[i].IsConnected() == 3 && m_FreqDividerMote[i] >= 150)
			{
				m_FreqDividerMote[i] = 0;
				CPluginManager::GetInstance().GetWiimote(0)->Wiimote_Update(i);
				return true;
			}
		}
	}

	// This event should be sent periodically after ACL connection is accepted
	// or CPU will disconnect WiiMote automatically
	// but don't send too many or it will jam the bus and cost extra CPU time
	// TODO: Figure out the correct frequency to send this thing
	if (m_HCIBuffer.m_address && !WII_IPCInterface::GetAddress() && m_WiiMotes[0].IsConnected())
	{
		m_FreqDividerSync++;
		if (m_FreqDividerSync >= 500)	// Feel free to tweak it
		{
			m_FreqDividerSync = 0;
			SendEventNumberOfCompletedPackets();
			memset(m_PacketCount, 0, sizeof(m_PacketCount));
			return true;
		}
	}

	return false;
}



// Events
// -----------------
// These messages are sent from the Wiimote to the game, for example RequestConnection()
// or ConnectionComplete().
//

// Our WII_IPC_HLE is so efficient that we could fill the buffer immediately
// rather than enqueue it to some other memory and this will do good for StateSave
//
void CWII_IPC_HLE_Device_usb_oh1_57e_305::AddEventToQueue(const SQueuedEvent& _event)
{
	if (m_HCIBuffer.m_address != NULL)
	{
		INFO_LOG(WII_IPC_WIIMOTE, "Sending HCI Packet to Address: 0x%08x ....", m_HCIBuffer.m_address);

		memcpy(Memory::GetPointer(m_HCIBuffer.m_buffer), _event.m_buffer, _event.m_size);

		// Calculate buffer size
		Memory::Write_U32((u32)_event.m_size, m_HCIBuffer.m_address + 0x4);

		// Send a reply to indicate HCI buffer is filled
		WII_IPCInterface::EnqReply(m_HCIBuffer.m_address);

		// Invalidate HCI buffer
		m_HCIBuffer.m_address = NULL;
		m_HCIBuffer.m_buffer = NULL;
	}
	else if (_event.m_size > HCI_MAX_SIZE)
	{
		ERROR_LOG(WII_IPC_HLE, "HCI Packet size too big!");
		PanicAlert("WII_IPC_HLE: HCI Packet size too big!");
	}
	else if (m_HCIPool.m_number >= 16)
	{
		ERROR_LOG(WII_IPC_HLE, "HCI Pool is full, something must be wrong!");
		PanicAlert("WII_IPC_HLE: HCI Pool is full, something must be wrong!");
	}
	else
	{
		memcpy(m_HCIPool.m_data + m_HCIPool.m_number * HCI_MAX_SIZE, _event.m_buffer, _event.m_size);
		// HCI Packet doesn't contain size info inside, so we have to store it somewhere
		m_HCIPool.m_size[m_HCIPool.m_number] = _event.m_size;
		m_HCIPool.m_number++;
	}
}

// Generally, CPU should send us a valid HCI buffer before issuing any HCI command
// but since we don't know the exact frequency at which IPC should be running
// so when IPC is running too fast that CPU can't catch up
// then CPU(actually it is the usb driver) sometimes throws out a command before sending us a HCI buffer
// So I put this contingent HCI Pool here until we figure out the true reason
//
void CWII_IPC_HLE_Device_usb_oh1_57e_305::PurgeHCIPool()
{
	if(m_HCIBuffer.m_address == NULL)
		return;

		INFO_LOG(WII_IPC_WIIMOTE, "Purging HCI Pool: 0x%08x ....", m_HCIBuffer.m_address);

		if(m_HCIPool.m_number > 0)
		{
			m_HCIPool.m_number--;
			// Fill the buffer
			u8* _Address = m_HCIPool.m_data + m_HCIPool.m_number * 64;
			memcpy(Memory::GetPointer(m_HCIBuffer.m_buffer), _Address, 64);
			// Write the packet size as return value
			Memory::Write_U32(m_HCIPool.m_size[m_HCIPool.m_number], m_HCIBuffer.m_address + 0x4);
			// Send a reply to indicate ACL buffer is sent
			WII_IPCInterface::EnqReply(m_HCIBuffer.m_address);
			// Invalidate ACL buffer
			m_HCIBuffer.m_address = NULL;
			m_HCIBuffer.m_buffer = NULL;
		}
}

bool CWII_IPC_HLE_Device_usb_oh1_57e_305::SendEventInquiryComplete()
{
	SQueuedEvent Event(sizeof(SHCIEventInquiryComplete), 0);

	SHCIEventInquiryComplete* pInquiryComplete = (SHCIEventInquiryComplete*)Event.m_buffer;    
	pInquiryComplete->EventType = HCI_EVENT_INQUIRY_COMPL;
	pInquiryComplete->PayloadLength = sizeof(SHCIEventInquiryComplete) - 2;
	pInquiryComplete->Status = 0x00;

	AddEventToQueue(Event);

	INFO_LOG(WII_IPC_WIIMOTE, "Event: Inquiry complete");

	return true;
}

bool CWII_IPC_HLE_Device_usb_oh1_57e_305::SendEventInquiryResponse()
{    
	if (m_WiiMotes.empty())
		return false;

	_dbg_assert_(WII_IPC_WIIMOTE, sizeof(SHCIEventInquiryResult) - 2 + (m_WiiMotes.size() * sizeof(hci_inquiry_response)) < 256);

	SQueuedEvent Event(static_cast<u32>(sizeof(SHCIEventInquiryResult) + m_WiiMotes.size()*sizeof(hci_inquiry_response)), 0);

	SHCIEventInquiryResult* pInquiryResult = (SHCIEventInquiryResult*)Event.m_buffer;  

	pInquiryResult->EventType = HCI_EVENT_INQUIRY_RESULT;
	pInquiryResult->PayloadLength = (u8)(sizeof(SHCIEventInquiryResult) - 2 + (m_WiiMotes.size() * sizeof(hci_inquiry_response)));
	pInquiryResult->num_responses = (u8)m_WiiMotes.size();

	for (size_t i=0; i<m_WiiMotes.size(); i++)
	{
		if (m_WiiMotes[i].IsConnected())
			continue;

		u8* pBuffer = Event.m_buffer + sizeof(SHCIEventInquiryResult) + i*sizeof(hci_inquiry_response);
		hci_inquiry_response* pResponse = (hci_inquiry_response*)pBuffer;   

		pResponse->bdaddr = m_WiiMotes[i].GetBD();
		pResponse->uclass[0]= m_WiiMotes[i].GetClass()[0];
		pResponse->uclass[1]= m_WiiMotes[i].GetClass()[1];
		pResponse->uclass[2]= m_WiiMotes[i].GetClass()[2];

		pResponse->page_scan_rep_mode = 1;
		pResponse->page_scan_period_mode = 0;
		pResponse->page_scan_mode = 0;
		pResponse->clock_offset = 0x3818;

		INFO_LOG(WII_IPC_WIIMOTE, "Event: Send Fake Inquriy of one controller");
		INFO_LOG(WII_IPC_WIIMOTE, "  bd: %02x:%02x:%02x:%02x:%02x:%02x",
			pResponse->bdaddr.b[0], pResponse->bdaddr.b[1], pResponse->bdaddr.b[2],
			pResponse->bdaddr.b[3], pResponse->bdaddr.b[4], pResponse->bdaddr.b[5]);
	}

	AddEventToQueue(Event);

	return true;
}

bool CWII_IPC_HLE_Device_usb_oh1_57e_305::SendEventConnectionComplete(const bdaddr_t& _bd)
{    
	CWII_IPC_HLE_WiiMote* pWiiMote = AccessWiiMote(_bd);
	if (pWiiMote == NULL)
		return false;

	SQueuedEvent Event(sizeof(SHCIEventConnectionComplete), 0);

	SHCIEventConnectionComplete* pConnectionComplete = (SHCIEventConnectionComplete*)Event.m_buffer;    

	pConnectionComplete->EventType = HCI_EVENT_CON_COMPL;
	pConnectionComplete->PayloadLength = sizeof(SHCIEventConnectionComplete) - 2;
	pConnectionComplete->Status = 0x00;
	pConnectionComplete->Connection_Handle = pWiiMote->GetConnectionHandle();
	pConnectionComplete->bdaddr = _bd;
	pConnectionComplete->LinkType = HCI_LINK_ACL;
	pConnectionComplete->EncryptionEnabled = HCI_ENCRYPTION_MODE_NONE;

	AddEventToQueue(Event);

#if MAX_LOGLEVEL >= DEBUG_LEVEL
	static char s_szLinkType[][128] =
	{
		{ "HCI_LINK_SCO		0x00 - Voice"},
		{ "HCI_LINK_ACL		0x01 - Data"},
		{ "HCI_LINK_eSCO	0x02 - eSCO"},
	};
#endif

	INFO_LOG(WII_IPC_WIIMOTE, "Event: SendEventConnectionComplete");
	INFO_LOG(WII_IPC_WIIMOTE, "  Connection_Handle: 0x%04x", pConnectionComplete->Connection_Handle);
	DEBUG_LOG(WII_IPC_WIIMOTE, "  bd: %02x:%02x:%02x:%02x:%02x:%02x",
		pConnectionComplete->bdaddr.b[0], pConnectionComplete->bdaddr.b[1], pConnectionComplete->bdaddr.b[2],
		pConnectionComplete->bdaddr.b[3], pConnectionComplete->bdaddr.b[4], pConnectionComplete->bdaddr.b[5]);
	DEBUG_LOG(WII_IPC_WIIMOTE, "  LinkType: %s", s_szLinkType[pConnectionComplete->LinkType]);
	DEBUG_LOG(WII_IPC_WIIMOTE, "  EncryptionEnabled: %i", pConnectionComplete->EncryptionEnabled);

	return true;
}

/* This is called from Update() after ScanEnable has been enabled. */
bool CWII_IPC_HLE_Device_usb_oh1_57e_305::SendEventRequestConnection(CWII_IPC_HLE_WiiMote& _rWiiMote)
{
	SQueuedEvent Event(sizeof(SHCIEventRequestConnection), 0);

	SHCIEventRequestConnection* pEventRequestConnection = (SHCIEventRequestConnection*)Event.m_buffer;

	pEventRequestConnection->EventType = HCI_EVENT_CON_REQ;
	pEventRequestConnection->PayloadLength = sizeof(SHCIEventRequestConnection) - 2;
	pEventRequestConnection->bdaddr = _rWiiMote.GetBD();
	pEventRequestConnection->uclass[0] = _rWiiMote.GetClass()[0];
	pEventRequestConnection->uclass[1] = _rWiiMote.GetClass()[1];
	pEventRequestConnection->uclass[2] = _rWiiMote.GetClass()[2];
	pEventRequestConnection->LinkType = HCI_LINK_ACL;

	// Log
#if MAX_LOGLEVEL >= DEBUG_LEVEL
	static char LinkType[][128] =
	{
		{ "HCI_LINK_SCO		0x00 - Voice"},
		{ "HCI_LINK_ACL		0x01 - Data"},
		{ "HCI_LINK_eSCO	0x02 - eSCO"},
	};
#endif

	INFO_LOG(WII_IPC_WIIMOTE, "<<<<<<< Request ACL Connection >>>>>>>");
	INFO_LOG(WII_IPC_WIIMOTE, "Event: SendEventRequestConnection");
	INFO_LOG(WII_IPC_WIIMOTE, "  bd: %02x:%02x:%02x:%02x:%02x:%02x",
		pEventRequestConnection->bdaddr.b[0], pEventRequestConnection->bdaddr.b[1], pEventRequestConnection->bdaddr.b[2],
		pEventRequestConnection->bdaddr.b[3], pEventRequestConnection->bdaddr.b[4], pEventRequestConnection->bdaddr.b[5]);
	DEBUG_LOG(WII_IPC_WIIMOTE, "  COD[0]: 0x%02x", pEventRequestConnection->uclass[0]);
	DEBUG_LOG(WII_IPC_WIIMOTE, "  COD[1]: 0x%02x", pEventRequestConnection->uclass[1]);
	DEBUG_LOG(WII_IPC_WIIMOTE, "  COD[2]: 0x%02x", pEventRequestConnection->uclass[2]);
	//DEBUG_LOG(WII_IPC_WIIMOTE, "  LinkType: %s", LinkType[pEventRequestConnection->LinkType]);

	AddEventToQueue(Event);

	return true;
}

bool CWII_IPC_HLE_Device_usb_oh1_57e_305::SendEventDisconnect(u16 _connectionHandle, u8 _Reason)
{
	CWII_IPC_HLE_WiiMote* pWiiMote = AccessWiiMote(_connectionHandle);
	if (pWiiMote == NULL)
		return false;

	SQueuedEvent Event(sizeof(SHCIEventDisconnectCompleted), _connectionHandle);

	SHCIEventDisconnectCompleted* pDisconnect = (SHCIEventDisconnectCompleted*)Event.m_buffer;
	pDisconnect->EventType = HCI_EVENT_DISCON_COMPL;
	pDisconnect->PayloadLength = sizeof(SHCIEventDisconnectCompleted) - 2;
	pDisconnect->Status = 0;
	pDisconnect->Connection_Handle = _connectionHandle;
	pDisconnect->Reason = _Reason;

	AddEventToQueue(Event);

	// Log
	INFO_LOG(WII_IPC_WIIMOTE, "Event: SendEventDisconnect");
	INFO_LOG(WII_IPC_WIIMOTE, "  Connection_Handle: 0x%04x", pDisconnect->Connection_Handle);
	INFO_LOG(WII_IPC_WIIMOTE, "  Reason: 0x%02x", pDisconnect->Reason);

	return true;
}

bool CWII_IPC_HLE_Device_usb_oh1_57e_305::SendEventAuthenticationCompleted(u16 _connectionHandle)
{
	CWII_IPC_HLE_WiiMote* pWiiMote = AccessWiiMote(_connectionHandle);
	if (pWiiMote == NULL)
		return false;

	SQueuedEvent Event(sizeof(SHCIEventAuthenticationCompleted), _connectionHandle);

	SHCIEventAuthenticationCompleted* pEventAuthenticationCompleted = (SHCIEventAuthenticationCompleted*)Event.m_buffer;
	pEventAuthenticationCompleted->EventType = HCI_EVENT_AUTH_COMPL;
	pEventAuthenticationCompleted->PayloadLength = sizeof(SHCIEventAuthenticationCompleted) - 2;
	pEventAuthenticationCompleted->Status = 0;
	pEventAuthenticationCompleted->Connection_Handle = _connectionHandle;

	// Log
	INFO_LOG(WII_IPC_WIIMOTE, "Event: SendEventAuthenticationCompleted");
	INFO_LOG(WII_IPC_WIIMOTE, "  Connection_Handle: 0x%04x", pEventAuthenticationCompleted->Connection_Handle);

	AddEventToQueue(Event);

	return true;
}

bool CWII_IPC_HLE_Device_usb_oh1_57e_305::SendEventRemoteNameReq(const bdaddr_t& _bd)
{    
	CWII_IPC_HLE_WiiMote* pWiiMote = AccessWiiMote(_bd);
	if (pWiiMote == NULL)
		return false;

	SQueuedEvent Event(sizeof(SHCIEventRemoteNameReq), 0);

	SHCIEventRemoteNameReq* pRemoteNameReq = (SHCIEventRemoteNameReq*)Event.m_buffer;

	pRemoteNameReq->EventType = HCI_EVENT_REMOTE_NAME_REQ_COMPL;
	pRemoteNameReq->PayloadLength = sizeof(SHCIEventRemoteNameReq) - 2;
	pRemoteNameReq->Status = 0x00;
	pRemoteNameReq->bdaddr = _bd;
	strcpy((char*)pRemoteNameReq->RemoteName, pWiiMote->GetName());

	INFO_LOG(WII_IPC_WIIMOTE, "Event: SendEventRemoteNameReq");
	INFO_LOG(WII_IPC_WIIMOTE, "  bd: %02x:%02x:%02x:%02x:%02x:%02x",
		pRemoteNameReq->bdaddr.b[0], pRemoteNameReq->bdaddr.b[1], pRemoteNameReq->bdaddr.b[2],
		pRemoteNameReq->bdaddr.b[3], pRemoteNameReq->bdaddr.b[4], pRemoteNameReq->bdaddr.b[5]);
	DEBUG_LOG(WII_IPC_WIIMOTE, "  RemoteName: %s", pRemoteNameReq->RemoteName);

	AddEventToQueue(Event);

	return true;
}

bool CWII_IPC_HLE_Device_usb_oh1_57e_305::SendEventReadRemoteFeatures(u16 _connectionHandle)
{
	CWII_IPC_HLE_WiiMote* pWiiMote = AccessWiiMote(_connectionHandle);
	if (pWiiMote == NULL)
		return false;

	SQueuedEvent Event(sizeof(SHCIEventReadRemoteFeatures), _connectionHandle);

	SHCIEventReadRemoteFeatures* pReadRemoteFeatures = (SHCIEventReadRemoteFeatures*)Event.m_buffer;

	pReadRemoteFeatures->EventType = HCI_EVENT_READ_REMOTE_FEATURES_COMPL;
	pReadRemoteFeatures->PayloadLength = sizeof(SHCIEventReadRemoteFeatures) - 2;
	pReadRemoteFeatures->Status = 0x00;
	pReadRemoteFeatures->ConnectionHandle = _connectionHandle;
	pReadRemoteFeatures->features[0] = pWiiMote->GetFeatures()[0];
	pReadRemoteFeatures->features[1] = pWiiMote->GetFeatures()[1];
	pReadRemoteFeatures->features[2] = pWiiMote->GetFeatures()[2];
	pReadRemoteFeatures->features[3] = pWiiMote->GetFeatures()[3];
	pReadRemoteFeatures->features[4] = pWiiMote->GetFeatures()[4];
	pReadRemoteFeatures->features[5] = pWiiMote->GetFeatures()[5];
	pReadRemoteFeatures->features[6] = pWiiMote->GetFeatures()[6];
	pReadRemoteFeatures->features[7] = pWiiMote->GetFeatures()[7];

	// Log
	INFO_LOG(WII_IPC_WIIMOTE, "Event: SendEventReadRemoteFeatures");
	DEBUG_LOG(WII_IPC_WIIMOTE, "  Connection_Handle: 0x%04x", pReadRemoteFeatures->ConnectionHandle);
	DEBUG_LOG(WII_IPC_WIIMOTE, "  features: %02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x",   
		pReadRemoteFeatures->features[0], pReadRemoteFeatures->features[1], pReadRemoteFeatures->features[2],
		pReadRemoteFeatures->features[3], pReadRemoteFeatures->features[4], pReadRemoteFeatures->features[5],
		pReadRemoteFeatures->features[6], pReadRemoteFeatures->features[7]);

	AddEventToQueue(Event);

	return true;
}

bool CWII_IPC_HLE_Device_usb_oh1_57e_305::SendEventReadRemoteVerInfo(u16 _connectionHandle)
{
	CWII_IPC_HLE_WiiMote* pWiiMote = AccessWiiMote(_connectionHandle);
	if (pWiiMote == NULL)
		return false;

	SQueuedEvent Event(sizeof(SHCIEventReadRemoteVerInfo), _connectionHandle);

	SHCIEventReadRemoteVerInfo* pReadRemoteVerInfo = (SHCIEventReadRemoteVerInfo*)Event.m_buffer;    
	pReadRemoteVerInfo->EventType = HCI_EVENT_READ_REMOTE_VER_INFO_COMPL;
	pReadRemoteVerInfo->PayloadLength = sizeof(SHCIEventReadRemoteVerInfo) - 2;
	pReadRemoteVerInfo->Status = 0x00;
	pReadRemoteVerInfo->ConnectionHandle = _connectionHandle;
	pReadRemoteVerInfo->lmp_version = pWiiMote->GetLMPVersion();
	pReadRemoteVerInfo->manufacturer = pWiiMote->GetManufactorID();
	pReadRemoteVerInfo->lmp_subversion = pWiiMote->GetLMPSubVersion();

	// Log
	INFO_LOG(WII_IPC_WIIMOTE, "Event: SendEventReadRemoteVerInfo");
	DEBUG_LOG(WII_IPC_WIIMOTE, "  Connection_Handle: 0x%04x", pReadRemoteVerInfo->ConnectionHandle);
	DEBUG_LOG(WII_IPC_WIIMOTE, "  lmp_version: 0x%02x", pReadRemoteVerInfo->lmp_version);
	DEBUG_LOG(WII_IPC_WIIMOTE, "  manufacturer: 0x%04x", pReadRemoteVerInfo->manufacturer);
	DEBUG_LOG(WII_IPC_WIIMOTE, "  lmp_subversion: 0x%04x", pReadRemoteVerInfo->lmp_subversion);

	AddEventToQueue(Event);

	return true;
}

void CWII_IPC_HLE_Device_usb_oh1_57e_305::SendEventCommandComplete(u16 _OpCode, void* _pData, u32 _DataSize)
{
	_dbg_assert_(WII_IPC_WIIMOTE, (sizeof(SHCIEventCommand) - 2 + _DataSize) < 256);

	SQueuedEvent Event(sizeof(SHCIEventCommand) + _DataSize, 0);

	SHCIEventCommand* pHCIEvent = (SHCIEventCommand*)Event.m_buffer;    
	pHCIEvent->EventType = HCI_EVENT_COMMAND_COMPL;          
	pHCIEvent->PayloadLength = (u8)(sizeof(SHCIEventCommand) - 2 + _DataSize);
	pHCIEvent->PacketIndicator = 0x01;     
	pHCIEvent->Opcode = _OpCode;

	// add the payload
	if ((_pData != NULL) && (_DataSize > 0))
	{
		u8* pPayload = Event.m_buffer + sizeof(SHCIEventCommand);
		memcpy(pPayload, _pData, _DataSize);
	}

	INFO_LOG(WII_IPC_WIIMOTE, "Event: Command Complete (Opcode: 0x%04x)", pHCIEvent->Opcode);

	AddEventToQueue(Event);
}

bool CWII_IPC_HLE_Device_usb_oh1_57e_305::SendEventCommandStatus(u16 _Opcode)
{
	// If we haven't sent this event or other events before, we will send it
	// If we have, then skip it
	if (m_LastCmd == NULL)
	{
		// Let's make a mark to show further events are scheduled
		// besides this should also guarantee we won't send this event twice
		// I think 65535 is big enough, so it won't trouble other events who also make use of g_LastCmd
		m_LastCmd = 0xFFFF;

		SQueuedEvent Event(sizeof(SHCIEventStatus), 0);

		SHCIEventStatus* pHCIEvent = (SHCIEventStatus*)Event.m_buffer;
		pHCIEvent->EventType = HCI_EVENT_COMMAND_STATUS;
		pHCIEvent->PayloadLength = sizeof(SHCIEventStatus) - 2;
		pHCIEvent->Status = 0x0;
		pHCIEvent->PacketIndicator = 0x01;
		pHCIEvent->Opcode = _Opcode;

		INFO_LOG(WII_IPC_WIIMOTE, "Event: Command Status (Opcode: 0x%04x)", pHCIEvent->Opcode); 

		AddEventToQueue(Event);

		return true;
	}
	else
	{	
		// If the mark matches, clear it
		// if not, keep it untouched
		if (m_LastCmd==0xFFFF)
			m_LastCmd = NULL;

		return false;
	}
}

bool CWII_IPC_HLE_Device_usb_oh1_57e_305::SendEventRoleChange(bdaddr_t _bd, bool _master)
{    
	CWII_IPC_HLE_WiiMote* pWiiMote = AccessWiiMote(_bd);
	if (pWiiMote == NULL)
		return false;

	SQueuedEvent Event(sizeof(SHCIEventRoleChange), 0);

	SHCIEventRoleChange* pRoleChange = (SHCIEventRoleChange*)Event.m_buffer;    

	pRoleChange->EventType = HCI_EVENT_ROLE_CHANGE;
	pRoleChange->PayloadLength = sizeof(SHCIEventRoleChange) - 2;
	pRoleChange->Status = 0x00;
	pRoleChange->bdaddr = _bd;
	pRoleChange->NewRole = _master ? 0x00 : 0x01;

	AddEventToQueue(Event);

	INFO_LOG(WII_IPC_WIIMOTE, "Event: SendEventRoleChange");
	DEBUG_LOG(WII_IPC_WIIMOTE, "  bd: %02x:%02x:%02x:%02x:%02x:%02x",
		pRoleChange->bdaddr.b[0], pRoleChange->bdaddr.b[1], pRoleChange->bdaddr.b[2],
		pRoleChange->bdaddr.b[3], pRoleChange->bdaddr.b[4], pRoleChange->bdaddr.b[5]);
	DEBUG_LOG(WII_IPC_WIIMOTE, "  NewRole: %i", pRoleChange->NewRole);

	return true;
}

bool CWII_IPC_HLE_Device_usb_oh1_57e_305::SendEventNumberOfCompletedPackets()
{
	int Num = m_WiiMotes.size();
	if (Num == NULL)
		return false;

	SQueuedEvent Event(sizeof(SHCIEventNumberOfCompletedPackets) + Num * 4, 0);

	SHCIEventNumberOfCompletedPackets* pNumberOfCompletedPackets = (SHCIEventNumberOfCompletedPackets*)Event.m_buffer;
	pNumberOfCompletedPackets->EventType = HCI_EVENT_NUM_COMPL_PKTS;
	pNumberOfCompletedPackets->PayloadLength = sizeof(SHCIEventNumberOfCompletedPackets) + Num * 4 - 2;
	pNumberOfCompletedPackets->NumberOfHandles = Num;

	u16 *pData = (u16 *)(Event.m_buffer + sizeof(SHCIEventNumberOfCompletedPackets));
	for (int i = 0; i < Num; i++)
	{
		pData[i] = m_WiiMotes[i].GetConnectionHandle();
		pData[Num + i] = m_PacketCount[i];
	}

	// Log
	INFO_LOG(WII_IPC_WIIMOTE, "Event: SendEventNumberOfCompletedPackets");
	DEBUG_LOG(WII_IPC_WIIMOTE, "  NumberOfConnectionHandle: 0x%04x", pNumberOfCompletedPackets->NumberOfHandles);

	AddEventToQueue(Event);

	return true;
}

bool CWII_IPC_HLE_Device_usb_oh1_57e_305::SendEventModeChange(u16 _connectionHandle, u8 _mode, u16 _value)
{
	CWII_IPC_HLE_WiiMote* pWiiMote = AccessWiiMote(_connectionHandle);
	if (pWiiMote == NULL)
		return false;

	SQueuedEvent Event(sizeof(SHCIEventModeChange), _connectionHandle);

	SHCIEventModeChange* pModeChange = (SHCIEventModeChange*)Event.m_buffer;
	pModeChange->EventType = HCI_EVENT_MODE_CHANGE;
	pModeChange->PayloadLength = sizeof(SHCIEventModeChange) - 2;
	pModeChange->Status = 0;
	pModeChange->Connection_Handle = _connectionHandle;
	pModeChange->CurrentMode = _mode;
	pModeChange->Value = _value;

	// Log
	INFO_LOG(WII_IPC_WIIMOTE, "Event: SendEventModeChange");
	DEBUG_LOG(WII_IPC_WIIMOTE, "  Connection_Handle: 0x%04x", pModeChange->Connection_Handle);
	DEBUG_LOG(WII_IPC_WIIMOTE, "  Current Mode: 0x%02x", pModeChange->CurrentMode = _mode);

	AddEventToQueue(Event);

	return true;
}

bool CWII_IPC_HLE_Device_usb_oh1_57e_305::SendEventLinkKeyNotification(const CWII_IPC_HLE_WiiMote& _rWiiMote)
{
	SQueuedEvent Event(sizeof(SHCIEventLinkKeyNotification), 0);

	SHCIEventLinkKeyNotification* pEventLinkKey = (SHCIEventLinkKeyNotification*)Event.m_buffer;

	pEventLinkKey->EventType = HCI_EVENT_RETURN_LINK_KEYS;
	pEventLinkKey->PayloadLength = sizeof(SHCIEventLinkKeyNotification) - 2;
	pEventLinkKey->numKeys = 1;
	pEventLinkKey->bdaddr = _rWiiMote.GetBD();
	memcpy(pEventLinkKey->LinkKey, _rWiiMote.GetLinkKey(), 16);

	AddEventToQueue(Event);

	INFO_LOG(WII_IPC_WIIMOTE, "Event: SendEventLinkKeyNotification");
	DEBUG_LOG(WII_IPC_WIIMOTE, "  bd: %02x:%02x:%02x:%02x:%02x:%02x",
		pEventLinkKey->bdaddr.b[0], pEventLinkKey->bdaddr.b[1], pEventLinkKey->bdaddr.b[2],
		pEventLinkKey->bdaddr.b[3], pEventLinkKey->bdaddr.b[4], pEventLinkKey->bdaddr.b[5]);

#if MAX_LOGLEVEL >= DEBUG_LEVEL
	LOG_LinkKey(pEventLinkKey->LinkKey);
#endif

	return true;
};

bool CWII_IPC_HLE_Device_usb_oh1_57e_305::SendEventRequestLinkKey(const bdaddr_t& _bd)
{
	SQueuedEvent Event(sizeof(SHCIEventRequestLinkKey), 0);

	SHCIEventRequestLinkKey* pEventRequestLinkKey = (SHCIEventRequestLinkKey*)Event.m_buffer;

	pEventRequestLinkKey->EventType = HCI_EVENT_LINK_KEY_REQ;
	pEventRequestLinkKey->PayloadLength = sizeof(SHCIEventRequestLinkKey) - 2;
	pEventRequestLinkKey->bdaddr = _bd;

	INFO_LOG(WII_IPC_WIIMOTE, "Event: SendEventRequestLinkKey");
	DEBUG_LOG(WII_IPC_WIIMOTE, "  bd: %02x:%02x:%02x:%02x:%02x:%02x",
		pEventRequestLinkKey->bdaddr.b[0], pEventRequestLinkKey->bdaddr.b[1], pEventRequestLinkKey->bdaddr.b[2],
		pEventRequestLinkKey->bdaddr.b[3], pEventRequestLinkKey->bdaddr.b[4], pEventRequestLinkKey->bdaddr.b[5]);

	AddEventToQueue(Event);

	return true;
};

bool CWII_IPC_HLE_Device_usb_oh1_57e_305::SendEventReadClockOffsetComplete(u16 _connectionHandle)
{
	CWII_IPC_HLE_WiiMote* pWiiMote = AccessWiiMote(_connectionHandle);
	if (pWiiMote == NULL)
		return false;

	SQueuedEvent Event(sizeof(SHCIEventReadClockOffsetComplete), _connectionHandle);

	SHCIEventReadClockOffsetComplete* pReadClockOffsetComplete = (SHCIEventReadClockOffsetComplete*)Event.m_buffer;
	pReadClockOffsetComplete->EventType = HCI_EVENT_READ_CLOCK_OFFSET_COMPL;
	pReadClockOffsetComplete->PayloadLength = sizeof(SHCIEventReadClockOffsetComplete) - 2;
	pReadClockOffsetComplete->Status = 0x00;
	pReadClockOffsetComplete->ConnectionHandle = _connectionHandle;
	pReadClockOffsetComplete->ClockOffset = 0x3818;

	// Log
	INFO_LOG(WII_IPC_WIIMOTE, "Event: SendEventReadClockOffsetComplete");
	DEBUG_LOG(WII_IPC_WIIMOTE, "  Connection_Handle: 0x%04x", pReadClockOffsetComplete->ConnectionHandle);
	DEBUG_LOG(WII_IPC_WIIMOTE, "  ClockOffset: 0x%04x", pReadClockOffsetComplete->ClockOffset);

	AddEventToQueue(Event);

	return true;
}

bool CWII_IPC_HLE_Device_usb_oh1_57e_305::SendEventConPacketTypeChange(u16 _connectionHandle, u16 _packetType)
{
	CWII_IPC_HLE_WiiMote* pWiiMote = AccessWiiMote(_connectionHandle);
	if (pWiiMote == NULL)
		return false;

	SQueuedEvent Event(sizeof(SHCIEventConPacketTypeChange), _connectionHandle);

	SHCIEventConPacketTypeChange* pChangeConPacketType = (SHCIEventConPacketTypeChange*)Event.m_buffer;
	pChangeConPacketType->EventType = HCI_EVENT_CON_PKT_TYPE_CHANGED;
	pChangeConPacketType->PayloadLength = sizeof(SHCIEventConPacketTypeChange) - 2;
	pChangeConPacketType->Status = 0x00;
	pChangeConPacketType->ConnectionHandle = _connectionHandle;
	pChangeConPacketType->PacketType = _packetType;

	// Log
	INFO_LOG(WII_IPC_WIIMOTE, "Event: SendEventConPacketTypeChange");
	DEBUG_LOG(WII_IPC_WIIMOTE, "  Connection_Handle: 0x%04x", pChangeConPacketType->ConnectionHandle);
	DEBUG_LOG(WII_IPC_WIIMOTE, "  PacketType: 0x%04x", pChangeConPacketType->PacketType);

	AddEventToQueue(Event);

	return true;
}


// Command dispatcher
// -----------------
// This is called from the USB_IOCTL_HCI_COMMAND_MESSAGE Ioctlv
//
//
void CWII_IPC_HLE_Device_usb_oh1_57e_305::ExecuteHCICommandMessage(const SHCICommandMessage& _rHCICommandMessage)
{
	u8* pInput = Memory::GetPointer(_rHCICommandMessage.m_PayLoadAddr + 3);
	SCommandMessage* pMsg = (SCommandMessage*)Memory::GetPointer(_rHCICommandMessage.m_PayLoadAddr);

	u16 ocf = HCI_OCF(pMsg->Opcode);
	u16 ogf = HCI_OGF(pMsg->Opcode);

	// Only show info if this is a new HCI command
	// or else we are continuing to execute last command
	if(m_LastCmd == NULL)
	{
		INFO_LOG(WII_IPC_WIIMOTE, "**************************************************");
		INFO_LOG(WII_IPC_WIIMOTE, "Execute HCI Command: 0x%04x (ocf: 0x%02x, ogf: 0x%02x)", pMsg->Opcode, ocf, ogf);
	}

	switch(pMsg->Opcode)
	{
		//
		// --- read commands ---
		//
	case HCI_CMD_RESET:
		CommandReset(pInput);
		break;

	case HCI_CMD_READ_BUFFER_SIZE:
		CommandReadBufferSize(pInput);
		break;

	case HCI_CMD_READ_LOCAL_VER:
		CommandReadLocalVer(pInput);
		break;

	case HCI_CMD_READ_BDADDR:
		CommandReadBDAdrr(pInput);
		break;

	case HCI_CMD_READ_LOCAL_FEATURES:
		CommandReadLocalFeatures(pInput);
		break;

	case HCI_CMD_READ_STORED_LINK_KEY:
		CommandReadStoredLinkKey(pInput);
		break;

	case HCI_CMD_WRITE_UNIT_CLASS:
		CommandWriteUnitClass(pInput);
		break;

	case HCI_CMD_WRITE_LOCAL_NAME:
		CommandWriteLocalName(pInput);
		break;

	case HCI_CMD_WRITE_PIN_TYPE:
		CommandWritePinType(pInput);
		break;

	case HCI_CMD_HOST_BUFFER_SIZE:
		CommandHostBufferSize(pInput);
		break;

	case HCI_CMD_WRITE_PAGE_TIMEOUT:
		CommandWritePageTimeOut(pInput);
		break;

	case HCI_CMD_WRITE_SCAN_ENABLE:
		CommandWriteScanEnable(pInput);
		break;

	case HCI_CMD_WRITE_INQUIRY_MODE:
		CommandWriteInquiryMode(pInput);
		break;

	case HCI_CMD_WRITE_PAGE_SCAN_TYPE:
		CommandWritePageScanType(pInput);
		break;

	case HCI_CMD_SET_EVENT_FILTER:
		CommandSetEventFilter(pInput);        
		break;

	case HCI_CMD_INQUIRY:
		CommandInquiry(pInput);
		break;

	case HCI_CMD_WRITE_INQUIRY_SCAN_TYPE:
		CommandWriteInquiryScanType(pInput);        
		break;

		// vendor specific...
	case 0xFC4C:
		CommandVendorSpecific_FC4C(pInput, _rHCICommandMessage.m_PayLoadSize - 3);
		break;

	case 0xFC4F:
		CommandVendorSpecific_FC4F(pInput, _rHCICommandMessage.m_PayLoadSize - 3);
		break;

	case HCI_CMD_INQUIRY_CANCEL:
		CommandInquiryCancel(pInput);        
		break;

	case HCI_CMD_REMOTE_NAME_REQ:
		CommandRemoteNameReq(pInput);
		break;

	case HCI_CMD_CREATE_CON:
		CommandCreateCon(pInput);
		break;

	case HCI_CMD_ACCEPT_CON:
		CommandAcceptCon(pInput);
		break;

	case HCI_CMD_CHANGE_CON_PACKET_TYPE:
		CommandChangeConPacketType(pInput);
		break;

	case HCI_CMD_READ_CLOCK_OFFSET:
		CommandReadClockOffset(pInput);
		break;

	case HCI_CMD_READ_REMOTE_VER_INFO:
		CommandReadRemoteVerInfo(pInput);
		break;

	case HCI_CMD_READ_REMOTE_FEATURES:
		CommandReadRemoteFeatures(pInput);
		break;
		
	case HCI_CMD_WRITE_LINK_POLICY_SETTINGS:
		CommandWriteLinkPolicy(pInput);
		break;

	case HCI_CMD_AUTH_REQ:
		CommandAuthenticationRequested(pInput);
		break;

	case HCI_CMD_SNIFF_MODE:
		CommandSniffMode(pInput);
		break;

	case HCI_CMD_DISCONNECT:
		CommandDisconnect(pInput);
		break;

	case HCI_CMD_WRITE_LINK_SUPERVISION_TIMEOUT:
		CommandWriteLinkSupervisionTimeout(pInput);
		break;

	case HCI_CMD_LINK_KEY_NEG_REP:
		CommandLinkKeyNegRep(pInput);
		break;

	case HCI_CMD_LINK_KEY_REP:
		CommandLinkKeyRep(pInput);
		break;

    case HCI_CMD_DELETE_STORED_LINK_KEY:
        CommandDeleteStoredLinkKey(pInput);
        break;

		//
		// --- default ---
		//
	default:
		{
			// send fake okay msg...
			SendEventCommandComplete(pMsg->Opcode, NULL, 0);

			if (ogf == 0x3f)
			{
				PanicAlert("Vendor specific HCI command");
				ERROR_LOG(WII_IPC_WIIMOTE, "Command: vendor specific: 0x%04X (ocf: 0x%x)", pMsg->Opcode, ocf);
				for (int i=0; i<pMsg->len; i++)
				{
					ERROR_LOG(WII_IPC_WIIMOTE, "  0x02%x", pInput[i]);
				}
			}
			else
			{
				_dbg_assert_msg_(WII_IPC_WIIMOTE, 0, "Unknown USB_IOCTL_CTRLMSG: 0x%04X (ocf: 0x%x  ogf 0x%x)", pMsg->Opcode, ocf, ogf);
			}
		}
		break;
	}

	if ((m_LastCmd == NULL) && (m_HCIPool.m_number == 0))
	{
		// If HCI command is finished and HCI pool is empty, send a reply to command
		WII_IPCInterface::EnqReply(_rHCICommandMessage.m_Address);
	}
}


//
//
// --- command helper
//
//
void CWII_IPC_HLE_Device_usb_oh1_57e_305::CommandInquiry(u8* _Input)
{
	if (SendEventCommandStatus(HCI_CMD_INQUIRY))
		return;

	if (m_LastCmd == NULL)
	{
		SendEventInquiryResponse();
		// Now let's set up a mark
		m_LastCmd = HCI_CMD_INQUIRY;
	}
	else
	{
		SendEventInquiryComplete();
		// Clean up
		m_LastCmd = NULL;
	}

	hci_inquiry_cp* pInquiry = (hci_inquiry_cp*)_Input;

	INFO_LOG(WII_IPC_WIIMOTE, "Command: HCI_CMD_INQUIRY:");
	DEBUG_LOG(WII_IPC_WIIMOTE, "write:");
	DEBUG_LOG(WII_IPC_WIIMOTE, "  LAP[0]: 0x%02x", pInquiry->lap[0]);
	DEBUG_LOG(WII_IPC_WIIMOTE, "  LAP[1]: 0x%02x", pInquiry->lap[1]);
	DEBUG_LOG(WII_IPC_WIIMOTE, "  LAP[2]: 0x%02x", pInquiry->lap[2]);
	DEBUG_LOG(WII_IPC_WIIMOTE, "  inquiry_length: %i (N x 1.28) sec", pInquiry->inquiry_length);
	DEBUG_LOG(WII_IPC_WIIMOTE, "  num_responses: %i (N x 1.28) sec", pInquiry->num_responses); 
}

void CWII_IPC_HLE_Device_usb_oh1_57e_305::CommandInquiryCancel(u8* _Input)
{
	// reply
	hci_inquiry_cancel_rp Reply;
	Reply.status = 0x00;

	INFO_LOG(WII_IPC_WIIMOTE, "Command: HCI_CMD_INQUIRY_CANCEL");

	SendEventCommandComplete(HCI_CMD_INQUIRY_CANCEL, &Reply, sizeof(hci_inquiry_cancel_rp));
}

void CWII_IPC_HLE_Device_usb_oh1_57e_305::CommandCreateCon(u8* _Input)
{
	if(SendEventCommandStatus(HCI_CMD_CREATE_CON))
		return;

	// command parameters
	hci_create_con_cp* pCreateCon = (hci_create_con_cp*)_Input;

	INFO_LOG(WII_IPC_WIIMOTE, "Command: HCI_CMD_CREATE_CON");
	DEBUG_LOG(WII_IPC_WIIMOTE, "Input:");
	DEBUG_LOG(WII_IPC_WIIMOTE, "  bd: %02x:%02x:%02x:%02x:%02x:%02x",
		pCreateCon->bdaddr.b[0], pCreateCon->bdaddr.b[1], pCreateCon->bdaddr.b[2],
		pCreateCon->bdaddr.b[3], pCreateCon->bdaddr.b[4], pCreateCon->bdaddr.b[5]);

	DEBUG_LOG(WII_IPC_WIIMOTE, "  pkt_type: %i", pCreateCon->pkt_type);
	DEBUG_LOG(WII_IPC_WIIMOTE, "  page_scan_rep_mode: %i", pCreateCon->page_scan_rep_mode);
	DEBUG_LOG(WII_IPC_WIIMOTE, "  page_scan_mode: %i", pCreateCon->page_scan_mode);
	DEBUG_LOG(WII_IPC_WIIMOTE, "  clock_offset: %i", pCreateCon->clock_offset);
	DEBUG_LOG(WII_IPC_WIIMOTE, "  accept_role_switch: %i", pCreateCon->accept_role_switch);

	SendEventConnectionComplete(pCreateCon->bdaddr);
}

void CWII_IPC_HLE_Device_usb_oh1_57e_305::CommandDisconnect(u8* _Input)
{
	if(SendEventCommandStatus(HCI_CMD_DISCONNECT))
		return;

	// command parameters
	hci_discon_cp* pDiscon = (hci_discon_cp*)_Input;

	INFO_LOG(WII_IPC_WIIMOTE, "Command: HCI_CMD_DISCONNECT");
	DEBUG_LOG(WII_IPC_WIIMOTE, "  ConnectionHandle: 0x%04x", pDiscon->con_handle);
	DEBUG_LOG(WII_IPC_WIIMOTE, "  Reason: 0x%02x", pDiscon->reason);

	SendEventDisconnect(pDiscon->con_handle, pDiscon->reason);
	
	PanicAlert("Wiimote (%i) has been disconnected by system due to idle time out.\n"
		"Don't panic, this is quite a normal behavior for power saving.\n\n"
		"To reconnect, Click \"Menu -> Tools -> Connect Wiimote\"", (pDiscon->con_handle & 0xFF) + 1);

	CWII_IPC_HLE_WiiMote* pWiimote = AccessWiiMote(pDiscon->con_handle);
	if (pWiimote)
		pWiimote->EventDisconnect();
}

void CWII_IPC_HLE_Device_usb_oh1_57e_305::CommandAcceptCon(u8* _Input)
{
	if(SendEventCommandStatus(HCI_CMD_ACCEPT_CON))
		return;

	// command parameters
	hci_accept_con_cp* pAcceptCon = (hci_accept_con_cp*)_Input;

	// this connection wants to be the master
	if ((m_LastCmd == NULL)&&(pAcceptCon->role == 0))
	{
		SendEventRoleChange(pAcceptCon->bdaddr, true);
		// Now let us set up a mark
		m_LastCmd = HCI_CMD_ACCEPT_CON;
		return;
	}
	else
	{
		SendEventConnectionComplete(pAcceptCon->bdaddr);
		// Clean up
		m_LastCmd = NULL;
	}

#if MAX_LOGLEVEL >= DEBUG_LEVEL
	static char s_szRole[][128] =
	{
		{ "Master (0x00)"},
		{ "Slave (0x01)"},
	};
#endif

	INFO_LOG(WII_IPC_WIIMOTE, "Command: HCI_CMD_ACCEPT_CON");
	DEBUG_LOG(WII_IPC_WIIMOTE, "  bd: %02x:%02x:%02x:%02x:%02x:%02x",
		pAcceptCon->bdaddr.b[0], pAcceptCon->bdaddr.b[1], pAcceptCon->bdaddr.b[2],
		pAcceptCon->bdaddr.b[3], pAcceptCon->bdaddr.b[4], pAcceptCon->bdaddr.b[5]);
	DEBUG_LOG(WII_IPC_WIIMOTE, "  role: %s", s_szRole[pAcceptCon->role]);
}

void CWII_IPC_HLE_Device_usb_oh1_57e_305::CommandLinkKeyRep(u8* _Input)
{
	// command parameters
	hci_link_key_rep_cp* pKeyRep = (hci_link_key_rep_cp*)_Input;

	INFO_LOG(WII_IPC_WIIMOTE, "Command: HCI_CMD_LINK_KEY_REP");
	DEBUG_LOG(WII_IPC_WIIMOTE, "  bd: %02x:%02x:%02x:%02x:%02x:%02x",
		pKeyRep->bdaddr.b[0], pKeyRep->bdaddr.b[1], pKeyRep->bdaddr.b[2],
		pKeyRep->bdaddr.b[3], pKeyRep->bdaddr.b[4], pKeyRep->bdaddr.b[5]);
	LOG_LinkKey(pKeyRep->key);


	hci_link_key_rep_rp Reply;
	Reply.status = 0x00;
	Reply.bdaddr = pKeyRep->bdaddr;

	SendEventCommandComplete(HCI_CMD_LINK_KEY_REP, &Reply, sizeof(hci_link_key_rep_rp));
}

void CWII_IPC_HLE_Device_usb_oh1_57e_305::CommandLinkKeyNegRep(u8* _Input)
{
	// command parameters
	hci_link_key_neg_rep_cp* pKeyNeg = (hci_link_key_neg_rep_cp*)_Input;

	INFO_LOG(WII_IPC_WIIMOTE, "Command: HCI_CMD_LINK_KEY_NEG_REP");
	DEBUG_LOG(WII_IPC_WIIMOTE, "  bd: %02x:%02x:%02x:%02x:%02x:%02x",
		pKeyNeg->bdaddr.b[0], pKeyNeg->bdaddr.b[1], pKeyNeg->bdaddr.b[2],
		pKeyNeg->bdaddr.b[3], pKeyNeg->bdaddr.b[4], pKeyNeg->bdaddr.b[5]);

	hci_link_key_neg_rep_rp Reply;
	Reply.status = 0x00;
	Reply.bdaddr = pKeyNeg->bdaddr;

	SendEventCommandComplete(HCI_CMD_LINK_KEY_NEG_REP, &Reply, sizeof(hci_link_key_neg_rep_rp));
}

void CWII_IPC_HLE_Device_usb_oh1_57e_305::CommandChangeConPacketType(u8* _Input)
{
	if(SendEventCommandStatus(HCI_CMD_CHANGE_CON_PACKET_TYPE))
		return;

	// command parameters
	hci_change_con_pkt_type_cp* pChangePacketType = (hci_change_con_pkt_type_cp*)_Input;

	INFO_LOG(WII_IPC_WIIMOTE, "Command: HCI_CMD_CHANGE_CON_PACKET_TYPE");
	DEBUG_LOG(WII_IPC_WIIMOTE, "  ConnectionHandle: 0x%04x", pChangePacketType->con_handle);
	DEBUG_LOG(WII_IPC_WIIMOTE, "  PacketType: 0x%04x", pChangePacketType->pkt_type);

	SendEventConPacketTypeChange(pChangePacketType->con_handle, pChangePacketType->pkt_type);
}

void CWII_IPC_HLE_Device_usb_oh1_57e_305::CommandAuthenticationRequested(u8* _Input)
{
	if(SendEventCommandStatus(HCI_CMD_AUTH_REQ))
		return;

	// command parameters
	hci_auth_req_cp* pAuthReq = (hci_auth_req_cp*)_Input;

	INFO_LOG(WII_IPC_WIIMOTE, "Command: HCI_CMD_AUTH_REQ");
	DEBUG_LOG(WII_IPC_WIIMOTE, "  ConnectionHandle: 0x%04x", pAuthReq->con_handle);

	SendEventAuthenticationCompleted(pAuthReq->con_handle);
}

void CWII_IPC_HLE_Device_usb_oh1_57e_305::CommandRemoteNameReq(u8* _Input)
{
	if(SendEventCommandStatus(HCI_CMD_REMOTE_NAME_REQ))
		return;

	// command parameters
	hci_remote_name_req_cp* pRemoteNameReq = (hci_remote_name_req_cp*)_Input;

	INFO_LOG(WII_IPC_WIIMOTE, "Command: HCI_CMD_REMOTE_NAME_REQ");
	DEBUG_LOG(WII_IPC_WIIMOTE, "  bd: %02x:%02x:%02x:%02x:%02x:%02x",
		pRemoteNameReq->bdaddr.b[0], pRemoteNameReq->bdaddr.b[1], pRemoteNameReq->bdaddr.b[2],
		pRemoteNameReq->bdaddr.b[3], pRemoteNameReq->bdaddr.b[4], pRemoteNameReq->bdaddr.b[5]);
	DEBUG_LOG(WII_IPC_WIIMOTE, "  page_scan_rep_mode: %i", pRemoteNameReq->page_scan_rep_mode);
	DEBUG_LOG(WII_IPC_WIIMOTE, "  page_scan_mode: %i", pRemoteNameReq->page_scan_mode);
	DEBUG_LOG(WII_IPC_WIIMOTE, "  clock_offset: %i", pRemoteNameReq->clock_offset);

	SendEventRemoteNameReq(pRemoteNameReq->bdaddr);
}

void CWII_IPC_HLE_Device_usb_oh1_57e_305::CommandReadRemoteFeatures(u8* _Input)
{
	if(SendEventCommandStatus(HCI_CMD_READ_REMOTE_FEATURES))
		return;

	// command parameters
	hci_read_remote_features_cp* pReadRemoteFeatures = (hci_read_remote_features_cp*)_Input;

	INFO_LOG(WII_IPC_WIIMOTE, "Command: HCI_CMD_READ_REMOTE_FEATURES");
	DEBUG_LOG(WII_IPC_WIIMOTE, "  ConnectionHandle: 0x%04x", pReadRemoteFeatures->con_handle);

	SendEventReadRemoteFeatures(pReadRemoteFeatures->con_handle);
}

void CWII_IPC_HLE_Device_usb_oh1_57e_305::CommandReadRemoteVerInfo(u8* _Input)
{
	if(SendEventCommandStatus(HCI_CMD_READ_REMOTE_VER_INFO))
		return;

	// command parameters
	hci_read_remote_ver_info_cp* pReadRemoteVerInfo = (hci_read_remote_ver_info_cp*)_Input;

	INFO_LOG(WII_IPC_WIIMOTE, "Command: HCI_CMD_READ_REMOTE_VER_INFO");
	DEBUG_LOG(WII_IPC_WIIMOTE, "  ConnectionHandle: 0x%02x", pReadRemoteVerInfo->con_handle);

	SendEventReadRemoteVerInfo(pReadRemoteVerInfo->con_handle);
}

void CWII_IPC_HLE_Device_usb_oh1_57e_305::CommandReadClockOffset(u8* _Input)
{
	if(SendEventCommandStatus(HCI_CMD_READ_CLOCK_OFFSET))
		return;

	// command parameters
	hci_read_clock_offset_cp* pReadClockOffset = (hci_read_clock_offset_cp*)_Input;

	INFO_LOG(WII_IPC_WIIMOTE, "Command: HCI_CMD_READ_CLOCK_OFFSET");
	DEBUG_LOG(WII_IPC_WIIMOTE, "  ConnectionHandle: 0x%02x", pReadClockOffset->con_handle);

	SendEventReadClockOffsetComplete(pReadClockOffset->con_handle);

	//	CWII_IPC_HLE_WiiMote* pWiiMote = AccessWiiMote(pReadClockOffset->con_handle);
	//	SendEventRequestLinkKey(pWiiMote->GetBD());
}

void CWII_IPC_HLE_Device_usb_oh1_57e_305::CommandSniffMode(u8* _Input)
{
	if(SendEventCommandStatus(HCI_CMD_SNIFF_MODE))
		return;

	// command parameters
	hci_sniff_mode_cp* pSniffMode = (hci_sniff_mode_cp*)_Input;

	INFO_LOG(WII_IPC_WIIMOTE, "Command: HCI_CMD_SNIFF_MODE");
	INFO_LOG(WII_IPC_WIIMOTE, "  ConnectionHandle: 0x%04x", pSniffMode->con_handle);
	DEBUG_LOG(WII_IPC_WIIMOTE, "  max_interval: 0x%04x", pSniffMode->max_interval);
	DEBUG_LOG(WII_IPC_WIIMOTE, "  min_interval: 0x%04x", pSniffMode->min_interval);
	DEBUG_LOG(WII_IPC_WIIMOTE, "  attempt: 0x%04x", pSniffMode->attempt);
	DEBUG_LOG(WII_IPC_WIIMOTE, "  timeout: 0x%04x", pSniffMode->timeout);

	SendEventModeChange(pSniffMode->con_handle, 0x02, pSniffMode->max_interval);  // 0x02 - sniff mode
}

void CWII_IPC_HLE_Device_usb_oh1_57e_305::CommandWriteLinkPolicy(u8* _Input)
{
	// command parameters
	hci_write_link_policy_settings_cp* pLinkPolicy = (hci_write_link_policy_settings_cp*)_Input;

	INFO_LOG(WII_IPC_WIIMOTE, "Command: HCI_CMD_WRITE_LINK_POLICY_SETTINGS");
	DEBUG_LOG(WII_IPC_WIIMOTE, "  ConnectionHandle: 0x%04x", pLinkPolicy->con_handle);
	DEBUG_LOG(WII_IPC_WIIMOTE, "  Policy: 0x%04x", pLinkPolicy->settings);

	hci_write_link_policy_settings_rp Reply;
	Reply.status = 0x00;
	Reply.con_handle = pLinkPolicy->con_handle;

	SendEventCommandComplete(HCI_CMD_WRITE_LINK_POLICY_SETTINGS, &Reply, sizeof(hci_write_link_policy_settings_rp));
}

void CWII_IPC_HLE_Device_usb_oh1_57e_305::CommandReset(u8* _Input)
{
	// reply
	hci_status_rp Reply;
	Reply.status = 0x00;

	INFO_LOG(WII_IPC_WIIMOTE, "Command: HCI_CMD_RESET");

	SendEventCommandComplete(HCI_CMD_RESET, &Reply, sizeof(hci_status_rp));
}

void CWII_IPC_HLE_Device_usb_oh1_57e_305::CommandSetEventFilter(u8* _Input)
{
	// command parameters
	hci_set_event_filter_cp* pSetEventFilter = (hci_set_event_filter_cp*)_Input;
	m_EventFilterType = pSetEventFilter->filter_type;
	m_EventFilterCondition = pSetEventFilter->filter_condition_type;

	// reply
	hci_set_event_filter_rp Reply;
	Reply.status = 0x00;

	INFO_LOG(WII_IPC_WIIMOTE, "Command: HCI_CMD_SET_EVENT_FILTER:");
	DEBUG_LOG(WII_IPC_WIIMOTE, "  filter_type: %i", pSetEventFilter->filter_type);
	DEBUG_LOG(WII_IPC_WIIMOTE, "  filter_condition_type: %i", pSetEventFilter->filter_condition_type);

	SendEventCommandComplete(HCI_CMD_SET_EVENT_FILTER, &Reply, sizeof(hci_set_event_filter_rp));
}

void CWII_IPC_HLE_Device_usb_oh1_57e_305::CommandWritePinType(u8* _Input)
{
	// command parameters
	hci_write_pin_type_cp* pWritePinType = (hci_write_pin_type_cp*)_Input;
	m_PINType =  pWritePinType->pin_type;

	// reply
	hci_write_pin_type_rp Reply;
	Reply.status = 0x00;

	INFO_LOG(WII_IPC_WIIMOTE, "Command: HCI_CMD_WRITE_PIN_TYPE:");
	DEBUG_LOG(WII_IPC_WIIMOTE, "  pin_type: %x", pWritePinType->pin_type);

	SendEventCommandComplete(HCI_CMD_WRITE_PIN_TYPE, &Reply, sizeof(hci_write_pin_type_rp));
}

void CWII_IPC_HLE_Device_usb_oh1_57e_305::CommandReadStoredLinkKey(u8* _Input)
{
	hci_read_stored_link_key_cp* ReadStoredLinkKey = (hci_read_stored_link_key_cp*)_Input;

	// reply
	hci_read_stored_link_key_rp Reply;    
	Reply.status = 0x00;

	Reply.max_num_keys = 255;
	if (ReadStoredLinkKey->read_all)
	{
		Reply.num_keys_read = (u16)m_WiiMotes.size();
	}
	else
	{
		ERROR_LOG(WII_IPC_WIIMOTE, "CommandReadStoredLinkKey");
		PanicAlert("CommandReadStoredLinkKey");
	}

	// generate link key
	// Let us have some fun :P
	if(m_LastCmd < m_WiiMotes.size())
	{
		SendEventLinkKeyNotification(m_WiiMotes[m_LastCmd]);
		m_LastCmd++;
		return;
	}
	else
	{
		SendEventCommandComplete(HCI_CMD_READ_STORED_LINK_KEY, &Reply, sizeof(hci_read_stored_link_key_rp));
		m_LastCmd = NULL;
	}

	// logging
	INFO_LOG(WII_IPC_WIIMOTE, "Command: HCI_CMD_READ_STORED_LINK_KEY:");
	DEBUG_LOG(WII_IPC_WIIMOTE, "input:");
	DEBUG_LOG(WII_IPC_WIIMOTE, "  bd: %02x:%02x:%02x:%02x:%02x:%02x",
		ReadStoredLinkKey->bdaddr.b[0], ReadStoredLinkKey->bdaddr.b[1], ReadStoredLinkKey->bdaddr.b[2],
		ReadStoredLinkKey->bdaddr.b[3], ReadStoredLinkKey->bdaddr.b[4], ReadStoredLinkKey->bdaddr.b[5]);
	DEBUG_LOG(WII_IPC_WIIMOTE, "  read_all: %i", ReadStoredLinkKey->read_all);
	DEBUG_LOG(WII_IPC_WIIMOTE, "return:");
	DEBUG_LOG(WII_IPC_WIIMOTE, "  max_num_keys: %i", Reply.max_num_keys);
	DEBUG_LOG(WII_IPC_WIIMOTE, "  num_keys_read: %i", Reply.num_keys_read);
}

void CWII_IPC_HLE_Device_usb_oh1_57e_305::CommandDeleteStoredLinkKey(u8* _Input)
{
	// command parameters
	hci_delete_stored_link_key_cp* pDeleteStoredLinkKey = (hci_delete_stored_link_key_cp*)_Input;

	INFO_LOG(WII_IPC_WIIMOTE, "Command: HCI_OCF_DELETE_STORED_LINK_KEY");
	DEBUG_LOG(WII_IPC_WIIMOTE, "  bd: %02x:%02x:%02x:%02x:%02x:%02x",
		pDeleteStoredLinkKey->bdaddr.b[0], pDeleteStoredLinkKey->bdaddr.b[1], pDeleteStoredLinkKey->bdaddr.b[2],
		pDeleteStoredLinkKey->bdaddr.b[3], pDeleteStoredLinkKey->bdaddr.b[4], pDeleteStoredLinkKey->bdaddr.b[5]);
	DEBUG_LOG(WII_IPC_WIIMOTE, "  delete_all: 0x%01x", pDeleteStoredLinkKey->delete_all);


	CWII_IPC_HLE_WiiMote* pWiiMote = AccessWiiMote(pDeleteStoredLinkKey->bdaddr);
	if (pWiiMote == NULL)
		return;

	hci_delete_stored_link_key_rp Reply;
	Reply.status = 0x00;
	Reply.num_keys_deleted = 0;

	SendEventCommandComplete(HCI_CMD_DELETE_STORED_LINK_KEY, &Reply, sizeof(hci_delete_stored_link_key_rp));

	ERROR_LOG(WII_IPC_WIIMOTE, "HCI: CommandDeleteStoredLinkKey... Probably the security for linking has failed. Could be a problem with loading the SCONF");
	PanicAlert("HCI: CommandDeleteStoredLinkKey... Probably the security for linking has failed. Could be a problem with loading the SCONF");
}

void CWII_IPC_HLE_Device_usb_oh1_57e_305::CommandWriteLocalName(u8* _Input)
{
	// command parameters
	hci_write_local_name_cp* pWriteLocalName = (hci_write_local_name_cp*)_Input;
	memcpy(m_LocalName, pWriteLocalName->name, HCI_UNIT_NAME_SIZE);

	// reply
	hci_write_local_name_rp Reply;
	Reply.status = 0x00;

	INFO_LOG(WII_IPC_WIIMOTE, "Command: HCI_CMD_WRITE_LOCAL_NAME:");
	DEBUG_LOG(WII_IPC_WIIMOTE, "  local_name: %s", pWriteLocalName->name);

	SendEventCommandComplete(HCI_CMD_WRITE_LOCAL_NAME, &Reply, sizeof(hci_write_local_name_rp));
}

// Here we normally receive the timeout interval.
// But not from homebrew games that use lwbt. Why not?
void CWII_IPC_HLE_Device_usb_oh1_57e_305::CommandWritePageTimeOut(u8* _Input)
{
#if MAX_LOGLEVEL >= DEBUG_LEVEL
	// command parameters
	hci_write_page_timeout_cp* pWritePageTimeOut = (hci_write_page_timeout_cp*)_Input;
#endif

	// reply
	hci_host_buffer_size_rp Reply;
	Reply.status = 0x00;

	INFO_LOG(WII_IPC_WIIMOTE, "Command: HCI_CMD_WRITE_PAGE_TIMEOUT:");
	DEBUG_LOG(WII_IPC_WIIMOTE, "  timeout: %i", pWritePageTimeOut->timeout);

	SendEventCommandComplete(HCI_CMD_WRITE_PAGE_TIMEOUT, &Reply, sizeof(hci_host_buffer_size_rp));
}

/* This will enable ScanEnable so that Update() can start the Wiimote. */
void CWII_IPC_HLE_Device_usb_oh1_57e_305::CommandWriteScanEnable(u8* _Input)
{
	// Command parameters
	hci_write_scan_enable_cp* pWriteScanEnable = (hci_write_scan_enable_cp*)_Input;
	m_ScanEnable = pWriteScanEnable->scan_enable;

	// Reply
	hci_write_scan_enable_rp Reply;
	Reply.status = 0x00;

#if MAX_LOGLEVEL >= DEBUG_LEVEL
	static char Scanning[][128] =
	{
		{ "HCI_NO_SCAN_ENABLE"},
		{ "HCI_INQUIRY_SCAN_ENABLE"},
		{ "HCI_PAGE_SCAN_ENABLE"},
		{ "HCI_INQUIRY_AND_PAGE_SCAN_ENABLE"},
	};
#endif

	INFO_LOG(WII_IPC_WIIMOTE, "Command: HCI_CMD_WRITE_SCAN_ENABLE: (0x%02x)", pWriteScanEnable->scan_enable);
	DEBUG_LOG(WII_IPC_WIIMOTE, "  scan_enable: %s", Scanning[pWriteScanEnable->scan_enable]);

	SendEventCommandComplete(HCI_CMD_WRITE_SCAN_ENABLE, &Reply, sizeof(hci_write_scan_enable_rp));
}

void CWII_IPC_HLE_Device_usb_oh1_57e_305::CommandWriteUnitClass(u8* _Input)
{
	// command parameters
	hci_write_unit_class_cp* pWriteUnitClass = (hci_write_unit_class_cp*)_Input;
	m_ClassOfDevice[0] = pWriteUnitClass->uclass[0];
	m_ClassOfDevice[1] = pWriteUnitClass->uclass[1];
	m_ClassOfDevice[2] = pWriteUnitClass->uclass[2];

	// reply
	hci_write_unit_class_rp Reply;
	Reply.status = 0x00;

	INFO_LOG(WII_IPC_WIIMOTE, "Command: HCI_CMD_WRITE_UNIT_CLASS:");
	DEBUG_LOG(WII_IPC_WIIMOTE, "  COD[0]: 0x%02x", pWriteUnitClass->uclass[0]);
	DEBUG_LOG(WII_IPC_WIIMOTE, "  COD[1]: 0x%02x", pWriteUnitClass->uclass[1]);
	DEBUG_LOG(WII_IPC_WIIMOTE, "  COD[2]: 0x%02x", pWriteUnitClass->uclass[2]);

	SendEventCommandComplete(HCI_CMD_WRITE_UNIT_CLASS, &Reply, sizeof(hci_write_unit_class_rp));
}

void CWII_IPC_HLE_Device_usb_oh1_57e_305::CommandHostBufferSize(u8* _Input)
{
	// command parameters
	hci_host_buffer_size_cp* pHostBufferSize = (hci_host_buffer_size_cp*)_Input;
	m_HostMaxACLSize = pHostBufferSize->max_acl_size;
	m_HostMaxSCOSize = pHostBufferSize->max_sco_size;
	m_HostNumACLPackets = pHostBufferSize->num_acl_pkts;
	m_HostNumSCOPackets = pHostBufferSize->num_sco_pkts;

	// reply
	hci_host_buffer_size_rp Reply;
	Reply.status = 0x00;

	INFO_LOG(WII_IPC_WIIMOTE, "Command: HCI_CMD_HOST_BUFFER_SIZE:");
	DEBUG_LOG(WII_IPC_WIIMOTE, "  max_acl_size: %i", pHostBufferSize->max_acl_size);
	DEBUG_LOG(WII_IPC_WIIMOTE, "  max_sco_size: %i", pHostBufferSize->max_sco_size);
	DEBUG_LOG(WII_IPC_WIIMOTE, "  num_acl_pkts: %i", pHostBufferSize->num_acl_pkts);
	DEBUG_LOG(WII_IPC_WIIMOTE, "  num_sco_pkts: %i", pHostBufferSize->num_sco_pkts);

	SendEventCommandComplete(HCI_CMD_HOST_BUFFER_SIZE, &Reply, sizeof(hci_host_buffer_size_rp));
}

void CWII_IPC_HLE_Device_usb_oh1_57e_305::CommandWriteLinkSupervisionTimeout(u8* _Input)
{
	// command parameters
	hci_write_link_supervision_timeout_cp* pSuperVision = (hci_write_link_supervision_timeout_cp*)_Input;

	INFO_LOG(WII_IPC_WIIMOTE, "Command: HCI_CMD_WRITE_LINK_SUPERVISION_TIMEOUT");
	DEBUG_LOG(WII_IPC_WIIMOTE, "  con_handle: 0x%04x", pSuperVision->con_handle);
	DEBUG_LOG(WII_IPC_WIIMOTE, "  timeout: 0x%02x", pSuperVision->timeout);

	hci_write_link_supervision_timeout_rp Reply;
	Reply.status = 0x00;
	Reply.con_handle = pSuperVision->con_handle;

	SendEventCommandComplete(HCI_CMD_WRITE_LINK_SUPERVISION_TIMEOUT, &Reply, sizeof(hci_write_link_supervision_timeout_rp));

	// Now it is a good time to link channels
	CWII_IPC_HLE_WiiMote* pWiimote = AccessWiiMote(pSuperVision->con_handle);
	if (pWiimote)
		pWiimote->EventConnectionAccepted();
}

void CWII_IPC_HLE_Device_usb_oh1_57e_305::CommandWriteInquiryScanType(u8* _Input)
{
#if MAX_LOGLEVEL >= DEBUG_LEVEL
	// command parameters
	hci_write_inquiry_scan_type_cp* pSetEventFilter = (hci_write_inquiry_scan_type_cp*)_Input;
#endif
	// reply
	hci_write_inquiry_scan_type_rp Reply;
	Reply.status = 0x00;

	INFO_LOG(WII_IPC_WIIMOTE, "Command: HCI_CMD_WRITE_INQUIRY_SCAN_TYPE:");
	DEBUG_LOG(WII_IPC_WIIMOTE, "  type: %i", pSetEventFilter->type);

	SendEventCommandComplete(HCI_CMD_WRITE_INQUIRY_SCAN_TYPE, &Reply, sizeof(hci_write_inquiry_scan_type_rp));
}

void CWII_IPC_HLE_Device_usb_oh1_57e_305::CommandWriteInquiryMode(u8* _Input)
{
#if MAX_LOGLEVEL >= 4
	// command parameters
	hci_write_inquiry_mode_cp* pInquiryMode = (hci_write_inquiry_mode_cp*)_Input;
#endif

	// reply
	hci_write_inquiry_mode_rp Reply;
	Reply.status = 0x00;

#if MAX_LOGLEVEL >= DEBUG_LEVEL
	static char InquiryMode[][128] =
	{
		{ "Standard Inquiry Result event format (default)" },
		{ "Inquiry Result format with RSSI" },
		{ "Inquiry Result with RSSI format or Extended Inquiry Result format" }
	};
#endif
	INFO_LOG(WII_IPC_WIIMOTE, "Command: HCI_CMD_WRITE_INQUIRY_MODE:");
	DEBUG_LOG(WII_IPC_WIIMOTE, "  mode: %s", InquiryMode[pInquiryMode->mode]);

	SendEventCommandComplete(HCI_CMD_WRITE_INQUIRY_MODE, &Reply, sizeof(hci_write_inquiry_mode_rp));
}

void CWII_IPC_HLE_Device_usb_oh1_57e_305::CommandWritePageScanType(u8* _Input)
{
#if MAX_LOGLEVEL >= DEBUG_LEVEL
	// command parameters
	hci_write_page_scan_type_cp* pWritePageScanType = (hci_write_page_scan_type_cp*)_Input;
#endif

	// reply
	hci_write_page_scan_type_rp Reply;
	Reply.status = 0x00;

#if MAX_LOGLEVEL >= DEBUG_LEVEL
	static char PageScanType[][128] =
	{
		{ "Mandatory: Standard Scan (default)" },
		{ "Optional: Interlaced Scan" }
	};
#endif

	INFO_LOG(WII_IPC_WIIMOTE, "Command: HCI_CMD_WRITE_PAGE_SCAN_TYPE:");
	DEBUG_LOG(WII_IPC_WIIMOTE, "  type: %s", PageScanType[pWritePageScanType->type]);

	SendEventCommandComplete(HCI_CMD_WRITE_PAGE_SCAN_TYPE, &Reply, sizeof(hci_write_page_scan_type_rp));
}

void CWII_IPC_HLE_Device_usb_oh1_57e_305::CommandReadLocalVer(u8* _Input)
{
	// reply
	hci_read_local_ver_rp Reply;
	Reply.status = 0x00;
	Reply.hci_version = 0x03;        // HCI version: 1.1
	Reply.hci_revision = 0x40a7;     // current revision (?)
	Reply.lmp_version = 0x03;        // LMP version: 1.1    
	Reply.manufacturer = 0x000F;     // manufacturer: reserved for tests
	Reply.lmp_subversion = 0x430e;   // LMP subversion

	INFO_LOG(WII_IPC_WIIMOTE, "Command: HCI_CMD_READ_LOCAL_VER:");
	DEBUG_LOG(WII_IPC_WIIMOTE, "return:");
	DEBUG_LOG(WII_IPC_WIIMOTE, "  status:         %i", Reply.status);
	DEBUG_LOG(WII_IPC_WIIMOTE, "  hci_revision:   %i", Reply.hci_revision);
	DEBUG_LOG(WII_IPC_WIIMOTE, "  lmp_version:    %i", Reply.lmp_version);
	DEBUG_LOG(WII_IPC_WIIMOTE, "  manufacturer:   %i", Reply.manufacturer);
	DEBUG_LOG(WII_IPC_WIIMOTE, "  lmp_subversion: %i", Reply.lmp_subversion);

	SendEventCommandComplete(HCI_CMD_READ_LOCAL_VER, &Reply, sizeof(hci_read_local_ver_rp));
}

void CWII_IPC_HLE_Device_usb_oh1_57e_305::CommandReadLocalFeatures(u8* _Input)
{
	// reply
	hci_read_local_features_rp Reply;    
	Reply.status = 0x00;
	Reply.features[0] = 0xFF;
	Reply.features[1] = 0xFF;
	Reply.features[2] = 0x8D;
	Reply.features[3] = 0xFE;
	Reply.features[4] = 0x9B;
	Reply.features[5] = 0xF9;
	Reply.features[6] = 0x00;
	Reply.features[7] = 0x80;

	INFO_LOG(WII_IPC_WIIMOTE, "Command: HCI_CMD_READ_LOCAL_FEATURES:");
	DEBUG_LOG(WII_IPC_WIIMOTE, "return:");
	DEBUG_LOG(WII_IPC_WIIMOTE, "  features: %02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x",   
		Reply.features[0], Reply.features[1], Reply.features[2],
		Reply.features[3], Reply.features[4], Reply.features[5],
		Reply.features[6], Reply.features[7]);

	SendEventCommandComplete(HCI_CMD_READ_LOCAL_FEATURES, &Reply, sizeof(hci_read_local_features_rp));
}

void CWII_IPC_HLE_Device_usb_oh1_57e_305::CommandReadBufferSize(u8* _Input)
{
	// reply
	hci_read_buffer_size_rp Reply;
	Reply.status = 0x00;
	Reply.max_acl_size = 0x0FFF;	//339;
	Reply.num_acl_pkts = 0xFF;		//10;
	Reply.max_sco_size = 64;
	Reply.num_sco_pkts = 0;
	// AyuanX: Are these parameters fixed or adjustable ???

	INFO_LOG(WII_IPC_WIIMOTE, "Command: HCI_CMD_READ_BUFFER_SIZE:");
	DEBUG_LOG(WII_IPC_WIIMOTE, "return:");
	DEBUG_LOG(WII_IPC_WIIMOTE, "  max_acl_size: %i", Reply.max_acl_size);
	DEBUG_LOG(WII_IPC_WIIMOTE, "  num_acl_pkts: %i", Reply.num_acl_pkts);
	DEBUG_LOG(WII_IPC_WIIMOTE, "  max_sco_size: %i", Reply.max_sco_size);
	DEBUG_LOG(WII_IPC_WIIMOTE, "  num_sco_pkts: %i", Reply.num_sco_pkts);

	SendEventCommandComplete(HCI_CMD_READ_BUFFER_SIZE, &Reply, sizeof(hci_read_buffer_size_rp));
}

void CWII_IPC_HLE_Device_usb_oh1_57e_305::CommandReadBDAdrr(u8* _Input)
{
	// reply
	hci_read_bdaddr_rp Reply;
	Reply.status = 0x00;
	Reply.bdaddr = m_ControllerBD;

	INFO_LOG(WII_IPC_WIIMOTE, "Command: HCI_CMD_READ_BDADDR:");
	DEBUG_LOG(WII_IPC_WIIMOTE, "return:");
	DEBUG_LOG(WII_IPC_WIIMOTE, "  bd: %02x:%02x:%02x:%02x:%02x:%02x",
		Reply.bdaddr.b[0], Reply.bdaddr.b[1], Reply.bdaddr.b[2],
		Reply.bdaddr.b[3], Reply.bdaddr.b[4], Reply.bdaddr.b[5]);

	SendEventCommandComplete(HCI_CMD_READ_BDADDR, &Reply, sizeof(hci_read_bdaddr_rp));
}

void CWII_IPC_HLE_Device_usb_oh1_57e_305::CommandVendorSpecific_FC4F(u8* _Input, u32 _Size)
{
	// callstack...
	// BTM_VendorSpecificCommad()
	// WUDiRemovePatch()
	// WUDiAppendRuntimePatch()
	// WUDiGetFirmwareVersion()
	// WUDiStackSetupComplete()

	// reply
	hci_status_rp Reply;
	Reply.status = 0x00;

	INFO_LOG(WII_IPC_WIIMOTE, "Command: CommandVendorSpecific_FC4F: (callstack WUDiRemovePatch)");
	INFO_LOG(WII_IPC_WIIMOTE, "input (size 0x%x):", _Size);

	Dolphin_Debugger::PrintDataBuffer(LogTypes::WII_IPC_WIIMOTE, _Input, _Size, "Data: ");

	SendEventCommandComplete(0xFC4F, &Reply, sizeof(hci_status_rp));
}

void CWII_IPC_HLE_Device_usb_oh1_57e_305::CommandVendorSpecific_FC4C(u8* _Input, u32 _Size)
{
	// reply
	hci_status_rp Reply;
	Reply.status = 0x00;

	INFO_LOG(WII_IPC_WIIMOTE, "Command: CommandVendorSpecific_FC4C:");
	INFO_LOG(WII_IPC_WIIMOTE, "input (size 0x%x):", _Size);
	Dolphin_Debugger::PrintDataBuffer(LogTypes::WII_IPC_WIIMOTE, _Input, _Size, "Data: ");

	SendEventCommandComplete(0xFC4C, &Reply, sizeof(hci_status_rp));
}


//
//
// --- helper
//
//
CWII_IPC_HLE_WiiMote* CWII_IPC_HLE_Device_usb_oh1_57e_305::AccessWiiMote(const bdaddr_t& _rAddr)
{
	for (size_t i=0; i<m_WiiMotes.size(); i++)
	{
		const bdaddr_t& BD = m_WiiMotes[i].GetBD();
		if ((_rAddr.b[0] == BD.b[0]) &&
			(_rAddr.b[1] == BD.b[1]) &&
			(_rAddr.b[2] == BD.b[2]) &&
			(_rAddr.b[3] == BD.b[3]) &&
			(_rAddr.b[4] == BD.b[4]) &&
			(_rAddr.b[5] == BD.b[5]))
			return &m_WiiMotes[i];
	}

	ERROR_LOG(WII_IPC_WIIMOTE,"Cant find WiiMote by bd: %02x:%02x:%02x:%02x:%02x:%02x",
		_rAddr.b[0], _rAddr.b[1], _rAddr.b[2], _rAddr.b[3], _rAddr.b[4], _rAddr.b[5]); 
	PanicAlert("Cant find WiiMote by bd: %02x:%02x:%02x:%02x:%02x:%02x",
		_rAddr.b[0], _rAddr.b[1], _rAddr.b[2], _rAddr.b[3], _rAddr.b[4], _rAddr.b[5]);
	return NULL;
}

CWII_IPC_HLE_WiiMote* CWII_IPC_HLE_Device_usb_oh1_57e_305::AccessWiiMote(u16 _ConnectionHandle)
{
	for (size_t i=0; i<m_WiiMotes.size(); i++)
	{
		if (m_WiiMotes[i].GetConnectionHandle() == _ConnectionHandle)
			return &m_WiiMotes[i];
	}

	ERROR_LOG(WII_IPC_WIIMOTE, "Cant find WiiMote by connection handle %02x", _ConnectionHandle);
	PanicAlert("Cant find WiiMote by connection handle %02x", _ConnectionHandle);
	return NULL;
}

void CWII_IPC_HLE_Device_usb_oh1_57e_305::LOG_LinkKey(const u8* _pLinkKey)
{
	DEBUG_LOG(WII_IPC_WIIMOTE, "  link key: "
				 "0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x "
				 "0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x "
				 , _pLinkKey[0], _pLinkKey[1], _pLinkKey[2], _pLinkKey[3], _pLinkKey[4], _pLinkKey[5], _pLinkKey[6], _pLinkKey[7]
				 , _pLinkKey[8], _pLinkKey[9], _pLinkKey[10], _pLinkKey[11], _pLinkKey[12], _pLinkKey[13], _pLinkKey[14], _pLinkKey[15]);

}


//
// CWII_IPC_HLE_Device_usb_oh0
//
CWII_IPC_HLE_Device_usb_oh0::CWII_IPC_HLE_Device_usb_oh0(u32 _DeviceID, const std::string& _rDeviceName)
: IWII_IPC_HLE_Device(_DeviceID, _rDeviceName)
{
}

CWII_IPC_HLE_Device_usb_oh0::~CWII_IPC_HLE_Device_usb_oh0()
{
}

bool CWII_IPC_HLE_Device_usb_oh0::Open(u32 _CommandAddress, u32 _Mode)
{
	Memory::Write_U32(GetDeviceID(), _CommandAddress+4);
	m_Active = true;
	return true;
}

bool CWII_IPC_HLE_Device_usb_oh0::Close(u32 _CommandAddress, bool _bForce)
{
	if (!_bForce)
		Memory::Write_U32(0, _CommandAddress + 0x4);
	m_Active = false;
	return true;
}

bool CWII_IPC_HLE_Device_usb_oh0::IOCtl(u32 _CommandAddress)
{
    // write return value
    Memory::Write_U32(0, _CommandAddress + 0x4);
    return true;
}

bool CWII_IPC_HLE_Device_usb_oh0::IOCtlV(u32 _CommandAddress)
{
	// write return value
	Memory::Write_U32(0, _CommandAddress + 0x4);
	return true;
}


//
// CWII_IPC_HLE_Device_usb_hid
//
CWII_IPC_HLE_Device_usb_hid::CWII_IPC_HLE_Device_usb_hid(u32 _DeviceID, const std::string& _rDeviceName)
: IWII_IPC_HLE_Device(_DeviceID, _rDeviceName)
{
}

CWII_IPC_HLE_Device_usb_hid::~CWII_IPC_HLE_Device_usb_hid()
{
}

bool CWII_IPC_HLE_Device_usb_hid::Open(u32 _CommandAddress, u32 _Mode)
{
	Memory::Write_U32(GetDeviceID(), _CommandAddress+4);
	m_Active = true;
	return true;
}

bool CWII_IPC_HLE_Device_usb_hid::Close(u32 _CommandAddress, bool _bForce)
{
	if (!_bForce)
		Memory::Write_U32(0, _CommandAddress+4);
	m_Active = false;
	return true;
}

bool CWII_IPC_HLE_Device_usb_hid::IOCtl(u32 _CommandAddress)
{
	// write return value
	Memory::Write_U32(0, _CommandAddress + 0x4);
	return true;
}

bool CWII_IPC_HLE_Device_usb_hid::IOCtlV(u32 _CommandAddress)
{
	// write return value
	Memory::Write_U32(0, _CommandAddress + 0x4);
	return true;
}
