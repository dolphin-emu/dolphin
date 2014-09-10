// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include <string>

#include "Common/CommonTypes.h"
#include "Common/StringUtil.h"

#include "Core/HLE/HLE_OS.h"
#include "Core/HW/Memmap.h"
#include "Core/PowerPC/PowerPC.h"

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
	if (*(u32*)Memory::GetPointer(GPR(3)) > 0x80000000)
	{
		GetStringVA(ReportMessage, 4);
	}
	else
	{
		GetStringVA(ReportMessage);
	}
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
	u32 ParameterCounter = strReg+1;
	u32 FloatingParameterCounter = 1;
	char *pString = (char*)Memory::GetPointer(GPR(strReg));
	if (!pString)
	{
		ERROR_LOG(OSREPORT, "r%i invalid", strReg);
		return;
	}

	while (*pString)
	{
		if (*pString == '%')
		{
			char* pArgument = ArgumentBuffer;
			*pArgument++ = *pString++;
			if (*pString == '%')
			{
				_rOutBuffer += "%";
				pString++;
				continue;
			}
			while (*pString < 'A' || *pString > 'z' || *pString == 'l' || *pString == '-')
				*pArgument++ = *pString++;

			*pArgument++ = *pString;
			*pArgument = 0;

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

			switch (*pString)
			{
			case 's':
				_rOutBuffer += StringFromFormat(ArgumentBuffer, (char*)Memory::GetPointer((u32)Parameter));
				break;

			case 'd':
			case 'i':
			{
				//u64 Double = Memory::Read_U64(Parameter);
				_rOutBuffer += StringFromFormat(ArgumentBuffer, Parameter);
				break;
			}

			case 'f':
			{
				_rOutBuffer += StringFromFormat(ArgumentBuffer,
												rPS0(FloatingParameterCounter));
				FloatingParameterCounter++;
				ParameterCounter--;
				break;
			}

			case 'p':
				// Override, so 64bit dolphin prints 32bit pointers, since the ppc is 32bit :)
				_rOutBuffer += StringFromFormat("%x", (u32)Parameter);
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
	if (_rOutBuffer[_rOutBuffer.length() - 1] == '\n')
		_rOutBuffer.resize(_rOutBuffer.length() - 1);
}

}  // end of namespace HLE_OS
