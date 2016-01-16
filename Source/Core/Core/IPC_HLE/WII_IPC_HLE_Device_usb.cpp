// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <cstring>
#include <memory>
#include <string>

#include "Common/Assert.h"
#include "Common/CommonPaths.h"
#include "Common/CommonTypes.h"
#include "Common/Logging/Log.h"
#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/CoreTiming.h"
#include "Core/Host.h"
#include "Core/Movie.h"
#include "Core/Debugger/Debugger_SymbolMap.h"
#include "Core/HW/Memmap.h"
#include "Core/HW/SystemTimers.h"
#include "Core/HW/WII_IPC.h"
#include "Core/HW/Wiimote.h"
#include "Core/IPC_HLE/WII_IPC_HLE.h"
#include "Core/IPC_HLE/WII_IPC_HLE_Device_usb.h"
#include "Core/IPC_HLE/WII_IPC_HLE_WiiMote.h"
#include "InputCommon/ControllerInterface/ControllerInterface.h"

SQueuedEvent::SQueuedEvent(u32 size, u16 connection_handle)
	: m_size(size)
	, m_connectionHandle(connection_handle)
{
	if (m_size > 1024)
	{
		// I know this code sux...
		PanicAlert("SQueuedEvent: allocate too big buffer!!");
	}
	memset(m_buffer, 0, 1024);
}

// The device class
CWII_IPC_HLE_Device_usb_oh1_57e_305::CWII_IPC_HLE_Device_usb_oh1_57e_305(u32 _DeviceID, const std::string& _rDeviceName)
	: IWII_IPC_HLE_Device(_DeviceID, _rDeviceName)
	, m_ScanEnable(0)
	, m_HCIEndpoint(0)
	, m_ACLEndpoint(0)
	, m_last_ticks(0)
{
	SysConf* sysconf;
	std::unique_ptr<SysConf> owned_sysconf;
	if (Core::g_want_determinism)
	{
		// See SysConf::UpdateLocation for comment about the Future.
		owned_sysconf.reset(new SysConf());
		sysconf = owned_sysconf.get();
		sysconf->LoadFromFile(File::GetUserPath(D_SESSION_WIIROOT_IDX) + DIR_SEP WII_SYSCONF_DIR DIR_SEP WII_SYSCONF);
	}
	else
	{
		sysconf = SConfig::GetInstance().m_SYSCONF;
	}

	// Activate only first Wiimote by default

	_conf_pads BT_DINF;
	SetUsbPointer(this);
	if (!sysconf->GetArrayData("BT.DINF", (u8*)&BT_DINF, sizeof(_conf_pads)))
	{
		PanicAlertT("Trying to read from invalid SYSCONF\nWiimote bt ids are not available");
	}
	else
	{
		bdaddr_t tmpBD = BDADDR_ANY;
		u8 i = 0;
		while (i < MAX_BBMOTES)
		{
			if (i < BT_DINF.num_registered)
			{
				tmpBD.b[5] = BT_DINF.active[i].bdaddr[0] = BT_DINF.registered[i].bdaddr[0];
				tmpBD.b[4] = BT_DINF.active[i].bdaddr[1] = BT_DINF.registered[i].bdaddr[1];
				tmpBD.b[3] = BT_DINF.active[i].bdaddr[2] = BT_DINF.registered[i].bdaddr[2];
				tmpBD.b[2] = BT_DINF.active[i].bdaddr[3] = BT_DINF.registered[i].bdaddr[3];
				tmpBD.b[1] = BT_DINF.active[i].bdaddr[4] = BT_DINF.registered[i].bdaddr[4];
				tmpBD.b[0] = BT_DINF.active[i].bdaddr[5] = BT_DINF.registered[i].bdaddr[5];
			}
			else
			{
				tmpBD.b[5] = BT_DINF.active[i].bdaddr[0] = BT_DINF.registered[i].bdaddr[0] = i;
				tmpBD.b[4] = BT_DINF.active[i].bdaddr[1] = BT_DINF.registered[i].bdaddr[1] = 0;
				tmpBD.b[3] = BT_DINF.active[i].bdaddr[2] = BT_DINF.registered[i].bdaddr[2] = 0x79;
				tmpBD.b[2] = BT_DINF.active[i].bdaddr[3] = BT_DINF.registered[i].bdaddr[3] = 0x19;
				tmpBD.b[1] = BT_DINF.active[i].bdaddr[4] = BT_DINF.registered[i].bdaddr[4] = 2;
				tmpBD.b[0] = BT_DINF.active[i].bdaddr[5] = BT_DINF.registered[i].bdaddr[5] = 0x11;
			}

			const char* wmName;
			if (i == WIIMOTE_BALANCE_BOARD)
				wmName = "Nintendo RVL-WBC-01";
			else
				wmName = "Nintendo RVL-CNT-01";
			memcpy(BT_DINF.registered[i].name, wmName, 20);
			memcpy(BT_DINF.active[i].name, wmName, 20);

			INFO_LOG(WII_IPC_WIIMOTE, "Wiimote %d BT ID %x,%x,%x,%x,%x,%x", i, tmpBD.b[0], tmpBD.b[1], tmpBD.b[2], tmpBD.b[3], tmpBD.b[4], tmpBD.b[5]);
			m_WiiMotes.push_back(CWII_IPC_HLE_WiiMote(this, i, tmpBD, false));
			i++;
		}

		// save now so that when games load sysconf file it includes the new Wiimotes
		// and the correct order for connected Wiimotes
		if (!sysconf->SetArrayData("BT.DINF", (u8*)&BT_DINF, sizeof(_conf_pads)) || !sysconf->Save())
			PanicAlertT("Failed to write BT.DINF to SYSCONF");
	}

	// The BCM2045's btaddr:
	m_ControllerBD.b[0] = 0x11;
	m_ControllerBD.b[1] = 0x02;
	m_ControllerBD.b[2] = 0x19;
	m_ControllerBD.b[3] = 0x79;
	m_ControllerBD.b[4] = 0x00;
	m_ControllerBD.b[5] = 0xFF;

	memset(m_PacketCount, 0, sizeof(m_PacketCount));

	Host_SetWiiMoteConnectionState(0);
}

CWII_IPC_HLE_Device_usb_oh1_57e_305::~CWII_IPC_HLE_Device_usb_oh1_57e_305()
{
	m_WiiMotes.clear();
	SetUsbPointer(nullptr);
}


CWII_IPC_HLE_Device_usb_oh1_57e_305::CtrlBuffer::CtrlBuffer(u32 address)
	: m_address(address)
{
	if (m_address == 0)
		return;

	u32 InBufferNum = Memory::Read_U32(m_address + 0x10);
	u32 BufferVector = Memory::Read_U32(m_address + 0x18);
	m_buffer = Memory::Read_U32(
		BufferVector + InBufferNum * sizeof(SIOCtlVBuffer::SBuffer));
}

void CWII_IPC_HLE_Device_usb_oh1_57e_305::CtrlBuffer::FillBuffer(const void* src, const size_t size) const
{
	Memory::CopyToEmu(m_buffer, src, size);
}

void CWII_IPC_HLE_Device_usb_oh1_57e_305::CtrlBuffer::SetRetVal(const u32 retval) const
{
	Memory::Write_U32(retval, m_address + 4);
}

void CWII_IPC_HLE_Device_usb_oh1_57e_305::DoState(PointerWrap &p)
{
	p.Do(m_Active);
	p.Do(m_ControllerBD);
	p.Do(m_CtrlSetup);
	p.Do(m_ACLSetup);
	p.DoPOD(m_HCIEndpoint);
	p.DoPOD(m_ACLEndpoint);
	p.Do(m_last_ticks);
	p.DoArray(m_PacketCount);
	p.Do(m_ScanEnable);
	p.Do(m_EventQueue);
	m_acl_pool.DoState(p);

	for (unsigned int i = 0; i < MAX_BBMOTES; i++)
		m_WiiMotes[i].DoState(p);
}

bool CWII_IPC_HLE_Device_usb_oh1_57e_305::RemoteDisconnect(u16 _connectionHandle)
{
	return SendEventDisconnect(_connectionHandle, 0x13);
}

IPCCommandResult CWII_IPC_HLE_Device_usb_oh1_57e_305::Open(u32 _CommandAddress, u32 _Mode)
{
	m_ScanEnable = 0;

	m_last_ticks = 0;
	memset(m_PacketCount, 0, sizeof(m_PacketCount));

	m_HCIEndpoint.m_address = 0;
	m_ACLEndpoint.m_address = 0;

	Memory::Write_U32(GetDeviceID(), _CommandAddress + 4);
	m_Active = true;
	return GetDefaultReply();
}

IPCCommandResult CWII_IPC_HLE_Device_usb_oh1_57e_305::Close(u32 _CommandAddress, bool _bForce)
{
	m_ScanEnable = 0;

	m_last_ticks = 0;
	memset(m_PacketCount, 0, sizeof(m_PacketCount));

	m_HCIEndpoint.m_address = 0;
	m_ACLEndpoint.m_address = 0;

	if (!_bForce)
		Memory::Write_U32(0, _CommandAddress + 4);
	m_Active = false;
	return GetDefaultReply();
}

IPCCommandResult CWII_IPC_HLE_Device_usb_oh1_57e_305::IOCtl(u32 _CommandAddress)
{
	//ERROR_LOG(WII_IPC_WIIMOTE, "Passing ioctl to ioctlv");
	return IOCtlV(_CommandAddress); // FIXME: Hack
}

