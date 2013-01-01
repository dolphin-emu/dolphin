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

#pragma once

#include "hci.h"
#include <vector>
#include <queue>
#include "WII_IPC_HLE.h"
#include "WII_IPC_HLE_Device.h"
#include "WII_IPC_HLE_WiiMote.h"

struct SQueuedEvent
{
	u8 m_buffer[1024];
	u32 m_size;
	u16 m_connectionHandle;

	SQueuedEvent(u32 size, u16 connectionHandle)
		: m_size(size)
		, m_connectionHandle(connectionHandle)
	{
		if (m_size > 1024)
		{
			// i know this code sux...
			PanicAlert("SQueuedEvent: allocate too big buffer!!");
		}
		memset(m_buffer, 0, 1024);
	}

	SQueuedEvent()
		: m_size(0)
		, m_connectionHandle(0)
	{
	}
};


// Important to remember that this class is for /dev/usb/oh1/57e/305 ONLY
// /dev/usb/oh1 -> internal usb bus
// 57e/305 -> VendorID/ProductID of device on usb bus
// This device is ONLY the internal bluetooth module (based on BCM2045 chip)
class CWII_IPC_HLE_Device_usb_oh1_57e_305 : public IWII_IPC_HLE_Device
{
public:
	CWII_IPC_HLE_Device_usb_oh1_57e_305(u32 _DeviceID, const std::string& _rDeviceName);

	virtual ~CWII_IPC_HLE_Device_usb_oh1_57e_305();

	virtual bool Open(u32 _CommandAddress, u32 _Mode);
	virtual bool Close(u32 _CommandAddress, bool _bForce);

	virtual bool IOCtlV(u32 _CommandAddress);
	virtual bool IOCtl(u32 _CommandAddress);

	virtual u32 Update();

	// Send ACL data back to bt stack
	void SendACLPacket(u16 _ConnectionHandle, u8* _pData, u32 _Size);

	bool RemoteDisconnect(u16 _connectionHandle);

// hack for wiimote plugin
public:
	std::vector<CWII_IPC_HLE_WiiMote> m_WiiMotes;
	CWII_IPC_HLE_WiiMote* AccessWiiMote(const bdaddr_t& _rAddr);
	CWII_IPC_HLE_WiiMote* AccessWiiMote(u16 _ConnectionHandle);

	void DoState(PointerWrap &p);

	void NetPlay_WiimoteUpdate(int _number);

private:
	enum USBIOCtl
	{
		USBV0_IOCTL_CTRLMSG		= 0,
		USBV0_IOCTL_BLKMSG		= 1,
		USBV0_IOCTL_INTRMSG		= 2,
	};

	enum USBEndpoint
	{
		HCI_CTRL		= 0x00,	
		HCI_EVENT		= 0x81,
		ACL_DATA_IN		= 0x82,
		ACL_DATA_OUT	= 0x02
	};

	struct SHCICommandMessage
	{
		u8  bRequestType;
		u8  bRequest;
		u16 wValue;
		u16 wIndex;
		u16 wLength;

		u32 m_PayLoadAddr;
		u32 m_PayLoadSize;
		u32 m_Address;
	};

	// This is a lightweight/specialized version of SIOCtlVBuffer
	struct CtrlBuffer
	{
		u32 m_address;
		u32 m_buffer;

		CtrlBuffer(u32 _Address) : m_address(_Address), m_buffer()
		{
			if (m_address)
			{
				u32 InBufferNum		= Memory::Read_U32(m_address + 0x10);
				u32 BufferVector	= Memory::Read_U32(m_address + 0x18);
				m_buffer = Memory::Read_U32(
					BufferVector + InBufferNum * sizeof(SIOCtlVBuffer::SBuffer));
			}
		}

		inline void FillBuffer(const void* src, const size_t size) const
		{
			memcpy(Memory::GetPointer(m_buffer), src, size);
		}

		inline void SetRetVal(const u32 retval) const
		{
			Memory::Write_U32(retval, m_address + 4);
		}

		inline bool IsValid() const
		{
			return m_address != 0;
		}

		inline void Invalidate()
		{
			m_address = m_buffer = 0;
		}
	};

	bdaddr_t m_ControllerBD;

	// this is used to trigger connecting via ACL
	u8 m_ScanEnable;

	SHCICommandMessage m_CtrlSetup;
	CtrlBuffer m_HCIEndpoint;
	std::deque<SQueuedEvent> m_EventQueue;
	
	u32 m_ACLSetup;
	CtrlBuffer m_ACLEndpoint;

	static const int m_acl_pkt_size = 339;
	static const int m_acl_pkts_num = 10;

	class ACLPool
	{
		u8 m_pool[m_acl_pkt_size * m_acl_pkts_num];
		int m_read_ptr;
		int m_write_ptr;

		struct
		{
			u16 size;
			u16 conn_handle;
		} m_info[m_acl_pkts_num];

	public:
		ACLPool()
			: m_read_ptr(0)
			, m_write_ptr(0)
		{}

		void Store(const u8* data, const u16 size, const u16 conn_handle)
		{
			_dbg_assert_msg_(WII_IPC_WIIMOTE,
				size < m_acl_pkt_size, "acl packet too large for pool");

			const int next_write_ptr = (m_write_ptr + 1) % m_acl_pkts_num;
			if (next_write_ptr == m_read_ptr)
			{
				// Many simultaneous exchanges of ACL packets tend to cause the
				// 10-packet limit to be exceeded.  Typically, this occurs when
				// many emulated Wiimotes are requesting connections at once.
				// See issue 4608 for more info.
				ERROR_LOG(WII_IPC_WIIMOTE, "ACL queue is full - current packet will be "
						"dropped! (m_write_ptr(%d) was about to overlap m_read_ptr(%d))",
						m_write_ptr, m_read_ptr);
				return;
			}

			memcpy(m_pool + m_acl_pkt_size * m_write_ptr, data, size);
			m_info[m_write_ptr].size = size;
			m_info[m_write_ptr].conn_handle = conn_handle;
			m_write_ptr = next_write_ptr;
		}

