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

#ifndef _FILESYSTEM_H
#define _FILESYSTEM_H

#include "Volume.h"

namespace DiscIO
{
// file info of an FST entry
struct SFileInfo
{
	u32 m_NameOffset;
	u64 m_Offset;
	u32 m_FileSize;
	char m_FullPath[512];

	bool IsDirectory() {return((m_NameOffset& 0xFF000000) != 0 ? true : false);}
};
class IFileSystem
{
	public:

		IFileSystem(const IVolume& _rVolume);

		virtual ~IFileSystem();

		virtual bool IsInitialized() = 0;


		virtual size_t GetFileList(std::vector<SFileInfo> *_rFilenames) = 0;

		virtual size_t GetFileSize(const char* _rFullPath) = 0;

		virtual size_t ReadFile(const char* _rFullPath, u8* _pBuffer, size_t _MaxBufferSize) = 0;

		virtual bool ExportFile(const char* _rFullPath, const char* _rExportFilename) = 0;

		virtual bool ExportAllFiles(const char* _rFullPath) = 0;

		virtual const char* GetFileName(u64 _Address) = 0;


		virtual const IVolume& GetVolume() {return(m_rVolume);}


	protected:

		const IVolume& m_rVolume;
};

IFileSystem* CreateFileSystem(const IVolume& _rVolume);
} // namespace

#endif