IPCCommandResult CWII_IPC_HLE_Device_usb_oh1_57e_305::IOCtlV(u32 _CommandAddress)
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

	switch (CommandBuffer.Parameter)
	{
	case USBV0_IOCTL_CTRLMSG: // HCI command is received from the stack
		{
			// This is the HCI datapath from CPU to Wiimote, the USB stuff is little endian..
			m_CtrlSetup.bRequestType  = *( u8*)Memory::GetPointer(CommandBuffer.InBuffer[0].m_Address);
			m_CtrlSetup.bRequest      = *( u8*)Memory::GetPointer(CommandBuffer.InBuffer[1].m_Address);
			m_CtrlSetup.wValue        = *(u16*)Memory::GetPointer(CommandBuffer.InBuffer[2].m_Address);
			m_CtrlSetup.wIndex        = *(u16*)Memory::GetPointer(CommandBuffer.InBuffer[3].m_Address);
			m_CtrlSetup.wLength       = *(u16*)Memory::GetPointer(CommandBuffer.InBuffer[4].m_Address);
			m_CtrlSetup.m_PayLoadAddr = CommandBuffer.PayloadBuffer[0].m_Address;
			m_CtrlSetup.m_PayLoadSize = CommandBuffer.PayloadBuffer[0].m_Size;
			m_CtrlSetup.m_Address     = CommandBuffer.m_Address;

			// check termination
			_dbg_assert_msg_(WII_IPC_WIIMOTE, *(u8*)Memory::GetPointer(CommandBuffer.InBuffer[5].m_Address) == 0,
				"WIIMOTE: Termination != 0");

			#if 0 // this log can get really annoying
			DEBUG_LOG(WII_IPC_WIIMOTE, "USB_IOCTL_CTRLMSG (0x%08x) - execute command", _CommandAddress);
			DEBUG_LOG(WII_IPC_WIIMOTE, "    bRequestType: 0x%x", m_CtrlSetup.bRequestType);
			DEBUG_LOG(WII_IPC_WIIMOTE, "    bRequest: 0x%x", m_CtrlSetup.bRequest);
			DEBUG_LOG(WII_IPC_WIIMOTE, "    wValue: 0x%x", m_CtrlSetup.wValue);
			DEBUG_LOG(WII_IPC_WIIMOTE, "    wIndex: 0x%x", m_CtrlSetup.wIndex);
			DEBUG_LOG(WII_IPC_WIIMOTE, "    wLength: 0x%x", m_CtrlSetup.wLength);
			DEBUG_LOG(WII_IPC_WIIMOTE, "    m_PayLoadAddr:  0x%x", m_CtrlSetup.m_PayLoadAddr);
			DEBUG_LOG(WII_IPC_WIIMOTE, "    m_PayLoadSize:  0x%x", m_CtrlSetup.m_PayLoadSize);
			#endif

			// Replies are generated inside
			ExecuteHCICommandMessage(m_CtrlSetup);
		}
		break;

	case USBV0_IOCTL_BLKMSG:
		{
			u8 Command = Memory::Read_U8(CommandBuffer.InBuffer[0].m_Address);
			switch (Command)
			{
			case ACL_DATA_OUT: // ACL data is received from the stack
				{
					// This is the ACL datapath from CPU to Wiimote
					// Here we only need to record the command address in case we need to delay the reply
					m_ACLSetup = CommandBuffer.m_Address;

					#if defined(_DEBUG) || defined(DEBUGFAST)
					DumpAsync(CommandBuffer.BufferVector, CommandBuffer.NumberInBuffer, CommandBuffer.NumberPayloadBuffer);
					#endif

					CtrlBuffer BulkBuffer(_CommandAddress);
					hci_acldata_hdr_t* pACLHeader = (hci_acldata_hdr_t*)Memory::GetPointer(BulkBuffer.m_buffer);

					_dbg_assert_(WII_IPC_WIIMOTE, HCI_BC_FLAG(pACLHeader->con_handle) == HCI_POINT2POINT);
					_dbg_assert_(WII_IPC_WIIMOTE, HCI_PB_FLAG(pACLHeader->con_handle) == HCI_PACKET_START);

					SendToDevice(HCI_CON_HANDLE(pACLHeader->con_handle),
						Memory::GetPointer(BulkBuffer.m_buffer + sizeof(hci_acldata_hdr_t)),
						pACLHeader->length);

					_SendReply = true;
				}
				break;

			case ACL_DATA_IN: // We are given an ACL buffer to fill
				{
					CtrlBuffer temp(_CommandAddress);
					m_ACLEndpoint = temp;

					DEBUG_LOG(WII_IPC_WIIMOTE, "ACL_DATA_IN: 0x%08x ", _CommandAddress);
				}
				break;

			default:
				{
					_dbg_assert_msg_(WII_IPC_WIIMOTE, 0, "Unknown USBV0_IOCTL_BLKMSG: %x", Command);
				}
				break;
			}
		}
		break;


	case USBV0_IOCTL_INTRMSG:
		{
			u8 Command = Memory::Read_U8(CommandBuffer.InBuffer[0].m_Address);
			if (Command == HCI_EVENT) // We are given a HCI buffer to fill
			{
				CtrlBuffer temp(_CommandAddress);
				m_HCIEndpoint = temp;

				DEBUG_LOG(WII_IPC_WIIMOTE, "HCI_EVENT: 0x%08x ", _CommandAddress);
			}
			else
			{
				_dbg_assert_msg_(WII_IPC_WIIMOTE, 0, "Unknown USBV0_IOCTL_INTRMSG: %x", Command);
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
			DumpAsync(CommandBuffer.BufferVector, CommandBuffer.NumberInBuffer, CommandBuffer.NumberPayloadBuffer);
			#endif
		}
		break;
	}

	// write return value
	Memory::Write_U32(0, _CommandAddress + 4);
	return _SendReply ? GetDefaultReply() : GetNoReply();
}

void CWII_IPC_HLE_Device_usb_oh1_57e_305::EnqueueReply(u32 CommandAddress)
{
	// IOS seems to write back the command that was responded to in the FD field, this
	// class does not overwrite the command so it is safe to read back.
	Memory::Write_U32(Memory::Read_U32(CommandAddress), CommandAddress + 8);
	// The original hardware overwrites the command type with the async reply type.
	Memory::Write_U32(IPC_REP_ASYNC, CommandAddress);

	WII_IPC_HLE_Interface::EnqueueReply(CommandAddress);
}

// Here we handle the USBV0_IOCTL_BLKMSG Ioctlv
void CWII_IPC_HLE_Device_usb_oh1_57e_305::SendToDevice(u16 _ConnectionHandle, u8* _pData, u32 _Size)
{
	CWII_IPC_HLE_WiiMote* pWiiMote = AccessWiiMote(_ConnectionHandle);
	if (pWiiMote == nullptr)
		return;

	INFO_LOG(WII_IPC_WIIMOTE, "Send ACL Packet to ConnectionHandle 0x%04x", _ConnectionHandle);
	IncDataPacket(_ConnectionHandle);
	pWiiMote->ExecuteL2capCmd(_pData, _Size);
}

void CWII_IPC_HLE_Device_usb_oh1_57e_305::IncDataPacket(u16 _ConnectionHandle)
{
	m_PacketCount[_ConnectionHandle & 0xff]++;

	// I don't think this makes sense or should be necessary
	// m_PacketCount refers to "completed" packets and is not related to some buffer size, yes?
#if 0
	if (m_PacketCount[_ConnectionHandle & 0xff] > (unsigned int)m_acl_pkts_num)
	{
		DEBUG_LOG(WII_IPC_WIIMOTE, "ACL buffer overflow");
		m_PacketCount[_ConnectionHandle & 0xff] = m_acl_pkts_num;
	}
#endif
}

// Here we send ACL packets to CPU. They will consist of header + data.
// The header is for example 07 00 41 00 which means size 0x0007 and channel 0x0041.
void CWII_IPC_HLE_Device_usb_oh1_57e_305::SendACLPacket(u16 connection_handle, const u8* data, u32 size)
{
	DEBUG_LOG(WII_IPC_WIIMOTE, "ACL packet from %x ready to send to stack...", connection_handle);

	if (m_ACLEndpoint.IsValid() && !m_HCIEndpoint.IsValid() && m_EventQueue.empty())
	{
		DEBUG_LOG(WII_IPC_WIIMOTE, "ACL endpoint valid, sending packet to %08x", m_ACLEndpoint.m_address);

		hci_acldata_hdr_t* header = reinterpret_cast<hci_acldata_hdr_t*>(Memory::GetPointer(m_ACLEndpoint.m_buffer));
		header->con_handle = HCI_MK_CON_HANDLE(connection_handle, HCI_PACKET_START, HCI_POINT2POINT);
		header->length     = size;

		// Write the packet to the buffer
		memcpy(reinterpret_cast<u8*>(header) + sizeof(hci_acldata_hdr_t), data, header->length);

		m_ACLEndpoint.SetRetVal(sizeof(hci_acldata_hdr_t) + size);
		EnqueueReply(m_ACLEndpoint.m_address);
		m_ACLEndpoint.Invalidate();
	}
	else
	{
		DEBUG_LOG(WII_IPC_WIIMOTE, "ACL endpoint not currently valid, queuing...");
		m_acl_pool.Store(data, size, connection_handle);
	}
}

// These messages are sent from the Wiimote to the game, for example RequestConnection()
// or ConnectionComplete().
//
// Our WII_IPC_HLE is so efficient that we could fill the buffer immediately
// rather than enqueue it to some other memory and this will do good for StateSave
void CWII_IPC_HLE_Device_usb_oh1_57e_305::AddEventToQueue(const SQueuedEvent& _event)
{
	DEBUG_LOG(WII_IPC_WIIMOTE, "HCI event %x completed...", ((hci_event_hdr_t*)_event.m_buffer)->event);

	if (m_HCIEndpoint.IsValid())
	{
		if (m_EventQueue.empty()) // fast path :)
		{
			DEBUG_LOG(WII_IPC_WIIMOTE, "HCI endpoint valid, sending packet to %08x", m_HCIEndpoint.m_address);
			m_HCIEndpoint.FillBuffer(_event.m_buffer, _event.m_size);
			m_HCIEndpoint.SetRetVal(_event.m_size);
			// Send a reply to indicate HCI buffer is filled
			EnqueueReply(m_HCIEndpoint.m_address);
			m_HCIEndpoint.Invalidate();
		}
		else // push new one, pop oldest
		{
			DEBUG_LOG(WII_IPC_WIIMOTE, "HCI endpoint not currently valid, queueing (%zu)...",
				m_EventQueue.size());
			m_EventQueue.push_back(_event);
			const SQueuedEvent& event = m_EventQueue.front();
			DEBUG_LOG(WII_IPC_WIIMOTE, "HCI event %x "
				"being written from queue (%zu) to %08x...",
				((hci_event_hdr_t*)event.m_buffer)->event,
				m_EventQueue.size()-1,
				m_HCIEndpoint.m_address);
			m_HCIEndpoint.FillBuffer(event.m_buffer, event.m_size);
			m_HCIEndpoint.SetRetVal(event.m_size);
			// Send a reply to indicate HCI buffer is filled
			EnqueueReply(m_HCIEndpoint.m_address);
			m_HCIEndpoint.Invalidate();
			m_EventQueue.pop_front();
		}
	}
	else
	{
		DEBUG_LOG(WII_IPC_WIIMOTE, "HCI endpoint not currently valid, queuing (%zu)...", m_EventQueue.size());
		m_EventQueue.push_back(_event);
	}
}

