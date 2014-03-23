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

		CARCFile(const std::string& _rFilename);

		CARCFile(const std::string& _rFilename, u32 offset);

		CARCFile(const u8* _pBuffer, size_t _BufferSize);

		virtual ~CARCFile();

		bool IsInitialized();

		size_t GetFileSize(const std::string& _rFullPath);

		size_t ReadFile(const std::string& _rFullPath, u8* _pBuffer, size_t _MaxBufferSize);

		bool ExportFile(const std::string& _rFullPath, const std::string& _rExportFilename);

		bool ExportAllFiles(const std::string& _rFullPath);


	private:

		u8* m_pBuffer;

		bool m_Initialized;

		typedef std::vector<SFileInfo>CFileInfoVector;
		CFileInfoVector m_FileInfoVector;

		bool ParseBuffer();

		size_t BuildFilenames(const size_t _FirstIndex, const size_t _LastIndex, const std::string& _szDirectory, const char* _szNameTable);

		const SFileInfo* FindFileInfo(const std::string& _rFullPath) const;
};
} // namespace
