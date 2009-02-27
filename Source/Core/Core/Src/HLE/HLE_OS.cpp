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
#include <string>

#include "Common.h"
#include "HLE_OS.h"

#include "../PowerPC/PowerPC.h"
#include "../HW/Memmap.h"

namespace HLE_OS
{

void GetStringVA(std::string& _rOutBuffer);

void HLE_OSPanic()
{
	std::string Error;
	GetStringVA(Error);

	PanicAlert("OSPanic: %s", Error.c_str());
	LOG(OSREPORT,"(PC=%08x), OSPanic: %s", LR, Error.c_str());

	NPC = LR;
}

void HLE_OSReport()
{
	std::string ReportMessage;
	GetStringVA(ReportMessage);
    NPC = LR;
	
    u32 hackPC = PC;
    PC = LR;

    PanicAlert("(PC=%08x) OSReport: %s", LR, ReportMessage.c_str());
	LOGV(OSREPORT,0,"(PC=%08x) OSReport: %s", LR, ReportMessage.c_str());	

    PC = hackPC;
}

void HLE_vprintf()
{
	std::string ReportMessage;
	GetStringVA(ReportMessage);
    NPC = LR;

    u32 hackPC = PC;
    PC = LR;

    PanicAlert("(PC=%08x) VPrintf: %s", LR, ReportMessage.c_str());
	LOG(OSREPORT,"(PC=%08x) VPrintf: %s", LR, ReportMessage.c_str());	

    PC = hackPC;
}

void HLE_printf()
{
	std::string ReportMessage;
	GetStringVA(ReportMessage);
    NPC = LR;

    u32 hackPC = PC;
    PC = LR;

    PanicAlert("(PC=%08x) Printf: %s ", LR, ReportMessage.c_str());
	LOG(OSREPORT,"(PC=%08x) Printf: %s ", LR, ReportMessage.c_str());	

    PC = hackPC;
}

void GetStringVA(std::string& _rOutBuffer)
{
	_rOutBuffer = "";
	char ArgumentBuffer[256];
	u32 ParameterCounter = 4;    
	u32 FloatingParameterCounter = 1;
	char* pString = (char*)Memory::GetPointer(GPR(3));
	if (!pString) {
		//PanicAlert("Invalid GetStringVA call");
		return;
	}

	while(*pString)
	{
		if (*pString == '%')
		{
			char* pArgument = ArgumentBuffer;
			*pArgument++ = *pString++;
			if(*pString == '%') {
				_rOutBuffer += "%";
				pString++;
				continue;
			}
			while(*pString < 'A' || *pString > 'z' || *pString == 'l' || *pString == '-')
				*pArgument++ = *pString++;

			*pArgument++ = *pString;
			*pArgument = NULL;

			u32 Parameter;
			if (ParameterCounter > 10)
			{
				Parameter = Memory::Read_U32(GPR(1) + 0x8 + ((ParameterCounter - 11) * 4));
			}
			else
			{
				Parameter = GPR(ParameterCounter);
			}
			ParameterCounter++;

			switch(*pString)
			{
			case 's':
				_rOutBuffer += StringFromFormat(ArgumentBuffer, (char*)Memory::GetPointer(Parameter));
				break;

			case 'd':
			case 'i':
				{
					//u64 Double = Memory::Read_U64(Parameter);
					_rOutBuffer += StringFromFormat(ArgumentBuffer, Parameter);
				}
				break;
			
			case 'f':
				{
					_rOutBuffer += StringFromFormat(ArgumentBuffer, 
													rPS0(FloatingParameterCounter));
					FloatingParameterCounter++;
					ParameterCounter--;
				}
				break;


			default:
				_rOutBuffer += StringFromFormat(ArgumentBuffer, Parameter);
				break;
			}
			pString++;
		}
		else
		{
			_rOutBuffer += StringFromFormat("%c", *pString); 
			pString++;
		}
	}
	if(_rOutBuffer[_rOutBuffer.length() - 1] == '\n')
		_rOutBuffer.resize(_rOutBuffer.length() - 1);
}

}
