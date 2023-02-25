// Copyright 2009 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <optional>
#include <string>
#include <vector>

#include "Common/CommonTypes.h"

namespace DSP
{
bool Assemble(const std::string& text, std::vector<u16>& code, bool force = false);
bool Disassemble(const std::vector<u16>& code, bool line_numbers, std::string& text);
bool Compare(const std::vector<u16>& code1, const std::vector<u16>& code2);

// Big-endian, for writing straight to file using File::WriteStringToFile.
std::string CodeToBinaryStringBE(const std::vector<u16>& code);
std::vector<u16> BinaryStringBEToCode(const std::string& str);

// Load code (big endian binary).
std::optional<std::vector<u16>> LoadBinary(const std::string& filename);
bool SaveBinary(const std::vector<u16>& code, const std::string& filename);

bool DumpDSPCode(const u8* code_be, size_t size_in_bytes, u32 crc);
}  // namespace DSP
