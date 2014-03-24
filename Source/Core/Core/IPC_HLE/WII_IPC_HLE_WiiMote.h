// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include <map>
#include <string>

#include "Common/ChunkFile.h"
#include "Core/IPC_HLE/hci.h"

class CWII_IPC_HLE_Device_usb_oh1_57e_305;

CWII_IPC_HLE_Device_usb_oh1_57e_305* GetUsbPointer();
void SetUsbPointer(CWII_IPC_HLE_Device_usb_oh1_57e_305* ptr);

class CBigEndianBuffer
{
public:
	CBigEndianBuffer(u8* pBuffer) : m_pBuffer(pBuffer) {}

	u8   Read8(u32 offset)              { return m_pBuffer[offset]; }
	u16  Read16(u32 offset)             { return Common::swap16(*(u16*)&m_pBuffer[offset]); }
	u32  Read32(u32 offset)             { return Common::swap32(*(u32*)&m_pBuffer[offset]); }

	void Write8(u32 offset, u8 data)    { m_pBuffer[offset] = data; }
	void Write16(u32 offset, u16 data)  { *(u16*)&m_pBuffer[offset] = Common::swap16(data); }
	void Write32(u32 offset, u32 data)  { *(u32*)&m_pBuffer[offset] = Common::swap32(data); }

	u8*  GetPointer(u32 offset)         { return &m_pBuffer[offset]; }

private:
	u8* m_pBuffer;
};


class CWII_IPC_HLE_WiiMote
{
public:
	CWII_IPC_HLE_WiiMote(CWII_IPC_HLE_Device_usb_oh1_57e_305* pHost, int Number, bdaddr_t _BD, bool ready = false);

	virtual ~CWII_IPC_HLE_WiiMote() {}

	void DoState(PointerWrap &p);

	// ugly Host handling....
	// we really have to clean all this code

	bool IsConnected() const { return m_ConnectionState == CONN_COMPLETE; }
	bool IsInactive() const { return m_ConnectionState == CONN_INACTIVE; }
	bool LinkChannel();
	void ResetChannels();
	void Activate(bool ready);
	void ExecuteL2capCmd(u8* pData, u32 Size); // From CPU
	void ReceiveL2capData(u16 scid, const void* pData, u32 Size); // From wiimote

	int NetPlay_GetWiimoteNum(int number);

	void EventConnectionAccepted();
	void EventDisconnect();
	bool EventPagingChanged(u8 pageMode);

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
	enum ConnectionState
	{
		CONN_INACTIVE = -1,
		CONN_READY,
		CONN_LINKING,
		CONN_COMPLETE
	};
	ConnectionState m_ConnectionState;

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
	u8 m_LinkKey[HCI_KEY_SIZE];
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

	void SendCommandToACL(u8 Ident, u8 Code, u8 CommandLength, u8* pCommandData);

	void SignalChannel(u8* pData, u32 Size);

	void SendConnectionRequest(u16 _SCID, u16 _PSM);
	void SendConfigurationRequest(u16 _SCID, u16 pMTU = 0, u16 pFlushTimeOut = 0);
	void SendDisconnectRequest(u16 _SCID);

	void ReceiveConnectionReq(u8 Ident, u8* pData, u32 Size);
	void ReceiveConnectionResponse(u8 Ident, u8* pData, u32 Size);
	void ReceiveDisconnectionReq(u8 Ident, u8* pData, u32 Size);
	void ReceiveConfigurationReq(u8 Ident, u8* pData, u32 Size);
	void ReceiveConfigurationResponse(u8 Ident, u8* pData, u32 Size);

	// some new ugly stuff
	// should be inside the plugin
	void HandleSDP(u16 _SCID, u8* pData, u32 Size);
	void SDPSendServiceSearchResponse(u16 _SCID, u16 TransactionID, u8* pServiceSearchPattern, u16 MaximumServiceRecordCount);

	void SDPSendServiceAttributeResponse(u16 _SCID, u16 TransactionID, u32 ServiceHandle,
											u16 StartAttrID, u16 EndAttrID,
											u16 MaximumAttributeByteCount, u8* pContinuationState);

	u16 AddAttribToList(int AttribID, u8* pBuffer);
};
