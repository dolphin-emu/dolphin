// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <string>
#include <vector>

#include "Common/CommonTypes.h"

namespace DiscIO
{

class IVolume;

// file info of an FST entry
struct SFileInfo
{
	u64 m_NameOffset = 0u;
	u64 m_Offset = 0u;
	u64 m_FileSize = 0u;
	std::string m_FullPath;

	bool IsDirectory() const { return (m_NameOffset & 0xFF000000) != 0; }

	SFileInfo(u64 name_offset, u64 offset, u64 filesize) :
		m_NameOffset(name_offset),
		m_Offset(offset),
		m_FileSize(filesize)
	{ }

	SFileInfo (SFileInfo const&) = default;
	SFileInfo () = default;
};

class IFileSystem
{
public:
	IFileSystem(const IVolume *_rVolume);

	virtual ~IFileSystem();
	virtual bool IsValid() const = 0;
	virtual const std::vector<SFileInfo>& GetFileList() = 0;
	virtual u64 GetFileSize(const std::string& _rFullPath) = 0;
	virtual u64 ReadFile(const std::string& _rFullPath, u8* _pBuffer, u64 _MaxBufferSize, u64 _OffsetInFile = 0) = 0;
	virtual bool ExportFile(const std::string& _rFullPath, const std::string& _rExportFilename) = 0;
	virtual bool ExportApploader(const std::string& _rExportFolder) const = 0;
	virtual bool ExportDOL(const std::string& _rExportFolder) const = 0;
	virtual const std::string GetFileName(u64 _Address) = 0;
	virtual u64 GetBootDOLOffset() const = 0;
	virtual u32 GetBootDOLSize(u64 dol_offset) const = 0;

protected:
	const IVolume *m_rVolume;
};

std::unique_ptr<IFileSystem> CreateFileSystem(const IVolume* volume);

} // namespace
