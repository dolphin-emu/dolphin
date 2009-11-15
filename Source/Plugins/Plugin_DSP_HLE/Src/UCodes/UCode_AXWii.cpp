// Copyright (C) 2003 Dolphin Project.

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

#if defined(HAVE_WX) && HAVE_WX
#include "../Debugger/Debugger.h"
//#include "../Logging/File.h" // For PrintFile
extern DSPDebuggerHLE * m_DebuggerFrame;
#endif 

#include "../MailHandler.h"
#include "Mixer.h"

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
	else if (_uMail == 0xCDD10000) // Action 0 - restart
	{
		m_rMailHandler.PushMail(0xDCD10001);
	}
	else if ((_uMail & 0xFFFF0000) == 0xCDD10000) // Action 1/2/3
	{
	}
	else
	{
		AXTask(_uMail);
	}
}

template<class ParamBlockType> void ProcessUpdates(ParamBlockType &PB)
{
	// ---------------------------------------------------------------------------------------
	/* Make the updates we are told to do. See comments to the GC version in UCode_AX.cpp */
	// ------------
	u16 *pDest = (u16 *)&PB;
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
			//DEBUG_LOG(DSPHLE, "Update: %i = %04x", updpar, upddata);
			pDest[updpar] = upddata;
		}
		if (updpar == 7 && upddata == 1) on++;
		if (updpar == 7 && upddata == 1) off++;
	}
	// hack: if we get both an on and an off select on rather than off
	if (on > 0 && off > 0) pDest[7] = 1;
}

template<class ParamBlockType>
void CUCode_AXWii::MixAdd_(short* _pBuffer, int _iSize, ParamBlockType &PB)
{
	if (_iSize > 1024 * 1024)
		_iSize = 1024 * 1024;

	// write zeroes to the beginning of templbuffer
	memset(templbuffer, 0, _iSize * sizeof(int));
	memset(temprbuffer, 0, _iSize * sizeof(int));

	// -------------------------------------------
	// write logging data to debugger
#if defined(HAVE_WX) && HAVE_WX
	/*
	If this is to be resurrected, it has to be moved into the main PB loop below.
	if (m_DebuggerFrame && _pBuffer)
	{
		lCUCode_AX->Logging(_pBuffer, _iSize, 0, true);

		// -------------------------------------------
		// Write the first block values
		int p = numberOfPBs - 1;
		if(numberOfPBs > p)
		{
			if(PBs[p].running && !m_DebuggerFrame->upd95)
			{
				const u32 blockAddr = (u32)(PBs[p].this_pb_hi<< 16) | PBs[p].this_pb_lo;
				const short *pSrc = (const short *)g_dspInitialize.pGetMemoryPointer(blockAddr);
				for (u32 i = 0; i < sizeof(AXParamBlockWii) / 2; i+=2)
				{
					if(i == 10 || i == 34 || i == 41 || i == 46 || i == 46 || i == 58 || i == 60
						|| i == 68 || i == 88 || i == 95)
						{m_DebuggerFrame->str0 += "\n"; m_DebuggerFrame->str95 += "\n";}

					std::string line = StringFromFormat("%02i|%02i : %s : %s",
						i/2, i,
						m_DebuggerFrame->PBn[i].c_str(), m_DebuggerFrame->PBp[i].c_str()
						);
					for (u32 j = 0; j < 50 - line.length(); ++j)						
						line += " ";
						m_DebuggerFrame->str0 += line;

					m_DebuggerFrame->str0 += "\n"; 
					m_DebuggerFrame->str95 += StringFromFormat(" : %02i|%02i : %04x%04x\n",
						i/2, i,
						Common::swap16(pSrc[i]), Common::swap16(pSrc[i+1]));
				}
				m_DebuggerFrame->m_bl95->AppendText(wxString::FromAscii(m_DebuggerFrame->str95.c_str()));
				m_DebuggerFrame->m_bl0->AppendText(wxString::FromAscii(m_DebuggerFrame->str0.c_str()));
				m_DebuggerFrame->upd95 = true;
			}	
		}
	}*/
	// -----------------
#endif

	u32 blockAddr = m_addressPBs;
	if (!blockAddr)
		return;
	for (int i = 0; i < NUMBER_OF_PBS; i++)
	{
		// read out pbs
		if (!ReadOutPBWii(blockAddr, PB))
			break;
		ProcessUpdates(PB);
		MixAddVoice(PB, templbuffer, temprbuffer, _iSize, true);
		if (!WriteBackPBWii(blockAddr, PB))
			break;
		
		// next block		
		blockAddr = (PB.next_pb_hi << 16) | PB.next_pb_lo;
		if (blockAddr == 0) break;
	}		

	// We write the sound to _pBuffer
	if (_pBuffer)
	{
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
	
#if defined(HAVE_WX) && HAVE_WX
	// write logging data to debugger again after the update
	if (m_DebuggerFrame && _pBuffer)
	{
		lCUCode_AX->Logging(_pBuffer, _iSize, 1, true);
	}
#endif
}


void CUCode_AXWii::Update(int cycles)
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
#if defined(HAVE_WX) && HAVE_WX
	va_list ap; 
	va_start(ap, _fmt); 
	if(m_DebuggerFrame) 
	    lCUCode_AX->SaveLog_(true, _fmt, ap); 
	va_end(ap);
#endif
}


