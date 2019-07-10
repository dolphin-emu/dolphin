// Copyright 2009 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/HW/DSPLLE/DSPSymbols.h"

#include <map>
#include <string>
#include <vector>

#include "Common/CommonTypes.h"
#include "Common/Logging/Log.h"

#include "Core/DSP/DSPCore.h"
#include "Core/DSP/DSPDisassembler.h"

namespace DSP::Symbols
{
DSPSymbolDB g_dsp_symbol_db;

static std::map<u16, int> addr_to_line;
static std::map<int, u16> line_to_addr;
static std::map<int, const char*> line_to_symbol;
static std::vector<std::string> lines;
static int line_counter = 0;

int Addr2Line(u16 address)  // -1 for not found
{
  std::map<u16, int>::iterator iter = addr_to_line.find(address);
  if (iter != addr_to_line.end())
    return iter->second;
  else
    return -1;
}

int Line2Addr(int line)  // -1 for not found
{
  std::map<int, u16>::iterator iter = line_to_addr.find(line);
  if (iter != line_to_addr.end())
    return iter->second;
  else
    return -1;
}

const char* GetLineText(int line)
{
  if (line > 0 && line < (int)lines.size())
  {
    return lines[line].c_str();
  }
  else
  {
    return "----";
  }
}

Common::Symbol* DSPSymbolDB::GetSymbolFromAddr(u32 addr)
{
  auto it = m_functions.find(addr);

  if (it != m_functions.end())
    return &it->second;

  for (auto& func : m_functions)
  {
    if (addr >= func.second.address && addr < func.second.address + func.second.size)
      return &func.second;
  }

  return nullptr;
}

void AutoDisassembly(u16 start_addr, u16 end_addr)
{
  AssemblerSettings settings;
  settings.show_pc = true;
  settings.show_hex = true;
  DSPDisassembler disasm(settings);

  u16 addr = start_addr;
  const u16* ptr = (start_addr >> 15) ? g_dsp.irom : g_dsp.iram;
  while (addr < end_addr)
  {
    line_to_addr[line_counter] = addr;
    addr_to_line[addr] = line_counter;

    std::string buf;
    if (!disasm.DisassembleOpcode(ptr, &addr, buf))
    {
      ERROR_LOG(DSPLLE, "disasm failed at %04x", addr);
      break;
    }

    // NOTICE_LOG(DSPLLE, "Added %04x %i %s", addr, line_counter, buf.c_str());
    lines.push_back(buf);
    line_counter++;
  }
}

void Clear()
{
  addr_to_line.clear();
  line_to_addr.clear();
  lines.clear();
  line_counter = 0;
}
}  // namespace DSP::Symbols
