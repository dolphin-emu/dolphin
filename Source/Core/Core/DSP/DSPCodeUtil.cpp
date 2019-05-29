// Copyright 2009 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/DSP/DSPCodeUtil.h"

#include <algorithm>
#include <iostream>
#include <string>
#include <vector>

#include "Common/CommonTypes.h"
#include "Common/File.h"
#include "Common/FileUtil.h"
#include "Common/MsgHandler.h"
#include "Common/StringUtil.h"
#include "Common/Swap.h"

#include "Core/DSP/DSPAssembler.h"
#include "Core/DSP/DSPDisassembler.h"

namespace DSP
{
bool Assemble(const std::string& text, std::vector<u16>& code, bool force)
{
  AssemblerSettings settings;
  // settings.pc = 0;
  // settings.decode_registers = false;
  // settings.decode_names = false;
  settings.force = force;
  // settings.print_tabs = false;
  // settings.ext_separator = '\'';

  // TODO: fix the terrible api of the assembler.
  DSPAssembler assembler(settings);
  if (!assembler.Assemble(text, code))
  {
    std::cerr << assembler.GetErrorString() << std::endl;
    return false;
  }

  return true;
}

bool Disassemble(const std::vector<u16>& code, bool line_numbers, std::string& text)
{
  if (code.empty())
    return false;

  AssemblerSettings settings;

  // These two prevent roundtripping.
  settings.show_hex = true;
  settings.show_pc = line_numbers;
  settings.ext_separator = '\'';
  settings.decode_names = true;
  settings.decode_registers = true;

  DSPDisassembler disasm(settings);
  bool success = disasm.Disassemble(code, text);
  return success;
}

bool Compare(const std::vector<u16>& code1, const std::vector<u16>& code2)
{
  if (code1.size() != code2.size())
    printf("Size difference! 1=%zu 2=%zu\n", code1.size(), code2.size());
  u32 count_equal = 0;
  const int min_size = std::min<int>((int)code1.size(), (int)code2.size());

  AssemblerSettings settings;
  DSPDisassembler disassembler(settings);
  for (int i = 0; i < min_size; i++)
  {
    if (code1[i] == code2[i])
    {
      count_equal++;
    }
    else
    {
      std::string line1, line2;
      u16 pc = i;
      disassembler.DisassembleOpcode(&code1[0], &pc, line1);
      pc = i;
      disassembler.DisassembleOpcode(&code2[0], &pc, line2);
      printf("!! %04x : %04x vs %04x - %s  vs  %s\n", i, code1[i], code2[i], line1.c_str(),
             line2.c_str());
    }
  }
  if (code2.size() != code1.size())
  {
    printf("Extra code words:\n");
    const std::vector<u16>& longest = code1.size() > code2.size() ? code1 : code2;
    for (int i = min_size; i < (int)longest.size(); i++)
    {
      u16 pc = i;
      std::string line;
      disassembler.DisassembleOpcode(&longest[0], &pc, line);
      printf("!! %s\n", line.c_str());
    }
  }
  printf("Equal instruction words: %i / %i\n", count_equal, min_size);
  return code1.size() == code2.size() && code1.size() == count_equal;
}

std::string CodeToBinaryStringBE(const std::vector<u16>& code)
{
  std::string str(code.size() * 2, '\0');

  for (size_t i = 0; i < code.size(); i++)
  {
    str[i * 2 + 0] = code[i] >> 8;
    str[i * 2 + 1] = code[i] & 0xff;
  }

  return str;
}

std::vector<u16> BinaryStringBEToCode(const std::string& str)
{
  std::vector<u16> code(str.size() / 2);

  for (size_t i = 0; i < code.size(); i++)
  {
    code[i] = ((u16)(u8)str[i * 2 + 0] << 8) | ((u16)(u8)str[i * 2 + 1]);
  }

  return code;
}

std::optional<std::vector<u16>> LoadBinary(const std::string& filename)
{
  std::string buffer;
  if (!File::ReadFileToString(filename, buffer))
    return std::nullopt;

  return std::make_optional(BinaryStringBEToCode(buffer));
}

bool SaveBinary(const std::vector<u16>& code, const std::string& filename)
{
  const std::string buffer = CodeToBinaryStringBE(code);

  return File::WriteStringToFile(filename, buffer);
}

bool DumpDSPCode(const u8* code_be, int size_in_bytes, u32 crc)
{
  const std::string root_name =
      File::GetUserPath(D_DUMPDSP_IDX) + StringFromFormat("DSP_UC_%08X", crc);
  const std::string binary_file = root_name + ".bin";
  const std::string text_file = root_name + ".txt";

  if (!File::IOFile(binary_file, "wb").WriteBytes(code_be, size_in_bytes))
  {
    PanicAlert("Can't dump UCode to file '%s'!!", binary_file.c_str());
    return false;
  }

  // The disassembler works in native endian
  std::vector<u16> code(size_in_bytes / 2);
  for (size_t i = 0; i < code.size(); i++)
    code[i] = Common::swap16(&code_be[i * 2]);

  std::string text;
  if (!Disassemble(code, true, text))
    return false;

  return File::WriteStringToFile(text_file, text);
}

}  // namespace DSP