// AX seems to bootup one task only and waits for resume-callbacks
// everytime the DSP has "spare time" it sends a resume-mail to the CPU
// and the __DSPHandler calls a AX-Callback which generates a new AXFrame
bool CUCode_AXWii::AXTask(u32& _uMail)
{
	u32 uAddress = _uMail;
	u32 Addr__AXStudio;
	u32 Addr__AXOutSBuffer;
	bool bExecuteList = true;

#if defined(HAVE_WX) && HAVE_WX
	if(m_DebuggerFrame) lCUCode_AX->SaveMail(true, uAddress); // Save mail for debugging
#endif

/*
	for (int i=0;i<64;i++) {
		NOTICE_LOG(DSPHLE,"%x - %08x",uAddress+(i*4),Memory_Read_U32(uAddress+(i*4)));
	}
*/

	while (bExecuteList)
	{
		u16 iCommand = Memory_Read_U16(uAddress);
		uAddress += 2;
		//NOTICE_LOG(DSPHLE,"AXWII - AXLIST CMD %X",iCommand);

		switch (iCommand)
		{
	    case 0x0000: 
		    Addr__AXStudio = Memory_Read_U32(uAddress);
		    uAddress += 4;
		    break;

	    case 0x0001:
		    uAddress += 4;
		    break;

		case 0x0003:
		    uAddress += 4;
		    break;

	    case 0x0004: 
			// PBs are here now
			m_addressPBs = Memory_Read_U32(uAddress);
			lCUCode_AX->m_addressPBs = m_addressPBs; // for the sake of logging
			soundStream->GetMixer()->SetHLEReady(true);
			soundStream->Update();
			uAddress += 4;
		    break;

	    case 0x0005:
			if (_CRC != 0xfa450138) 
			{
				uAddress += 10;
			}
			else // Wii Sports uCode
			{
			}
		    break;

	    case 0x0006:
		    uAddress += 10;
		    break; 

		case 0x0007:   // AXLIST_SBUFFER
		    Addr__AXOutSBuffer = Memory_Read_U32(uAddress);
		    uAddress += 10;
		    break;

		case 0x000a:
			if (_CRC != 0xfa450138) // AXLIST_COMPRESSORTABLE
			{
			    uAddress += 8;
			}
			else  // Wii Sports uCode
			{
				uAddress += 4;
			}
		    break;

		case 0x000b:
			if (_CRC != 0xfa450138) 
			{
				uAddress += 10;
				m_rMailHandler.PushMail(0xDCD10004);
			}
			else // Wii Sports uCode
			{
				uAddress += 2;
			}
			break;

		case 0x000c:
			if (_CRC != 0xfa450138) 
			{
				uAddress += 10;
				m_rMailHandler.PushMail(0xDCD10004);
			}
			else  // Wii Sports uCode
			{
				uAddress += 8;
				m_rMailHandler.PushMail(0xDCD10004);
			}
			break;

		case 0x000d:
			if (_CRC != 0xfa450138) 
			{
				uAddress += 16;
			}
			else  // Wii Sports uCode 
			{
				uAddress += 16; //??
				m_rMailHandler.PushMail(0xDCD10004);
			}
			break;

	    case 0x000e:
			if (_CRC != 0xfa450138) 
			{
				// This is the end.
				bExecuteList = false;
				//m_rMailHandler.PushMail(0xDCD10002);
			}
			else  // Wii Sports uCode
			{
				uAddress += 16;
			}
			break;

	    case 0x000f: // only for Wii Sports uCode
			// This is the end.
			bExecuteList = false;
			//m_rMailHandler.PushMail(0xDCD10002);
			break;

	    default:
			ERROR_LOG(DSPHLE,"DSPHLE - AXwii - AXLIST - Unknown CMD: %x",iCommand);
		    // unknown command so stop the execution of this TaskList
		    bExecuteList = false;
			break;
		}
	}

	m_rMailHandler.PushMail(0xDCD10002); //its here in case there is a CMD fuckup
	return true;
}


void CUCode_AXWii::MixAdd(short* _pBuffer, int _iSize)
{
	if(_CRC == 0xfa450138)
	{
		AXParamBlockWii PB;
		MixAdd_( _pBuffer, _iSize, PB);
	}
	else
	{
		AXParamBlockWii_ PB;
		MixAdd_(_pBuffer, _iSize, PB);
	}
}
