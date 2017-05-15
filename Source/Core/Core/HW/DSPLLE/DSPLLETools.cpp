// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#ifdef _WIN32
#include <Windows.h>
#endif

#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include "Common/CommonTypes.h"
#include "Common/FileUtil.h"
#include "Common/StringUtil.h"
#include "Core/DSP/DSPCodeUtil.h"
#include "Core/DSP/DSPCore.h"
#include "Core/DSP/DSPDisassembler.h"
#include "Core/HW/DSPLLE/DSPLLETools.h"

namespace DSP
{
namespace LLE
{
// TODO make this useful :p
bool DumpCWCode(u32 _Address, u32 _Length)
{
  std::string filename = File::GetUserPath(D_DUMPDSP_IDX) + "DSP_UCode.bin";
  File::IOFile pFile(filename, "wb");

  if (pFile)
  {
    for (size_t i = _Address; i != _Address + _Length; ++i)
    {
      u16 val = g_dsp.iram[i];
      fprintf(pFile.GetHandle(), "    cw 0x%04x \n", val);
    }
    return true;
  }

  return false;
}
}  // namespace LLE
}  // namespace DSP
