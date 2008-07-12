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

#include "stdafx.h"

#include <string>

#include "FileSystemGCWii.h"
#include "StringUtil.h"

namespace DiscIO
{
CFileSystemGCWii::CFileSystemGCWii(const IVolume& _rVolume)
	: IFileSystem(_rVolume),
	m_Initialized(false),
	m_OffsetShift(0)
{
	m_Initialized = InitFileSystem();
}


CFileSystemGCWii::~CFileSystemGCWii()
{}


bool
CFileSystemGCWii::IsInitialized()
{
	return(m_Initialized);
}


size_t
CFileSystemGCWii::GetFileSize(const char* _rFullPath)
{
	if (!m_Initialized)
	{
		return(0);
	}

	const SFileInfo* pFileInfo = FindFileInfo(_rFullPath);

	if (pFileInfo != NULL)
	{
		return(pFileInfo->m_FileSize);
	}

	return(0);
}


const char*
CFileSystemGCWii::GetFileName(u64 _Address)
{
	for (size_t i = 0; i < m_FileInfoVector.size(); i++)
	{
		if ((m_FileInfoVector[i].m_Offset <= _Address) &&
		    ((m_FileInfoVector[i].m_Offset + m_FileInfoVector[i].m_FileSize) > _Address))
		{
			return(m_FileInfoVector[i].m_FullPath);
		}
	}

	return(NULL);
}


size_t
CFileSystemGCWii::ReadFile(const char* _rFullPath, u8* _pBuffer, size_t _MaxBufferSize)
{
	if (!m_Initialized)
	{
		return(0);
	}

	const SFileInfo* pFileInfo = FindFileInfo(_rFullPath);

	if (pFileInfo == NULL)
	{
		return(0);
	}

	if (pFileInfo->m_FileSize > _MaxBufferSize)
	{
		return(0);
	}

	m_rVolume.Read(pFileInfo->m_Offset, pFileInfo->m_FileSize, _pBuffer);
	return(pFileInfo->m_FileSize);
}


bool
CFileSystemGCWii::ExportFile(const char* _rFullPath, const char* _rExportFilename)
{
	size_t filesize = GetFileSize(_rFullPath);

	if (filesize == 0)
	{
		return(false);
	}

	u8* buffer = new u8[filesize];

	if (!ReadFile(_rFullPath, buffer, filesize))
	{
		delete[] buffer;
		return(false);
	}

	FILE* f = fopen(_rExportFilename, "wb");

	if (f)
	{
		fwrite(buffer, filesize, 1, f);
		fclose(f);
		delete[] buffer;
		return(true);
	}

	delete[] buffer;
	return(false);
}


bool
CFileSystemGCWii::ExportAllFiles(const char* _rFullPath)
{
	return(false);
}


u32
CFileSystemGCWii::Read32(u64 _Offset) const
{
	u32 Temp = 0;
	m_rVolume.Read(_Offset, 4, (u8*)&Temp);
	return(Common::swap32(Temp));
}


void CFileSystemGCWii::GetStringFromOffset(u64 _Offset, char* Filename) const
{
	m_rVolume.Read(_Offset, 255, (u8*)Filename);
}


const CFileSystemGCWii::SFileInfo*
CFileSystemGCWii::FindFileInfo(const char* _rFullPath) const
{
	for (size_t i = 0; i < m_FileInfoVector.size(); i++)
	{
		if (!stricmp(m_FileInfoVector[i].m_FullPath, _rFullPath))
		{
			return(&m_FileInfoVector[i]);
		}
	}

	return(NULL);
}


bool
CFileSystemGCWii::InitFileSystem()
{
	if (Read32(0x18) == 0x5D1C9EA3)
	{
		m_OffsetShift = 2;
	}
	else if (Read32(0x1c) == 0xC2339F3D)
	{
		m_OffsetShift = 0;
	}
	else
	{
		return(false);
	}

	// read the whole FST
	u32 FSTOffset = Read32(0x424) << m_OffsetShift;
	// u32 FSTSize     = Read32(0x428);
	// u32 FSTMaxSize  = Read32(0x42C);


	// read all fileinfos
	SFileInfo Root;
	Root.m_NameOffset = Read32(FSTOffset + 0x0);
	Root.m_Offset     = (u64)Read32(FSTOffset + 0x4) << m_OffsetShift;
	Root.m_FileSize   = Read32(FSTOffset + 0x8);

	if (Root.IsDirectory())
	{
		m_FileInfoVector.resize(Root.m_FileSize);
		u64 NameTableOffset = FSTOffset;

		for (size_t i = 0; i < m_FileInfoVector.size(); i++)
		{
			u64 Offset = FSTOffset + (i * 0xC);
			m_FileInfoVector[i].m_NameOffset = Read32(Offset + 0x0);
			m_FileInfoVector[i].m_Offset     = (u64)Read32(Offset + 0x4) << m_OffsetShift;
			m_FileInfoVector[i].m_FileSize   = Read32(Offset + 0x8);
			NameTableOffset += 0xC;
		}

		BuildFilenames(1, m_FileInfoVector.size(), NULL, NameTableOffset);
	}

	return(true);
}


// __________________________________________________________________________________________________
//
// Changed this stuff from C++ string to C strings for speed in debug mode. Doesn't matter in release, but
// std::string is SLOW in debug mode.
size_t
CFileSystemGCWii::BuildFilenames(const size_t _FirstIndex, const size_t _LastIndex, const char* _szDirectory, u64 _NameTableOffset)
{
	size_t CurrentIndex = _FirstIndex;

	while (CurrentIndex < _LastIndex)
	{
		SFileInfo& rFileInfo = m_FileInfoVector[CurrentIndex];
		u64 uOffset = _NameTableOffset + (rFileInfo.m_NameOffset & 0xFFFFFF);
		char filename[512];
		GetStringFromOffset(uOffset, filename);

		// check next index
		if (rFileInfo.IsDirectory())
		{
			// this is a directory, build up the new szDirectory
			if (_szDirectory != NULL)
			{
				CharArrayFromFormat(rFileInfo.m_FullPath, "%s%s\\", _szDirectory, filename);
			}
			else
			{
				CharArrayFromFormat(rFileInfo.m_FullPath, "%s\\", filename);
			}

			CurrentIndex = BuildFilenames(CurrentIndex + 1, rFileInfo.m_FileSize, rFileInfo.m_FullPath, _NameTableOffset);
		}
		else
		{
			// this is a filename
			if (_szDirectory != NULL)
			{
				CharArrayFromFormat(rFileInfo.m_FullPath, "%s%s", _szDirectory, filename);
			}
			else
			{
				CharArrayFromFormat(rFileInfo.m_FullPath, "%s", filename);
			}

			CurrentIndex++;
		}
	}

	return(CurrentIndex);
}
} // namespace

