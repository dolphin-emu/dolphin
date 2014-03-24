// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "Common/CommonTypes.h"
#include "DiscIO/Filesystem.h"

namespace DiscIO
{

class IVolume;

class CFileSystemGCWii : public IFileSystem
{
public:
	CFileSystemGCWii(const IVolume* rVolume);
	virtual ~CFileSystemGCWii();
	virtual bool IsValid() const override { return m_Valid; }
	virtual u64 GetFileSize(const std::string& rFullPath) override;
	virtual size_t GetFileList(std::vector<const SFileInfo *> &rFilenames) override;
	virtual const std::string GetFileName(u64 Address) override;
	virtual u64 ReadFile(const std::string& rFullPath, u8* pBuffer, size_t MaxBufferSize) override;
	virtual bool ExportFile(const std::string& rFullPath, const std::string& rExportFilename) override;
	virtual bool ExportApploader(const std::string& rExportFolder) const override;
	virtual bool ExportDOL(const std::string& rExportFolder) const override;
	virtual bool GetBootDOL(u8* &buffer, u32 DolSize) const override;
	virtual u32 GetBootDOLSize() const override;

private:
	bool m_Initialized;
	bool m_Valid;
	u32 m_OffsetShift; // WII offsets are all shifted

	std::vector <SFileInfo> m_FileInfoVector;
	u32 Read32(u64 Offset) const;
	std::string GetStringFromOffset(u64 Offset) const;
	const SFileInfo* FindFileInfo(const std::string& rFullPath);
	bool DetectFileSystem();
	void InitFileSystem();
	size_t BuildFilenames(const size_t FirstIndex, const size_t LastIndex, const std::string& szDirectory, u64 NameTableOffset);
};

} // namespace
