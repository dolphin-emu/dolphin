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

#include "../MailHandler.h"
#include "Mixer.h"

#include "UCodes.h"
#include "UCode_AXStructs.h"
#include "UCode_AX.h" // for some functions in CUCode_AX
#include "UCode_AXWii.h"
#include "UCode_AX_Voice.h"


CUCode_AXWii::CUCode_AXWii(DSPHLE *dsp_hle, u32 l_CRC)
	: IUCode(dsp_hle, l_CRC)
	, m_addressPBs(0xFFFFFFFF)
{
	// we got loaded
	m_rMailHandler.PushMail(DSP_INIT);

	templbuffer = new int[1024 * 1024];
	temprbuffer = new int[1024 * 1024];

	wiisportsHack = m_CRC == 0xfa450138;
}

CUCode_AXWii::~CUCode_AXWii()
{
	m_rMailHandler.Clear();
	delete [] templbuffer;
	delete [] temprbuffer;
}

void CUCode_AXWii::HandleMail(u32 _uMail)
{
	if (m_UploadSetupInProgress) 
	{
		PrepareBootUCode(_uMail);
		return;
	}
	else if ((_uMail & 0xFFFF0000) == MAIL_AX_ALIST)
	{
		// We are expected to get a new CmdBlock
		DEBUG_LOG(DSPHLE, "GetNextCmdBlock (%ibytes)", (u16)_uMail);
	}
	else switch(_uMail)
	{
		case 0xCDD10000: // Action 0 - AX_ResumeTask()
			m_rMailHandler.PushMail(DSP_RESUME);
			break;

		case 0xCDD10001: // Action 1 - new ucode upload
			DEBUG_LOG(DSPHLE,"DSP IROM - New Ucode!");
			// TODO find a better way to protect from HLEMixer?
			soundStream->GetMixer()->SetHLEReady(false);
			m_UploadSetupInProgress = true;
			break;

		case 0xCDD10002: // Action 2 - IROM_Reset(); ( WII: De Blob, Cursed Mountain,...)
			DEBUG_LOG(DSPHLE,"DSP IROM - Reset!");
			m_DSPHLE->SetUCode(UCODE_ROM);
			return;

		case 0xCDD10003: // Action 3 - AX_GetNextCmdBlock()
			break;

		default:
			DEBUG_LOG(DSPHLE, " >>>> u32 MAIL : AXTask Mail (%08x)", _uMail);
			AXTask(_uMail);
			break;
	}
}

void CUCode_AXWii::MixAdd(short* _pBuffer, int _iSize)
{
	AXPBWii PB;

	if (_iSize > 1024 * 1024)
		_iSize = 1024 * 1024;

	memset(templbuffer, 0, _iSize * sizeof(int));
	memset(temprbuffer, 0, _iSize * sizeof(int));

	u32 blockAddr = m_addressPBs;
	if (!blockAddr)
		return;

	for (int i = 0; i < NUMBER_OF_PBS; i++)
	{
		if (!ReadPB(blockAddr, PB))
			break;

		if (wiisportsHack)
			MixAddVoice(*(AXPBWiiSports*)&PB, templbuffer, temprbuffer, _iSize);
		else
			MixAddVoice(PB, templbuffer, temprbuffer, _iSize);

		if (!WritePB(blockAddr, PB))
			break;
		
		// next PB, or done
		blockAddr = (PB.next_pb_hi << 16) | PB.next_pb_lo;
		if (!blockAddr)
			break;
	}		

	// We write the sound to _pBuffer
	if (_pBuffer)
	{
		for (int i = 0; i < _iSize; i++)
		{
			// Clamp into 16-bit. Maybe we should add a volume compressor here.
			int left  = templbuffer[i] + _pBuffer[0];
			int right = temprbuffer[i] + _pBuffer[1];
			if (left  < -32767)  left = -32767;
			else if (left  >  32767)  left =  32767;
			if (right < -32767) right = -32767;
			else if (right >  32767) right =  32767;
			*_pBuffer++ = left;
			*_pBuffer++ = right;
		}
	}
}


void CUCode_AXWii::Update(int cycles)
{
	if (NeedsResumeMail())
	{
		m_rMailHandler.PushMail(DSP_RESUME);
		DSP::GenerateDSPInterruptFromDSPEmu(DSP::INT_DSP);
	}
	// check if we have to send something
	else if (!m_rMailHandler.IsEmpty())
	{
		DSP::GenerateDSPInterruptFromDSPEmu(DSP::INT_DSP);
	}
}

// AX seems to bootup one task only and waits for resume-callbacks
// everytime the DSP has "spare time" it sends a resume-mail to the CPU
// and the __DSPHandler calls a AX-Callback which generates a new AXFrame
bool CUCode_AXWii::AXTask(u32& _uMail)
{
	u32 uAddress = _uMail;
	//u32 Addr__AXStudio;
	//u32 Addr__AXOutSBuffer;
	bool bExecuteList = true;

/*
	for (int i=0;i<64;i++) {
		NOTICE_LOG(DSPHLE,"%x - %08x",uAddress+(i*4),HLEMemory_Read_U32(uAddress+(i*4)));
	}
*/

	while (bExecuteList)
	{
		u16 iCommand = HLEMemory_Read_U16(uAddress);
		uAddress += 2;
		//NOTICE_LOG(DSPHLE,"AXWII - AXLIST CMD %X",iCommand);

		switch (iCommand)
		{
	    case 0x0000: 
		    //Addr__AXStudio = HLEMemory_Read_U32(uAddress);
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
			m_addressPBs = HLEMemory_Read_U32(uAddress);
			if (soundStream)
				soundStream->GetMixer()->SetHLEReady(true);
//			soundStream->Update();
			uAddress += 4;
		    break;

	    case 0x0005:
			if (!wiisportsHack)
				uAddress += 10;
		    break;

	    case 0x0006:
		    uAddress += 10;
		    break; 

		case 0x0007:   // AXLIST_SBUFFER
		    //Addr__AXOutSBuffer = HLEMemory_Read_U32(uAddress);
		    uAddress += 10;
		    break;

		case 0x0008:
		    uAddress += 26;
			break;

		case 0x000a:
			uAddress += wiisportsHack ? 4 : 8; // AXLIST_COMPRESSORTABLE
		    break;

		case 0x000b:
			uAddress += wiisportsHack ? 2 : 10;
			break;

		case 0x000c:
			uAddress += wiisportsHack ? 8 : 10;
			break;

		case 0x000d:
			uAddress += 16;
			break;

	    case 0x000e:
			if (wiisportsHack) 
				uAddress += 16;
			else
				bExecuteList = false;
			break;

	    case 0x000f: // only for Wii Sports uCode
			bExecuteList = false;
			break;

	    default:
			INFO_LOG(DSPHLE,"DSPHLE - AXwii - AXLIST - Unknown CMD: %x",iCommand);
		    // unknown command so stop the execution of this TaskList
		    bExecuteList = false;
			break;
		}
	}

	m_rMailHandler.PushMail(DSP_YIELD); //its here in case there is a CMD fuckup
	return true;
}

void CUCode_AXWii::DoState(PointerWrap &p)
{
	std::lock_guard<std::mutex> lk(m_csMix);

	p.Do(m_addressPBs);
	p.Do(wiisportsHack);

	DoStateShared(p);
}
