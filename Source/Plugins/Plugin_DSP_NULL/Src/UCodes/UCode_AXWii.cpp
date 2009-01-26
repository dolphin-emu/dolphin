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

#include "StringUtil.h"

#include "../MailHandler.h"

#include "UCodes.h"
#include "UCode_AXStructs.h"
#include "UCode_AX.h" // for some functions in CUCode_AX
#include "UCode_AXWii.h"
#include "UCode_AX_Voice.h"


// ------------------------------------------------------------------
// Declarations
// -----------
extern bool gSequenced;

// -----------


CUCode_AXWii::CUCode_AXWii(CMailHandler& _rMailHandler, u32 l_CRC)
	: IUCode(_rMailHandler)
	, m_addressPBs(0xFFFFFFFF)
	, _CRC(l_CRC)
{
	// we got loaded
	m_rMailHandler.PushMail(0xDCD10000);
	m_rMailHandler.PushMail(0x80000000);  // handshake ??? only (crc == 0xe2136399) needs it ...

	templbuffer = new int[1024 * 1024];
	temprbuffer = new int[1024 * 1024];

	lCUCode_AX = new CUCode_AX(_rMailHandler);
	lCUCode_AX->_CRC = l_CRC;
}

CUCode_AXWii::~CUCode_AXWii()
{
	m_rMailHandler.Clear();
	delete [] templbuffer;
	delete [] temprbuffer;
}

void CUCode_AXWii::HandleMail(u32 _uMail)
{
	if ((_uMail & 0xFFFF0000) == MAIL_AX_ALIST)
	{
		// a new List
	}
	else
	{
		AXTask(_uMail);
	}
}


void CUCode_AXWii::MixAdd(short* _pBuffer, int _iSize)
{
	if(_CRC == 0xfa450138)
	{
		AXParamBlockWii PBs[NUMBER_OF_PBS];
		MixAdd_( _pBuffer, _iSize, PBs);
	}
	else
	{
		AXParamBlockWii_ PBs[NUMBER_OF_PBS];
		MixAdd_(_pBuffer, _iSize, PBs);
	}
}


template<class ParamBlockType>
void CUCode_AXWii::MixAdd_(short* _pBuffer, int _iSize, ParamBlockType &PBs)
{
	//AXParamBlockWii PBs[NUMBER_OF_PBS];

	// read out pbs
	int numberOfPBs = ReadOutPBsWii(m_addressPBs, PBs, NUMBER_OF_PBS);
	
	if (_iSize > 1024 * 1024)
		_iSize = 1024 * 1024;

	// write zeroes to the beginning of templbuffer
	memset(templbuffer, 0, _iSize * sizeof(int));
	memset(temprbuffer, 0, _iSize * sizeof(int));

	// -------------------------------------------
	// write logging data to debugger
	/* Make the updates we are told to do. See comments to the GC version in UCode_AX.cpp */
	// ------------
	for (int i = 0; i < numberOfPBs; i++)
	{
		u16 *pDest = (u16 *)&PBs[i];
		u16 upd0 = pDest[41]; u16 upd1 = pDest[42]; u16 upd2 = pDest[43]; // num_updates
		u16 upd_hi = pDest[44]; // update addr
		u16	upd_lo = pDest[45];
		int numupd = upd0 + upd1 + upd2;
		if(numupd > 64) numupd = 64; // prevent to high values
		const u32 updaddr   = (u32)(upd_hi << 16) | upd_lo;		
		int on = false, off = false;
		for (int j = 0; j < numupd; j++) // make alll updates
		{	
			const u16 updpar   = Memory_Read_U16(updaddr);
			const u16 upddata   = Memory_Read_U16(updaddr + 2);
			// some safety checks, I hope it's enough
			if( (  (updaddr > 0x80000000 && updaddr < 0x817fffff)
				|| (updaddr > 0x90000000 && updaddr < 0x93ffffff) )
				&& updpar < 127 && updpar > 3 && upddata >= 0 // updpar > 3 because we don't want to change
				// 0-3, those are important
				//&& (upd0 || upd1 || upd2) // We should use these in some way to I think
				// but I don't know how or when
				&& gSequenced) // on and off option
			{
				//PanicAlert("Update %i: %i = %04x", i, updpar, upddata);
				//DebugLog("Update: %i = %04x", updpar, upddata);
				pDest[updpar] = upddata;
			}
			if (updpar == 7 && upddata == 1) on++;
			if (updpar == 7 && upddata == 1) off++;
		}
		// hack: if we get both an on and an off select on rather than off
		if (on > 0 && off > 0) pDest[7] = 1;
	}

	//PrintFile(1, "%08x %04x %04x\n", updaddr, updpar, upddata);
	// ------------


	for (int i = 0; i < numberOfPBs; i++)
	{		
		MixAddVoice(PBs[i], templbuffer, temprbuffer, _iSize, true);
	}		

	WriteBackPBsWii(m_addressPBs, PBs, numberOfPBs);
	// We write the sound to _pBuffer
	for (int i = 0; i < _iSize; i++)
	{
		// Clamp into 16-bit. Maybe we should add a volume compressor here.
		int left  = templbuffer[i] + _pBuffer[0];
		int right = temprbuffer[i] + _pBuffer[1];
		if (left < -32767)  left = -32767;
		if (left > 32767)   left = 32767;
		if (right < -32767) right = -32767;
		if (right >  32767) right = 32767;
		*_pBuffer++ = left;
		*_pBuffer++ = right;
	}

}


