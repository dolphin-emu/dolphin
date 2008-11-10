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

#include "../Debugger/Debugger.h"
#include "../Logging/Console.h" // for aprintf

#ifdef _WIN32
#include "../PCHW/DSoundStream.h"
#endif
#include "../PCHW/Mixer.h"
#include "../MailHandler.h"

#include "UCodes.h"
#include "UCode_AXStructs.h"
#include "UCode_AX.h" // for some functions in CUCode_AX
#include "UCode_AXWii.h"
#include "UCode_AX_Voice.h"


// ------------------------------------------------------------------
// Declarations
// -----------
extern u32 gLastBlock;
extern CDebugger * m_frame;
// -----------


CUCode_AXWii::CUCode_AXWii(CMailHandler& _rMailHandler)
	: IUCode(_rMailHandler)
	, m_addressPBs(0xFFFFFFFF)
{
	// we got loaded
	m_rMailHandler.PushMail(0xDCD10000);
	m_rMailHandler.PushMail(0x80000000);  // handshake ??? only (crc == 0xe2136399) needs it ...

	templbuffer = new int[1024 * 1024];
	temprbuffer = new int[1024 * 1024];

	lCUCode_AX = new CUCode_AX(_rMailHandler);
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

int ReadOutPBsWii(u32 pbs_address, AXParamBlockWii* _pPBs, int _num)
{
	int count = 0;
	u32 blockAddr = pbs_address;
	u32 pAddr = 0;

	// reading and 'halfword' swap
	for (int i = 0; i < _num; i++)
	{
		const short *pSrc = (const short *)g_dspInitialize.pGetMemoryPointer(blockAddr);
		pAddr = blockAddr;
		u32 nextBlock = (Common::swap16(pSrc[162]) << 16 | Common::swap16(pSrc[163]));
		if (pSrc != NULL && nextBlock == blockAddr + 320) // new way to stop
		{
			short *pDest = (short *)&_pPBs[i];
			for (int p = 0; p < sizeof(AXParamBlockWii) / 2; p++)
			{		
				pDest[p] = Common::swap16(pSrc[p]);

				#if defined(_DEBUG) || defined(DEBUGFAST)
					gLastBlock = blockAddr + p*2 + 2;  // save last block location
				#endif
			}
			blockAddr = (_pPBs[i].next_pb_hi << 16) | _pPBs[i].next_pb_lo;
			count++;
		}
		else
			break;
	}

	// return the number of read PBs
	return count;
}

void WriteBackPBsWii(u32 pbs_address, AXParamBlockWii* _pPBs, int _num)
{
	u32 blockAddr = pbs_address;

	// write back and 'halfword'swap
	for (int i = 0; i < _num; i++)
	{
		short* pSrc  = (short*)&_pPBs[i];
		short* pDest = (short*)g_dspInitialize.pGetMemoryPointer(blockAddr);
		for (size_t p = 0; p < sizeof(AXParamBlockWii) / 2; p++)
		{			
			pDest[p] = Common::swap16(pSrc[p]);
		}

		// next block
		blockAddr = (_pPBs[i].next_pb_hi << 16) | _pPBs[i].next_pb_lo;
	}
}

void CUCode_AXWii::MixAdd(short* _pBuffer, int _iSize)
{
	AXParamBlockWii PBs[NUMBER_OF_PBS];

	// read out pbs
	int numberOfPBs = ReadOutPBsWii(m_addressPBs, PBs, NUMBER_OF_PBS);

	if (_iSize > 1024 * 1024)
		_iSize = 1024 * 1024;

	memset(templbuffer, 0, _iSize * sizeof(int));
	memset(temprbuffer, 0, _iSize * sizeof(int));

	// write logging data to debugger
	if (m_frame)
	{
		lCUCode_AX->Logging(_pBuffer, _iSize, 0, true);
	}
	
	// ---------------------------------------------------------------------------------------
	// Make the updates we are told to do
	// This code is buggy, TODO - fix. If multiple updates in a ms, only does first.
	// ------------
	/*
	for (int i = 0; i < numberOfPBs; i++) {
		u16 *pDest = (u16 *)&PBs[i];
		u16 upd0 = pDest[34]; u16 upd1 = pDest[35]; u16 upd2 = pDest[36]; // num_updates
		u16 upd_hi = pDest[39]; // update addr
		u16	upd_lo = pDest[40];
		const u32 updaddr   = (u32)(upd_hi << 16) | upd_lo;
		const u16 updpar   = Memory_Read_U16(updaddr);
		const u16 upddata   = Memory_Read_U16(updaddr + 2);
		// some safety checks, I hope it's enough, how long does the memory go?
		if(updaddr > 0x80000000 && updaddr < 0x82000000
			&& updpar < 63 && updpar > 3 && upddata >= 0 // updpar > 3 because we don't want to change
			// 0-3, those are important
			&& (upd0 || upd1 || upd2 || upd3 || upd4) // We should use these in some way to I think
			// but I don't know how or when
			&& gSequenced) // on and off option
		{
			pDest[updpar] = upddata;
		}
	}*/

	//aprintf(1, "%08x %04x %04x\n", updaddr, updpar, upddata);
	// ------------

	for (int i = 0; i < numberOfPBs; i++)
	{		
		AXParamBlockWii& pb = PBs[i];
		MixAddVoice(pb, templbuffer, temprbuffer, _iSize);
	}	

	WriteBackPBsWii(m_addressPBs, PBs, numberOfPBs);

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

	// write logging data to debugger again after the update
	if (m_frame)
	{
		lCUCode_AX->Logging(_pBuffer, _iSize, 1, true);
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
void CUCode_AXWii::SaveLog(const char* _fmt, ...) { if(m_frame) lCUCode_AX->SaveLog_(true, _fmt); }


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
	u32 Addr__AXOutSBuffer_1;
	u32 Addr__AXOutSBuffer_2;
	u32 Addr__A;
	u32 Addr__12;
	u32 Addr__5_1;
	u32 Addr__5_2;
	u32 Addr__6;
	u32 Addr__9;

	bool bExecuteList = true;

	if(m_frame) lCUCode_AX->SaveMail(true, uAddress); // Save mail for debugging

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
		    SaveLog("%08x : AXLIST 1: %08x", uAddress, address);
		    }
		    break;

		case 0x0003:
		    {
		    u32 address = Memory_Read_U32(uAddress);
		    uAddress += 4;
		    SaveLog("%08x : AXLIST 3: %08x", uAddress, address);
		    }
		    break;

	    case 0x0004:  // PBs are here now
		    m_addressPBs = Memory_Read_U32(uAddress);
			lCUCode_AX->m_addressPBs = m_addressPBs; // for the sake of logging
		    mixer_HLEready = true;
		    SaveLog("%08x : AXLIST PB address: %08x", uAddress, m_addressPBs);
#ifdef _WIN32
		    DebugLog("Update the SoundThread to be in sync");
		    DSound::DSound_UpdateSound(); //do it in this thread to avoid sync problems
#endif
		    uAddress += 4;
		    break;

	    case 0x0005:
		    Addr__5_1 = Memory_Read_U32(uAddress);
		    uAddress += 4;
		    Addr__5_2 = Memory_Read_U32(uAddress);
		    uAddress += 4;
			
			uAddress += 2;
		    SaveLog("%08x : AXLIST 5_1 5_2 addresses: %08x %08x", uAddress, Addr__5_1, Addr__5_2);
		    break;

	    case 0x0006:
		    Addr__6   = Memory_Read_U32(uAddress);
		    uAddress += 10;
		    SaveLog("%08x : AXLIST 6 address: %08x", uAddress, Addr__6);
		    break; 

/*	    case 0x0007:   // AXLIST_SBUFFER
		    Addr__AXOutSBuffer = Memory_Read_U32(uAddress);
		    uAddress += 4;
			// uAddress += 12;
		    DebugLog("AXLIST OutSBuffer address: %08x", Addr__AXOutSBuffer);
		    break;*/

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
			    char szTemp[2048];
				sprintf(szTemp, "Unknown AX-Command 0x%x (address: 0x%08x). Last valid: %02x\n",
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
