// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include <vector>

#include "Common/Common.h"
#include "Common/MsgHandler.h"
#include "Common/Logging/Log.h"
#include "Core/IPC_HLE/WiiMote_HID_Attr.h"

#if 0
// 0x00 (checked)
u8 ServiceRecordHandle[] = { 0x0a, 0x00, 0x01, 0x00, 0x00 };

// 0x01 (checked)
u8 SrvClassIDList[] = {
	0x35, 0x03,
	0x19, 0x11, 0x24
};

// 0x04 (checked)
u8 ProtocolDescriptorList[] = {
	0x35, 0x0D,
	0x35, 0x06,
	0x19, 0x01, 0x00, // Element 0
	0x09, 0x00, 0x11, // Element 1
	0x35, 0x03,
	0x19, 0x00, 0x11  // Element 0
};

// 0x5 (checked)
u8 BrowseGroupList[] = {
	0x35, 0x03,
	0x19, 0x10, 0x02
};

// 0x6 (checked)
u8 LanguageBaseAttributeIDList[] = {
	0x35, 0x09,
	0x09, 0x65, 0x6e,
	0x09, 0x00, 0x6a,
	0x09, 0x01, 0x00
};

// 0x09 (checked)
u8 BluetoothProfileDescriptorList[] = {
	0x35, 0x08,
	0x35, 0x06,
	0x19, 0x11, 0x24,
	0x09, 0x01, 0x00
};

// 0x0D (checked)
u8 AdditionalProtocolDescriptorLists[] = {
	0x35, 0x0F,
	0x35, 0x0D,
	0x35, 0x06,
	0x19, 0x01, 0x00,
	0x09, 0x00, 0x13,
	0x35, 0x03,
	0x19, 0x00, 0x11
};

// 0x100
u8 ServiceName[] = { 0x25, 0x13, 'N','i','n','t','e','n','d','o',' ','R','V','L','-','C','N','T','-','0','1' };
// 0x101
u8 ServiceDescription[] = { 0x25, 0x13, 'N','i','n','t','e','n','d','o',' ','R','V','L','-','C','N','T','-','0','1' };
// 0x102
u8 ProviderName [] = { 0x25, 0x8, 'N','i','n','t','e','n','d','o'};

// 0x200
u8 HIDDeviceReleaseNumber[] = { 0x09, 0x01, 0x00 };
// 0x201
u8 HIDParserVersion[] = { 0x09, 0x01, 0x11 };
// 0x202
u8 HIDDeviceSubclass[] = { 0x09, 0x00, 0x04 };
// 0x203
u8 HIDCountryCode[] = { 0x09, 0x00, 0x33 };
// 0x204
u8 HIDVirtualCable[] = { 0x09, 0x00, 0x00 };
// 0x205
u8 HIDReconnectInitiate[] = { 0x09, 0x00, 0x01 };

// 0x206
u8 HIDDescriptorList[] = {
	0x35, 0xDF,
	0x35, 0xDD,
	0x08, 0x22,  // Element 0
	0x25, 0xD9,  // hmm... <- 0x25 is a string but there is Data

	// 0xD9 Bytes - Element 1
	0x05, 0x01, 0x09, 0x05, 0xa1, 0x01, 0x85, 0x10,
	0x15, 0x00, 0x26, 0xff, 0x00, 0x75, 0x08, 0x95,
	0x01, 0x06, 0x00, 0xff, 0x09, 0x01, 0x91, 0x00,
	0x85, 0x11, 0x95, 0x01, 0x09, 0x01, 0x91, 0x00,
	0x85, 0x12, 0x95, 0x02, 0x09, 0x01, 0x91, 0x00,
	0x85, 0x13, 0x95, 0x01, 0x09, 0x01, 0x91, 0x00,
	0x85, 0x14, 0x95, 0x01, 0x09, 0x01, 0x91, 0x00,
	0x85, 0x15, 0x95, 0x01, 0x09, 0x01, 0x91, 0x00,
	0x85, 0x16, 0x95, 0x15, 0x09, 0x01, 0x91, 0x00,
	0x85, 0x17, 0x95, 0x06, 0x09, 0x01, 0x91, 0x00,
	0x85, 0x18, 0x95, 0x15, 0x09, 0x01, 0x91, 0x00,
	0x85, 0x19, 0x95, 0x01, 0x09, 0x01, 0x91, 0x00,
	0x85, 0x1a, 0x95, 0x01, 0x09, 0x01, 0x91, 0x00,
	0x85, 0x20, 0x95, 0x06, 0x09, 0x01, 0x81, 0x00,
	0x85, 0x21, 0x95, 0x15, 0x09, 0x01, 0x81, 0x00,
	0x85, 0x22, 0x95, 0x04, 0x09, 0x01, 0x81, 0x00,
	0x85, 0x30, 0x95, 0x02, 0x09, 0x01, 0x81, 0x00,
	0x85, 0x31, 0x95, 0x05, 0x09, 0x01, 0x81, 0x00,
	0x85, 0x32, 0x95, 0x0a, 0x09, 0x01, 0x81, 0x00,
	0x85, 0x33, 0x95, 0x11, 0x09, 0x01, 0x81, 0x00,
	0x85, 0x34, 0x95, 0x15, 0x09, 0x01, 0x81, 0x00,
	0x85, 0x35, 0x95, 0x15, 0x09, 0x01, 0x81, 0x00,
	0x85, 0x36, 0x95, 0x15, 0x09, 0x01, 0x81, 0x00,
	0x85, 0x37, 0x95, 0x15, 0x09, 0x01, 0x81, 0x00,
	0x85, 0x3d, 0x95, 0x15, 0x09, 0x01, 0x81, 0x00,
	0x85, 0x3e, 0x95, 0x15, 0x09, 0x01, 0x81, 0x00,
	0x85, 0x3f, 0x95, 0x15, 0x09, 0x01, 0x81, 0x00,
	0xc0 // end tag
};

