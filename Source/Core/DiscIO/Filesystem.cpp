// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DiscIO/Filesystem.h"
#include "DiscIO/FileSystemGCWii.h"

namespace DiscIO
{

IFileSystem::IFileSystem(const IVolume *_rVolume)
	: m_rVolume(_rVolume)
{}


IFileSystem::~IFileSystem()
{}


IFileSystem* CreateFileSystem(const IVolume* _rVolume)
{
	IFileSystem* pFileSystem = new CFileSystemGCWii(_rVolume);

	if (!pFileSystem)
		return nullptr;

	if (!pFileSystem->IsValid())
	{
		delete pFileSystem;
		pFileSystem = nullptr;
	}

	return pFileSystem;
}

} // namespace