		void WriteToEndpoint(CtrlBuffer& endpoint);

		bool IsEmpty() const
		{
			return m_write_ptr == m_read_ptr;
		}

		int GetWritePos() const
		{
			return m_write_ptr;
		}

		int GetReadPos() const
		{
			return m_read_ptr;
		}

		// For SaveStates
		void DoState(PointerWrap &p)
		{
			p.Do(m_write_ptr);
			p.Do(m_read_ptr);
			p.DoArray((u8 *)m_pool, sizeof(m_pool));
			p.DoArray((u8 *)m_info, sizeof(m_info));
		}
	} m_acl_pool;

	u32 m_PacketCount[4];
	u64 m_last_ticks;

	// Send ACL data to a device (wiimote)
	void IncDataPacket(u16 _ConnectionHandle);
	void SendToDevice(u16 _ConnectionHandle, u8* _pData, u32 _Size);

	// Events
	void AddEventToQueue(const SQueuedEvent& _event);
	bool SendEventCommandStatus(u16 _Opcode);
	void SendEventCommandComplete(u16 _OpCode, void* _pData, u32 _DataSize);
	bool SendEventInquiryResponse();
	bool SendEventInquiryComplete();
	bool SendEventRemoteNameReq(const bdaddr_t& _bd);
	bool SendEventRequestConnection(CWII_IPC_HLE_WiiMote& _rWiiMote);
	bool SendEventConnectionComplete(const bdaddr_t& _bd);
	bool SendEventReadClockOffsetComplete(u16 _connectionHandle);
	bool SendEventConPacketTypeChange(u16 _connectionHandle, u16 _packetType);
	bool SendEventReadRemoteVerInfo(u16 _connectionHandle);
	bool SendEventReadRemoteFeatures(u16 _connectionHandle);
	bool SendEventRoleChange(bdaddr_t _bd, bool _master);
	bool SendEventNumberOfCompletedPackets();
	bool SendEventAuthenticationCompleted(u16 _connectionHandle);	
	bool SendEventModeChange(u16 _connectionHandle, u8 _mode, u16 _value);
	bool SendEventDisconnect(u16 _connectionHandle, u8 _Reason);
	bool SendEventRequestLinkKey(const bdaddr_t& _bd);
	bool SendEventLinkKeyNotification(const u8 num_to_send);

	// Execute HCI Message
	void ExecuteHCICommandMessage(const SHCICommandMessage& _rCtrlMessage);

	// OGF 0x01	Link control commands and return parameters
	void CommandWriteInquiryMode(u8* _Input);
	void CommandWritePageScanType(u8* _Input);
	void CommandHostBufferSize(u8* _Input);
	void CommandInquiryCancel(u8* _Input);
	void CommandRemoteNameReq(u8* _Input);
	void CommandCreateCon(u8* _Input);
	void CommandAcceptCon(u8* _Input);
	void CommandReadClockOffset(u8* _Input);
	void CommandReadRemoteVerInfo(u8* _Input);
	void CommandReadRemoteFeatures(u8* _Input);
	void CommandAuthenticationRequested(u8* _Input);
	void CommandInquiry(u8* _Input);
	void CommandDisconnect(u8* _Input);
	void CommandLinkKeyNegRep(u8* _Input);
	void CommandLinkKeyRep(u8* _Input);
    void CommandDeleteStoredLinkKey(u8* _Input);
	void CommandChangeConPacketType(u8* _Input);

	// OGF 0x02	Link policy commands and return parameters
	void CommandWriteLinkPolicy(u8* _Input);
	void CommandSniffMode(u8* _Input);

	// OGF 0x03	Host Controller and Baseband commands and return parameters
	void CommandReset(u8* _Input);
	void CommandWriteLocalName(u8* _Input);
	void CommandWritePageTimeOut(u8* _Input);
	void CommandWriteScanEnable(u8* _Input);
	void CommandWriteUnitClass(u8* _Input);
	void CommandReadStoredLinkKey(u8* _Input);
	void CommandWritePinType(u8* _Input);
	void CommandSetEventFilter(u8* _Input);
	void CommandWriteInquiryScanType(u8* _Input);
	void CommandWriteLinkSupervisionTimeout(u8* _Input);

	// OGF 0x04	Informational commands and return parameters 
	void CommandReadBufferSize(u8* _Input);
	void CommandReadLocalVer(u8* _Input);
	void CommandReadLocalFeatures(u8* _Input);
	void CommandReadBDAdrr(u8* _Input);

	// OGF 0x3F Vendor specific
	void CommandVendorSpecific_FC4C(u8* _Input, u32 _Size);
	void CommandVendorSpecific_FC4F(u8* _Input, u32 _Size);	

	// Debugging
	void LOG_LinkKey(const u8* _pLinkKey);

#pragma pack(push,1)
#define CONF_PAD_MAX_REGISTERED 10
#define CONF_PAD_MAX_ACTIVE 4
	
	struct _conf_pad_device
	{
		u8 bdaddr[6];
		char name[0x40];
	};

	struct _conf_pads
	{
		u8 num_registered;
		_conf_pad_device registered[CONF_PAD_MAX_REGISTERED];
		_conf_pad_device active[CONF_PAD_MAX_ACTIVE];
		_conf_pad_device balance_board;
		u8 unknown[0x45];
	};
#pragma pack(pop)

};