// 0x207
u8 HIDLANGIDBaseList[] = {
	0x35, 0x08,
	0x35, 0x06,
	0x09, 0x04, 0x09,
	0x09, 0x01, 0x00
};

// 0x208
u8 HIDSDPDisable[] = { 0x28, 0x00 };
// 0x209
u8 HIDBatteryPower[] = { 0x28, 0x01 };
// 0x20a
u8 HIDRemoteWake[] = { 0x28, 0x01 };
// 0x20b
u8 HIDUnk_020B[] = { 0x09, 0x01, 0x00 };
// 0x20c
u8 HIDUnk_020C[] = { 0x09, 0x0c, 0x80 };
// 0x20d
u8 HIDUnk_020D[] = { 0x28, 0x00 };
// 0x20e
u8 HIDBootDevice[] = { 0x28, 0x00 };
#endif


static u8 packet1[] = {
	0x00, 0x7b, 0x00, 0x76, 0x36, 0x01, 0xcc, 0x09, 0x00, 0x00, 0x0a, 0x00, 0x01,
	0x00, 0x00, 0x09, 0x00, 0x01, 0x35, 0x03, 0x19, 0x11, 0x24, 0x09, 0x00, 0x04, 0x35, 0x0d, 0x35,
	0x06, 0x19, 0x01, 0x00, 0x09, 0x00, 0x11, 0x35, 0x03, 0x19, 0x00, 0x11, 0x09, 0x00, 0x05, 0x35,
	0x03, 0x19, 0x10, 0x02, 0x09, 0x00, 0x06, 0x35, 0x09, 0x09, 0x65, 0x6e, 0x09, 0x00, 0x6a, 0x09,
	0x01, 0x00, 0x09, 0x00, 0x09, 0x35, 0x08, 0x35, 0x06, 0x19, 0x11, 0x24, 0x09, 0x01, 0x00, 0x09,
	0x00, 0x0d, 0x35, 0x0f, 0x35, 0x0d, 0x35, 0x06, 0x19, 0x01, 0x00, 0x09, 0x00, 0x13, 0x35, 0x03,
	0x19, 0x00, 0x11, 0x09, 0x01, 0x00, 0x25, 0x13, 0x4e, 0x69, 0x6e, 0x74, 0x65, 0x6e, 0x64, 0x6f,
	0x20, 0x52, 0x56, 0x4c, 0x2d, 0x43, 0x4e, 0x54, 0x2d, 0x30, 0x31, 0x09, 0x01, 0x02, 0x00, 0x76,
};

static u8 packet2[] = {
	0x00, 0x7b, 0x00, 0x76, 0x01, 0x25, 0x13, 0x4e, 0x69, 0x6e, 0x74, 0x65, 0x6e,
	0x64, 0x6f, 0x20, 0x52, 0x56, 0x4c, 0x2d, 0x43, 0x4e, 0x54, 0x2d, 0x30, 0x31, 0x09, 0x01, 0x02,
	0x25, 0x08, 0x4e, 0x69, 0x6e, 0x74, 0x65, 0x6e, 0x64, 0x6f, 0x09, 0x02, 0x00, 0x09, 0x01, 0x00,
	0x09, 0x02, 0x01, 0x09, 0x01, 0x11, 0x09, 0x02, 0x02, 0x08, 0x04, 0x09, 0x02, 0x03, 0x08, 0x33,
	0x09, 0x02, 0x04, 0x28, 0x00, 0x09, 0x02, 0x05, 0x28, 0x01, 0x09, 0x02, 0x06, 0x35, 0xdf, 0x35,
	0xdd, 0x08, 0x22, 0x25, 0xd9, 0x05, 0x01, 0x09, 0x05, 0xa1, 0x01, 0x85, 0x10, 0x15, 0x00, 0x26,
	0xff, 0x00, 0x75, 0x08, 0x95, 0x01, 0x06, 0x00, 0xff, 0x09, 0x01, 0x91, 0x00, 0x85, 0x11, 0x95,
	0x01, 0x09, 0x01, 0x91, 0x00, 0x85, 0x12, 0x95, 0x02, 0x09, 0x01, 0x91, 0x00, 0x02, 0x00, 0xec,
};