u32 CWII_IPC_HLE_Device_usb_oh1_57e_305::Update()
{
	bool packet_transferred = false;

	// check HCI queue
	if (!m_EventQueue.empty() && m_HCIEndpoint.IsValid())
	{
		// an endpoint has become available, and we have a stored response.
		const SQueuedEvent& event = m_EventQueue.front();
		DEBUG_LOG(WII_IPC_WIIMOTE,
			"HCI event %x being written from queue (%zu) to %08x...",
			((hci_event_hdr_t*)event.m_buffer)->event,
			m_EventQueue.size()-1,
			m_HCIEndpoint.m_address);
		m_HCIEndpoint.FillBuffer(event.m_buffer, event.m_size);
		m_HCIEndpoint.SetRetVal(event.m_size);
		// Send a reply to indicate HCI buffer is filled
		EnqueueReply(m_HCIEndpoint.m_address);
		m_HCIEndpoint.Invalidate();
		m_EventQueue.pop_front();
		packet_transferred = true;
	}

	// check ACL queue
	if (!m_acl_pool.IsEmpty() && m_ACLEndpoint.IsValid() && m_EventQueue.empty())
	{
		m_acl_pool.WriteToEndpoint(m_ACLEndpoint);
		packet_transferred = true;
	}

	// We wait for ScanEnable to be sent from the Bluetooth stack through HCI_CMD_WRITE_SCAN_ENABLE
	// before we initiate the connection.
	//
	// FiRES: TODO find a better way to do this

	// Create ACL connection
	if (m_HCIEndpoint.IsValid() && (m_ScanEnable & HCI_PAGE_SCAN_ENABLE))
	{
		for (auto& wiimote : m_WiiMotes)
		{
			if (wiimote.EventPagingChanged(m_ScanEnable))
			{
				Host_SetWiiMoteConnectionState(1);
				SendEventRequestConnection(wiimote);
			}
		}
	}

	// Link channels when connected
	if (m_ACLEndpoint.IsValid())
	{
		for (auto& wiimote : m_WiiMotes)
		{
			if (wiimote.LinkChannel())
				break;
		}
	}

	// The Real Wiimote sends report every ~5ms (200 Hz).
	const u64 interval = SystemTimers::GetTicksPerSecond() / 200;
	const u64 now = CoreTiming::GetTicks();

	if (now - m_last_ticks > interval)
	{
		g_controller_interface.UpdateInput();
		for (unsigned int i = 0; i < m_WiiMotes.size(); i++)
			Wiimote::Update(i, m_WiiMotes[i].IsConnected());
		m_last_ticks = now;
	}

	SendEventNumberOfCompletedPackets();

	return packet_transferred;
}

void CWII_IPC_HLE_Device_usb_oh1_57e_305::ACLPool::Store(const u8* data, const u16 size, const u16 conn_handle)
{
	if (m_queue.size() >= 100)
	{
		// Many simultaneous exchanges of ACL packets tend to cause the queue to fill up.
		ERROR_LOG(WII_IPC_WIIMOTE, "ACL queue size reached 100 - current packet will be dropped!");
		return;
	}

	_dbg_assert_msg_(WII_IPC_WIIMOTE,
		size < m_acl_pkt_size, "ACL packet too large for pool");

	m_queue.push_back(Packet());
	auto& packet = m_queue.back();

	std::copy(data, data + size, packet.data);
	packet.size = size;
	packet.conn_handle = conn_handle;
}

void CWII_IPC_HLE_Device_usb_oh1_57e_305::ACLPool::WriteToEndpoint(CtrlBuffer& endpoint)
{
	auto& packet = m_queue.front();

	const u8* const data = packet.data;
	const u16 size = packet.size;
	const u16 conn_handle = packet.conn_handle;

	DEBUG_LOG(WII_IPC_WIIMOTE, "ACL packet being written from "
		"queue to %08x", endpoint.m_address);

	hci_acldata_hdr_t* pHeader = (hci_acldata_hdr_t*)Memory::GetPointer(endpoint.m_buffer);
	pHeader->con_handle = HCI_MK_CON_HANDLE(conn_handle, HCI_PACKET_START, HCI_POINT2POINT);
	pHeader->length = size;

	// Write the packet to the buffer
	std::copy(data, data + size, (u8*)pHeader + sizeof(hci_acldata_hdr_t));

	endpoint.SetRetVal(sizeof(hci_acldata_hdr_t) + size);

	m_queue.pop_front();

	EnqueueReply(endpoint.m_address);
	endpoint.Invalidate();
}

void CWII_IPC_HLE_Device_usb_oh1_57e_305::ACLPool::DoState(PointerWrap& p)
{
	p.Do(m_queue);
}

bool CWII_IPC_HLE_Device_usb_oh1_57e_305::SendEventInquiryComplete()
{
	SQueuedEvent Event(sizeof(SHCIEventInquiryComplete), 0);

	SHCIEventInquiryComplete* pInquiryComplete = (SHCIEventInquiryComplete*)Event.m_buffer;
	pInquiryComplete->EventType = HCI_EVENT_INQUIRY_COMPL;
	pInquiryComplete->PayloadLength = sizeof(SHCIEventInquiryComplete) - 2;
	pInquiryComplete->EventStatus = 0x00;

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

	for (size_t i=0; i < m_WiiMotes.size(); i++)
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

		INFO_LOG(WII_IPC_WIIMOTE, "Event: Send Fake Inquiry of one controller");
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
	if (pWiiMote == nullptr)
		return false;

	SQueuedEvent Event(sizeof(SHCIEventConnectionComplete), 0);

	SHCIEventConnectionComplete* pConnectionComplete = (SHCIEventConnectionComplete*)Event.m_buffer;

	pConnectionComplete->EventType = HCI_EVENT_CON_COMPL;
	pConnectionComplete->PayloadLength = sizeof(SHCIEventConnectionComplete) - 2;
	pConnectionComplete->EventStatus = 0x00;
	pConnectionComplete->Connection_Handle = pWiiMote->GetConnectionHandle();
	pConnectionComplete->bdaddr = _bd;
	pConnectionComplete->LinkType = HCI_LINK_ACL;
	pConnectionComplete->EncryptionEnabled = HCI_ENCRYPTION_MODE_NONE;

	AddEventToQueue(Event);

	CWII_IPC_HLE_WiiMote* pWiimote = AccessWiiMote(pConnectionComplete->Connection_Handle);
	if (pWiimote)
		pWiimote->EventConnectionAccepted();

	static char s_szLinkType[][128] =
	{
		{ "HCI_LINK_SCO     0x00 - Voice"},
		{ "HCI_LINK_ACL     0x01 - Data"},
		{ "HCI_LINK_eSCO    0x02 - eSCO"},
	};

	INFO_LOG(WII_IPC_WIIMOTE, "Event: SendEventConnectionComplete");
	INFO_LOG(WII_IPC_WIIMOTE, "  Connection_Handle: 0x%04x", pConnectionComplete->Connection_Handle);
	DEBUG_LOG(WII_IPC_WIIMOTE, "  bd: %02x:%02x:%02x:%02x:%02x:%02x",
		pConnectionComplete->bdaddr.b[0], pConnectionComplete->bdaddr.b[1], pConnectionComplete->bdaddr.b[2],
		pConnectionComplete->bdaddr.b[3], pConnectionComplete->bdaddr.b[4], pConnectionComplete->bdaddr.b[5]);
	DEBUG_LOG(WII_IPC_WIIMOTE, "  LinkType: %s", s_szLinkType[pConnectionComplete->LinkType]);
	DEBUG_LOG(WII_IPC_WIIMOTE, "  EncryptionEnabled: %i", pConnectionComplete->EncryptionEnabled);

	return true;
}

// This is called from Update() after ScanEnable has been enabled.
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

	AddEventToQueue(Event);

	static char LinkType[][128] =
	{
		{ "HCI_LINK_SCO     0x00 - Voice"},
		{ "HCI_LINK_ACL     0x01 - Data" },
		{ "HCI_LINK_eSCO    0x02 - eSCO" },
	};

	INFO_LOG(WII_IPC_WIIMOTE, "Event: SendEventRequestConnection");
	INFO_LOG(WII_IPC_WIIMOTE, "  bd: %02x:%02x:%02x:%02x:%02x:%02x",
		pEventRequestConnection->bdaddr.b[0], pEventRequestConnection->bdaddr.b[1], pEventRequestConnection->bdaddr.b[2],
		pEventRequestConnection->bdaddr.b[3], pEventRequestConnection->bdaddr.b[4], pEventRequestConnection->bdaddr.b[5]);
	DEBUG_LOG(WII_IPC_WIIMOTE, "  COD[0]: 0x%02x", pEventRequestConnection->uclass[0]);
	DEBUG_LOG(WII_IPC_WIIMOTE, "  COD[1]: 0x%02x", pEventRequestConnection->uclass[1]);
	DEBUG_LOG(WII_IPC_WIIMOTE, "  COD[2]: 0x%02x", pEventRequestConnection->uclass[2]);
	DEBUG_LOG(WII_IPC_WIIMOTE, "  LinkType: %s", LinkType[pEventRequestConnection->LinkType]);

	return true;
}

bool CWII_IPC_HLE_Device_usb_oh1_57e_305::SendEventDisconnect(u16 _connectionHandle, u8 _Reason)
{
	CWII_IPC_HLE_WiiMote* pWiiMote = AccessWiiMote(_connectionHandle);
	if (pWiiMote == nullptr)
		return false;

	SQueuedEvent Event(sizeof(SHCIEventDisconnectCompleted), _connectionHandle);

	SHCIEventDisconnectCompleted* pDisconnect = (SHCIEventDisconnectCompleted*)Event.m_buffer;
	pDisconnect->EventType = HCI_EVENT_DISCON_COMPL;
	pDisconnect->PayloadLength = sizeof(SHCIEventDisconnectCompleted) - 2;
	pDisconnect->EventStatus = 0;
	pDisconnect->Connection_Handle = _connectionHandle;
	pDisconnect->Reason = _Reason;

	AddEventToQueue(Event);

	INFO_LOG(WII_IPC_WIIMOTE, "Event: SendEventDisconnect");
	INFO_LOG(WII_IPC_WIIMOTE, "  Connection_Handle: 0x%04x", pDisconnect->Connection_Handle);
	INFO_LOG(WII_IPC_WIIMOTE, "  Reason: 0x%02x", pDisconnect->Reason);

	return true;
}

bool CWII_IPC_HLE_Device_usb_oh1_57e_305::SendEventAuthenticationCompleted(u16 _connectionHandle)
{
	CWII_IPC_HLE_WiiMote* pWiiMote = AccessWiiMote(_connectionHandle);
	if (pWiiMote == nullptr)
		return false;

	SQueuedEvent Event(sizeof(SHCIEventAuthenticationCompleted), _connectionHandle);

	SHCIEventAuthenticationCompleted* pEventAuthenticationCompleted = (SHCIEventAuthenticationCompleted*)Event.m_buffer;
	pEventAuthenticationCompleted->EventType = HCI_EVENT_AUTH_COMPL;
	pEventAuthenticationCompleted->PayloadLength = sizeof(SHCIEventAuthenticationCompleted) - 2;
	pEventAuthenticationCompleted->EventStatus = 0;
	pEventAuthenticationCompleted->Connection_Handle = _connectionHandle;

	INFO_LOG(WII_IPC_WIIMOTE, "Event: SendEventAuthenticationCompleted");
	INFO_LOG(WII_IPC_WIIMOTE, "  Connection_Handle: 0x%04x", pEventAuthenticationCompleted->Connection_Handle);

	AddEventToQueue(Event);

	return true;
}

