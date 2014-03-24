// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "DiscIO/Filesystem.h"
#include "DiscIO/FileSystemGCWii.h"

namespace DiscIO
{

IFileSystem::IFileSystem(const IVolume *rVolume)
	: m_rVolume(rVolume)
{}


IFileSystem::~IFileSystem()
{}


IFileSystem* CreateFileSystem(const IVolume* rVolume)
{
	IFileSystem* pFileSystem = new CFileSystemGCWii(rVolume);

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
