// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <string>

#include "Common/CommonTypes.h"
#include "Common/Logging/Log.h"
#include "Common/MsgHandler.h"
#include "Common/StringUtil.h"
#include "Core/HLE/HLE_OS.h"
#include "Core/HW/Memmap.h"
#include "Core/PowerPC/PowerPC.h"

namespace HLE_OS
{
std::string GetStringVA(u32 strReg = 3);

void HLE_OSPanic()
{
  std::string error = GetStringVA();
  std::string msg = GetStringVA(5);

  PanicAlert("OSPanic: %s: %s", error.c_str(), msg.c_str());
  ERROR_LOG(OSREPORT, "%08x->%08x| OSPanic: %s: %s", LR, PC, error.c_str(), msg.c_str());

  NPC = LR;
}

// Generalized func for just printing string pointed to by r3.
void HLE_GeneralDebugPrint()
{
  std::string report_message;

  // Is gpr3 pointing to a pointer rather than an ASCII string
  if (PowerPC::HostRead_U32(GPR(3)) > 0x80000000)
  {
    if (GPR(4) > 0x80000000)
    {
      // ___blank(void* this, const char* fmt, ...);
      report_message = GetStringVA(4);
    }
    else
    {
      // ___blank(void* this, int log_type, const char* fmt, ...);
      report_message = GetStringVA(5);
    }
  }
  else
  {
    if (GPR(3) > 0x80000000)
    {
      // ___blank(const char* fmt, ...);
      report_message = GetStringVA();
    }
    else
    {
      // ___blank(int log_type, const char* fmt, ...);
      report_message = GetStringVA(4);
    }
  }

  NPC = LR;

  NOTICE_LOG(OSREPORT, "%08x->%08x| %s", LR, PC, SHIFTJISToUTF8(report_message).c_str());
}

// __write_console is slightly abnormal
void HLE_write_console()
{
  std::string report_message = GetStringVA(4);

  NPC = LR;

  NOTICE_LOG(OSREPORT, "%08x->%08x| %s", LR, PC, SHIFTJISToUTF8(report_message).c_str());
}

std::string GetStringVA(u32 strReg)
{
  std::string ArgumentBuffer;
  u32 ParameterCounter = strReg + 1;
  u32 FloatingParameterCounter = 1;

  std::string result;
  std::string string = PowerPC::HostGetString(GPR(strReg));

  for (size_t i = 0; i < string.size(); i++)
  {
    if (string[i] == '%')
    {
      ArgumentBuffer = '%';
      i++;
      if (string[i] == '%')
      {
        result += '%';
        continue;
      }

      while (i < string.size() &&
             (string[i] < 'A' || string[i] > 'z' || string[i] == 'l' || string[i] == '-'))
      {
        ArgumentBuffer += string[i++];
      }
      if (i >= string.size())
        break;

      ArgumentBuffer += string[i];

      u64 Parameter;
      if (ParameterCounter > 10)
      {
        Parameter = PowerPC::HostRead_U32(GPR(1) + 0x8 + ((ParameterCounter - 11) * 4));
      }
      else
      {
        if (string[i - 1] == 'l' &&
            string[i - 2] == 'l')  // hax, just seen this on sysmenu osreport
        {
          Parameter = GPR(++ParameterCounter);
          Parameter = (Parameter << 32) | GPR(++ParameterCounter);
        }
        else  // normal, 32bit
          Parameter = GPR(ParameterCounter);
      }
      ParameterCounter++;

      switch (string[i])
      {
      case 's':
        result += StringFromFormat(ArgumentBuffer.c_str(),
                                   PowerPC::HostGetString((u32)Parameter).c_str());
        break;

      case 'd':
      case 'i':
      {
        result += StringFromFormat(ArgumentBuffer.c_str(), Parameter);
        break;
      }

      case 'f':
      {
        result += StringFromFormat(ArgumentBuffer.c_str(), rPS0(FloatingParameterCounter));
        FloatingParameterCounter++;
        ParameterCounter--;
        break;
      }

      case 'p':
        // Override, so 64bit Dolphin prints 32bit pointers, since the ppc is 32bit :)
        result += StringFromFormat("%x", (u32)Parameter);
        break;

      case 'n':
        PowerPC::HostWrite_U32(static_cast<u32>(result.size()), static_cast<u32>(Parameter));
        // %n doesn't output anything, so the result variable is untouched
        break;

      default:
        result += StringFromFormat(ArgumentBuffer.c_str(), Parameter);
        break;
      }
    }
    else
    {
      result += string[i];
    }
  }

  if (!result.empty() && result.back() == '\n')
    result.pop_back();

  return result;
}

}  // end of namespace HLE_OS
