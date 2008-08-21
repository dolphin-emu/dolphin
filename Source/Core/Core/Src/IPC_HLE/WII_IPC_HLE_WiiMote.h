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
#include "WII_IPC_HLE_Device_usb.h"

class CWII_IPC_HLE_Device_usb_oh1_57e_305;
class CWII_IPC_HLE_WiiMote
{
public:

	CWII_IPC_HLE_WiiMote(CWII_IPC_HLE_Device_usb_oh1_57e_305* _pHost, int _Number);

	virtual ~CWII_IPC_HLE_WiiMote()
	{}

	const bdaddr_t& GetBD() const { return m_BD; }

	const uint8_t* GetClass() const { return uclass; }

	u16 GetConnectionHandle() const { return m_ControllerConnectionHandle; }

	const u8* GetFeatures() const { return features; }

	const char* GetName() const { return m_Name.c_str(); }

	void SendACLFrame(u8* _pData, u32 _Size);

private:

	// STATE_TO_SAVE
	bdaddr_t m_BD;

	u16 m_ControllerConnectionHandle;

	uint8_t uclass[HCI_CLASS_SIZE];

	u8 features[HCI_FEATURES_SIZE];

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
	std::map<u32, SChannel> m_Channel;

	bool DoesChannelExist(u16 _SCID)
	{
		return m_Channel.find(_SCID) != m_Channel.end();
	}


	void SendCommandToACL(u8 _Ident, u8 _Code, u8 _CommandLength, u8* _pCommandData);

	void SignalChannel(u8* _pData, u32 _Size);

	void CommandConnectionReq(u8 _Ident, u8* _pData, u32 _Size);
	void CommandCofigurationReq(u8 _Ident, u8* _pData, u32 _Size);
};
#endif
