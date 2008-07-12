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

#include "Common.h"
#include "../Globals.h"

#ifdef _WIN32
#include "../PCHW/DSoundStream.h"
#endif
#include "../PCHW/Mixer.h"
#include "../MailHandler.h"

#include "UCodes.h"
#include "UCode_AX.h"

CUCode_AX::CUCode_AX(CMailHandler& _rMailHandler)
	: IUCode(_rMailHandler)
{
	// we got loaded
	m_rMailHandler.PushMail(0xDCD10000);
	m_rMailHandler.PushMail(0x80000000); // handshake ??? only (crc == 0xe2136399) needs it ...
}


CUCode_AX::~CUCode_AX()
{
	m_rMailHandler.Clear();
}


void CUCode_AX::HandleMail(u32 _uMail)
{
	if ((_uMail & 0xFFFF0000) == 0xBABE0000)
	{
		// a new List
	}
	else
	{
		AXTask(_uMail);
	}
}


void
CUCode_AX::MixAdd(short* _pBuffer, int _iSize)
{}


void CUCode_AX::Update()
{
	// check if we have to sent something
	if (!m_rMailHandler.IsEmpty())
	{
		g_dspInitialize.pGenerateDSPInterrupt();
	}
}


bool CUCode_AX::AXTask(u32& _uMail)
{
	u32 uAddress = _uMail;

	DebugLog("AXTask - AXCommandList-Addr: 0x%08x", uAddress);

	u32 Addr__AXStudio;
	u32 Addr__AXOutSBuffer;
	u32 Addr__AXOutSBuffer_1;
	u32 Addr__AXOutSBuffer_2;

	u32 Addr__A;
	u32 Addr__12;
	u32 Addr__4_1;
	u32 Addr__4_2;
	u32 Addr__5_1;
	u32 Addr__5_2;
	u32 Addr__6;
	u32 Addr__9;

	bool bExecuteList = true;

	while (bExecuteList)
	{
		u16 iCommand = Memory_Read_U16(uAddress);
		uAddress += 2;

		switch (iCommand)
		{
		    case 0x00: //00
			    Addr__AXStudio = Memory_Read_U32(uAddress);
			    uAddress += 4;
			    DebugLog("AXLIST studio address: %08x", Addr__AXStudio);
			    break;

		    case 0x001:
				{
					u32 address = Memory_Read_U32(uAddress);
					uAddress += 4;
					u16 param1 = Memory_Read_U16(uAddress);
					uAddress += 2;
					u16 param2 = Memory_Read_U16(uAddress);
					uAddress += 2;
					u16 param3 = Memory_Read_U16(uAddress);
					uAddress += 2;
					DebugLog("AXLIST 1: %08x, %04x, %04x, %04x", address, param1, param2, param3);
				}
			    break;

		    case 0x02: //02
				{
					uAddress += 4;
#ifdef _WIN32
					DebugLog("Update the SoundThread to be in sync");
					DSound::DSound_UpdateSound(); //do it in this thread to avoid sync problems
#endif
				}
			    break;

		    case 0x0003: // ????
			    DebugLog("AXLIST command 0x0003 ????");
			    break;

		    case 0x0004:
			    Addr__4_1 = Memory_Read_U32(uAddress);
			    uAddress += 4;
			    Addr__4_2 = Memory_Read_U32(uAddress);
			    uAddress += 4;
			    DebugLog("AXLIST 4_1 4_2 addresses: %08x %08x", Addr__4_1, Addr__4_2);
			    break;

		    case 0x0005:
			    Addr__5_1 = Memory_Read_U32(uAddress);
			    uAddress += 4;
			    Addr__5_2 = Memory_Read_U32(uAddress);
			    uAddress += 4;
			    DebugLog("AXLIST 5_1 5_2 addresses: %08x %08x", Addr__5_1, Addr__5_2);
			    break;

		    case 0x0006:
			    Addr__6   = Memory_Read_U32(uAddress);
			    uAddress += 4;
			    DebugLog("AXLIST 6 address: %08x", Addr__6);
			    break;

		    case 0x07:
			    Addr__AXOutSBuffer = Memory_Read_U32(uAddress);
			    uAddress += 4;
			    DebugLog("AXLIST OutSBuffer address: %08x", Addr__AXOutSBuffer);
			    break;

		    case 0x0009:
			    Addr__9   = Memory_Read_U32(uAddress);
			    uAddress += 4;
			    DebugLog("AXLIST 6 address: %08x", Addr__9);
			    break;

		    case 0x0a:
			    Addr__A   = Memory_Read_U32(uAddress);
			    uAddress += 4;
			    DebugLog("AXLIST CompressorTable address: %08x", Addr__A);
			    break;

		    case 0x000e:
			    Addr__AXOutSBuffer_1 = Memory_Read_U32(uAddress);
			    uAddress += 4;
			    Addr__AXOutSBuffer_2 = Memory_Read_U32(uAddress);
			    uAddress += 4;
			    DebugLog("AXLIST sbuf2 addresses: %08x %08x", Addr__AXOutSBuffer_1, Addr__AXOutSBuffer_2);
			    break;

		    case 0x0f:
			    bExecuteList = false;
			    DebugLog("AXLIST end");
			    break;

		    case 0x0010: //monkey ball2
			    DebugLog("AXLIST unknown");
			    //should probably read/skip stuff here
			    uAddress += 8;
			    break;

		    case 0x0011:
			    uAddress += 4;
			    break;

		    case 0x0012:
			    Addr__12  = Memory_Read_U16(uAddress);
			    uAddress += 2;
			    break;

		    case 0x0013:
			    uAddress += 6 * 4; // 6 Addresses
			    break;

		    default:
		    {
			    static bool bFirst = true;

			    if (bFirst == true)
			    {
				    char szTemp[2048];
				    sprintf(szTemp, "Unknown AX-Command 0x%x (address: 0x%08x)\n", iCommand, uAddress - 2);
				    int num = 0;

				    while (num < 64)
				    {
					    char szTemp2[128] = "";
					    sprintf(szTemp2, "0x%04x\n", Memory_Read_U16(uAddress + num));
					    strcat(szTemp, szTemp2);
					    num += 2;
				    }

				    PanicAlert(szTemp);

				    bFirst = false;
			    }

			    // unknown command so stop the execution of this TaskList
			    bExecuteList = false;
		    }
			break;
		}
	}

	DebugLog("AXTask - done, send resume");

	// i hope resume is okay AX
	m_rMailHandler.PushMail(0xDCD10001);

	return(true);
}
