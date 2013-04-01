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

#include "VolumeHandler.h"
#include "VolumeCreator.h"

namespace VolumeHandler
{

std::shared_ptr<DiscIO::IVolume> g_pVolume;

std::shared_ptr<DiscIO::IVolume> GetVolume()
{
	return g_pVolume;
}

void EjectVolume() 
{
	g_pVolume.reset();
}

bool SetVolumeName(const std::string& _rFullPath)
{
    g_pVolume = DiscIO::CreateVolumeFromFilename(_rFullPath);
	return (g_pVolume != nullptr);
}

void SetVolumeDirectory(const std::string& _rFullPath, bool _bIsWii, const std::string& _rApploader, const std::string& _rDOL)
{
    g_pVolume = DiscIO::CreateVolumeFromDirectory(_rFullPath, _bIsWii, _rApploader, _rDOL);
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

bool ReadToPtr(u8* ptr, u64 _dwOffset, u64 _dwLength)
{
    if (g_pVolume != nullptr && ptr)
    {
        g_pVolume->Read(_dwOffset, _dwLength, ptr);
        return true;
    }
    return false;
}

bool RAWReadToPtr( u8* ptr, u64 _dwOffset, u64 _dwLength )
{
	if (g_pVolume != nullptr && ptr)
	{
		g_pVolume->RAWRead(_dwOffset, _dwLength, ptr);
		return true;
	}
	return false;
}

bool IsValid()
{
    return (g_pVolume != nullptr);
}

bool IsWii()
{
	if (g_pVolume)
		return IsVolumeWiiDisc(*g_pVolume);

    return false;
}

} // end of namespace VolumeHandler
