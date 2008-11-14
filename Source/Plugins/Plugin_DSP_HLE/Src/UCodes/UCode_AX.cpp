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

#include "FileUtil.h" // for IsDirectory
#include "StringUtil.h" // for StringFromFormat
#include "../Debugger/Debugger.h"
#include "../Logging/Console.h" // for aprintf
#include <sstream>

#ifdef _WIN32
#include "../PCHW/DSoundStream.h"
#endif
#include "../PCHW/Mixer.h"
#include "../MailHandler.h"

#include "UCodes.h"
#include "UCode_AXStructs.h"
#include "UCode_AX.h"
#include "UCode_AX_Voice.h"


// ------------------------------------------------------------------
// Externals
// -----------
extern bool gSSBM;
extern bool gSSBMremedy1;
extern bool gSSBMremedy2;
extern bool gSequenced;
extern bool gVolume;
extern bool gReset;
extern std::string gpName;
extern CDebugger* m_frame;
std::vector<std::string> sMailLog, sMailTime;
// -----------


CUCode_AX::CUCode_AX(CMailHandler& _rMailHandler)
	: IUCode(_rMailHandler)
	, m_addressPBs(0xFFFFFFFF)
{
	// we got loaded
	m_rMailHandler.PushMail(0xDCD10000);
	m_rMailHandler.PushMail(0x80000000);  // handshake ??? only (crc == 0xe2136399) needs it ...

	templbuffer = new int[1024 * 1024];
	temprbuffer = new int[1024 * 1024];
}

CUCode_AX::~CUCode_AX()
{
	m_rMailHandler.Clear();
	delete [] templbuffer;
	delete [] temprbuffer;
}


// ============================================
// Save file to harddrive
// ----------------
void CUCode_AX::SaveLogFile(std::string f, int resizeTo, bool type, bool Wii)
{
	if(gpName.length() > 0) // thios is currently off in the Release build
	{
			std::ostringstream ci;
			std::ostringstream cType;
	        
		ci << (resizeTo - 1); // write ci
		cType << type; // write cType

		std::string FileName = FULL_MAIL_LOGS_DIR + gpName;
		FileName += "_sep"; FileName += ci.str(); FileName += "_sep"; FileName += cType.str();
		FileName += Wii ? "_sepWii_sep" : "_sepGC_sep"; FileName += ".log";

		FILE* fhandle = fopen(FileName.c_str(), "w");
		fprintf(fhandle, f.c_str());
		fflush(fhandle); fhandle = NULL;
	}
}


// ============================================
// Save the logged AX mail
// ----------------
void CUCode_AX::SaveLog_(bool Wii, const char* _fmt, va_list ap)
{
if(m_frame->ScanMails)
{	
	char Msg[512];
	vsprintf(Msg, _fmt, ap);

	//wxMessageBox( wxString::Format("SaveLog_ again: %s\n", Msg) );
	
	if(strcmp(Msg, "Begin") == 0)
	{
		TmpMailLog = "";
	}
	else if(strcmp(Msg, "End") == 0)
	{
		if(saveNext && saveNext < 100) // limit because saveNext is not initialized
		{		
			//wprintf("End");

			// Save the timestamps and comment
                        std::ostringstream ci;
                        ci << (saveNext - 1);
			TmpMailLog += "\n\n";
			TmpMailLog += "-----------------------------------------------------------------------\n";
			TmpMailLog += "Current mail: " + gpName + " mail " + ci.str() + "\n";
			if(Wii)
				TmpMailLog += "Current CRC: " + StringFromFormat("0x%08x \n\n", _CRC);			

			for (int i = 0; i < sMailTime.size(); i++)
			{
				char tmpbuf[128]; sprintf(tmpbuf, "Mail %i received: %s\n", i, sMailTime.at(i).c_str());
				TmpMailLog += tmpbuf;
			}
			TmpMailLog += "-----------------------------------------------------------------------";

			sMailLog.push_back(TmpMailLog);
			// Save file to disc
			if(m_frame->StoreMails)
			{
				SaveLogFile(TmpMailLog, saveNext, 1, Wii);
			}
			
			m_frame->DoUpdateMail(); // update the view
			saveNext = 0;
		}
	}
	else
	{
		TmpMailLog += Msg;
		TmpMailLog += "\n";
		LOG_(1, Msg); // also write it to the log
	}
}
}
// ----------------


