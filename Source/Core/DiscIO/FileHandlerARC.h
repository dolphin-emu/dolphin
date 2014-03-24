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
class CARCFile
{
	public:

		CARCFile(const std::string& rFilename);

		CARCFile(const std::string& rFilename, u32 offset);

		CARCFile(const u8* pBuffer, size_t BufferSize);

		virtual ~CARCFile();

		bool IsInitialized();

		size_t GetFileSize(const std::string& rFullPath);

		size_t ReadFile(const std::string& rFullPath, u8* pBuffer, size_t MaxBufferSize);

		bool ExportFile(const std::string& rFullPath, const std::string& rExportFilename);

		bool ExportAllFiles(const std::string& rFullPath);


	private:

		u8* m_pBuffer;

		bool m_Initialized;

		typedef std::vector<SFileInfo>CFileInfoVector;
		CFileInfoVector m_FileInfoVector;

		bool ParseBuffer();

		size_t BuildFilenames(const size_t FirstIndex, const size_t LastIndex, const std::string& szDirectory, const char* szNameTable);

		const SFileInfo* FindFileInfo(const std::string& rFullPath) const;
};
} // namespace
