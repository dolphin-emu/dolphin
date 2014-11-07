// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include <algorithm>
#include <cinttypes>
#include <cstddef>
#include <cstring>
#include <string>
#include <vector>

#include "Common/CommonTypes.h"
#include "Common/FileUtil.h"
#include "Common/StringUtil.h"

#include "DiscIO/Filesystem.h"
#include "DiscIO/FileSystemGCWii.h"
#include "DiscIO/Volume.h"

namespace DiscIO
{
CFileSystemGCWii::CFileSystemGCWii(const IVolume *_rVolume)
	: IFileSystem(_rVolume)
	, m_Initialized(false)
	, m_Valid(false)
	, m_OffsetShift(0)
{
	m_Valid = DetectFileSystem();
}

CFileSystemGCWii::~CFileSystemGCWii()
{
	m_FileInfoVector.clear();
}

u64 CFileSystemGCWii::GetFileSize(const std::string& _rFullPath)
{
	if (!m_Initialized)
		InitFileSystem();

	const SFileInfo* pFileInfo = FindFileInfo(_rFullPath);

	if (pFileInfo != nullptr && !pFileInfo->IsDirectory())
		return pFileInfo->m_FileSize;

	return 0;
}

const std::string CFileSystemGCWii::GetFileName(u64 _Address)
{
	if (!m_Initialized)
		InitFileSystem();

	for (auto& fileInfo : m_FileInfoVector)
	{
		if ((fileInfo.m_Offset <= _Address) &&
		    ((fileInfo.m_Offset + fileInfo.m_FileSize) > _Address))
		{
			return fileInfo.m_FullPath;
		}
	}

	return "";
}

u64 CFileSystemGCWii::ReadFile(const std::string& _rFullPath, u8* _pBuffer, size_t _MaxBufferSize)
{
	if (!m_Initialized)
		InitFileSystem();

	const SFileInfo* pFileInfo = FindFileInfo(_rFullPath);
	if (pFileInfo == nullptr)
		return 0;

	if (pFileInfo->m_FileSize > _MaxBufferSize)
		return 0;

	DEBUG_LOG(DISCIO, "Filename: %s. Offset: %" PRIx64 ". Size: %" PRIx64, _rFullPath.c_str(),
		pFileInfo->m_Offset, pFileInfo->m_FileSize);

	m_rVolume->Read(pFileInfo->m_Offset, pFileInfo->m_FileSize, _pBuffer);
	return pFileInfo->m_FileSize;
}

bool CFileSystemGCWii::ExportFile(const std::string& _rFullPath, const std::string& _rExportFilename)
{
	if (!m_Initialized)
		InitFileSystem();

	const SFileInfo* pFileInfo = FindFileInfo(_rFullPath);

	if (!pFileInfo)
		return false;

	u64 remainingSize = pFileInfo->m_FileSize;
	u64 fileOffset = pFileInfo->m_Offset;

	File::IOFile f(_rExportFilename, "wb");
	if (!f)
		return false;

	bool result = true;

	while (remainingSize)
	{
		// Limit read size to 128 MB
		size_t readSize = (size_t)std::min(remainingSize, (u64)0x08000000);

		std::vector<u8> buffer(readSize);

		result = m_rVolume->Read(fileOffset, readSize, &buffer[0]);

		if (!result)
			break;

		f.WriteBytes(&buffer[0], readSize);

		remainingSize -= readSize;
		fileOffset += readSize;
	}

	return result;
}

bool CFileSystemGCWii::ExportApploader(const std::string& _rExportFolder) const
{
	u32 AppSize = Read32(0x2440 + 0x14);// apploader size
	AppSize += Read32(0x2440 + 0x18);   // + trailer size
	AppSize += 0x20;                    // + header size
	DEBUG_LOG(DISCIO,"AppSize -> %x", AppSize);

	std::vector<u8> buffer(AppSize);
	if (m_rVolume->Read(0x2440, AppSize, &buffer[0]))
	{
		std::string exportName(_rExportFolder + "/apploader.img");

		File::IOFile AppFile(exportName, "wb");
		if (AppFile)
		{
			AppFile.WriteBytes(&buffer[0], AppSize);
			return true;
		}
	}

	return false;
}

u32 CFileSystemGCWii::GetBootDOLSize() const
{
	u32 DolOffset = Read32(0x420) << m_OffsetShift;
	u32 DolSize = 0, offset = 0, size = 0;

	// Iterate through the 7 code segments
	for (u8 i = 0; i < 7; i++)
	{
		offset = Read32(DolOffset + 0x00 + i * 4);
		size   = Read32(DolOffset + 0x90 + i * 4);
		if (offset + size > DolSize)
			DolSize = offset + size;
	}

	// Iterate through the 11 data segments
	for (u8 i = 0; i < 11; i++)
	{
		offset = Read32(DolOffset + 0x1c + i * 4);
		size   = Read32(DolOffset + 0xac + i * 4);
		if (offset + size > DolSize)
			DolSize = offset + size;
	}
	return DolSize;
}

bool CFileSystemGCWii::GetBootDOL(u8* &buffer, u32 DolSize) const
{
	u32 DolOffset = Read32(0x420) << m_OffsetShift;
	return m_rVolume->Read(DolOffset, DolSize, buffer);
}

bool CFileSystemGCWii::ExportDOL(const std::string& _rExportFolder) const
{
	u32 DolOffset = Read32(0x420) << m_OffsetShift;
	u32 DolSize = GetBootDOLSize();

	std::vector<u8> buffer(DolSize);
	if (m_rVolume->Read(DolOffset, DolSize, &buffer[0]))
	{
		std::string exportName(_rExportFolder + "/boot.dol");

		File::IOFile DolFile(exportName, "wb");
		if (DolFile)
		{
			DolFile.WriteBytes(&buffer[0], DolSize);
			return true;
		}
	}

	return false;
}

u32 CFileSystemGCWii::Read32(u64 _Offset) const
{
	u32 Temp = 0;
	m_rVolume->Read(_Offset, 4, (u8*)&Temp);
	return Common::swap32(Temp);
}

std::string CFileSystemGCWii::GetStringFromOffset(u64 _Offset) const
{
	std::string data;
	data.resize(255);
	m_rVolume->Read(_Offset, data.size(), (u8*)&data[0]);
	data.erase(std::find(data.begin(), data.end(), 0x00), data.end());

	// TODO: Should we really always use SHIFT-JIS?
	// It makes some filenames in Pikmin (NTSC-U) sane, but is it correct?
	return SHIFTJISToUTF8(data);
}

size_t CFileSystemGCWii::GetFileList(std::vector<const SFileInfo *> &_rFilenames)
{
	if (!m_Initialized)
		InitFileSystem();

	if (_rFilenames.size())
		PanicAlert("GetFileList : input list has contents?");
	_rFilenames.clear();
	_rFilenames.reserve(m_FileInfoVector.size());
	for (auto& fileInfo : m_FileInfoVector)
		_rFilenames.push_back(&fileInfo);
	return m_FileInfoVector.size();
}

const SFileInfo* CFileSystemGCWii::FindFileInfo(const std::string& _rFullPath)
{
	if (!m_Initialized)
		InitFileSystem();

	for (auto& fileInfo : m_FileInfoVector)
	{
		if (!strcasecmp(fileInfo.m_FullPath.c_str(), _rFullPath.c_str()))
			return &fileInfo;
	}

	return nullptr;
}

bool CFileSystemGCWii::DetectFileSystem()
{
	if (Read32(0x18) == 0x5D1C9EA3)
	{
		m_OffsetShift = 2; // Wii file system
		return true;
	}
	else if (Read32(0x1c) == 0xC2339F3D)
	{
		m_OffsetShift = 0; // GC file system
		return true;
	}

	return false;
}

void CFileSystemGCWii::InitFileSystem()
{
	m_Initialized = true;

	// read the whole FST
	u64 FSTOffset = (u64)Read32(0x424) << m_OffsetShift;
	// u32 FSTSize     = Read32(0x428);
	// u32 FSTMaxSize  = Read32(0x42C);


	// read all fileinfos
	SFileInfo Root;
	Root.m_NameOffset = Read32(FSTOffset + 0x0);
	Root.m_Offset     = (u64)Read32(FSTOffset + 0x4) << m_OffsetShift;
	Root.m_FileSize   = Read32(FSTOffset + 0x8);

	if (Root.IsDirectory())
	{
		if (m_FileInfoVector.size())
			PanicAlert("Wtf?");
		u64 NameTableOffset = FSTOffset;

		m_FileInfoVector.reserve((unsigned int)Root.m_FileSize);
		for (u32 i = 0; i < Root.m_FileSize; i++)
		{
			SFileInfo sfi;
			u64 Offset = FSTOffset + (i * 0xC);
			sfi.m_NameOffset = Read32(Offset + 0x0);
			sfi.m_Offset     = (u64)Read32(Offset + 0x4) << m_OffsetShift;
			sfi.m_FileSize   = Read32(Offset + 0x8);

			m_FileInfoVector.push_back(sfi);
			NameTableOffset += 0xC;
		}

		BuildFilenames(1, m_FileInfoVector.size(), "", NameTableOffset);
	}
}

size_t CFileSystemGCWii::BuildFilenames(const size_t _FirstIndex, const size_t _LastIndex, const std::string& _szDirectory, u64 _NameTableOffset)
{
	size_t CurrentIndex = _FirstIndex;

	while (CurrentIndex < _LastIndex)
	{
		SFileInfo& rFileInfo = m_FileInfoVector[CurrentIndex];
		u64 const uOffset = _NameTableOffset + (rFileInfo.m_NameOffset & 0xFFFFFF);

		rFileInfo.m_FullPath = _szDirectory + GetStringFromOffset(uOffset);

		// check next index
		if (rFileInfo.IsDirectory())
		{
			rFileInfo.m_FullPath += '/';
			CurrentIndex = BuildFilenames(CurrentIndex + 1, (size_t) rFileInfo.m_FileSize, rFileInfo.m_FullPath, _NameTableOffset);
		}
		else
		{
			++CurrentIndex;
		}
	}

	return CurrentIndex;
}

}  // namespace
