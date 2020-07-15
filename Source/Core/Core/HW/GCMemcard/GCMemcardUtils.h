// Copyright 2020 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <string>
#include <variant>

#include "Core/HW/GCMemcard/GCMemcard.h"

namespace Memcard
{
bool HasSameIdentity(const DEntry& lhs, const DEntry& rhs);

enum class ReadSavefileErrorCode
{
  OpenFileFail,
  IOError,
  DataCorrupted,
};

// Reads a Gamecube memory card savefile from a file.
// Supported formats are GCI, GCS (Gameshark), and SAV (MaxDrive).
std::variant<ReadSavefileErrorCode, Savefile> ReadSavefile(const std::string& filename);

enum class SavefileFormat
{
  GCI,
  GCS,
  SAV,
};

// Writes a Gamecube memory card savefile to a file.
bool WriteSavefile(const std::string& filename, const Savefile& savefile, SavefileFormat format);

// Generates a filename (without extension) for the given directory entry.
std::string GenerateFilename(const DEntry& entry);
}  // namespace Memcard