bool CWII_IPC_HLE_Device_usb_oh1_57e_305::SendEventRemoteNameReq(const bdaddr_t& _bd)
{
	CWII_IPC_HLE_WiiMote* pWiiMote = AccessWiiMote(_bd);
	if (pWiiMote == nullptr)
		return false;

	SQueuedEvent Event(sizeof(SHCIEventRemoteNameReq), 0);

	SHCIEventRemoteNameReq* pRemoteNameReq = (SHCIEventRemoteNameReq*)Event.m_buffer;

	pRemoteNameReq->EventType = HCI_EVENT_REMOTE_NAME_REQ_COMPL;
	pRemoteNameReq->PayloadLength = sizeof(SHCIEventRemoteNameReq) - 2;
	pRemoteNameReq->EventStatus = 0x00;
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
	if (pWiiMote == nullptr)
		return false;

	SQueuedEvent Event(sizeof(SHCIEventReadRemoteFeatures), _connectionHandle);

	SHCIEventReadRemoteFeatures* pReadRemoteFeatures = (SHCIEventReadRemoteFeatures*)Event.m_buffer;

	pReadRemoteFeatures->EventType = HCI_EVENT_READ_REMOTE_FEATURES_COMPL;
	pReadRemoteFeatures->PayloadLength = sizeof(SHCIEventReadRemoteFeatures) - 2;
	pReadRemoteFeatures->EventStatus = 0x00;
	pReadRemoteFeatures->ConnectionHandle = _connectionHandle;
	pReadRemoteFeatures->features[0] = pWiiMote->GetFeatures()[0];
	pReadRemoteFeatures->features[1] = pWiiMote->GetFeatures()[1];
	pReadRemoteFeatures->features[2] = pWiiMote->GetFeatures()[2];
	pReadRemoteFeatures->features[3] = pWiiMote->GetFeatures()[3];
	pReadRemoteFeatures->features[4] = pWiiMote->GetFeatures()[4];
	pReadRemoteFeatures->features[5] = pWiiMote->GetFeatures()[5];
	pReadRemoteFeatures->features[6] = pWiiMote->GetFeatures()[6];
	pReadRemoteFeatures->features[7] = pWiiMote->GetFeatures()[7];

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
	if (pWiiMote == nullptr)
		return false;

	SQueuedEvent Event(sizeof(SHCIEventReadRemoteVerInfo), _connectionHandle);

	SHCIEventReadRemoteVerInfo* pReadRemoteVerInfo = (SHCIEventReadRemoteVerInfo*)Event.m_buffer;
	pReadRemoteVerInfo->EventType = HCI_EVENT_READ_REMOTE_VER_INFO_COMPL;
	pReadRemoteVerInfo->PayloadLength = sizeof(SHCIEventReadRemoteVerInfo) - 2;
	pReadRemoteVerInfo->EventStatus = 0x00;
	pReadRemoteVerInfo->ConnectionHandle = _connectionHandle;
	pReadRemoteVerInfo->lmp_version = pWiiMote->GetLMPVersion();
	pReadRemoteVerInfo->manufacturer = pWiiMote->GetManufactorID();
	pReadRemoteVerInfo->lmp_subversion = pWiiMote->GetLMPSubVersion();

	INFO_LOG(WII_IPC_WIIMOTE, "Event: SendEventReadRemoteVerInfo");
	DEBUG_LOG(WII_IPC_WIIMOTE, "  Connection_Handle: 0x%04x", pReadRemoteVerInfo->ConnectionHandle);
	DEBUG_LOG(WII_IPC_WIIMOTE, "  lmp_version: 0x%02x", pReadRemoteVerInfo->lmp_version);
	DEBUG_LOG(WII_IPC_WIIMOTE, "  manufacturer: 0x%04x", pReadRemoteVerInfo->manufacturer);
	DEBUG_LOG(WII_IPC_WIIMOTE, "  lmp_subversion: 0x%04x", pReadRemoteVerInfo->lmp_subversion);

	AddEventToQueue(Event);

	return true;
}

void CWII_IPC_HLE_Device_usb_oh1_57e_305::SendEventCommandComplete(u16 opcode, const void* data, u32 data_size)
{
	_dbg_assert_(WII_IPC_WIIMOTE, (sizeof(SHCIEventCommand) - 2 + data_size) < 256);

	SQueuedEvent event(sizeof(SHCIEventCommand) + data_size, 0);

	SHCIEventCommand* hci_event = reinterpret_cast<SHCIEventCommand*>(event.m_buffer);
	hci_event->EventType = HCI_EVENT_COMMAND_COMPL;
	hci_event->PayloadLength = (u8)(sizeof(SHCIEventCommand) - 2 + data_size);
	hci_event->PacketIndicator = 0x01;
	hci_event->Opcode = opcode;

	// add the payload
	if (data != nullptr && data_size > 0)
	{
		u8* payload = event.m_buffer + sizeof(SHCIEventCommand);
		memcpy(payload, data, data_size);
	}

	INFO_LOG(WII_IPC_WIIMOTE, "Event: Command Complete (Opcode: 0x%04x)", hci_event->Opcode);

	AddEventToQueue(event);
}

bool CWII_IPC_HLE_Device_usb_oh1_57e_305::SendEventCommandStatus(u16 _Opcode)
{
	SQueuedEvent Event(sizeof(SHCIEventStatus), 0);

	SHCIEventStatus* pHCIEvent = (SHCIEventStatus*)Event.m_buffer;
	pHCIEvent->EventType = HCI_EVENT_COMMAND_STATUS;
	pHCIEvent->PayloadLength = sizeof(SHCIEventStatus) - 2;
	pHCIEvent->EventStatus = 0x0;
	pHCIEvent->PacketIndicator = 0x01;
	pHCIEvent->Opcode = _Opcode;

	INFO_LOG(WII_IPC_WIIMOTE, "Event: Command Status (Opcode: 0x%04x)", pHCIEvent->Opcode);

	AddEventToQueue(Event);

	return true;
}

bool CWII_IPC_HLE_Device_usb_oh1_57e_305::SendEventRoleChange(bdaddr_t _bd, bool _master)
{
	CWII_IPC_HLE_WiiMote* pWiiMote = AccessWiiMote(_bd);
	if (pWiiMote == nullptr)
		return false;

	SQueuedEvent Event(sizeof(SHCIEventRoleChange), 0);

	SHCIEventRoleChange* pRoleChange = (SHCIEventRoleChange*)Event.m_buffer;

	pRoleChange->EventType = HCI_EVENT_ROLE_CHANGE;
	pRoleChange->PayloadLength = sizeof(SHCIEventRoleChange) - 2;
	pRoleChange->EventStatus = 0x00;
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
	SQueuedEvent Event((u32)(sizeof(hci_event_hdr_t) + sizeof(hci_num_compl_pkts_ep) + (sizeof(hci_num_compl_pkts_info) * m_WiiMotes.size())), 0);

	INFO_LOG(WII_IPC_WIIMOTE, "Event: SendEventNumberOfCompletedPackets");

	hci_event_hdr_t* event_hdr    = (hci_event_hdr_t*)Event.m_buffer;
	hci_num_compl_pkts_ep* event  = (hci_num_compl_pkts_ep*)((u8*)event_hdr + sizeof(hci_event_hdr_t));
	hci_num_compl_pkts_info* info = (hci_num_compl_pkts_info*)((u8*)event + sizeof(hci_num_compl_pkts_ep));

	event_hdr->event  = HCI_EVENT_NUM_COMPL_PKTS;
	event_hdr->length = sizeof(hci_num_compl_pkts_ep);
	event->num_con_handles = 0;

	u32 acc = 0;

	for (unsigned int i = 0; i < m_WiiMotes.size(); i++)
	{
		event_hdr->length += sizeof(hci_num_compl_pkts_info);
		event->num_con_handles++;
		info->compl_pkts = m_PacketCount[i];
		info->con_handle = m_WiiMotes[i].GetConnectionHandle();

		DEBUG_LOG(WII_IPC_WIIMOTE, "  Connection_Handle: 0x%04x", info->con_handle);
		DEBUG_LOG(WII_IPC_WIIMOTE, "  Number_Of_Completed_Packets: %i", info->compl_pkts);

		acc += info->compl_pkts;
		m_PacketCount[i] = 0;
		info++;
	}

	if (acc)
	{
		AddEventToQueue(Event);
	}
	else
	{
		INFO_LOG(WII_IPC_WIIMOTE, "SendEventNumberOfCompletedPackets: no packets; no event");
	}

	return true;
}

bool CWII_IPC_HLE_Device_usb_oh1_57e_305::SendEventModeChange(u16 _connectionHandle, u8 _mode, u16 _value)
{
	CWII_IPC_HLE_WiiMote* pWiiMote = AccessWiiMote(_connectionHandle);
	if (pWiiMote == nullptr)
		return false;

	SQueuedEvent Event(sizeof(SHCIEventModeChange), _connectionHandle);

	SHCIEventModeChange* pModeChange = (SHCIEventModeChange*)Event.m_buffer;
	pModeChange->EventType = HCI_EVENT_MODE_CHANGE;
	pModeChange->PayloadLength = sizeof(SHCIEventModeChange) - 2;
	pModeChange->EventStatus = 0;
	pModeChange->Connection_Handle = _connectionHandle;
	pModeChange->CurrentMode = _mode;
	pModeChange->Value = _value;

	INFO_LOG(WII_IPC_WIIMOTE, "Event: SendEventModeChange");
	DEBUG_LOG(WII_IPC_WIIMOTE, "  Connection_Handle: 0x%04x", pModeChange->Connection_Handle);
	DEBUG_LOG(WII_IPC_WIIMOTE, "  Current Mode: 0x%02x", pModeChange->CurrentMode = _mode);

	AddEventToQueue(Event);

	return true;
}

