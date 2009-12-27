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
#ifndef _WII_IPC_HLE_WII_MOTE_
#define _WII_IPC_HLE_WII_MOTE_

#include <map>
#include <string>
#include "hci.h"
#include "ChunkFile.h"

class CWII_IPC_HLE_Device_usb_oh1_57e_305;

CWII_IPC_HLE_Device_usb_oh1_57e_305* GetUsbPointer();

enum
{
	SDP_CHANNEL			 = 0x01,
	HID_CONTROL_CHANNEL  = 0x11,
	HID_INTERRUPT_CHANNEL= 0x13,

	// L2CAP command codes
	L2CAP_COMMAND_REJ    = 0x01,
	L2CAP_CONN_REQ       = 0x02,
	L2CAP_CONN_RSP       = 0x03,
	L2CAP_CONF_REQ       = 0x04,
	L2CAP_CONF_RSP       = 0x05,
	L2CAP_DISCONN_REQ    = 0x06,
	L2CAP_DISCONN_RSP    = 0x07,
	L2CAP_ECHO_REQ       = 0x08,
	L2CAP_ECHO_RSP       = 0x09,
	L2CAP_INFO_REQ       = 0x0a,
	L2CAP_INFO_RSP       = 0x0b,

	// connect result
	L2CAP_CR_SUCCESS     = 0x0000,
	L2CAP_CR_PEND        = 0x0001,
	L2CAP_CR_BAD_PSM     = 0x0002,
	L2CAP_CR_SEC_BLOCK   = 0x0003,
	L2CAP_CR_NO_MEM      = 0x0004,

	//connect status
	L2CAP_CS_NO_INFO     = 0x0000,
	L2CAP_CS_AUTHEN_PEND = 0x0001,
	L2CAP_CS_AUTHOR_PEND = 0x0002,
};

#pragma pack(push, 1)

struct SL2CAP_Header
{
	u16 Length;
	u16 CID;
};

struct SL2CAP_Command
{
	u8 code;
	u8 ident;
	u16 len;
};

struct SL2CAP_CommandConnectionReq // 0x02
{
	u16 psm;
	u16 scid;
};

struct SL2CAP_ConnectionResponse // 0x03
{
	u16 dcid;
	u16 scid;
	u16 result;
	u16 status;
};

struct SL2CAP_Options
{
	u8 type;
	u8 length;
};

struct SL2CAP_OptionsMTU
{
	u16 MTU;
};

struct SL2CAP_OptionsFlushTimeOut
{
	u16 TimeOut;
};

struct SL2CAP_CommandConfigurationReq // 0x04
{
	u16 dcid;
	u16 flags;
};

struct SL2CAP_CommandConfigurationResponse // 0x05
{
	u16 scid;
	u16 flags;
	u16 result;
};

struct SL2CAP_CommandDisconnectionReq // 0x06
{
	u16 dcid;
	u16 scid;
};

struct SL2CAP_CommandDisconnectionResponse // 0x07
{
	u16 dcid;
	u16 scid;
};

#pragma pack(pop)

class CBigEndianBuffer
{
public:
	CBigEndianBuffer(u8* pBuffer) 
		: m_pBuffer(pBuffer)
	{
	}

	u8 Read8(u32 offset)
	{
		return m_pBuffer[offset];
	}

	u16 Read16(u32 offset)
	{
		return Common::swap16(*(u16*)&m_pBuffer[offset]);
	}

	u32 Read32(u32 offset)
	{
		return Common::swap32(*(u32*)&m_pBuffer[offset]);
	}

	void Write8(u32 offset, u8 data)
	{
		m_pBuffer[offset] = data;
	}

	void Write16(u32 offset, u16 data)
	{
		*(u16*)&m_pBuffer[offset] = Common::swap16(data);
	}

	void Write32(u32 offset, u32 data)
	{
		*(u32*)&m_pBuffer[offset] = Common::swap32(data);
	}

	u8* GetPointer(u32 offset)
	{
		return &m_pBuffer[offset];
	}

private:

	u8* m_pBuffer;

};




class CWII_IPC_HLE_WiiMote
{
public:

	CWII_IPC_HLE_WiiMote(CWII_IPC_HLE_Device_usb_oh1_57e_305* _pHost, int _Number, bool ready = false);

