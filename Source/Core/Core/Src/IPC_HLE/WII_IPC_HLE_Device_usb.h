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
#ifndef _WII_IPC_HLE_DEVICE_USB_H_
#define _WII_IPC_HLE_DEVICE_USB_H_

#include "WII_IPC_HLE_Device.h"
#include "hci.h"
#include <vector>
#include <queue>

#include "WII_IPC_HLE_WiiMote.h"

class CWII_IPC_HLE_WiiMote;
struct SCommandMessage;
struct SHCIEventCommand;
struct usb_ctrl_setup;

union UACLHeader
{
	struct
	{
		unsigned ConnectionHandle : 12;
		unsigned PBFlag : 2;
		unsigned BCFlag : 2;
		unsigned Size : 16;
	};
	u32 Hex;
};

class CWII_IPC_HLE_Device_usb_oh1_57e_305 : public IWII_IPC_HLE_Device
{
public:

	CWII_IPC_HLE_Device_usb_oh1_57e_305(u32 _DeviceID, const std::string& _rDeviceName);

	virtual ~CWII_IPC_HLE_Device_usb_oh1_57e_305();

	virtual bool Open(u32 _CommandAddress);

	virtual bool IOCtlV(u32 _CommandAddress);
	virtual bool IOCtl(u32 _CommandAddress);

	virtual u32 Update();

	void SendACLFrame(u16 _ConnectionHandle, u8* _pData, u32 _Size);

private:

	enum
	{
		USB_IOCTL_HCI_COMMAND_MESSAGE	= 0,
		USB_IOCTL_BLKMSG				= 1,
		USB_IOCTL_INTRMSG				= 2,
		USB_IOCTL_SUSPENDDEV			= 5,
		USB_IOCTL_RESUMEDEV				= 6,
		USB_IOCTL_GETDEVLIST			= 12,
		USB_IOCTL_DEVREMOVALHOOK		= 26,
		USB_IOCTL_DEVINSERTHOOK			= 27,
	};

	enum
	{
		HCI_EVENT_ENDPOINT				= 0x81,
		ACL_DATA_ENDPOINT_READ          = 0x02,
		ACL_DATA_ENDPOINT				= 0x82,
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
	};


	// STATE_TO_SAVE
	std::queue<SHCICommandMessage> m_HCICommandMessageQueue;

	bool m_ACLAnswer;
	SIOCtlVBuffer* m_pACLBuffer;

	SIOCtlVBuffer* m_pHCIBuffer;

	bool SendEventCommandStatus(u16 _Opcode);
	void SendEventCommandComplete(u16 _OpCode, void* _pData, u32 _DataSize);

	bool SendEventInquiryResponse();
	bool SendEventInquiryComplete();

	bool SendEventRemoteNameReq();
	bool SendEventRequestConnection();
	bool SendEventConnectionComplete();
	bool SendEventReadClockOffsetComplete();
	bool SendEventReadRemoteVerInfo();
	bool SendEventReadRemoteFeatures();

	void ExecuteHCICommandMessage(const SHCICommandMessage& _rCtrlMessage);

	// commands
	void CommandReset(u8* _Input);
	void CommandReadBufferSize(u8* _Input);
	void CommandReadLocalVer(u8* _Input);
	void CommandReadBDAdrr(u8* _Input);
	void CommandReadLocalFeatures(u8* _Input);
	void CommandReadStoredLinkKey(u8* _Input);
	void CommandWriteUnitClass(u8* _Input);
	void CommandWriteLocalName(u8* _Input);
	void CommandWritePinType(u8* _Input);
	void CommandHostBufferSize(u8* _Input);
	void CommandWritePageTimeOut(u8* _Input);
	void CommandWriteScanEnable(u8* _Input);
	void CommandWriteInquiryMode(u8* _Input);
	void CommandWritePageScanType(u8* _Input);
	void CommandSetEventFilter(u8* _Input);
	void CommandInquiry(u8* _Input);
	void CommandWriteInquiryScanType(u8* _Input);
	void CommandVendorSpecific_FC4C(u8* _Input, u32 _Size);
	void CommandVendorSpecific_FC4F(u8* _Input, u32 _Size);
	void CommandInquiryCancel(u8* _Input);
	void CommandRemoteNameReq(u8* _Input);
	void CommandCreateCon(u8* _Input);
	void CommandAcceptCon(u8* _Input);
	void CommandReadClockOffset(u8* _Input);
	void CommandReadRemoteVerInfo(u8* _Input);
	void CommandReadRemoteFeatures(u8* _Input);
	void CommandWriteLinkPolicy(u8* _Input);

	void SendToDevice(u16 _ConnectionHandle, u8* _pData, u32 _Size);

	enum EState
	{
		STATE_NONE,
		STATE_INQUIRY_RESPONSE,
		STATE_INQUIRY_COMPLETE,
		STATE_START_REMOTE_NAME_REQ,
		STATE_REMOTE_NAME_REQ,
		STATE_CONNECTION_COMPLETE_EVENT,
		STATE_READ_CLOCK_OFFSET,
		STATE_READ_REMOTE_VER_INFO,
		STATE_READ_REMOTE_FEATURES,
		STATE_CONNECT_WIIMOTE
	};

	EState m_State;
	bdaddr_t m_StateTempBD;
	u16 m_StateTempConnectionHandle;

	struct ACLFrame {
		u16 ConnectionHandle;
		u8* data;
		u32 size;
	};

	std::queue<ACLFrame> m_AclFrameQue;

	u8 scan_enable;

	//TODO: get rid of these, integrate into EState.
	enum EDelayedEvent
	{
		EVENT_NONE,
		EVENT_REQUEST_CONNECTION,
		EVENT_CONNECTION_COMPLETE
	};
	EDelayedEvent m_DelayedEvent;
	void SetDelayedEvent(EDelayedEvent e);

	bdaddr_t m_ControllerBD;
	u8 m_ClassOfDevice[HCI_CLASS_SIZE];
	char m_LocalName[HCI_UNIT_NAME_SIZE];
	u8 m_PINType;
	u8 filter_type;
	u8 filter_condition_type;


	u16 Host_max_acl_size; /* Max. size of ACL packet (bytes) */
	u8 Host_max_sco_size; /* Max. size of SCO packet (bytes) */
	u16 Host_num_acl_pkts;  /* Max. number of ACL packets */
	u16 Host_num_sco_pkts;  /* Max. number of SCO packets */


	std::vector<CWII_IPC_HLE_WiiMote> m_WiiMotes;
	CWII_IPC_HLE_WiiMote* AccessWiiMote(const bdaddr_t& _rAddr);
	CWII_IPC_HLE_WiiMote* AccessWiiMote(u16 _ConnectionHandle);
	void ClearBD(bdaddr_t& _rAddr);

};

#endif

