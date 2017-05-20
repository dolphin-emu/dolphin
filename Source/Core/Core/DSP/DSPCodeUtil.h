// Copyright 2009 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <string>
#include <vector>

#include "Common/CommonTypes.h"

namespace DSP
{
bool Assemble(const std::string& text, std::vector<u16>& code, bool force = false);
bool Disassemble(const std::vector<u16>& code, bool line_numbers, std::string& text);
bool Compare(const std::vector<u16>& code1, const std::vector<u16>& code2);

// Big-endian, for writing straight to file using File::WriteStringToFile.
void CodeToBinaryStringBE(const std::vector<u16>& code, std::string& str);
void BinaryStringBEToCode(const std::string& str, std::vector<u16>& code);

// Load code (big endian binary).
bool LoadBinary(const std::string& filename, std::vector<u16>& code);
bool SaveBinary(const std::vector<u16>& code, const std::string& filename);

bool DumpDSPCode(const u8* code_be, int size_in_bytes, u32 crc);
}  // namespace DSP
