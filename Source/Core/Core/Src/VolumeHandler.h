// Copyright (C) 2003 Dolphin Project.

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.0.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License 2.0 for more details.

// A copy of the GPL 2.0 should have been included with the program.
// If not, see http://www.gnu.org/licenses/

// Official SVN repository and contact information can be found at
// http://code.google.com/p/dolphin-emu/

// Disc volume handler. It's here because Wii discs can consist of multiple volumes.
// GC discs are seen as one big volume.

#ifndef _VOLUMEHANDLER_H_
#define _VOLUMEHANDLER_H_

#include <string>
#include "CommonTypes.h"
#include "Volume.h"

namespace VolumeHandler
{

bool SetVolumeName(const std::string& _rFullPath);
void SetVolumeDirectory(const std::string& _rFullPath, bool _bIsWii, const std::string& _rApploader = "", const std::string& _rDOL = "");

u32 Read32(u64 _Offset);
bool ReadToPtr(u8* ptr, u64 _dwOffset, u64 _dwLength);
bool RAWReadToPtr(u8* ptr, u64 _dwOffset, u64 _dwLength);

bool IsValid();
bool IsWii();

DiscIO::IVolume *GetVolume();

void EjectVolume();

}  // namespace

#endif
