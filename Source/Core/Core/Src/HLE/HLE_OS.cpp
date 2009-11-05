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
#include <string>

#include "Common.h"
#include "HLE_OS.h"

#include "../PowerPC/PowerPC.h"
#include "../HW/Memmap.h"

namespace HLE_OS
{

void GetStringVA(std::string& _rOutBuffer, u32 strReg = 3);

void HLE_OSPanic()
{
	std::string Error, Msg;
	GetStringVA(Error);
	GetStringVA(Msg, 5);

	PanicAlert("OSPanic: %s: %s", Error.c_str(), Msg.c_str());
	ERROR_LOG(OSREPORT, "%08x->%08x| OSPanic: %s: %s", LR, PC, Error.c_str(), Msg.c_str());

	NPC = LR;
}

// Generalized func for just printing string pointed to by r3.
void HLE_GeneralDebugPrint()
{
	std::string ReportMessage;
	GetStringVA(ReportMessage);
	NPC = LR;

	//PanicAlert("(%08x->%08x) %s", LR, PC, ReportMessage.c_str());
	NOTICE_LOG(OSREPORT, "%08x->%08x| %s", LR, PC, ReportMessage.c_str());	
}

// __write_console is slightly abnormal
void HLE_write_console()
{
	std::string ReportMessage;
	GetStringVA(ReportMessage, 4);
	NPC = LR;

	//PanicAlert("(%08x->%08x) %s", LR, PC, ReportMessage.c_str());
	NOTICE_LOG(OSREPORT, "%08x->%08x| %s", LR, PC, ReportMessage.c_str());	
}

void GetStringVA(std::string& _rOutBuffer, u32 strReg)
{
	_rOutBuffer = "";
	char ArgumentBuffer[256];
	u32 ParameterCounter = 4;    
	u32 FloatingParameterCounter = 1;
	char* pString = (char*)Memory::GetPointer(GPR(strReg));
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

			u64 Parameter;
			if (ParameterCounter > 10)
			{
				Parameter = Memory::Read_U32(GPR(1) + 0x8 + ((ParameterCounter - 11) * 4));
			}
			else
			{
				if ((*(pString-2) == 'l') && (*(pString-1) == 'l')) // hax, just seen this on sysmenu osreport
				{
					Parameter = GPR(++ParameterCounter);
					Parameter = (Parameter<<32)|GPR(++ParameterCounter);
				}
				else // normal, 32bit
					Parameter = GPR(ParameterCounter);
			}
			ParameterCounter++;

			switch(*pString)
			{
			case 's':
				_rOutBuffer += StringFromFormat(ArgumentBuffer, (char*)Memory::GetPointer((u32)Parameter));
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
