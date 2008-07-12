// Copyright (C) 2003-2008 Dolphin Project.

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

#include "VolumeHandler.h"
#include "VolumeCreator.h"

namespace VolumeHandler
{

DiscIO::IVolume* g_pVolume = NULL;

DiscIO::IVolume *GetVolume() {
	return g_pVolume;
}

void SetVolumeName(const std::string& _rFullPath)
{
    if (g_pVolume)
    {
        delete g_pVolume;
        g_pVolume = NULL;
    }

    g_pVolume = DiscIO::CreateVolumeFromFilename(_rFullPath);
}


u32 Read32(u64 _Offset)
{
    if (g_pVolume != NULL)
    {
        u32 Temp;
        g_pVolume->Read(_Offset, 4, (u8*)&Temp);
        return Common::swap32(Temp);
    }
    return 0;
}

bool ReadToPtr(u8* ptr, unsigned __int64 _dwOffset, unsigned __int64 _dwLength)
{
    if (g_pVolume != NULL)
    {
        g_pVolume->Read(_dwOffset, _dwLength, ptr);
        return true;
    }

    return false;
}

bool IsValid()
{
    return g_pVolume != NULL;
}

bool IsWii()
{
	if (g_pVolume)
		return IsVolumeWiiDisc(*g_pVolume);

    return false;
}

}