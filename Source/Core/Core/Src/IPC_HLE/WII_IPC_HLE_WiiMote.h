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
#ifndef _WII_IPC_HLE_WII_MOTE_
#define _WII_IPC_HLE_WII_MOTE_

#include <map>

class CWII_IPC_HLE_Device_usb_oh1_57e_305;


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

	CWII_IPC_HLE_WiiMote(CWII_IPC_HLE_Device_usb_oh1_57e_305* _pHost, int _Number);

	virtual ~CWII_IPC_HLE_WiiMote()
	{}

	//////////////////////////////////////////////////////////////
	// ugly Host handling....
	// we really have to clean all this code

	bool IsConnected() const { return m_Connected; }


	void EventConnectionAccepted();
	void EventDisconnect();
	bool EventPagingChanged(u8 _pageMode);

	const bdaddr_t& GetBD() const { return m_BD; }

	const uint8_t* GetClass() const { return uclass; }

	u16 GetConnectionHandle() const { return m_ControllerConnectionHandle; }

	const u8* GetFeatures() const { return features; }

	const char* GetName() const { return m_Name.c_str(); }

	u8 GetLMPVersion() const { return lmp_version; }

	u16 GetLMPSubVersion() const { return lmp_subversion; }

	u8 GetManufactorID() const { return 0xF; }  // Broadcom Corporation

	void SendACLFrame(u8* _pData, u32 _Size);	//to wiimote

	void Connect();

	void SendL2capData(u16 scid, const void* _pData, u32 _Size);	//from wiimote

	const u8* GetLinkKey() const { return m_LinkKey; }

private:

	bool m_Connected;

	// STATE_TO_SAVE
	bdaddr_t m_BD;

	u16 m_ControllerConnectionHandle;

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

	void SendConnectionRequest(u16 scid, u16 psm);
	void SendConfigurationRequest(u16 scid, u16* MTU, u16* FlushTimeOut);
	void SendDisconnectRequest(u16 scid);

	void CommandConnectionReq(u8 _Ident, u8* _pData, u32 _Size);
	void CommandCofigurationReq(u8 _Ident, u8* _pData, u32 _Size);
	void CommandConnectionResponse(u8 _Ident, u8* _pData, u32 _Size);
	void CommandDisconnectionReq(u8 _Ident, u8* _pData, u32 _Size);
	void CommandCofigurationResponse(u8 _Ident, u8* _pData, u32 _Size);


	//////////////////
	// some new ugly stuff
	//
	// should be inside the plugin 
	//
	void HandleSDP(u16 cid, u8* _pData, u32 _Size);
	void SDPSendServiceSearchResponse(u16 cid, u16 TransactionID, u8* pServiceSearchPattern, u16 MaximumServiceRecordCount);

	void SDPSendServiceAttributeResponse(u16 cid, u16 TransactionID, u32 ServiceHandle, 
											u16 startAttrID, u16 endAttrID, 
											u16 MaximumAttributeByteCount, u8* pContinuationState);

	u16 AddAttribToList(int attribID, u8* pBuffer);
};

#endif