bool CWII_IPC_HLE_Device_usb_oh1_57e_305::SendEventLinkKeyNotification(const u8 num_to_send)
{
	u8 payload_length = sizeof(hci_return_link_keys_ep) + sizeof(hci_link_key_rep_cp) * num_to_send;
	SQueuedEvent Event(2 + payload_length, 0);
	SHCIEventLinkKeyNotification* pEventLinkKey = (SHCIEventLinkKeyNotification*)Event.m_buffer;

	INFO_LOG(WII_IPC_WIIMOTE, "Event: SendEventLinkKeyNotification");

	// event header
	pEventLinkKey->EventType     = HCI_EVENT_RETURN_LINK_KEYS;
	pEventLinkKey->PayloadLength = payload_length;
	// this is really hci_return_link_keys_ep.num_keys
	pEventLinkKey->numKeys = num_to_send;

	// copy infos - this only works correctly if we're meant to start at first device and read all keys
	for (int i = 0; i < num_to_send; i++)
	{
		hci_link_key_rep_cp* link_key_info
			= (hci_link_key_rep_cp*)((u8*)&pEventLinkKey->bdaddr + sizeof(hci_link_key_rep_cp) * i);
		link_key_info->bdaddr = m_WiiMotes[i].GetBD();
		memcpy(link_key_info->key, m_WiiMotes[i].GetLinkKey(), HCI_KEY_SIZE);

		DEBUG_LOG(WII_IPC_WIIMOTE, "  bd: %02x:%02x:%02x:%02x:%02x:%02x",
			link_key_info->bdaddr.b[0], link_key_info->bdaddr.b[1], link_key_info->bdaddr.b[2],
			link_key_info->bdaddr.b[3], link_key_info->bdaddr.b[4], link_key_info->bdaddr.b[5]);
		LOG_LinkKey(link_key_info->key);
	}

	AddEventToQueue(Event);

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
	if (pWiiMote == nullptr)
		return false;

	SQueuedEvent Event(sizeof(SHCIEventReadClockOffsetComplete), _connectionHandle);

	SHCIEventReadClockOffsetComplete* pReadClockOffsetComplete = (SHCIEventReadClockOffsetComplete*)Event.m_buffer;
	pReadClockOffsetComplete->EventType = HCI_EVENT_READ_CLOCK_OFFSET_COMPL;
	pReadClockOffsetComplete->PayloadLength = sizeof(SHCIEventReadClockOffsetComplete) - 2;
	pReadClockOffsetComplete->EventStatus = 0x00;
	pReadClockOffsetComplete->ConnectionHandle = _connectionHandle;
	pReadClockOffsetComplete->ClockOffset = 0x3818;

	INFO_LOG(WII_IPC_WIIMOTE, "Event: SendEventReadClockOffsetComplete");
	DEBUG_LOG(WII_IPC_WIIMOTE, "  Connection_Handle: 0x%04x", pReadClockOffsetComplete->ConnectionHandle);
	DEBUG_LOG(WII_IPC_WIIMOTE, "  ClockOffset: 0x%04x", pReadClockOffsetComplete->ClockOffset);

	AddEventToQueue(Event);

	return true;
}

bool CWII_IPC_HLE_Device_usb_oh1_57e_305::SendEventConPacketTypeChange(u16 _connectionHandle, u16 _packetType)
{
	CWII_IPC_HLE_WiiMote* pWiiMote = AccessWiiMote(_connectionHandle);
	if (pWiiMote == nullptr)
		return false;

	SQueuedEvent Event(sizeof(SHCIEventConPacketTypeChange), _connectionHandle);

	SHCIEventConPacketTypeChange* pChangeConPacketType = (SHCIEventConPacketTypeChange*)Event.m_buffer;
	pChangeConPacketType->EventType = HCI_EVENT_CON_PKT_TYPE_CHANGED;
	pChangeConPacketType->PayloadLength = sizeof(SHCIEventConPacketTypeChange) - 2;
	pChangeConPacketType->EventStatus = 0x00;
	pChangeConPacketType->ConnectionHandle = _connectionHandle;
	pChangeConPacketType->PacketType = _packetType;

	INFO_LOG(WII_IPC_WIIMOTE, "Event: SendEventConPacketTypeChange");
	DEBUG_LOG(WII_IPC_WIIMOTE, "  Connection_Handle: 0x%04x", pChangeConPacketType->ConnectionHandle);
	DEBUG_LOG(WII_IPC_WIIMOTE, "  PacketType: 0x%04x", pChangeConPacketType->PacketType);

	AddEventToQueue(Event);

	return true;
}


// Command dispatcher
// This is called from the USBV0_IOCTL_CTRLMSG Ioctlv
void CWII_IPC_HLE_Device_usb_oh1_57e_305::ExecuteHCICommandMessage(const SHCICommandMessage& _rHCICommandMessage)
{
	u8* pInput = Memory::GetPointer(_rHCICommandMessage.m_PayLoadAddr + 3);
	SCommandMessage* pMsg = (SCommandMessage*)Memory::GetPointer(_rHCICommandMessage.m_PayLoadAddr);

	u16 ocf = HCI_OCF(pMsg->Opcode);
	u16 ogf = HCI_OGF(pMsg->Opcode);

	INFO_LOG(WII_IPC_WIIMOTE, "**************************************************");
	INFO_LOG(WII_IPC_WIIMOTE, "Execute HCI Command: 0x%04x (ocf: 0x%02x, ogf: 0x%02x)", pMsg->Opcode, ocf, ogf);

	switch (pMsg->Opcode)
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

	default:
		// send fake okay msg...
		SendEventCommandComplete(pMsg->Opcode, nullptr, 0);

		if (ogf == HCI_OGF_VENDOR)
		{
			ERROR_LOG(WII_IPC_WIIMOTE, "Command: vendor specific: 0x%04X (ocf: 0x%x)", pMsg->Opcode, ocf);
			for (int i = 0; i < pMsg->len; i++)
			{
				ERROR_LOG(WII_IPC_WIIMOTE, "  0x02%x", pInput[i]);
			}
		}
		else
		{
			_dbg_assert_msg_(WII_IPC_WIIMOTE, 0,
				"Unknown USB_IOCTL_CTRLMSG: 0x%04X (ocf: 0x%x  ogf 0x%x)", pMsg->Opcode, ocf, ogf);
		}
		break;
	}

	// HCI command is finished, send a reply to command
	EnqueueReply(_rHCICommandMessage.m_Address);
}


//
//
// --- command helper
//
//
void CWII_IPC_HLE_Device_usb_oh1_57e_305::CommandInquiry(const u8* input)
{
	// Inquiry should not be called normally
	const hci_inquiry_cp* inquiry = reinterpret_cast<const hci_inquiry_cp*>(input);

	INFO_LOG(WII_IPC_WIIMOTE, "Command: HCI_CMD_INQUIRY:");
	DEBUG_LOG(WII_IPC_WIIMOTE, "write:");
	DEBUG_LOG(WII_IPC_WIIMOTE, "  LAP[0]: 0x%02x", inquiry->lap[0]);
	DEBUG_LOG(WII_IPC_WIIMOTE, "  LAP[1]: 0x%02x", inquiry->lap[1]);
	DEBUG_LOG(WII_IPC_WIIMOTE, "  LAP[2]: 0x%02x", inquiry->lap[2]);
	DEBUG_LOG(WII_IPC_WIIMOTE, "  inquiry_length: %i (N x 1.28) sec", inquiry->inquiry_length);
	DEBUG_LOG(WII_IPC_WIIMOTE, "  num_responses: %i (N x 1.28) sec", inquiry->num_responses);

	SendEventCommandStatus(HCI_CMD_INQUIRY);
	SendEventInquiryResponse();
	SendEventInquiryComplete();
}

void CWII_IPC_HLE_Device_usb_oh1_57e_305::CommandInquiryCancel(const u8* input)
{
	hci_inquiry_cancel_rp reply;
	reply.status = 0x00;

	INFO_LOG(WII_IPC_WIIMOTE, "Command: HCI_CMD_INQUIRY_CANCEL");

	SendEventCommandComplete(HCI_CMD_INQUIRY_CANCEL, &reply, sizeof(hci_inquiry_cancel_rp));
}

void CWII_IPC_HLE_Device_usb_oh1_57e_305::CommandCreateCon(const u8* input)
{
	const hci_create_con_cp* create_connection = reinterpret_cast<const hci_create_con_cp*>(input);

	INFO_LOG(WII_IPC_WIIMOTE, "Command: HCI_CMD_CREATE_CON");
	DEBUG_LOG(WII_IPC_WIIMOTE, "Input:");
	DEBUG_LOG(WII_IPC_WIIMOTE, "  bd: %02x:%02x:%02x:%02x:%02x:%02x",
		create_connection->bdaddr.b[0], create_connection->bdaddr.b[1], create_connection->bdaddr.b[2],
		create_connection->bdaddr.b[3], create_connection->bdaddr.b[4], create_connection->bdaddr.b[5]);

	DEBUG_LOG(WII_IPC_WIIMOTE, "  pkt_type: %i", create_connection->pkt_type);
	DEBUG_LOG(WII_IPC_WIIMOTE, "  page_scan_rep_mode: %i", create_connection->page_scan_rep_mode);
	DEBUG_LOG(WII_IPC_WIIMOTE, "  page_scan_mode: %i", create_connection->page_scan_mode);
	DEBUG_LOG(WII_IPC_WIIMOTE, "  clock_offset: %i", create_connection->clock_offset);
	DEBUG_LOG(WII_IPC_WIIMOTE, "  accept_role_switch: %i", create_connection->accept_role_switch);

	SendEventCommandStatus(HCI_CMD_CREATE_CON);
	SendEventConnectionComplete(create_connection->bdaddr);
}

void CWII_IPC_HLE_Device_usb_oh1_57e_305::CommandDisconnect(const u8* input)
{
	const hci_discon_cp* disconnect = reinterpret_cast<const hci_discon_cp*>(input);

	INFO_LOG(WII_IPC_WIIMOTE, "Command: HCI_CMD_DISCONNECT");
	DEBUG_LOG(WII_IPC_WIIMOTE, "  ConnectionHandle: 0x%04x", disconnect->con_handle);
	DEBUG_LOG(WII_IPC_WIIMOTE, "  Reason: 0x%02x", disconnect->reason);

	Host_SetWiiMoteConnectionState(0);
	DisplayDisconnectMessage((disconnect->con_handle & 0xFF) + 1, disconnect->reason);

	SendEventCommandStatus(HCI_CMD_DISCONNECT);
	SendEventDisconnect(disconnect->con_handle, disconnect->reason);

	CWII_IPC_HLE_WiiMote* wiimote = AccessWiiMote(disconnect->con_handle);
	if (wiimote)
		wiimote->EventDisconnect();
}

void CWII_IPC_HLE_Device_usb_oh1_57e_305::CommandAcceptCon(const u8* input)
{
	const hci_accept_con_cp* accept_connection = reinterpret_cast<const hci_accept_con_cp*>(input);

	static char roles[][128] =
	{
		{ "Master (0x00)"},
		{ "Slave (0x01)"},
	};

	INFO_LOG(WII_IPC_WIIMOTE, "Command: HCI_CMD_ACCEPT_CON");
	DEBUG_LOG(WII_IPC_WIIMOTE, "  bd: %02x:%02x:%02x:%02x:%02x:%02x",
		accept_connection->bdaddr.b[0], accept_connection->bdaddr.b[1], accept_connection->bdaddr.b[2],
		accept_connection->bdaddr.b[3], accept_connection->bdaddr.b[4], accept_connection->bdaddr.b[5]);
	DEBUG_LOG(WII_IPC_WIIMOTE, "  role: %s", roles[accept_connection->role]);

	SendEventCommandStatus(HCI_CMD_ACCEPT_CON);

	// this connection wants to be the master
	if (accept_connection->role == 0)
	{
		SendEventRoleChange(accept_connection->bdaddr, true);
	}

	SendEventConnectionComplete(accept_connection->bdaddr);
}

