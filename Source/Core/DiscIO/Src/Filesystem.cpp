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

#include "Filesystem.h"

#include "VolumeCreator.h"
#include "FileSystemGCWii.h"

namespace DiscIO
{

IFileSystem::IFileSystem(std::shared_ptr<const IVolume> _rVolume)
	: m_rVolume(_rVolume)
{}


IFileSystem::~IFileSystem()
{}


std::unique_ptr<IFileSystem> CreateFileSystem(std::shared_ptr<const IVolume> _rVolume)
{
	auto pFileSystem = make_unique<CFileSystemGCWii>(_rVolume);

	if (!pFileSystem)
		return nullptr;

	if (!pFileSystem->IsValid())
		pFileSystem.reset();

	return std::move(pFileSystem);
}

} // namespace
