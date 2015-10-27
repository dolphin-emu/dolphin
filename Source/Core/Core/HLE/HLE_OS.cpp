// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <string>

#include "Common/CommonTypes.h"
#include "Common/MsgHandler.h"
#include "Common/StringUtil.h"
#include "Common/Logging/Log.h"
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
	if (PowerPC::HostRead_U32(GPR(3)) > 0x80000000)
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
	std::string ArgumentBuffer = "";
	u32 ParameterCounter = strReg+1;
	u32 FloatingParameterCounter = 1;
	std::string string = PowerPC::HostGetString(GPR(strReg));

	for(u32 i = 0; i < string.size(); i++)
	{
		if (string[i] == '%')
		{
			ArgumentBuffer = "%";
			i++;
			if (string[i] == '%')
			{
				_rOutBuffer += "%";
				continue;
			}
			while (string[i] < 'A' || string[i] > 'z' || string[i] == 'l' || string[i] == '-')
				ArgumentBuffer += string[i++];

			ArgumentBuffer += string[i];

			u64 Parameter;
			if (ParameterCounter > 10)
			{
				Parameter = PowerPC::HostRead_U32(GPR(1) + 0x8 + ((ParameterCounter - 11) * 4));
			}
			else
			{
				if (string[i-1] == 'l' && string[i-2] == 'l') // hax, just seen this on sysmenu osreport
				{
					Parameter = GPR(++ParameterCounter);
					Parameter = (Parameter<<32)|GPR(++ParameterCounter);
				}
				else // normal, 32bit
					Parameter = GPR(ParameterCounter);
			}
			ParameterCounter++;

			switch (string[i])
			{
			case 's':
				_rOutBuffer += StringFromFormat(ArgumentBuffer.c_str(), PowerPC::HostGetString((u32)Parameter).c_str());
				break;

			case 'd':
			case 'i':
			{
				_rOutBuffer += StringFromFormat(ArgumentBuffer.c_str(), Parameter);
				break;
			}

			case 'f':
			{
				_rOutBuffer += StringFromFormat(ArgumentBuffer.c_str(),
												rPS0(FloatingParameterCounter));
				FloatingParameterCounter++;
				ParameterCounter--;
				break;
			}

			case 'p':
				// Override, so 64bit Dolphin prints 32bit pointers, since the ppc is 32bit :)
				_rOutBuffer += StringFromFormat("%x", (u32)Parameter);
				break;

			default:
				_rOutBuffer += StringFromFormat(ArgumentBuffer.c_str(), Parameter);
				break;
			}
		}
		else
		{
			_rOutBuffer += string[i];
		}
	}
	if (!_rOutBuffer.empty() && _rOutBuffer[_rOutBuffer.length() - 1] == '\n')
		_rOutBuffer.resize(_rOutBuffer.length() - 1);
}

}  // end of namespace HLE_OS