void CWII_IPC_HLE_Device_usb_oh1_57e_305::CommandLinkKeyRep(const u8* input)
{
	const hci_link_key_rep_cp* key_rep = reinterpret_cast<const hci_link_key_rep_cp*>(input);

	INFO_LOG(WII_IPC_WIIMOTE, "Command: HCI_CMD_LINK_KEY_REP");
	DEBUG_LOG(WII_IPC_WIIMOTE, "  bd: %02x:%02x:%02x:%02x:%02x:%02x",
		key_rep->bdaddr.b[0], key_rep->bdaddr.b[1], key_rep->bdaddr.b[2],
		key_rep->bdaddr.b[3], key_rep->bdaddr.b[4], key_rep->bdaddr.b[5]);
	LOG_LinkKey(key_rep->key);


	hci_link_key_rep_rp reply;
	reply.status = 0x00;
	reply.bdaddr = key_rep->bdaddr;

	SendEventCommandComplete(HCI_CMD_LINK_KEY_REP, &reply, sizeof(hci_link_key_rep_rp));
}

void CWII_IPC_HLE_Device_usb_oh1_57e_305::CommandLinkKeyNegRep(const u8* input)
{
	const hci_link_key_neg_rep_cp* key_neg = reinterpret_cast<const hci_link_key_neg_rep_cp*>(input);

	INFO_LOG(WII_IPC_WIIMOTE, "Command: HCI_CMD_LINK_KEY_NEG_REP");
	DEBUG_LOG(WII_IPC_WIIMOTE, "  bd: %02x:%02x:%02x:%02x:%02x:%02x",
		key_neg->bdaddr.b[0], key_neg->bdaddr.b[1], key_neg->bdaddr.b[2],
		key_neg->bdaddr.b[3], key_neg->bdaddr.b[4], key_neg->bdaddr.b[5]);

	hci_link_key_neg_rep_rp reply;
	reply.status = 0x00;
	reply.bdaddr = key_neg->bdaddr;

	SendEventCommandComplete(HCI_CMD_LINK_KEY_NEG_REP, &reply, sizeof(hci_link_key_neg_rep_rp));
}

void CWII_IPC_HLE_Device_usb_oh1_57e_305::CommandChangeConPacketType(const u8* input)
{
	const hci_change_con_pkt_type_cp* change_packet_type = reinterpret_cast<const hci_change_con_pkt_type_cp*>(input);

	// ntd stack sets packet type 0xcc18, which is HCI_PKT_DH5 | HCI_PKT_DM5 | HCI_PKT_DH1 | HCI_PKT_DM1
	// dunno what to do...run awayyyyyy!
	INFO_LOG(WII_IPC_WIIMOTE, "Command: HCI_CMD_CHANGE_CON_PACKET_TYPE");
	DEBUG_LOG(WII_IPC_WIIMOTE, "  ConnectionHandle: 0x%04x", change_packet_type->con_handle);
	DEBUG_LOG(WII_IPC_WIIMOTE, "  PacketType: 0x%04x", change_packet_type->pkt_type);

	SendEventCommandStatus(HCI_CMD_CHANGE_CON_PACKET_TYPE);
	SendEventConPacketTypeChange(change_packet_type->con_handle, change_packet_type->pkt_type);
}

void CWII_IPC_HLE_Device_usb_oh1_57e_305::CommandAuthenticationRequested(const u8* input)
{
	const hci_auth_req_cp* auth_req = reinterpret_cast<const hci_auth_req_cp*>(input);

	INFO_LOG(WII_IPC_WIIMOTE, "Command: HCI_CMD_AUTH_REQ");
	DEBUG_LOG(WII_IPC_WIIMOTE, "  ConnectionHandle: 0x%04x", auth_req->con_handle);

	SendEventCommandStatus(HCI_CMD_AUTH_REQ);
	SendEventAuthenticationCompleted(auth_req->con_handle);
}

void CWII_IPC_HLE_Device_usb_oh1_57e_305::CommandRemoteNameReq(const u8* input)
{
	const hci_remote_name_req_cp* remote_name_req = reinterpret_cast<const hci_remote_name_req_cp*>(input);

	INFO_LOG(WII_IPC_WIIMOTE, "Command: HCI_CMD_REMOTE_NAME_REQ");
	DEBUG_LOG(WII_IPC_WIIMOTE, "  bd: %02x:%02x:%02x:%02x:%02x:%02x",
		remote_name_req->bdaddr.b[0], remote_name_req->bdaddr.b[1], remote_name_req->bdaddr.b[2],
		remote_name_req->bdaddr.b[3], remote_name_req->bdaddr.b[4], remote_name_req->bdaddr.b[5]);
	DEBUG_LOG(WII_IPC_WIIMOTE, "  page_scan_rep_mode: %i", remote_name_req->page_scan_rep_mode);
	DEBUG_LOG(WII_IPC_WIIMOTE, "  page_scan_mode: %i", remote_name_req->page_scan_mode);
	DEBUG_LOG(WII_IPC_WIIMOTE, "  clock_offset: %i", remote_name_req->clock_offset);

	SendEventCommandStatus(HCI_CMD_REMOTE_NAME_REQ);
	SendEventRemoteNameReq(remote_name_req->bdaddr);
}

void CWII_IPC_HLE_Device_usb_oh1_57e_305::CommandReadRemoteFeatures(const u8* input)
{
	const hci_read_remote_features_cp* read_remote_features = reinterpret_cast<const hci_read_remote_features_cp*>(input);

	INFO_LOG(WII_IPC_WIIMOTE, "Command: HCI_CMD_READ_REMOTE_FEATURES");
	DEBUG_LOG(WII_IPC_WIIMOTE, "  ConnectionHandle: 0x%04x", read_remote_features->con_handle);

	SendEventCommandStatus(HCI_CMD_READ_REMOTE_FEATURES);
	SendEventReadRemoteFeatures(read_remote_features->con_handle);
}

void CWII_IPC_HLE_Device_usb_oh1_57e_305::CommandReadRemoteVerInfo(const u8* input)
{
	const hci_read_remote_ver_info_cp* read_remote_ver_info = reinterpret_cast<const hci_read_remote_ver_info_cp*>(input);

	INFO_LOG(WII_IPC_WIIMOTE, "Command: HCI_CMD_READ_REMOTE_VER_INFO");
	DEBUG_LOG(WII_IPC_WIIMOTE, "  ConnectionHandle: 0x%02x", read_remote_ver_info->con_handle);

	SendEventCommandStatus(HCI_CMD_READ_REMOTE_VER_INFO);
	SendEventReadRemoteVerInfo(read_remote_ver_info->con_handle);
}

void CWII_IPC_HLE_Device_usb_oh1_57e_305::CommandReadClockOffset(const u8* input)
{
	const hci_read_clock_offset_cp* read_clock_offset = reinterpret_cast<const hci_read_clock_offset_cp*>(input);

	INFO_LOG(WII_IPC_WIIMOTE, "Command: HCI_CMD_READ_CLOCK_OFFSET");
	DEBUG_LOG(WII_IPC_WIIMOTE, "  ConnectionHandle: 0x%02x", read_clock_offset->con_handle);

	SendEventCommandStatus(HCI_CMD_READ_CLOCK_OFFSET);
	SendEventReadClockOffsetComplete(read_clock_offset->con_handle);
}

void CWII_IPC_HLE_Device_usb_oh1_57e_305::CommandSniffMode(const u8* input)
{
	const hci_sniff_mode_cp* sniff_mode = reinterpret_cast<const hci_sniff_mode_cp*>(input);

	INFO_LOG(WII_IPC_WIIMOTE, "Command: HCI_CMD_SNIFF_MODE");
	INFO_LOG(WII_IPC_WIIMOTE, "  ConnectionHandle: 0x%04x", sniff_mode->con_handle);
	DEBUG_LOG(WII_IPC_WIIMOTE, "  max_interval: %f msec", sniff_mode->max_interval * .625);
	DEBUG_LOG(WII_IPC_WIIMOTE, "  min_interval: %f msec", sniff_mode->min_interval * .625);
	DEBUG_LOG(WII_IPC_WIIMOTE, "  attempt: %f msec", sniff_mode->attempt * 1.25);
	DEBUG_LOG(WII_IPC_WIIMOTE, "  timeout: %f msec", sniff_mode->timeout * 1.25);

	SendEventCommandStatus(HCI_CMD_SNIFF_MODE);
	SendEventModeChange(sniff_mode->con_handle, 0x02, sniff_mode->max_interval);  // 0x02 - sniff mode
}

void CWII_IPC_HLE_Device_usb_oh1_57e_305::CommandWriteLinkPolicy(const u8* input)
{
	const hci_write_link_policy_settings_cp* link_policy = reinterpret_cast<const hci_write_link_policy_settings_cp*>(input);

	INFO_LOG(WII_IPC_WIIMOTE, "Command: HCI_CMD_WRITE_LINK_POLICY_SETTINGS");
	DEBUG_LOG(WII_IPC_WIIMOTE, "  ConnectionHandle: 0x%04x", link_policy->con_handle);
	DEBUG_LOG(WII_IPC_WIIMOTE, "  Policy: 0x%04x", link_policy->settings);

	SendEventCommandStatus(HCI_CMD_WRITE_LINK_POLICY_SETTINGS);
}

void CWII_IPC_HLE_Device_usb_oh1_57e_305::CommandReset(const u8* input)
{
	hci_status_rp reply;
	reply.status = 0x00;

	INFO_LOG(WII_IPC_WIIMOTE, "Command: HCI_CMD_RESET");

	SendEventCommandComplete(HCI_CMD_RESET, &reply, sizeof(hci_status_rp));
}

void CWII_IPC_HLE_Device_usb_oh1_57e_305::CommandSetEventFilter(const u8* input)
{
	const hci_set_event_filter_cp* set_event_filter = reinterpret_cast<const hci_set_event_filter_cp*>(input);

	hci_set_event_filter_rp reply;
	reply.status = 0x00;

	INFO_LOG(WII_IPC_WIIMOTE, "Command: HCI_CMD_SET_EVENT_FILTER:");
	DEBUG_LOG(WII_IPC_WIIMOTE, "  filter_type: %i", set_event_filter->filter_type);
	DEBUG_LOG(WII_IPC_WIIMOTE, "  filter_condition_type: %i", set_event_filter->filter_condition_type);

	SendEventCommandComplete(HCI_CMD_SET_EVENT_FILTER, &reply, sizeof(hci_set_event_filter_rp));
}

void CWII_IPC_HLE_Device_usb_oh1_57e_305::CommandWritePinType(const u8* input)
{
	const hci_write_pin_type_cp* write_pin_type = reinterpret_cast<const hci_write_pin_type_cp*>(input);

	hci_write_pin_type_rp reply;
	reply.status = 0x00;

	INFO_LOG(WII_IPC_WIIMOTE, "Command: HCI_CMD_WRITE_PIN_TYPE:");
	DEBUG_LOG(WII_IPC_WIIMOTE, "  pin_type: %x", write_pin_type->pin_type);

	SendEventCommandComplete(HCI_CMD_WRITE_PIN_TYPE, &reply, sizeof(hci_write_pin_type_rp));
}

