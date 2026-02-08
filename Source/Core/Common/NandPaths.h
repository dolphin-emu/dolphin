// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <optional>
#include <string>

#include "Common/CommonTypes.h"

namespace Common
{
enum class FromWhichRoot
{
  Configured,  // not related to currently running game - use D_WIIROOT_IDX
  Session,     // request from currently running game - use D_SESSION_WIIROOT_IDX
};

std::string RootUserPath(FromWhichRoot from);

// The following functions return paths relative to the NAND root.
// If a FromWhichRoot is passed, the NAND root on the host filesystem will be prepended to the path.
// TODO: remove the from parameter after all code is migrated off direct FS access.

// Returns /import/%08x/%08x. Intended for use by ES.
std::string GetImportTitlePath(u64 title_id, std::optional<FromWhichRoot> from = {});

std::string GetTicketFileName(u64 title_id, std::optional<FromWhichRoot> from = {});
std::string GetV1TicketFileName(u64 title_id, std::optional<FromWhichRoot> from = {});
std::string GetTitlePath(u64 title_id, std::optional<FromWhichRoot> from = {});
std::string GetTitleDataPath(u64 title_id, std::optional<FromWhichRoot> from = {});
std::string GetTitleContentPath(u64 title_id, std::optional<FromWhichRoot> from = {});
std::string GetTMDFileName(u64 title_id, std::optional<FromWhichRoot> from = {});
std::string GetMiiDatabasePath(std::optional<FromWhichRoot> from = {});

// Returns whether a path is within an installed title's directory.
bool IsTitlePath(const std::string& path, std::optional<FromWhichRoot> from = {},
                 u64* title_id = nullptr);

// Escapes characters that are invalid or have special meanings in the host file system
std::string EscapeFileName(const std::string& filename);
// Escapes characters that are invalid or have special meanings in the host file system
std::string EscapePath(const std::string& path);
// Reverses escaping done by EscapeFileName
std::string UnescapeFileName(const std::string& filename);
// Tests for a file name being "safe" as per the escaping defined in EscapeFileName
bool IsFileNameSafe(const std::string_view filename);
}  // namespace Common
