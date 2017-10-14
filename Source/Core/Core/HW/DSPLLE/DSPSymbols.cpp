// Copyright 2009 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/HW/DSPLLE/DSPSymbols.h"

#include <cctype>
#include <list>
#include <map>
#include <string>
#include <vector>

#include "Common/CommonTypes.h"
#include "Common/File.h"
#include "Common/StringUtil.h"

#include "Core/DSP/DSPCore.h"
#include "Core/DSP/DSPDisassembler.h"

namespace DSP
{
namespace Symbols
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

Symbol* DSPSymbolDB::GetSymbolFromAddr(u32 addr)
{
  XFuncMap::iterator it = functions.find(addr);

  if (it != functions.end())
  {
    return &it->second;
  }
  else
  {
    for (auto& func : functions)
    {
      if (addr >= func.second.address && addr < func.second.address + func.second.size)
        return &func.second;
    }
  }
  return nullptr;
}

bool ReadAnnotatedAssembly(const std::string& filename)
{
  File::IOFile f(filename, "r");
  if (!f)
  {
    ERROR_LOG(DSPLLE, "Bah! ReadAnnotatedAssembly couldn't find the file %s", filename.c_str());
    return false;
  }
  char line[512];

  int last_addr = 0;

  lines.reserve(3000);

  // Symbol generation
  int brace_count = 0;
  bool symbol_in_progress = false;

  int symbol_count = 0;
  Symbol current_symbol;

  while (fgets(line, 512, f.GetHandle()))
  {
    // Scan string for the first 4-digit hex string.
    size_t len = strlen(line);
    int first_hex = -1;
    bool hex_found = false;
    for (unsigned int i = 0; i < strlen(line); i++)
    {
      const char c = line[i];
      if (isxdigit(c))
      {
        if (first_hex == -1)
        {
          first_hex = i;
        }
        else
        {
          // Remove hex notation
          if ((int)i == first_hex + 3 && (first_hex == 0 || line[first_hex - 1] != 'x') &&
              (i >= len - 1 || line[i + 1] == ' '))
          {
            hex_found = true;
            break;
          }
        }
      }
      else
      {
        if (i - first_hex < 3)
        {
          first_hex = -1;
        }
        if (isalpha(c))
          break;
      }
    }

    // Scan for function starts
    if (!memcmp(line, "void", 4))
    {
      char temp[256];
      for (size_t i = 6; i < len; i++)
      {
        if (line[i] == '(')
        {
          // Yep, got one.
          memcpy(temp, line + 5, i - 5);
          temp[i - 5] = 0;

          // Mark symbol so the next hex sets the address
          current_symbol.Rename(temp);
          current_symbol.address = 0xFFFF;
          current_symbol.index = symbol_count++;
          symbol_in_progress = true;

          // Reset brace count.
          brace_count = 0;
        }
      }
    }

    // Scan for braces
    for (size_t i = 0; i < len; i++)
    {
      if (line[i] == '{')
        brace_count++;
      if (line[i] == '}')
      {
        brace_count--;
        if (brace_count == 0 && symbol_in_progress)
        {
          // Commit this symbol.
          current_symbol.size = last_addr - current_symbol.address + 1;
          g_dsp_symbol_db.AddCompleteSymbol(current_symbol);
          current_symbol.address = 0xFFFF;
          symbol_in_progress = false;
        }
      }
    }

    if (hex_found)
    {
      int hex = 0;
      sscanf(line + first_hex, "%04x", &hex);

      // Sanity check
      if (hex > last_addr + 3 || hex < last_addr - 3)
      {
        static int errors = 0;
        INFO_LOG(DSPLLE, "Got Insane Hex Digit %04x (%04x) from %s", hex, last_addr, line);
        errors++;
        if (errors > 10)
        {
          return false;
        }
      }
      else
      {
        // if (line_counter >= 200 && line_counter <= 220)
        // 	NOTICE_LOG(DSPLLE, "Got Hex Digit %04x from %s, line %i", hex, line, line_counter);
        if (symbol_in_progress && current_symbol.address == 0xFFFF)
          current_symbol.address = hex;

        line_to_addr[line_counter] = hex;
        addr_to_line[hex] = line_counter;
        last_addr = hex;
      }
    }

    lines.push_back(TabsToSpaces(4, line));
    line_counter++;
  }

  return true;
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

}  // namespace Symbols
}  // namespace DSP