void CUCode_AXWii::Update()
{
	// check if we have to sent something
	if (!m_rMailHandler.IsEmpty())
	{
		g_dspInitialize.pGenerateDSPInterrupt();
	}
}


// Shortcut
void CUCode_AXWii::SaveLog(const char* _fmt, ...)
{
}


// AX seems to bootup one task only and waits for resume-callbacks
// everytime the DSP has "spare time" it sends a resume-mail to the CPU
// and the __DSPHandler calls a AX-Callback which generates a new AXFrame
bool CUCode_AXWii::AXTask(u32& _uMail)
{
	u32 uAddress = _uMail;
	SaveLog("Begin");
	SaveLog("=====================================================================");
	SaveLog("%08x: AXTask - AXCommandList-Addr", uAddress);

	u32 Addr__AXStudio;
	u32 Addr__AXOutSBuffer;
        //	u32 Addr__AXOutSBuffer_1;
        //	u32 Addr__AXOutSBuffer_2;
	u32 Addr__A;
        //	u32 Addr__12;
	u32 Addr__5_1;
	u32 Addr__5_2;
	u32 Addr__6;
        //	u32 Addr__9;

	bool bExecuteList = true;
	if (false) 
	{
		// PanicAlert("%i", sizeof(AXParamBlockWii));  // 252 ??
		FILE *f = fopen("D:\\axdump.txt", "a");
		if (!f)
			f = fopen("D:\\axdump.txt", "w");

		u32 addr = uAddress;
		for (int i = 0; i < 100; i++) {
			fprintf(f, "%02x\n", Memory_Read_U16(addr + i * 2));
		}
		fprintf(f, "===========------------------------------------------------=\n");
		fclose(f);
	}
	else
	{
		// PanicAlert("%i", sizeof(AXParamBlock));  // 192
	}

	while (bExecuteList)
	{
		static int last_valid_command = 0;
		u16 iCommand = Memory_Read_U16(uAddress);
		uAddress += 2;
		switch (iCommand)
		{
	    case 0x0000: //00
		    Addr__AXStudio = Memory_Read_U32(uAddress);
		    uAddress += 4;
		    SaveLog("%08x : AXLIST studio address: %08x", uAddress, Addr__AXStudio);
		    break;

	    case 0x0001:
		    {
		    u32 address = Memory_Read_U32(uAddress);
		    uAddress += 4;
		    }
		    break;

		case 0x0003:
		    {
		    u32 address = Memory_Read_U32(uAddress);
		    uAddress += 4;
		    }
		    break;

	    case 0x0004:  // PBs are here now
		    m_addressPBs = Memory_Read_U32(uAddress);
			lCUCode_AX->m_addressPBs = m_addressPBs; // for the sake of logging

		    uAddress += 4;
		    break;

	    case 0x0005:
			if(Memory_Read_U16(uAddress) > 25) // this occured in Wii Sports
			{
				Addr__5_1 = Memory_Read_U32(uAddress);
				uAddress += 4;
				Addr__5_2 = Memory_Read_U32(uAddress);
				uAddress += 4;
				
				uAddress += 2;
			}
			else
			{
				uAddress += 2;
				SaveLog("%08x : AXLIST Empty 0x0005", uAddress);
			}
		    break;

	    case 0x0006:
		    Addr__6   = Memory_Read_U32(uAddress);
		    uAddress += 10;
		    SaveLog("%08x : AXLIST 6 address: %08x", uAddress, Addr__6);
		    break; 

/**/	    case 0x0007:   // AXLIST_SBUFFER
		    Addr__AXOutSBuffer = Memory_Read_U32(uAddress);
		    uAddress += 10;
			// uAddress += 12;
		    SaveLog("%08x : AXLIST OutSBuffer (0x0007) address: %08x", uAddress, Addr__AXOutSBuffer);			
		    break;

/*	    case 0x0009:
		    Addr__9   = Memory_Read_U32(uAddress);
		    uAddress += 4;
		    DebugLog("AXLIST 6 address: %08x", Addr__9);
		    break;*/

		case 0x000a:  // AXLIST_COMPRESSORTABLE
		    Addr__A   = Memory_Read_U32(uAddress);
		    uAddress += 4;
		    //Addr__A   = Memory_Read_U32(uAddress);
		    uAddress += 4;
		    SaveLog("%08x : AXLIST CompressorTable address: %08x", uAddress, Addr__A);
		    break;

		case 0x000b:
			uAddress += 2;  // one 0x8000 in rabbids
			uAddress += 4 * 2; // then two RAM addressses
			break;

		case 0x000c:
			uAddress += 2;  // one 0x8000 in rabbids
			uAddress += 4 * 2; // then two RAM addressses
			break;

		case 0x000d:
			uAddress += 4 * 4;
			break;

	    case 0x000e:
			// This is the end.
			bExecuteList = false;
			SaveLog("%08x : AXLIST end, wii stylee.", uAddress);
			break;

	    default:
			{
		    static bool bFirst = true;
		    if (bFirst == true)
		    {
				// A little more descreet way to say that there is a problem, that also let you
				// take a look at the mail (and possible previous mails) in the debugger
				SaveLog("%08x : Unknown AX-Command 0x%04x", uAddress, iCommand);
				bExecuteList = false;
				break;

			    char szTemp[2048];
				sprintf(szTemp, "Unknown AX-Command 0x%04x (address: 0x%08x). Last valid: %02x\n",
					    iCommand, uAddress - 2, last_valid_command);
			    int num = -32;
			    while (num < 64+32)
			    {
				    char szTemp2[128] = "";
					sprintf(szTemp2, "%s0x%04x\n", num == 0 ? ">>" : "  ", Memory_Read_U16(uAddress + num));
				    strcat(szTemp, szTemp2);
				    num += 2;
			    }

				// Wii AX will always show this
			    PanicAlert(szTemp);
			   // bFirst = false;
		    }

		    // unknown command so stop the execution of this TaskList
		    bExecuteList = false;
			}
			break;
		}
		if (bExecuteList)
			last_valid_command = iCommand;
	}
	SaveLog("AXTask - done, send resume");
	SaveLog("=====================================================================");
	SaveLog("End");

	// i hope resume is okay AX
	m_rMailHandler.PushMail(0xDCD10001);
	return true;
}
