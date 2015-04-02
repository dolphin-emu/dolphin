// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.


// Disc volume handler. It's here because Wii discs can consist of multiple volumes.
// GC discs are seen as one big volume.

#pragma once

#include <string>

#include "Common/CommonTypes.h"
#include "DiscIO/Volume/Volume.h"

namespace VolumeHandler
{

bool SetVolumeName(const std::string& _rFullPath);
void SetVolumeDirectory(const std::string& _rFullPath, bool _bIsWii, const std::string& _rApploader = "", const std::string& _rDOL = "");

// decrypt parameter must be false if not reading a Wii disc
u32 Read32(u64 _Offset, bool decrypt);
// decrypt parameter must be false if not reading a Wii disc
bool ReadToPtr(u8* ptr, u64 _dwOffset, u64 _dwLength, bool decrypt);

bool IsValid();
bool IsWiiDisc();

DiscIO::IVolume *GetVolume();

void EjectVolume();

} // end of namespace VolumeHandler
