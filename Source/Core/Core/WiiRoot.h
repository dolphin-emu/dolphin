// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

namespace Core
{
enum class RestoreReason
{
  EmulationEnd,
  CrashRecovery,
};

void InitializeWiiRoot(bool use_temporary);
void ShutdownWiiRoot();

void BackupWiiSettings();
void RestoreWiiSettings(RestoreReason reason);

// Initialize or clean up the filesystem contents.
void InitializeWiiFileSystemContents();
void CleanUpWiiFileSystemContents();
}  // namespace Core
