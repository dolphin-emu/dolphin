// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#ifndef _FILESYSTEM_H
#define _FILESYSTEM_H

#include "Volume.h"

namespace DiscIO
{

// file info of an FST entry
struct SFileInfo
{
	u64 m_NameOffset;
	u64 m_Offset;
	u64 m_FileSize;
	char m_FullPath[512];

	bool IsDirectory() const { return (m_NameOffset & 0xFF000000) != 0 ? true : false; }

	SFileInfo() : m_NameOffset(0), m_Offset(0), m_FileSize(0) {
		memset(m_FullPath, 0, sizeof(m_FullPath));
	}

	SFileInfo(const SFileInfo &rhs) : m_NameOffset(rhs.m_NameOffset), 
		m_Offset(rhs.m_Offset), m_FileSize(rhs.m_FileSize) {
		memcpy(m_FullPath, rhs.m_FullPath, strlen(rhs.m_FullPath) + 1);
	}
};

class IFileSystem
{
public:
	IFileSystem(const IVolume *_rVolume);

	virtual ~IFileSystem();
	virtual bool IsValid() const = 0;
	virtual size_t GetFileList(std::vector<const SFileInfo *> &_rFilenames) = 0;
	virtual u64 GetFileSize(const char* _rFullPath) = 0;
	virtual u64 ReadFile(const char* _rFullPath, u8* _pBuffer, size_t _MaxBufferSize) = 0;
	virtual bool ExportFile(const char* _rFullPath, const char* _rExportFilename) = 0;
	virtual bool ExportApploader(const char* _rExportFolder) const = 0;
	virtual bool ExportDOL(const char* _rExportFolder) const = 0;
	virtual const char* GetFileName(u64 _Address) = 0;
	virtual bool GetBootDOL(u8* &buffer, u32 DolSize) const = 0;
	virtual u32 GetBootDOLSize() const = 0;

	virtual const IVolume *GetVolume() const { return m_rVolume; }
protected:
	const IVolume *m_rVolume;
};

IFileSystem* CreateFileSystem(const IVolume *_rVolume);

} // namespace

#endif