// ============================================
// Save the whole AX mail
// ----------------
void CUCode_AX::SaveMail(bool Wii, u32 _uMail)
{
if(m_frame->ScanMails)
{
	int i = 0;
	std::string sTemp;
	std::string sTempEnd;
	std::string * sAct = &sTemp;
	bool doOnce = true; // for the while loop, to avoid getting stuck

	// Go through the mail
	while (i < 250)
	{
		// Make a new row for each AX-Command
		u16 axcomm = Memory_Read_U16(_uMail + i);
		if(axcomm < 15 && axcomm != 0) // we can at most write 8 messages per log
		{	
			*sAct += "\n";
		}

		char szTemp2[128] = "";
		sprintf(szTemp2, "%08x : 0x%04x\n", _uMail + i, axcomm);
		*sAct += szTemp2;

		 // set i to 160 so that we show some things after the end to			
		if ((axcomm == AXLIST_END || axcomm == 0x000e) && doOnce)
		{
			i = 160;
			sAct = &sTempEnd;
			doOnce = false;
		}

		i += 2;
	}

	// Compare this mail to old mails
	int addnew = 0;
	for (int i = 0; i < m_frame->sMail.size(); i++)
	{
		if(m_frame->sMail.at(i).length() != sTemp.length())
		{
			//wxMessageBox( wxString::Format("%s  \n\n%s", m_frame->sMail.at(i).c_str(),
			//	sTemp.c_str()) );
			addnew++;	
		}
	}


	// In case the mail didn't match any saved mail, save it
	if(addnew == m_frame->sMail.size())
	{		
		//wprintf("%i  |  %i\n", addnew, m_frame->sMail.size());
		int resizeTo = m_frame->sMail.size() + 1;		

		// ------------------------------------
		// get timestamp
		wxDateTime datetime = wxDateTime::UNow();	
		char Msg[128];
		sprintf(Msg, "%04i-%02i-%02i  %02i:%02i:%02i:%03i",
			datetime.GetYear(), datetime.GetMonth() + 1, datetime.GetDay(),
			datetime.GetHour(), datetime.GetMinute(), datetime.GetSecond(), datetime.GetMillisecond());
		sMailTime.push_back(Msg);
		// ------------------------------------

		m_frame->sMail.push_back(sTemp); // save the main comparison mail
		std::string lMail = sTemp +  "------------------\n" + sTempEnd;
		m_frame->sFullMail.push_back(lMail);

		// enable the radio button and update view
		if(resizeTo <= m_frame->m_RadioBox[3]->GetCount())
		{
			m_frame->m_RadioBox[3]->Enable(resizeTo - 1, true);
			m_frame->m_RadioBox[3]->Select(resizeTo - 1);
		}

		addnew = 0;
		saveNext = resizeTo; // save the log to

		// ------------------------------------
		// Save as file
		if(m_frame->StoreMails)
		{
			//wprintf("m_frame->sMail.size(): %i  |  resizeTo:%i\n", m_frame->sMail.size(), resizeTo);
			SaveLogFile(lMail, resizeTo, 0, Wii);
		}
		
	}
	sTemp = "";
	sTempEnd = "";
}
}
// ----------------


