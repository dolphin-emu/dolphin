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

#define WIIMOTE_EEPROM_SIZE (16*1024)
#define WIIMOTE_REG_SPEAKER_SIZE 10
#define WIIMOTE_REG_EXT_SIZE 0x100
#define WIIMOTE_REG_IR_SIZE 0x34

class CWII_IPC_HLE_Device_usb_oh1_57e_305;
struct wm_report;
struct wm_leds;
struct wm_read_data;
struct wm_request_status;
struct wm_write_data;
struct wm_data_reporting;

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

	void Connect();

private:

	// STATE_TO_SAVE
	bdaddr_t m_BD;

	u16 m_ControllerConnectionHandle;

	uint8_t uclass[HCI_CLASS_SIZE];

	u8 features[HCI_FEATURES_SIZE];

	u8 m_Eeprom[WIIMOTE_EEPROM_SIZE];

	u8 m_RegSpeaker[WIIMOTE_REG_SPEAKER_SIZE];
	u8 m_RegExt[WIIMOTE_REG_EXT_SIZE];
	u8 m_RegIr[WIIMOTE_REG_IR_SIZE];

	std::string m_Name;

	CWII_IPC_HLE_Device_usb_oh1_57e_305* m_pHost;

	u8 m_Leds;


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

	void SendL2capData(u16 scid, void* _pData, u32 _Size);

	void SendCommandToACL(u8 _Ident, u8 _Code, u8 _CommandLength, u8* _pCommandData);

	void SignalChannel(u8* _pData, u32 _Size);

	void HidOutput(u8* _pData, u32 _Size);

	void HidOutputReport(wm_report* sr);

	void WmLeds(wm_leds* leds);
	void WmReadData(wm_read_data* rd);
	void WmWriteData(wm_write_data* wd);
	void WmRequestStatus(wm_request_status* rs);
	void WmDataReporting(wm_data_reporting* dr);

	void SendConnectionRequest(u16 scid, u16 psm);
	void SendConfigurationRequest(u16 scid);
	void SendReadDataReply(void* _Base, u16 _Address, u8 _Size);
	void SendWriteDataReply();

	int WriteWmReport(u8* dst, u8 channel);

	void CommandConnectionReq(u8 _Ident, u8* _pData, u32 _Size);
	void CommandCofigurationReq(u8 _Ident, u8* _pData, u32 _Size);
	void CommandConnectionResponse(u8 _Ident, u8* _pData, u32 _Size);
	void CommandCofigurationResponse(u8 _Ident, u8* _pData, u32 _Size);
};
#endif
