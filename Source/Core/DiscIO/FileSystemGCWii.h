// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
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
	CFileSystemGCWii(const IVolume* _rVolume);
	virtual ~CFileSystemGCWii();

	bool IsValid() const override { return m_Valid; }
	u64 GetFileSize(const std::string& _rFullPath) override;
	const std::vector<SFileInfo>& GetFileList() override;
	const std::string GetFileName(u64 _Address) override;
	u64 ReadFile(const std::string& _rFullPath, u8* _pBuffer, u64 _MaxBufferSize, u64 _OffsetInFile) override;
	bool ExportFile(const std::string& _rFullPath, const std::string&_rExportFilename) override;
	bool ExportApploader(const std::string& _rExportFolder) const override;
	bool ExportDOL(const std::string& _rExportFolder) const override;
	u64 GetBootDOLOffset() const override;
	u32 GetBootDOLSize(u64 dol_offset) const override;

private:
	bool m_Initialized;
	bool m_Valid;
	bool m_Wii;
	std::vector<SFileInfo> m_FileInfoVector;

	std::string GetStringFromOffset(u64 _Offset) const;
	const SFileInfo* FindFileInfo(const std::string& _rFullPath);
	bool DetectFileSystem();
	void InitFileSystem();
	size_t BuildFilenames(const size_t _FirstIndex, const size_t _LastIndex, const std::string& _szDirectory, u64 _NameTableOffset);
	u32 GetOffsetShift() const;
};

} // namespace
