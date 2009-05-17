// Copyright (C) 2003-2009 Dolphin Project.

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

#include "../Core.h" // Local core functions
#include "WII_IPC_HLE_Device_usb.h"

#ifdef _WIN32
#include "WII_IPC_HLE_Device_Usb_Kbd_win32.h"
#endif
#ifdef __linux__
#include "WII_IPC_HLE_Device_Usb_Kbd_linux.h"
#endif
#ifdef __APPLE__
#include "WII_IPC_HLE_Device_Usb_Kbd_apple.h"
#endif



CWII_IPC_HLE_Device_usb_kbd::CWII_IPC_HLE_Device_usb_kbd(u32 _DeviceID, const std::string& _rDeviceName)
: IWII_IPC_HLE_Device(_DeviceID, _rDeviceName)
{
}

CWII_IPC_HLE_Device_usb_kbd::~CWII_IPC_HLE_Device_usb_kbd()
{}

bool CWII_IPC_HLE_Device_usb_kbd::Open(u32 _CommandAddress, u32 _Mode)
{
    Memory::Write_U32(GetDeviceID(), _CommandAddress+4);

	IniFile ini;
	ini.Load(CONFIG_FILE);
	ini.Get("USB Keyboard", "Layout", &m_KeyboardLayout, KBD_LAYOUT_QWERTY);

	for(int i = 0; i < 256; i++)
		m_OldKeyBuffer[i] = false;
	m_OldModifiers = 0x00;

	PushMessage(MSG_KBD_CONNECT, 0x00, NULL);

    return true;
}

bool CWII_IPC_HLE_Device_usb_kbd::IOCtl(u32 _CommandAddress)
{
    u32 Parameter = Memory::Read_U32(_CommandAddress +0x0C);
    u32 BufferIn =  Memory::Read_U32(_CommandAddress + 0x10);
    u32 BufferInSize =  Memory::Read_U32(_CommandAddress + 0x14);
    u32 BufferOut = Memory::Read_U32(_CommandAddress + 0x18);
    u32 BufferOutSize = Memory::Read_U32(_CommandAddress + 0x1C);

	if (!m_MessageQueue.empty())
	{
		WriteMessage(BufferOut, m_MessageQueue.front());
		m_MessageQueue.pop();
	}

    Memory::Write_U32(0, _CommandAddress + 0x4);
    return true;
}

bool CWII_IPC_HLE_Device_usb_kbd::IsKeyPressed(int _Key)
{
#ifdef _WIN32
	if (GetAsyncKeyState(_Key) & 0x8000)
		return true;
	else
		return false;
#else
	// TODO: do it for non-Windows platforms
	return false;
#endif
}

u32 CWII_IPC_HLE_Device_usb_kbd::Update()
{
	u8 Modifiers = 0x00;
	u8 PressedKeys[6] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
	bool GotEvent = false;
	int i, j;

	j = 0;
	for (i = 0; i < 256; i++)
	{
		bool KeyPressedNow = IsKeyPressed(i);
		bool KeyPressedBefore = m_OldKeyBuffer[i];
		u8 KeyCode;

		if (KeyPressedNow ^ KeyPressedBefore)
		{
			if (KeyPressedNow)
			{
				switch(m_KeyboardLayout)
				{
				case KBD_LAYOUT_QWERTY:
					KeyCode = m_KeyCodesQWERTY[i];
					break;

				case KBD_LAYOUT_AZERTY:
					KeyCode = m_KeyCodesAZERTY[i];
					break;
				}

				if(KeyCode == 0x00)
					continue; 

				PressedKeys[j] = KeyCode;

				j++;
				if(j == 6) break;
			}

			GotEvent = true;
		}

		m_OldKeyBuffer[i] = KeyPressedNow;
	}

#ifdef _WIN32
	if (GetAsyncKeyState(VK_LCONTROL) & 0x8000)
		Modifiers |= 0x01;
	if (GetAsyncKeyState(VK_LSHIFT) & 0x8000)
		Modifiers |= 0x02;
	if (GetAsyncKeyState(VK_MENU) & 0x8000)
		Modifiers |= 0x04;
	if (GetAsyncKeyState(VK_LWIN) & 0x8000)
		Modifiers |= 0x08;
	if (GetAsyncKeyState(VK_RCONTROL) & 0x8000)
		Modifiers |= 0x10;
	if (GetAsyncKeyState(VK_RSHIFT) & 0x8000)
		Modifiers |= 0x20;
	if (GetAsyncKeyState(VK_MENU) & 0x8000) // TODO: VK_MENU is for ALT, not for ALT GR (ALT GR seems to work though...)
		Modifiers |= 0x40;
	if (GetAsyncKeyState(VK_RWIN) & 0x8000)
		Modifiers |= 0x80;
#else
	// TODO: modifiers for non-Windows platforms
#endif

	if(Modifiers ^ m_OldModifiers)
	{
		GotEvent = true;
		m_OldModifiers = Modifiers;
	}

	if (GotEvent)
		PushMessage(MSG_EVENT, Modifiers, PressedKeys);

	return 0;
}

void CWII_IPC_HLE_Device_usb_kbd::PushMessage(u32 _Message, u8 _Modifiers, u8 * _PressedKeys)
{
	SMessageData MsgData;

	MsgData.dwMessage = _Message;
	MsgData.dwUnk1 = 0;
	MsgData.bModifiers = _Modifiers;
	MsgData.bUnk2 = 0;

	if (_PressedKeys)
		memcpy(MsgData.bPressedKeys, _PressedKeys, sizeof(MsgData.bPressedKeys));
	else
		memset(MsgData.bPressedKeys, 0, sizeof(MsgData.bPressedKeys));

	m_MessageQueue.push(MsgData);
}

void CWII_IPC_HLE_Device_usb_kbd::WriteMessage(u32 _Address, SMessageData _Message)
{
	// TODO: the MessageData struct could be written directly in memory,
	// instead of writing each member separately
	Memory::Write_U32(_Message.dwMessage, _Address);
	Memory::Write_U32(_Message.dwUnk1, _Address + 0x4);
	Memory::Write_U8(_Message.bModifiers, _Address + 0x8);
	Memory::Write_U8(_Message.bUnk2, _Address + 0x9);
	Memory::Write_U8(_Message.bPressedKeys[0], _Address + 0xA);
	Memory::Write_U8(_Message.bPressedKeys[1], _Address + 0xB);
	Memory::Write_U8(_Message.bPressedKeys[2], _Address + 0xC);
	Memory::Write_U8(_Message.bPressedKeys[3], _Address + 0xD);
	Memory::Write_U8(_Message.bPressedKeys[4], _Address + 0xE);
	Memory::Write_U8(_Message.bPressedKeys[5], _Address + 0xF);
}
