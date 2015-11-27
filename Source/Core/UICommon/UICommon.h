// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "DiscIO/Volume.h"

namespace UICommon
{

void Init();
void Shutdown();

void CreateDirectories();
void SetUserDirectory(const std::string& custom_path);

u8 GetSADRCountryCode(DiscIO::IVolume::ELanguage language);

} // namespace UICommon