static u8 packet3[] = {
	0x00, 0x7b, 0x00, 0x76, 0x85, 0x13, 0x95, 0x01, 0x09, 0x01, 0x91, 0x00, 0x85,
	0x14, 0x95, 0x01, 0x09, 0x01, 0x91, 0x00, 0x85, 0x15, 0x95, 0x01, 0x09, 0x01, 0x91, 0x00, 0x85,
	0x16, 0x95, 0x15, 0x09, 0x01, 0x91, 0x00, 0x85, 0x17, 0x95, 0x06, 0x09, 0x01, 0x91, 0x00, 0x85,
	0x18, 0x95, 0x15, 0x09, 0x01, 0x91, 0x00, 0x85, 0x19, 0x95, 0x01, 0x09, 0x01, 0x91, 0x00, 0x85,
	0x1a, 0x95, 0x01, 0x09, 0x01, 0x91, 0x00, 0x85, 0x20, 0x95, 0x06, 0x09, 0x01, 0x81, 0x00, 0x85,
	0x21, 0x95, 0x15, 0x09, 0x01, 0x81, 0x00, 0x85, 0x22, 0x95, 0x04, 0x09, 0x01, 0x81, 0x00, 0x85,
	0x30, 0x95, 0x02, 0x09, 0x01, 0x81, 0x00, 0x85, 0x31, 0x95, 0x05, 0x09, 0x01, 0x81, 0x00, 0x85,
	0x32, 0x95, 0x0a, 0x09, 0x01, 0x81, 0x00, 0x85, 0x33, 0x95, 0x11, 0x09, 0x01, 0x02, 0x01, 0x62,
};

static u8 packet4[] = {
	0x00, 0x70, 0x00, 0x6d, 0x81, 0x00, 0x85, 0x34, 0x95, 0x15, 0x09, 0x01, 0x81,
	0x00, 0x85, 0x35, 0x95, 0x15, 0x09, 0x01, 0x81, 0x00, 0x85, 0x36, 0x95, 0x15, 0x09, 0x01, 0x81,
	0x00, 0x85, 0x37, 0x95, 0x15, 0x09, 0x01, 0x81, 0x00, 0x85, 0x3d, 0x95, 0x15, 0x09, 0x01, 0x81,
	0x00, 0x85, 0x3e, 0x95, 0x15, 0x09, 0x01, 0x81, 0x00, 0x85, 0x3f, 0x95, 0x15, 0x09, 0x01, 0x81,
	0x00, 0xc0, 0x09, 0x02, 0x07, 0x35, 0x08, 0x35, 0x06, 0x09, 0x04, 0x09, 0x09, 0x01, 0x00, 0x09,
	0x02, 0x08, 0x28, 0x00, 0x09, 0x02, 0x09, 0x28, 0x01, 0x09, 0x02, 0x0a, 0x28, 0x01, 0x09, 0x02,
	0x0b, 0x09, 0x01, 0x00, 0x09, 0x02, 0x0c, 0x09, 0x0c, 0x80, 0x09, 0x02, 0x0d, 0x28, 0x00, 0x09,
	0x02, 0x0e, 0x28, 0x00, 0x00,

};


static u8 packet4_0x10001[] = {
	0x00, 0x60, 0x00, 0x5d, 0x36, 0x00, 0x5a, 0x09, 0x00, 0x00, 0x0a, 0x00, 0x01,
	0x00, 0x01, 0x09, 0x00, 0x01, 0x35, 0x03, 0x19, 0x12, 0x00, 0x09, 0x00, 0x04, 0x35, 0x0d, 0x35,
	0x06, 0x19, 0x01, 0x00, 0x09, 0x00, 0x01, 0x35, 0x03, 0x19, 0x00, 0x01, 0x09, 0x00, 0x05, 0x35,
	0x03, 0x19, 0x10, 0x02, 0x09, 0x00, 0x09, 0x35, 0x08, 0x35, 0x06, 0x19, 0x12, 0x00, 0x09, 0x01,
	0x00, 0x09, 0x02, 0x00, 0x09, 0x01, 0x00, 0x09, 0x02, 0x01, 0x09, 0x05, 0x7e, 0x09, 0x02, 0x02,
	0x09, 0x03, 0x06, 0x09, 0x02, 0x03, 0x09, 0x06, 0x00, 0x09, 0x02, 0x04, 0x28, 0x01, 0x09, 0x02,
	0x05, 0x09, 0x00, 0x02, 0x00,
};