void CWII_IPC_HLE_Device_usb_oh1_57e_305::CommandReadStoredLinkKey(const u8* input)
{
	const hci_read_stored_link_key_cp* read_stored_link_key = reinterpret_cast<const hci_read_stored_link_key_cp*>(input);

	hci_read_stored_link_key_rp reply;
	reply.status = 0x00;
	reply.num_keys_read = 0;
	reply.max_num_keys = 255;

	if (read_stored_link_key->read_all == 1)
	{
		reply.num_keys_read = (u16)m_WiiMotes.size();
	}
	else
	{
		ERROR_LOG(WII_IPC_WIIMOTE, "CommandReadStoredLinkKey isn't looking for all devices");
	}

	INFO_LOG(WII_IPC_WIIMOTE, "Command: HCI_CMD_READ_STORED_LINK_KEY:");
	DEBUG_LOG(WII_IPC_WIIMOTE, "input:");
	DEBUG_LOG(WII_IPC_WIIMOTE, "  bd: %02x:%02x:%02x:%02x:%02x:%02x",
		read_stored_link_key->bdaddr.b[0], read_stored_link_key->bdaddr.b[1], read_stored_link_key->bdaddr.b[2],
		read_stored_link_key->bdaddr.b[3], read_stored_link_key->bdaddr.b[4], read_stored_link_key->bdaddr.b[5]);
	DEBUG_LOG(WII_IPC_WIIMOTE, "  read_all: %i", read_stored_link_key->read_all);
	DEBUG_LOG(WII_IPC_WIIMOTE, "return:");
	DEBUG_LOG(WII_IPC_WIIMOTE, "  max_num_keys: %i", reply.max_num_keys);
	DEBUG_LOG(WII_IPC_WIIMOTE, "  num_keys_read: %i", reply.num_keys_read);

	SendEventLinkKeyNotification((u8)reply.num_keys_read);
	SendEventCommandComplete(HCI_CMD_READ_STORED_LINK_KEY, &reply, sizeof(hci_read_stored_link_key_rp));
}

void CWII_IPC_HLE_Device_usb_oh1_57e_305::CommandDeleteStoredLinkKey(const u8* input)
{
	const hci_delete_stored_link_key_cp* delete_stored_link_key = reinterpret_cast<const hci_delete_stored_link_key_cp*>(input);

	INFO_LOG(WII_IPC_WIIMOTE, "Command: HCI_OCF_DELETE_STORED_LINK_KEY");
	DEBUG_LOG(WII_IPC_WIIMOTE, "  bd: %02x:%02x:%02x:%02x:%02x:%02x",
		delete_stored_link_key->bdaddr.b[0], delete_stored_link_key->bdaddr.b[1], delete_stored_link_key->bdaddr.b[2],
		delete_stored_link_key->bdaddr.b[3], delete_stored_link_key->bdaddr.b[4], delete_stored_link_key->bdaddr.b[5]);
	DEBUG_LOG(WII_IPC_WIIMOTE, "  delete_all: 0x%01x", delete_stored_link_key->delete_all);


	CWII_IPC_HLE_WiiMote* wiiMote = AccessWiiMote(delete_stored_link_key->bdaddr);
	if (wiiMote == nullptr)
		return;

	hci_delete_stored_link_key_rp reply;
	reply.status = 0x00;
	reply.num_keys_deleted = 0;

	SendEventCommandComplete(HCI_CMD_DELETE_STORED_LINK_KEY, &reply, sizeof(hci_delete_stored_link_key_rp));

	ERROR_LOG(WII_IPC_WIIMOTE, "HCI: CommandDeleteStoredLinkKey... Probably the security for linking has failed. Could be a problem with loading the SCONF");
}

void CWII_IPC_HLE_Device_usb_oh1_57e_305::CommandWriteLocalName(const u8* input)
{
	const hci_write_local_name_cp* write_local_name = reinterpret_cast<const hci_write_local_name_cp*>(input);

	hci_write_local_name_rp reply;
	reply.status = 0x00;

	INFO_LOG(WII_IPC_WIIMOTE, "Command: HCI_CMD_WRITE_LOCAL_NAME:");
	DEBUG_LOG(WII_IPC_WIIMOTE, "  local_name: %s", write_local_name->name);

	SendEventCommandComplete(HCI_CMD_WRITE_LOCAL_NAME, &reply, sizeof(hci_write_local_name_rp));
}

// Here we normally receive the timeout interval.
// But not from homebrew games that use lwbt. Why not?
void CWII_IPC_HLE_Device_usb_oh1_57e_305::CommandWritePageTimeOut(const u8* input)
{
	const hci_write_page_timeout_cp* write_page_timeout = reinterpret_cast<const hci_write_page_timeout_cp*>(input);

	hci_host_buffer_size_rp reply;
	reply.status = 0x00;

	INFO_LOG(WII_IPC_WIIMOTE, "Command: HCI_CMD_WRITE_PAGE_TIMEOUT:");
	DEBUG_LOG(WII_IPC_WIIMOTE, "  timeout: %i", write_page_timeout->timeout);

	SendEventCommandComplete(HCI_CMD_WRITE_PAGE_TIMEOUT, &reply, sizeof(hci_host_buffer_size_rp));
}

/* This will enable ScanEnable so that Update() can start the Wiimote. */
void CWII_IPC_HLE_Device_usb_oh1_57e_305::CommandWriteScanEnable(const u8* input)
{
	const hci_write_scan_enable_cp* write_scan_enable = reinterpret_cast<const hci_write_scan_enable_cp*>(input);
	m_ScanEnable = write_scan_enable->scan_enable;

	hci_write_scan_enable_rp reply;
	reply.status = 0x00;

	static char scanning[][128] =
	{
		{ "HCI_NO_SCAN_ENABLE"},
		{ "HCI_INQUIRY_SCAN_ENABLE"},
		{ "HCI_PAGE_SCAN_ENABLE"},
		{ "HCI_INQUIRY_AND_PAGE_SCAN_ENABLE"},
	};

	INFO_LOG(WII_IPC_WIIMOTE, "Command: HCI_CMD_WRITE_SCAN_ENABLE: (0x%02x)", write_scan_enable->scan_enable);
	DEBUG_LOG(WII_IPC_WIIMOTE, "  scan_enable: %s", scanning[write_scan_enable->scan_enable]);

	SendEventCommandComplete(HCI_CMD_WRITE_SCAN_ENABLE, &reply, sizeof(hci_write_scan_enable_rp));
}

void CWII_IPC_HLE_Device_usb_oh1_57e_305::CommandWriteUnitClass(const u8* input)
{
	const hci_write_unit_class_cp* write_unit_class = reinterpret_cast<const hci_write_unit_class_cp*>(input);

	hci_write_unit_class_rp reply;
	reply.status = 0x00;

	INFO_LOG(WII_IPC_WIIMOTE, "Command: HCI_CMD_WRITE_UNIT_CLASS:");
	DEBUG_LOG(WII_IPC_WIIMOTE, "  COD[0]: 0x%02x", write_unit_class->uclass[0]);
	DEBUG_LOG(WII_IPC_WIIMOTE, "  COD[1]: 0x%02x", write_unit_class->uclass[1]);
	DEBUG_LOG(WII_IPC_WIIMOTE, "  COD[2]: 0x%02x", write_unit_class->uclass[2]);

	SendEventCommandComplete(HCI_CMD_WRITE_UNIT_CLASS, &reply, sizeof(hci_write_unit_class_rp));
}

void CWII_IPC_HLE_Device_usb_oh1_57e_305::CommandHostBufferSize(const u8* input)
{
	const hci_host_buffer_size_cp* host_buffer_size = reinterpret_cast<const hci_host_buffer_size_cp*>(input);

	hci_host_buffer_size_rp reply;
	reply.status = 0x00;

	INFO_LOG(WII_IPC_WIIMOTE, "Command: HCI_CMD_HOST_BUFFER_SIZE:");
	DEBUG_LOG(WII_IPC_WIIMOTE, "  max_acl_size: %i", host_buffer_size->max_acl_size);
	DEBUG_LOG(WII_IPC_WIIMOTE, "  max_sco_size: %i", host_buffer_size->max_sco_size);
	DEBUG_LOG(WII_IPC_WIIMOTE, "  num_acl_pkts: %i", host_buffer_size->num_acl_pkts);
	DEBUG_LOG(WII_IPC_WIIMOTE, "  num_sco_pkts: %i", host_buffer_size->num_sco_pkts);

	SendEventCommandComplete(HCI_CMD_HOST_BUFFER_SIZE, &reply, sizeof(hci_host_buffer_size_rp));
}

void CWII_IPC_HLE_Device_usb_oh1_57e_305::CommandWriteLinkSupervisionTimeout(const u8* input)
{
	const hci_write_link_supervision_timeout_cp* supervision = reinterpret_cast<const hci_write_link_supervision_timeout_cp*>(input);

	// timeout of 0 means timing out is disabled
	INFO_LOG(WII_IPC_WIIMOTE, "Command: HCI_CMD_WRITE_LINK_SUPERVISION_TIMEOUT");
	DEBUG_LOG(WII_IPC_WIIMOTE, "  con_handle: 0x%04x", supervision->con_handle);
	DEBUG_LOG(WII_IPC_WIIMOTE, "  timeout: 0x%02x", supervision->timeout);

	hci_write_link_supervision_timeout_rp reply;
	reply.status = 0x00;
	reply.con_handle = supervision->con_handle;

	SendEventCommandComplete(HCI_CMD_WRITE_LINK_SUPERVISION_TIMEOUT, &reply, sizeof(hci_write_link_supervision_timeout_rp));
}

void CWII_IPC_HLE_Device_usb_oh1_57e_305::CommandWriteInquiryScanType(const u8* input)
{
	const hci_write_inquiry_scan_type_cp* set_event_filter = reinterpret_cast<const hci_write_inquiry_scan_type_cp*>(input);

	hci_write_inquiry_scan_type_rp reply;
	reply.status = 0x00;

	INFO_LOG(WII_IPC_WIIMOTE, "Command: HCI_CMD_WRITE_INQUIRY_SCAN_TYPE:");
	DEBUG_LOG(WII_IPC_WIIMOTE, "  type: %i", set_event_filter->type);

	SendEventCommandComplete(HCI_CMD_WRITE_INQUIRY_SCAN_TYPE, &reply, sizeof(hci_write_inquiry_scan_type_rp));
}

