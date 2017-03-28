// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <vector>

#include "Common/CommonTypes.h"

namespace IOS
{
namespace ES
{
struct Content;
class TMDReader;

TMDReader FindImportTMD(u64 title_id);
TMDReader FindInstalledTMD(u64 title_id);

// Get installed titles (in /title) without checking for TMDs at all.
std::vector<u64> GetInstalledTitles();
// Get titles which are being imported (in /import) without checking for TMDs at all.
std::vector<u64> GetTitleImports();
// Get titles for which there is a ticket (in /ticket).
std::vector<u64> GetTitlesWithTickets();

std::vector<Content> GetStoredContentsFromTMD(const TMDReader& tmd);

// Start a title import.
bool InitImport(u64 title_id);
// Clean up the import content directory and move it back to /title.
bool FinishImport(const IOS::ES::TMDReader& tmd);
// Write a TMD for a title in /import atomically.
bool WriteImportTMD(const IOS::ES::TMDReader& tmd);
}  // namespace ES
}  // namespace IOS
