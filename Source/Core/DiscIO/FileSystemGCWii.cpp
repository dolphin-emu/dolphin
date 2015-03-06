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
		return pFileInfo->m_file_size;

	return 0;
}

const std::string CFileSystemGCWii::GetFileName(u64 _address)
{
	if (!m_Initialized)
		InitFileSystem();

	for (auto& fileInfo : m_FileInfoVector)
	{
		if ((fileInfo.m_offset <= _address) &&
		    ((fileInfo.m_offset + fileInfo.m_file_size) > _address))
		{
			return fileInfo.m_full_path;
		}
	}

	return "";
}

u64 CFileSystemGCWii::ReadFile(const std::string& _rFullPath, u8* _pBuffer, size_t _max_buffer_size)
{
	if (!m_Initialized)
		InitFileSystem();

	const SFileInfo* pFileInfo = FindFileInfo(_rFullPath);
	if (pFileInfo == nullptr)
		return 0;

	if (pFileInfo->m_file_size > _max_buffer_size)
		return 0;

	DEBUG_LOG(DISCIO, "Filename: %s. Offset: %" PRIx64 ". Size: %" PRIx64, _rFullPath.c_str(),
		pFileInfo->m_offset, pFileInfo->m_file_size);

	m_rVolume->Read(pFileInfo->m_offset, pFileInfo->m_file_size, _pBuffer, m_Wii);
	return pFileInfo->m_file_size;
}

bool CFileSystemGCWii::ExportFile(const std::string& _rFullPath, const std::string& _rExportFilename)
{
	if (!m_Initialized)
		InitFileSystem();

	const SFileInfo* pFileInfo = FindFileInfo(_rFullPath);

	if (!pFileInfo)
		return false;

	u64 remaining_size = pFileInfo->m_file_size;
	u64 fileOffset = pFileInfo->m_offset;

	File::IOFile f(_rExportFilename, "wb");
	if (!f)
		return false;

	bool result = true;

	while (remaining_size)
	{
		// Limit read size to 128 MB
		size_t readSize = (size_t)std::min(remaining_size, (u64)0x08000000);

		std::vector<u8> buffer(readSize);

		result = m_rVolume->Read(fileOffset, readSize, &buffer[0], m_Wii);

		if (!result)
			break;

		f.WriteBytes(&buffer[0], readSize);

		remaining_size -= readSize;
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
	u32 DOL_offset = Read32(0x420) << GetOffsetShift();
	u32 DOL_size = 0, offset = 0, size = 0;

	// Iterate through the 7 code segments
	for (u8 i = 0; i < 7; i++)
	{
		offset = Read32(DOL_offset + 0x00 + i * 4);
		size   = Read32(DOL_offset + 0x90 + i * 4);
		if (offset + size > DOL_size)
			DOL_size = offset + size;
	}

	// Iterate through the 11 data segments
	for (u8 i = 0; i < 11; i++)
	{
		offset = Read32(DOL_offset + 0x1c + i * 4);
		size   = Read32(DOL_offset + 0xac + i * 4);
		if (offset + size > DOL_size)
			DOL_size = offset + size;
	}
	return DOL_size;
}

bool CFileSystemGCWii::GetBootDOL(u8* &buffer, u32 DOL_size) const
{
	u32 DOL_offset = Read32(0x420) << GetOffsetShift();
	return m_rVolume->Read(DOL_offset, DOL_size, buffer, m_Wii);
}

bool CFileSystemGCWii::ExportDOL(const std::string& _rExportFolder) const
{
	u32 DOL_offset = Read32(0x420) << GetOffsetShift();
	u32 DOL_size = GetBootDOLSize();

	std::vector<u8> buffer(DOL_size);
	if (m_rVolume->Read(DOL_offset, DOL_size, &buffer[0], m_Wii))
	{
		std::string exportName(_rExportFolder + "/boot.dol");

		File::IOFile DolFile(exportName, "wb");
		if (DolFile)
		{
			DolFile.WriteBytes(&buffer[0], DOL_size);
			return true;
		}
	}

	return false;
}

u32 CFileSystemGCWii::Read32(u64 _offset) const
{
	u32 Temp = 0;
	m_rVolume->Read(_offset, 4, (u8*)&Temp, m_Wii);
	return Common::swap32(Temp);
}

std::string CFileSystemGCWii::GetStringFromOffset(u64 _offset) const
{
	std::string data(255, 0x00);
	m_rVolume->Read(_offset, data.size(), (u8*)&data[0], m_Wii);
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
		if (!strcasecmp(fileInfo.m_full_path.c_str(), _rFullPath.c_str()))
			return &fileInfo;
	}

	return nullptr;
}

bool CFileSystemGCWii::DetectFileSystem()
{
	if (Read32(0x18) == 0x5D1C9EA3)
	{
		m_Wii = true;
		return true;
	}
	else if (Read32(0x1c) == 0xC2339F3D)
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
	u64 FST_offset = static_cast<u64>(Read32(0x424)) << shift;
	// u32 FST_size     = Read32(0x428);
	// u32 FST_max_size = Read32(0x42C);


	// read all fileinfos
	SFileInfo Root
	{
		Read32(FST_offset + 0x0),
		static_cast<u64>(FST_offset + 0x4) << shift,
		Read32(FST_offset + 0x8)
	};

	if (!Root.IsDirectory())
		return;

	if (m_FileInfoVector.size())
		PanicAlert("Wtf?");
	u64 NameTableOffset = FST_offset;

	m_FileInfoVector.reserve((size_t)Root.m_file_size);
	for (u32 i = 0; i < Root.m_file_size; i++)
	{
		u64 const read_offset = FST_offset + (i * 0xC);
		u64 const name_offset = Read32(read_offset + 0x0);
		u64 const offset = static_cast<u64>(Read32(read_offset + 0x4)) << shift;
		u64 const size = Read32(read_offset + 0x8);
		m_FileInfoVector.emplace_back(name_offset, offset, size);
		NameTableOffset += 0xC;
	}

	BuildFilenames(1, m_FileInfoVector.size(), "", NameTableOffset);
}

size_t CFileSystemGCWii::BuildFilenames(const size_t _first_index, const size_t _last_index, const std::string& _szDirectory, u64 _name_table_offset)
{
	size_t CurrentIndex = _first_index;

	while (CurrentIndex < _last_index)
	{
		SFileInfo& rFileInfo = m_FileInfoVector[CurrentIndex];
		u64 const uOffset = _name_table_offset + (rFileInfo.m_name_offset & 0xFFFFFF);
		std::string const offset_str { GetStringFromOffset(uOffset) };
		bool const is_dir = rFileInfo.IsDirectory();
		rFileInfo.m_full_path.reserve(_szDirectory.size() + offset_str.size());

		rFileInfo.m_full_path.append(_szDirectory.data(), _szDirectory.size())
		                     .append(offset_str.data(), offset_str.size())
		                     .append("/", size_t(is_dir));

		if (!is_dir)
		{
			++CurrentIndex;
			continue;
		}

		// check next index
		CurrentIndex = BuildFilenames(CurrentIndex + 1, (size_t) rFileInfo.m_file_size, rFileInfo.m_full_path, _name_table_offset);
	}

	return CurrentIndex;
}

u32 CFileSystemGCWii::GetOffsetShift() const
{
	return m_Wii ? 2 : 0;
}

}  // namespace