const u8* GetAttribPacket(u32 serviceHandle, u32 cont, u32& _size)
{
	if (serviceHandle == 0x10000)
	{
		if (cont == 0)
		{
			_size = sizeof(packet1);
			return packet1;
		}
		else if (cont == 0x76)
		{
			_size = sizeof(packet2);
			return packet2;
		}
		else if (cont == 0xec)
		{
			_size = sizeof(packet3);
			return packet3;
		}
		else if (cont == 0x162)
		{
			_size = sizeof(packet4);
			return packet4;
		}
	}

	if (serviceHandle == 0x10001)
	{
		_dbg_assert_(WII_IPC_WIIMOTE, cont == 0x00);
		_size = sizeof(packet4_0x10001);
		return packet4_0x10001;
	}

	return nullptr;
}

// XXX keep these?
#if 0
CAttribTable m_AttribTable;

void InitAttribTable()
{
	m_AttribTable.push_back(SAttrib(0x00, ServiceRecordHandle, sizeof(ServiceRecordHandle)));
	m_AttribTable.push_back(SAttrib(0x01, SrvClassIDList, sizeof(SrvClassIDList)));
	m_AttribTable.push_back(SAttrib(0x04, ProtocolDescriptorList, sizeof(ProtocolDescriptorList)));
	m_AttribTable.push_back(SAttrib(0x05, BrowseGroupList, sizeof(BrowseGroupList)));
	m_AttribTable.push_back(SAttrib(0x06, LanguageBaseAttributeIDList, sizeof(LanguageBaseAttributeIDList)));
	m_AttribTable.push_back(SAttrib(0x09, BluetoothProfileDescriptorList, sizeof(BluetoothProfileDescriptorList)));
	m_AttribTable.push_back(SAttrib(0x0D, AdditionalProtocolDescriptorLists, sizeof(AdditionalProtocolDescriptorLists)));


	m_AttribTable.push_back(SAttrib(0x100, ServiceName, sizeof(ServiceName)));
	m_AttribTable.push_back(SAttrib(0x101, ServiceDescription, sizeof(ServiceDescription)));
	m_AttribTable.push_back(SAttrib(0x102, ProviderName, sizeof(ProviderName)));

	m_AttribTable.push_back(SAttrib(0x200, HIDDeviceReleaseNumber, sizeof(HIDDeviceReleaseNumber)));
	m_AttribTable.push_back(SAttrib(0x201, HIDParserVersion, sizeof(HIDParserVersion)));
	m_AttribTable.push_back(SAttrib(0x202, HIDDeviceSubclass, sizeof(HIDDeviceSubclass)));
	m_AttribTable.push_back(SAttrib(0x203, HIDCountryCode, sizeof(HIDCountryCode)));
	m_AttribTable.push_back(SAttrib(0x204, HIDVirtualCable, sizeof(HIDVirtualCable)));
	m_AttribTable.push_back(SAttrib(0x205, HIDReconnectInitiate, sizeof(HIDReconnectInitiate)));
	m_AttribTable.push_back(SAttrib(0x206, HIDDescriptorList, sizeof(HIDDescriptorList)));
	m_AttribTable.push_back(SAttrib(0x207, HIDLANGIDBaseList, sizeof(HIDLANGIDBaseList)));
	m_AttribTable.push_back(SAttrib(0x208, HIDSDPDisable, sizeof(HIDSDPDisable)));
	m_AttribTable.push_back(SAttrib(0x209, HIDBatteryPower, sizeof(HIDBatteryPower)));
	m_AttribTable.push_back(SAttrib(0x20a, HIDRemoteWake, sizeof(HIDRemoteWake)));
	m_AttribTable.push_back(SAttrib(0x20b, HIDUnk_020B, sizeof(HIDUnk_020B)));
	m_AttribTable.push_back(SAttrib(0x20c, HIDUnk_020C, sizeof(HIDUnk_020C)));
	m_AttribTable.push_back(SAttrib(0x20d, HIDUnk_020D, sizeof(HIDUnk_020D)));
	m_AttribTable.push_back(SAttrib(0x20e, HIDBootDevice, sizeof(HIDBootDevice)));
}

const CAttribTable& GetAttribTable()
{
	if (m_AttribTable.empty())
	{
		InitAttribTable();
	}

	return m_AttribTable;
}
#endif
