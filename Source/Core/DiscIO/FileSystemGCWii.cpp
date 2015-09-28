// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <algorithm>
#include <cinttypes>
#include <cstddef>
#include <cstring>
#include <string>
#include <vector>

#include "Common/CommonTypes.h"
#include "Common/FileUtil.h"
#include "Common/MsgHandler.h"
#include "Common/StringUtil.h"
#include "Common/Logging/Log.h"
#include "DiscIO/Filesystem.h"
#include "DiscIO/FileSystemGCWii.h"
#include "DiscIO/Volume.h"

namespace DiscIO
{
CFileSystemGCWii::CFileSystemGCWii(const IVolume *_rVolume)
	: IFileSystem(_rVolume)
	, m_Initialized(false)
	, m_Valid(false)
	, m_Wii(false)
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

u64 CFileSystemGCWii::ReadFile(const std::string& _rFullPath, u8* _pBuffer, u64 _MaxBufferSize, u64 _OffsetInFile)
{
	if (!m_Initialized)
		InitFileSystem();

	const SFileInfo* pFileInfo = FindFileInfo(_rFullPath);
	if (pFileInfo == nullptr)
		return 0;

	if (_OffsetInFile >= pFileInfo->m_FileSize)
		return 0;

	u64 read_length = std::min(_MaxBufferSize, pFileInfo->m_FileSize - _OffsetInFile);

	DEBUG_LOG(DISCIO, "Reading %" PRIx64 " bytes at %" PRIx64 " from file %s. Offset: %" PRIx64 " Size: %" PRIx64,
	          read_length, _OffsetInFile, _rFullPath.c_str(), pFileInfo->m_Offset, pFileInfo->m_FileSize);

	m_rVolume->Read(pFileInfo->m_Offset + _OffsetInFile, read_length, _pBuffer, m_Wii);
	return read_length;
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

		result = m_rVolume->Read(fileOffset, readSize, &buffer[0], m_Wii);

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
	u32 AppSize = m_rVolume->Read32(0x2440 + 0x14, m_Wii); // apploader size
	AppSize    += m_rVolume->Read32(0x2440 + 0x18, m_Wii); // + trailer size
	AppSize    += 0x20;                                    // + header size
	DEBUG_LOG(DISCIO,"AppSize -> %x", AppSize);

	std::vector<u8> buffer(AppSize);
	if (m_rVolume->Read(0x2440, AppSize, &buffer[0], m_Wii))
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
	u32 DolOffset = m_rVolume->Read32(0x420, m_Wii) << GetOffsetShift();
	u32 DolSize = 0, offset = 0, size = 0;

	// Iterate through the 7 code segments
	for (u8 i = 0; i < 7; i++)
	{
		offset = m_rVolume->Read32(DolOffset + 0x00 + i * 4, m_Wii);
		size   = m_rVolume->Read32(DolOffset + 0x90 + i * 4, m_Wii);
		if (offset + size > DolSize)
			DolSize = offset + size;
	}

	// Iterate through the 11 data segments
	for (u8 i = 0; i < 11; i++)
	{
		offset = m_rVolume->Read32(DolOffset + 0x1c + i * 4, m_Wii);
		size   = m_rVolume->Read32(DolOffset + 0xac + i * 4, m_Wii);
		if (offset + size > DolSize)
			DolSize = offset + size;
	}
	return DolSize;
}

bool CFileSystemGCWii::GetBootDOL(u8* &buffer, u32 DolSize) const
{
	u32 DolOffset = m_rVolume->Read32(0x420, m_Wii) << GetOffsetShift();
	return m_rVolume->Read(DolOffset, DolSize, buffer, m_Wii);
}

bool CFileSystemGCWii::ExportDOL(const std::string& _rExportFolder) const
{
	u32 DolOffset = m_rVolume->Read32(0x420, m_Wii) << GetOffsetShift();
	u32 DolSize = GetBootDOLSize();

	std::vector<u8> buffer(DolSize);
	if (m_rVolume->Read(DolOffset, DolSize, &buffer[0], m_Wii))
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

std::string CFileSystemGCWii::GetStringFromOffset(u64 _Offset) const
{
	std::string data(255, 0x00);
	m_rVolume->Read(_Offset, data.size(), (u8*)&data[0], m_Wii);
	data.erase(std::find(data.begin(), data.end(), 0x00), data.end());

	// TODO: Should we really always use SHIFT-JIS?
	// It makes some filenames in Pikmin (NTSC-U) sane, but is it correct?
	return SHIFTJISToUTF8(data);
}

const std::vector<SFileInfo>& CFileSystemGCWii::GetFileList()
{
	if (!m_Initialized)
		InitFileSystem();

	return m_FileInfoVector;
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
	if (m_rVolume->Read32(0x18, false) == 0x5D1C9EA3)
	{
		m_Wii = true;
		return true;
	}
	else if (m_rVolume->Read32(0x1c, false) == 0xC2339F3D)
	{
		m_Wii = false;
		return true;
	}

	return false;
}

void CFileSystemGCWii::InitFileSystem()
{
	m_Initialized = true;
	u32 const shift = GetOffsetShift();

	// read the whole FST
	u64 FSTOffset = static_cast<u64>(m_rVolume->Read32(0x424, m_Wii)) << shift;
	// u32 FSTSize     = Read32(0x428);
	// u32 FSTMaxSize  = Read32(0x42C);


	// read all fileinfos
	SFileInfo Root
	{
		m_rVolume->Read32(FSTOffset + 0x0, m_Wii),
		static_cast<u64>(FSTOffset + 0x4) << shift,
		m_rVolume->Read32(FSTOffset + 0x8, m_Wii)
	};

	if (!Root.IsDirectory())
		return;

	if (m_FileInfoVector.size())
		PanicAlert("Wtf?");
	u64 NameTableOffset = FSTOffset;

	m_FileInfoVector.reserve((size_t)Root.m_FileSize);
	for (u32 i = 0; i < Root.m_FileSize; i++)
	{
		u64 const read_offset = FSTOffset + (i * 0xC);
		u64 const name_offset = m_rVolume->Read32(read_offset + 0x0, m_Wii);
		u64 const offset = static_cast<u64>(m_rVolume->Read32(read_offset + 0x4, m_Wii)) << shift;
		u64 const size = m_rVolume->Read32(read_offset + 0x8, m_Wii);
		m_FileInfoVector.emplace_back(name_offset, offset, size);
		NameTableOffset += 0xC;
	}

	BuildFilenames(1, m_FileInfoVector.size(), "", NameTableOffset);
}

size_t CFileSystemGCWii::BuildFilenames(const size_t _FirstIndex, const size_t _LastIndex, const std::string& _szDirectory, u64 _NameTableOffset)
{
	size_t CurrentIndex = _FirstIndex;

	while (CurrentIndex < _LastIndex)
	{
		SFileInfo& rFileInfo = m_FileInfoVector[CurrentIndex];
		u64 const uOffset = _NameTableOffset + (rFileInfo.m_NameOffset & 0xFFFFFF);
		std::string const offset_str { GetStringFromOffset(uOffset) };
		bool const is_dir = rFileInfo.IsDirectory();
		rFileInfo.m_FullPath.reserve(_szDirectory.size() + offset_str.size());

		rFileInfo.m_FullPath.append(_szDirectory.data(), _szDirectory.size())
		                    .append(offset_str.data(), offset_str.size())
		                    .append("/", size_t(is_dir));

		if (!is_dir)
		{
			++CurrentIndex;
			continue;
		}

		// check next index
		CurrentIndex = BuildFilenames(CurrentIndex + 1, (size_t) rFileInfo.m_FileSize, rFileInfo.m_FullPath, _NameTableOffset);
	}

	return CurrentIndex;
}

u32 CFileSystemGCWii::GetOffsetShift() const
{
	return m_Wii ? 2 : 0;
}

}  // namespace