int ReadOutPBs(u32 pbs_address, AXParamBlock* _pPBs, int _num)
{
	int count = 0;
	u32 blockAddr = pbs_address;

	// reading and 'halfword' swap
	for (int i = 0; i < _num; i++)
	{
		const short *pSrc = (const short *)g_dspInitialize.pGetMemoryPointer(blockAddr);
		if (pSrc != NULL)
		{
			short *pDest = (short *)&_pPBs[i];
			for (size_t p = 0; p < sizeof(AXParamBlock) / 2; p++)
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

void WriteBackPBs(u32 pbs_address, AXParamBlock* _pPBs, int _num)
{
	u32 blockAddr = pbs_address;

	// write back and 'halfword'swap
	for (int i = 0; i < _num; i++)
	{
		short* pSrc  = (short*)&_pPBs[i];
		short* pDest = (short*)g_dspInitialize.pGetMemoryPointer(blockAddr);
		for (size_t p = 0; p < sizeof(AXParamBlock) / 2; p++)
		{
			pDest[p] = Common::swap16(pSrc[p]);
		}

		// next block
		blockAddr = (_pPBs[i].next_pb_hi << 16) | _pPBs[i].next_pb_lo;
	}
}

void CUCode_AX::MixAdd(short* _pBuffer, int _iSize)
{
	AXParamBlock PBs[NUMBER_OF_PBS];

	// read out pbs
	int numberOfPBs = ReadOutPBs(m_addressPBs, PBs, NUMBER_OF_PBS);

	if (_iSize > 1024 * 1024)
		_iSize = 1024 * 1024;

	memset(templbuffer, 0, _iSize * sizeof(int));
	memset(temprbuffer, 0, _iSize * sizeof(int));

	// write logging data to debugger
	if (m_frame)
	{
		CUCode_AX::Logging(_pBuffer, _iSize, 0, false);
	}
	
	// ---------------------------------------------------------------------------------------
	// Make the updates we are told to do
	// This code is buggy, TODO - fix. If multiple updates in a ms, only does first.
	// ------------
	for (int i = 0; i < numberOfPBs; i++) {
		u16 *pDest = (u16 *)&PBs[i];
		u16 upd0 = pDest[34]; u16 upd1 = pDest[35]; u16 upd2 = pDest[36]; // num_updates
		u16 upd3 = pDest[37]; u16 upd4 = pDest[38];
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
	}

	//aprintf(1, "%08x %04x %04x\n", updaddr, updpar, upddata);
	// ------------

	for (int i = 0; i < numberOfPBs; i++)
	{
		AXParamBlock& pb = PBs[i];
		MixAddVoice(pb, templbuffer, temprbuffer, _iSize, false);
	}

	// write back out pbs
	WriteBackPBs(m_addressPBs, PBs, numberOfPBs);

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
		CUCode_AX::Logging(_pBuffer, _iSize, 1, false);
	}
}


// ------------------------------------------------------------------------------
// Handle incoming mail
// -----------
void CUCode_AX::HandleMail(u32 _uMail)
{
	if ((_uMail & 0xFFFF0000) == MAIL_AX_ALIST)
	{
		// a new List
		DebugLog(" >>>> u32 MAIL : General Mail (%08x)", _uMail);
	}
	else
	{
		DebugLog(" >>>> u32 MAIL : AXTask Mail (%08x)", _uMail);
		AXTask(_uMail);
		
	}
}


// ------------------------------------------------------------------------------
// Update with DSP Interrupt
// -----------
void CUCode_AX::Update()
{
	// check if we have to sent something
	if (!m_rMailHandler.IsEmpty())
	{
		g_dspInitialize.pGenerateDSPInterrupt();
	}
}
// -----------


// Shortcut to avoid having to write SaveLog(false, ...) every time
void CUCode_AX::SaveLog(const char* _fmt, ...)
{
	va_list ap; va_start(ap, _fmt); if(m_frame) SaveLog_(false, _fmt, ap); va_end(ap);
}


// ============================================
// AX seems to bootup one task only and waits for resume-callbacks
// everytime the DSP has "spare time" it sends a resume-mail to the CPU
// and the __DSPHandler calls a AX-Callback which generates a new AXFrame
bool CUCode_AX::AXTask(u32& _uMail)
{
	u32 uAddress = _uMail;
	SaveLog("Begin");
	SaveLog("=====================================================================");
	SaveLog("%08x : AXTask - AXCommandList-Addr:", uAddress);

	u32 Addr__AXStudio;
	u32 Addr__AXOutSBuffer;
	u32 Addr__AXOutSBuffer_1;
	u32 Addr__AXOutSBuffer_2;
	u32 Addr__A;
	u32 Addr__12;
	u32 Addr__4_1;
	u32 Addr__4_2;
	u32 Addr__4_3;
	u32 Addr__4_4;
	u32 Addr__5_1;
	u32 Addr__5_2;
	u32 Addr__6;
	u32 Addr__9;

	bool bExecuteList = true;

	if(m_frame) SaveMail(false, _uMail); // Save mail for debugging

	while (bExecuteList)
	{
		static int last_valid_command = 0;
		u16 iCommand = Memory_Read_U16(uAddress);
		uAddress += 2;
		switch (iCommand)
		{
	    case AXLIST_STUDIOADDR: //00
		    Addr__AXStudio = Memory_Read_U32(uAddress);
		    uAddress += 4;
		    SaveLog("%08x : AXLIST studio address: %08x", uAddress, Addr__AXStudio);
		    break;

	    case 0x001: // 2byte x 10
	    {
		    u32 address = Memory_Read_U32(uAddress);
		    uAddress += 4;
		    u16 param1 = Memory_Read_U16(uAddress);
		    uAddress += 2;
		    u16 param2 = Memory_Read_U16(uAddress);
		    uAddress += 2;
		    u16 param3 = Memory_Read_U16(uAddress);
		    uAddress += 2;
		    SaveLog("%08x : AXLIST 1: %08x, %04x, %04x, %04x", uAddress, address, param1, param2, param3);
	    }
		    break;

	    //
	    // Somewhere we should be getting a bitmask of AX_SYNC values
	    // that tells us what has been updated
	    // Dunno if important
	    //
	    case AXLIST_PBADDR: //02
		    {
		    m_addressPBs = Memory_Read_U32(uAddress);
		    uAddress += 4;

		    mixer_HLEready = true;
		    SaveLog("%08x : AXLIST PB address: %08x", uAddress, m_addressPBs);
#ifdef _WIN32
		    SaveLog("Update the SoundThread to be in sync");
		    DSound::DSound_UpdateSound(); //do it in this thread to avoid sync problems
#endif
		    }
		    break;

	    case 0x0003:
		    SaveLog("%08x : AXLIST command 0x0003 ????");
		    break;

	    case 0x0004:  // AUX?
		    Addr__4_1 = Memory_Read_U32(uAddress);
		    uAddress += 4;
		    Addr__4_2 = Memory_Read_U32(uAddress);
		    uAddress += 4;
		    SaveLog("%08x : AXLIST 4_1 4_2 addresses: %08x %08x", uAddress, Addr__4_1, Addr__4_2);
		    break;

	    case 0x0005:
		    Addr__5_1 = Memory_Read_U32(uAddress);
		    uAddress += 4;
		    Addr__5_2 = Memory_Read_U32(uAddress);
		    uAddress += 4;
		    SaveLog("%08x : AXLIST 5_1 5_2 addresses: %08x %08x", uAddress, Addr__5_1, Addr__5_2);
		    break;

	    case 0x0006:
		    Addr__6   = Memory_Read_U32(uAddress);
		    uAddress += 4;
		    SaveLog("%08x : AXLIST 6 address: %08x", uAddress, Addr__6);
		    break;

	    case AXLIST_SBUFFER:
		    Addr__AXOutSBuffer = Memory_Read_U32(uAddress);
		    uAddress += 4;
		    SaveLog("%08x : AXLIST OutSBuffer address: %08x", uAddress, Addr__AXOutSBuffer);
		    break;

	    case 0x0009:
		    Addr__9   = Memory_Read_U32(uAddress);
		    uAddress += 4;
		    SaveLog("%08x : AXLIST 6 address: %08x", Addr__9);
		    break;

	    case AXLIST_COMPRESSORTABLE:  // 0xa
		    Addr__A   = Memory_Read_U32(uAddress);
		    uAddress += 4;
		    SaveLog("%08x : AXLIST CompressorTable address: %08x", uAddress, Addr__A);
		    break;

	    case 0x000e:
		    Addr__AXOutSBuffer_1 = Memory_Read_U32(uAddress);
		    uAddress += 4;

			// Addr__AXOutSBuffer_2 is the address in RAM that we are supposed to mix to.
			// Although we don't, currently.
		    Addr__AXOutSBuffer_2 = Memory_Read_U32(uAddress);
		    uAddress += 4;
		    SaveLog("%08x : AXLIST sbuf2 addresses: %08x %08x", uAddress, Addr__AXOutSBuffer_1, Addr__AXOutSBuffer_2);
		    break;

	    case AXLIST_END:
		    bExecuteList = false;
		    SaveLog("%08x : AXLIST end", uAddress);
		    break;

	    case 0x0010:  //Super Monkey Ball 2
		    SaveLog("%08x : AXLIST 0x0010", uAddress);
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
		    uAddress += 6 * 4; // 6 Addresses.
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
