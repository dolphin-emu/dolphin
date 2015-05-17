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
	CFileSystemGCWii(const IVolume* _rVolume);
	virtual ~CFileSystemGCWii();
	virtual bool IsValid() const override { return m_Valid; }
	virtual u64 GetFileSize(const std::string& _rFullPath) override;
	virtual size_t GetFileList(std::vector<const SFileInfo *> &_rFilenames) override;
	virtual const std::string GetFileName(u64 _Address) override;
	virtual u64 ReadFile(const std::string& _rFullPath, u8* _pBuffer, u64 _MaxBufferSize, u64 _OffsetInFile) override;
	virtual bool ExportFile(const std::string& _rFullPath, const std::string&_rExportFilename) override;
	virtual bool ExportApploader(const std::string& _rExportFolder) const override;
	virtual bool ExportDOL(const std::string& _rExportFolder) const override;
	virtual bool GetBootDOL(u8* &buffer, u32 DolSize) const override;
	virtual u32 GetBootDOLSize() const override;

private:
	bool m_Initialized;
	bool m_Valid;
	bool m_Wii;

	std::vector <SFileInfo> m_FileInfoVector;
	u32 Read32(u64 _Offset) const;
	std::string GetStringFromOffset(u64 _Offset) const;
	const SFileInfo* FindFileInfo(const std::string& _rFullPath);
	bool DetectFileSystem();
	void InitFileSystem();
	size_t BuildFilenames(const size_t _FirstIndex, const size_t _LastIndex, const std::string& _szDirectory, u64 _NameTableOffset);
	u32 GetOffsetShift() const;
};

} // namespace
