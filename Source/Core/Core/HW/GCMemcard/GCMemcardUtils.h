// Copyright 2020 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <string>
#include <variant>

#include "Core/HW/GCMemcard/GCMemcard.h"

namespace Memcard
{
bool HasSameIdentity(const DEntry& lhs, const DEntry& rhs);

// Check if any two given savefiles have the same identity.
bool HasDuplicateIdentity(const std::vector<Savefile>& savefiles);

enum class ReadSavefileErrorCode
{
  OpenFileFail,
  IOError,
  DataCorrupted,
};

// Reads a GameCube memory card savefile from a file.
// Supported formats are GCI, GCS (GameShark), and SAV (MaxDrive).
std::variant<ReadSavefileErrorCode, Savefile> ReadSavefile(const std::string& filename);

enum class SavefileFormat
{
  GCI,
  GCS,
  SAV,
};

// Writes a GameCube memory card savefile to a file.
bool WriteSavefile(const std::string& filename, const Savefile& savefile, SavefileFormat format);

// Generates a filename (without extension) for the given directory entry.
std::string GenerateFilename(const DEntry& entry);

// Returns the expected extension for a filename in the given format. Includes the leading dot.
std::string GetDefaultExtension(SavefileFormat format);

// Reads multiple savefiles from a card. Returns empty vector if even a single file can't be read.
std::vector<Savefile> GetSavefiles(const GCMemcard& card, const std::vector<u8>& file_indices);

// Gets the total amount of blocks the given saves use.
size_t GetBlockCount(const std::vector<Savefile>& savefiles);
}  // namespace Memcard
