// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#ifndef _FILESYSTEM_GC_WII_H
#define _FILESYSTEM_GC_WII_H

#include <vector>

#include "Filesystem.h"

namespace DiscIO
{

class CFileSystemGCWii : public IFileSystem
{
public:
	CFileSystemGCWii(const IVolume* _rVolume);
	virtual ~CFileSystemGCWii();
	virtual bool IsValid() const { return m_Valid; }
	virtual u64 GetFileSize(const char* _rFullPath);
	virtual size_t GetFileList(std::vector<const SFileInfo *> &_rFilenames);
	virtual const char* GetFileName(u64 _Address);
	virtual u64 ReadFile(const char* _rFullPath, u8* _pBuffer, size_t _MaxBufferSize);
	virtual bool ExportFile(const char* _rFullPath, const char* _rExportFilename);
	virtual bool ExportApploader(const char* _rExportFolder) const;
	virtual bool ExportDOL(const char* _rExportFolder) const;
	virtual bool GetBootDOL(u8* &buffer, u32 DolSize) const;
	virtual u32 GetBootDOLSize() const;

private:
	bool m_Initialized;
	bool m_Valid;
	u32 m_OffsetShift; // WII offsets are all shifted
	
	std::vector <SFileInfo> m_FileInfoVector;
	u32 Read32(u64 _Offset) const;
	std::string GetStringFromOffset(u64 _Offset) const;
	const SFileInfo* FindFileInfo(const char* _rFullPath);
	bool DetectFileSystem();
	void InitFileSystem();
	size_t BuildFilenames(const size_t _FirstIndex, const size_t _LastIndex, const char* _szDirectory, u64 _NameTableOffset);
};

} // namespace

#endif