	virtual ~CWII_IPC_HLE_WiiMote()
	{}

	void DoState(PointerWrap &p);

	// ugly Host handling....
	// we really have to clean all this code

	int IsConnected() const { return m_Connected; }
	bool LinkChannel();
	void ResetChannels();
	void Activate(bool ready);
	void ShowStatus(const void* _pData); // Show status
	void UpdateStatus(); // Update status
	void ExecuteL2capCmd(u8* _pData, u32 _Size);	// From CPU
	void ReceiveL2capData(u16 scid, const void* _pData, u32 _Size);	// From wiimote

	void EventConnectionAccepted();
	void EventDisconnect();
	bool EventPagingChanged(u8 _pageMode);

	const bdaddr_t& GetBD() const { return m_BD; }
	const uint8_t* GetClass() const { return uclass; }
	u16 GetConnectionHandle() const { return m_ConnectionHandle; }
	const u8* GetFeatures() const { return features; }
	const char* GetName() const { return m_Name.c_str(); }
	u8 GetLMPVersion() const { return lmp_version; }
	u16 GetLMPSubVersion() const { return lmp_subversion; }
	u16 GetManufactorID() const { return 0x000F; }  // Broadcom Corporation
	const u8* GetLinkKey() const { return m_LinkKey; }

private:

	// -1: inactive, 0: ready, 1: connecting 2: linking 3: connected & linked
	int m_Connected;

	bool m_HIDControlChannel_Connected;
	bool m_HIDControlChannel_ConnectedWait;
	bool m_HIDControlChannel_Config;
	bool m_HIDControlChannel_ConfigWait;
	bool m_HIDInterruptChannel_Connected;
	bool m_HIDInterruptChannel_ConnectedWait;
	bool m_HIDInterruptChannel_Config;
	bool m_HIDInterruptChannel_ConfigWait;

	// STATE_TO_SAVE
	bdaddr_t m_BD;
	u16 m_ConnectionHandle;
	uint8_t uclass[HCI_CLASS_SIZE];
	u8 features[HCI_FEATURES_SIZE];
	u8 lmp_version;
	u16 lmp_subversion;
	u8 m_LinkKey[16];
	std::string m_Name;
	CWII_IPC_HLE_Device_usb_oh1_57e_305* m_pHost;

	struct SChannel 
	{
		u16 SCID;
		u16 DCID;
		u16 PSM;

		u16 MTU;
		u16 FlushTimeOut;
	};

	typedef std::map<u32, SChannel> CChannelMap;
	CChannelMap m_Channel;

	bool DoesChannelExist(u16 _SCID)
	{
		return m_Channel.find(_SCID) != m_Channel.end();
	}

	void SendCommandToACL(u8 _Ident, u8 _Code, u8 _CommandLength, u8* _pCommandData);

	void SignalChannel(u8* _pData, u32 _Size);

	void SendConnectionRequest(u16 _SCID, u16 _PSM);
	void SendConfigurationRequest(u16 _SCID, u16 _pMTU = NULL, u16 _pFlushTimeOut = NULL);
	void SendDisconnectRequest(u16 _SCID);

	void ReceiveConnectionReq(u8 _Ident, u8* _pData, u32 _Size);
	void ReceiveConnectionResponse(u8 _Ident, u8* _pData, u32 _Size);
	void ReceiveDisconnectionReq(u8 _Ident, u8* _pData, u32 _Size);
	void ReceiveConfigurationReq(u8 _Ident, u8* _pData, u32 _Size);
	void ReceiveConfigurationResponse(u8 _Ident, u8* _pData, u32 _Size);
	
	// some new ugly stuff
	//
	// should be inside the plugin 
	//
	void HandleSDP(u16 _SCID, u8* _pData, u32 _Size);
	void SDPSendServiceSearchResponse(u16 _SCID, u16 _TransactionID, u8* _pServiceSearchPattern, u16 _MaximumServiceRecordCount);

	void SDPSendServiceAttributeResponse(u16 _SCID, u16 TransactionID, u32 _ServiceHandle, 
											u16 _StartAttrID, u16 _EndAttrID, 
											u16 _MaximumAttributeByteCount, u8* _pContinuationState);

	u16 AddAttribToList(int _AttribID, u8* _pBuffer);
};

#endif

