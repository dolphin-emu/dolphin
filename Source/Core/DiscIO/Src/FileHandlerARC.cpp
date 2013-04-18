// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "Common.h"

#include "FileHandlerARC.h"
#include "StringUtil.h"
#include "Blob.h"
#include "FileUtil.h"

#define ARC_ID 0x55aa382d

namespace DiscIO
{
CARCFile::CARCFile(const std::string& _rFilename)
	: m_pBuffer(NULL)
	, m_Initialized(false)
{
	DiscIO::IBlobReader* pReader = DiscIO::CreateBlobReader(_rFilename.c_str());
	if (pReader != NULL)
	{
		u64 FileSize = pReader->GetDataSize();
		m_pBuffer = new u8[(u32)FileSize];
		pReader->Read(0, FileSize, m_pBuffer);
		delete pReader;

		m_Initialized = ParseBuffer();
	}
}

CARCFile::CARCFile(const std::string& _rFilename, u32 offset)
	: m_pBuffer(NULL)
	, m_Initialized(false)
{
	DiscIO::IBlobReader* pReader = DiscIO::CreateBlobReader(_rFilename.c_str());
	if (pReader != NULL)
	{
		u64 FileSize = pReader->GetDataSize() - offset;
		m_pBuffer = new u8[(u32)FileSize];
		pReader->Read(offset, FileSize, m_pBuffer);
		delete pReader;

		m_Initialized = ParseBuffer();
	}
}

CARCFile::CARCFile(const u8* _pBuffer, size_t _BufferSize)
	: m_pBuffer(NULL)
	, m_Initialized(false)
{
	m_pBuffer = new u8[_BufferSize];

	if (m_pBuffer)
	{
		memcpy(m_pBuffer, _pBuffer, _BufferSize);
		m_Initialized = ParseBuffer();
	}
}


CARCFile::~CARCFile()
{
	delete [] m_pBuffer;
}


bool
CARCFile::IsInitialized()
{
	return(m_Initialized);
}


size_t
CARCFile::GetFileSize(const std::string& _rFullPath)
{
	if (!m_Initialized)
	{
		return(0);
	}

	const SFileInfo* pFileInfo = FindFileInfo(_rFullPath);

	if (pFileInfo != NULL)
	{
		return((size_t) pFileInfo->m_FileSize);
	}

	return(0);
}


size_t
CARCFile::ReadFile(const std::string& _rFullPath, u8* _pBuffer, size_t _MaxBufferSize)
{
	if (!m_Initialized)
	{
		return(0);
	}

	const SFileInfo* pFileInfo = FindFileInfo(_rFullPath);

	if (pFileInfo == NULL)
	{
		return(0);
	}

	if (pFileInfo->m_FileSize > _MaxBufferSize)
	{
		return(0);
	}

	memcpy(_pBuffer, &m_pBuffer[pFileInfo->m_Offset], (size_t)pFileInfo->m_FileSize);
	return((size_t) pFileInfo->m_FileSize);
}


bool
CARCFile::ExportFile(const std::string& _rFullPath, const std::string& _rExportFilename)
{
	if (!m_Initialized)
	{
		return(false);
	}

	const SFileInfo* pFileInfo = FindFileInfo(_rFullPath);

	if (pFileInfo == NULL)
	{
		return(false);
	}

	File::IOFile pFile(_rExportFilename, "wb");

	return pFile.WriteBytes(&m_pBuffer[pFileInfo->m_Offset], (size_t) pFileInfo->m_FileSize);
}


bool
CARCFile::ExportAllFiles(const std::string& _rFullPath)
{
	return(false);
}


bool
CARCFile::ParseBuffer()
{
	// check ID
	u32 ID = Common::swap32(*(u32*)(m_pBuffer));

	if (ID != ARC_ID)
		return false;

	// read header
	u32 FSTOffset  = Common::swap32(*(u32*)(m_pBuffer + 0x4));
	//u32 FSTSize    = Common::swap32(*(u32*)(m_pBuffer + 0x8));
	//u32 FileOffset = Common::swap32(*(u32*)(m_pBuffer + 0xC));

	// read all file infos
	SFileInfo Root;
	Root.m_NameOffset = Common::swap32(*(u32*)(m_pBuffer + FSTOffset + 0x0));
	Root.m_Offset     = Common::swap32(*(u32*)(m_pBuffer + FSTOffset + 0x4));
	Root.m_FileSize   = Common::swap32(*(u32*)(m_pBuffer + FSTOffset + 0x8));

	if (Root.IsDirectory())
	{
		m_FileInfoVector.resize((unsigned int)Root.m_FileSize);
		const char* szNameTable = (char*)(m_pBuffer + FSTOffset);

		for (size_t i = 0; i < m_FileInfoVector.size(); i++)
		{
			u8* Offset = m_pBuffer + FSTOffset + (i * 0xC);
			m_FileInfoVector[i].m_NameOffset = Common::swap32(*(u32*)(Offset + 0x0));
			m_FileInfoVector[i].m_Offset   = Common::swap32(*(u32*)(Offset + 0x4));
			m_FileInfoVector[i].m_FileSize = Common::swap32(*(u32*)(Offset + 0x8));

			szNameTable += 0xC;
		}

		BuildFilenames(1, m_FileInfoVector.size(), NULL, szNameTable);
	}

	return(true);
}


size_t
CARCFile::BuildFilenames(const size_t _FirstIndex, const size_t _LastIndex, const char* _szDirectory, const char* _szNameTable)
{
	size_t CurrentIndex = _FirstIndex;

	while (CurrentIndex < _LastIndex)
	{
		SFileInfo& rFileInfo = m_FileInfoVector[CurrentIndex];
		int uOffset = rFileInfo.m_NameOffset & 0xFFFFFF;

		// check next index
		if (rFileInfo.IsDirectory())
		{
			// this is a directory, build up the new szDirectory
			if (_szDirectory != NULL)
			{
				sprintf(rFileInfo.m_FullPath, "%s%s/", _szDirectory, &_szNameTable[uOffset]);
			}
			else
			{
				sprintf(rFileInfo.m_FullPath, "%s/", &_szNameTable[uOffset]);
			}

			CurrentIndex = BuildFilenames(CurrentIndex + 1, (size_t) rFileInfo.m_FileSize, rFileInfo.m_FullPath, _szNameTable);
		}
		else
		{
			// this is a filename
			if (_szDirectory != NULL)
			{
				sprintf(rFileInfo.m_FullPath, "%s%s", _szDirectory, &_szNameTable[uOffset]);
			}
			else
			{
				sprintf(rFileInfo.m_FullPath, "%s", &_szNameTable[uOffset]);
			}

			CurrentIndex++;
		}
	}

	return(CurrentIndex);
}


const SFileInfo*
CARCFile::FindFileInfo(std::string _rFullPath) const
{
	for (size_t i = 0; i < m_FileInfoVector.size(); i++)
	{
		if (!strcasecmp(m_FileInfoVector[i].m_FullPath, _rFullPath.c_str()))
		{
			return(&m_FileInfoVector[i]);
		}
	}

	return(NULL);
}
} // namespace