void CWII_IPC_HLE_Device_usb_oh1_57e_305::CommandWriteInquiryMode(const u8* input)
{
	const hci_write_inquiry_mode_cp* inquiry_mode = reinterpret_cast<const hci_write_inquiry_mode_cp*>(input);

	hci_write_inquiry_mode_rp reply;
	reply.status = 0x00;

	static char inquiry_mode_tag[][128] =
	{
		{ "Standard Inquiry Result event format (default)" },
		{ "Inquiry Result format with RSSI" },
		{ "Inquiry Result with RSSI format or Extended Inquiry Result format" }
	};
	INFO_LOG(WII_IPC_WIIMOTE, "Command: HCI_CMD_WRITE_INQUIRY_MODE:");
	DEBUG_LOG(WII_IPC_WIIMOTE, "  mode: %s", inquiry_mode_tag[inquiry_mode->mode]);

	SendEventCommandComplete(HCI_CMD_WRITE_INQUIRY_MODE, &reply, sizeof(hci_write_inquiry_mode_rp));
}

void CWII_IPC_HLE_Device_usb_oh1_57e_305::CommandWritePageScanType(const u8* input)
{
	const hci_write_page_scan_type_cp* write_page_scan_type = reinterpret_cast<const hci_write_page_scan_type_cp*>(input);

	hci_write_page_scan_type_rp reply;
	reply.status = 0x00;

	static char page_scan_type[][128] =
	{
		{ "Mandatory: Standard Scan (default)" },
		{ "Optional: Interlaced Scan" }
	};

	INFO_LOG(WII_IPC_WIIMOTE, "Command: HCI_CMD_WRITE_PAGE_SCAN_TYPE:");
	DEBUG_LOG(WII_IPC_WIIMOTE, "  type: %s", page_scan_type[write_page_scan_type->type]);

	SendEventCommandComplete(HCI_CMD_WRITE_PAGE_SCAN_TYPE, &reply, sizeof(hci_write_page_scan_type_rp));
}

void CWII_IPC_HLE_Device_usb_oh1_57e_305::CommandReadLocalVer(const u8* input)
{
	hci_read_local_ver_rp reply;
	reply.status = 0x00;
	reply.hci_version = 0x03;        // HCI version: 1.1
	reply.hci_revision = 0x40a7;     // current revision (?)
	reply.lmp_version = 0x03;        // LMP version: 1.1
	reply.manufacturer = 0x000F;     // manufacturer: reserved for tests
	reply.lmp_subversion = 0x430e;   // LMP subversion

	INFO_LOG(WII_IPC_WIIMOTE, "Command: HCI_CMD_READ_LOCAL_VER:");
	DEBUG_LOG(WII_IPC_WIIMOTE, "return:");
	DEBUG_LOG(WII_IPC_WIIMOTE, "  status:         %i", reply.status);
	DEBUG_LOG(WII_IPC_WIIMOTE, "  hci_revision:   %i", reply.hci_revision);
	DEBUG_LOG(WII_IPC_WIIMOTE, "  lmp_version:    %i", reply.lmp_version);
	DEBUG_LOG(WII_IPC_WIIMOTE, "  manufacturer:   %i", reply.manufacturer);
	DEBUG_LOG(WII_IPC_WIIMOTE, "  lmp_subversion: %i", reply.lmp_subversion);

	SendEventCommandComplete(HCI_CMD_READ_LOCAL_VER, &reply, sizeof(hci_read_local_ver_rp));
}

void CWII_IPC_HLE_Device_usb_oh1_57e_305::CommandReadLocalFeatures(const u8* input)
{
	hci_read_local_features_rp reply;
	reply.status = 0x00;
	reply.features[0] = 0xFF;
	reply.features[1] = 0xFF;
	reply.features[2] = 0x8D;
	reply.features[3] = 0xFE;
	reply.features[4] = 0x9B;
	reply.features[5] = 0xF9;
	reply.features[6] = 0x00;
	reply.features[7] = 0x80;

	INFO_LOG(WII_IPC_WIIMOTE, "Command: HCI_CMD_READ_LOCAL_FEATURES:");
	DEBUG_LOG(WII_IPC_WIIMOTE, "return:");
	DEBUG_LOG(WII_IPC_WIIMOTE, "  features: %02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x",
		reply.features[0], reply.features[1], reply.features[2],
		reply.features[3], reply.features[4], reply.features[5],
		reply.features[6], reply.features[7]);

	SendEventCommandComplete(HCI_CMD_READ_LOCAL_FEATURES, &reply, sizeof(hci_read_local_features_rp));
}

void CWII_IPC_HLE_Device_usb_oh1_57e_305::CommandReadBufferSize(const u8* input)
{
	hci_read_buffer_size_rp reply;
	reply.status = 0x00;
	reply.max_acl_size = m_acl_pkt_size;
	// Due to how the widcomm stack which Nintendo uses is coded, we must never
	// let the stack think the controller is buffering more than 10 data packets
	// - it will cause a u8 underflow and royally screw things up.
	reply.num_acl_pkts = m_acl_pkts_num;
	reply.max_sco_size = 64;
	reply.num_sco_pkts = 0;

	INFO_LOG(WII_IPC_WIIMOTE, "Command: HCI_CMD_READ_BUFFER_SIZE:");
	DEBUG_LOG(WII_IPC_WIIMOTE, "return:");
	DEBUG_LOG(WII_IPC_WIIMOTE, "  max_acl_size: %i", reply.max_acl_size);
	DEBUG_LOG(WII_IPC_WIIMOTE, "  num_acl_pkts: %i", reply.num_acl_pkts);
	DEBUG_LOG(WII_IPC_WIIMOTE, "  max_sco_size: %i", reply.max_sco_size);
	DEBUG_LOG(WII_IPC_WIIMOTE, "  num_sco_pkts: %i", reply.num_sco_pkts);

	SendEventCommandComplete(HCI_CMD_READ_BUFFER_SIZE, &reply, sizeof(hci_read_buffer_size_rp));
}

void CWII_IPC_HLE_Device_usb_oh1_57e_305::CommandReadBDAdrr(const u8* input)
{
	hci_read_bdaddr_rp reply;
	reply.status = 0x00;
	reply.bdaddr = m_ControllerBD;

	INFO_LOG(WII_IPC_WIIMOTE, "Command: HCI_CMD_READ_BDADDR:");
	DEBUG_LOG(WII_IPC_WIIMOTE, "return:");
	DEBUG_LOG(WII_IPC_WIIMOTE, "  bd: %02x:%02x:%02x:%02x:%02x:%02x",
		reply.bdaddr.b[0], reply.bdaddr.b[1], reply.bdaddr.b[2],
		reply.bdaddr.b[3], reply.bdaddr.b[4], reply.bdaddr.b[5]);

	SendEventCommandComplete(HCI_CMD_READ_BDADDR, &reply, sizeof(hci_read_bdaddr_rp));
}

void CWII_IPC_HLE_Device_usb_oh1_57e_305::CommandVendorSpecific_FC4F(const u8* input, u32 size)
{
	// callstack...
	// BTM_VendorSpecificCommad()
	// WUDiRemovePatch()
	// WUDiAppendRuntimePatch()
	// WUDiGetFirmwareVersion()
	// WUDiStackSetupComplete()

	hci_status_rp reply;
	reply.status = 0x00;

	INFO_LOG(WII_IPC_WIIMOTE, "Command: CommandVendorSpecific_FC4F: (callstack WUDiRemovePatch)");
	INFO_LOG(WII_IPC_WIIMOTE, "Input (size 0x%x):", size);

	Dolphin_Debugger::PrintDataBuffer(LogTypes::WII_IPC_WIIMOTE, input, size, "Data: ");

	SendEventCommandComplete(0xFC4F, &reply, sizeof(hci_status_rp));
}

void CWII_IPC_HLE_Device_usb_oh1_57e_305::CommandVendorSpecific_FC4C(const u8* input, u32 size)
{
	hci_status_rp reply;
	reply.status = 0x00;

	INFO_LOG(WII_IPC_WIIMOTE, "Command: CommandVendorSpecific_FC4C:");
	INFO_LOG(WII_IPC_WIIMOTE, "Input (size 0x%x):", size);
	Dolphin_Debugger::PrintDataBuffer(LogTypes::WII_IPC_WIIMOTE, input, size, "Data: ");

	SendEventCommandComplete(0xFC4C, &reply, sizeof(hci_status_rp));
}


//
//
// --- helper
//
//
CWII_IPC_HLE_WiiMote* CWII_IPC_HLE_Device_usb_oh1_57e_305::AccessWiiMote(const bdaddr_t& _rAddr)
{
	for (auto& wiimote : m_WiiMotes)
	{
		const bdaddr_t& BD = wiimote.GetBD();
		if ((_rAddr.b[0] == BD.b[0]) &&
			(_rAddr.b[1] == BD.b[1]) &&
			(_rAddr.b[2] == BD.b[2]) &&
			(_rAddr.b[3] == BD.b[3]) &&
			(_rAddr.b[4] == BD.b[4]) &&
			(_rAddr.b[5] == BD.b[5]))
			return &wiimote;
	}

	ERROR_LOG(WII_IPC_WIIMOTE,"Can't find WiiMote by bd: %02x:%02x:%02x:%02x:%02x:%02x",
		_rAddr.b[0], _rAddr.b[1], _rAddr.b[2], _rAddr.b[3], _rAddr.b[4], _rAddr.b[5]);
	return nullptr;
}

CWII_IPC_HLE_WiiMote* CWII_IPC_HLE_Device_usb_oh1_57e_305::AccessWiiMote(u16 _ConnectionHandle)
{
	for (auto& wiimote : m_WiiMotes)
	{
		if (wiimote.GetConnectionHandle() == _ConnectionHandle)
			return &wiimote;
	}

	ERROR_LOG(WII_IPC_WIIMOTE, "Can't find Wiimote by connection handle %02x", _ConnectionHandle);
	PanicAlertT("Can't find Wiimote by connection handle %02x", _ConnectionHandle);
	return nullptr;
}

void CWII_IPC_HLE_Device_usb_oh1_57e_305::DisplayDisconnectMessage(const int wiimoteNumber, const int reason)
{
	// TODO: If someone wants to be fancy we could also figure out what the values for pDiscon->reason mean
	// and display things like "Wiimote %i disconnected due to inactivity!" etc.
	Core::DisplayMessage(StringFromFormat("Wiimote %i disconnected by emulated software", wiimoteNumber), 3000);
}

void CWII_IPC_HLE_Device_usb_oh1_57e_305::LOG_LinkKey(const u8* _pLinkKey)
{
	DEBUG_LOG(WII_IPC_WIIMOTE, "  link key: "
				"0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x "
				"0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x "
				, _pLinkKey[0], _pLinkKey[1], _pLinkKey[2], _pLinkKey[3], _pLinkKey[4], _pLinkKey[5], _pLinkKey[6], _pLinkKey[7]
				, _pLinkKey[8], _pLinkKey[9], _pLinkKey[10], _pLinkKey[11], _pLinkKey[12], _pLinkKey[13], _pLinkKey[14], _pLinkKey[15]);

}
