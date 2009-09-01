// Copyright (C) 2003 Dolphin Project.

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

#include "stdafx.h"
#include "NANDContentLoader.h"

#include <algorithm>
#include <cctype> 
#include "AES/aes.h"
#include "MathUtil.h"
#include "FileUtil.h"
#include "Log.h"
#include "WiiWad.h"

namespace DiscIO
{


class CSharedContent
{
public:

	static CSharedContent& AccessInstance() { return m_Instance; }

	std::string GetFilenameFromSHA1(u8* _pHash);

private:


	CSharedContent();

	virtual ~CSharedContent();

	struct SElement
	{
		u8 FileName[8];
		u8 SHA1Hash[20];
	};

	std::vector<SElement> m_Elements;
	static CSharedContent m_Instance;
};

CSharedContent CSharedContent::m_Instance;

CSharedContent::CSharedContent()
{
	char szFilename[1024];
	sprintf(szFilename, "%sshared1/content.map", FULL_WII_USER_DIR);
	if (File::Exists(szFilename))
	{
		FILE* pFile = fopen(szFilename, "rb");
		while(!feof(pFile))
		{
			SElement Element;
			if (fread(&Element, sizeof(SElement), 1, pFile) == 1)
			{
				m_Elements.push_back(Element);
			}
		}
		fclose(pFile);
	}
}

CSharedContent::~CSharedContent()
{}

std::string CSharedContent::GetFilenameFromSHA1(u8* _pHash)
{
	for (size_t i=0; i<m_Elements.size(); i++)
	{
		if (memcmp(_pHash, m_Elements[i].SHA1Hash, 20) == 0)
		{
			char szFilename[1024];
			sprintf(szFilename,  "%sshared1/%c%c%c%c%c%c%c%c.app", FULL_WII_USER_DIR, 
				m_Elements[i].FileName[0], m_Elements[i].FileName[1], m_Elements[i].FileName[2], m_Elements[i].FileName[3],
				m_Elements[i].FileName[4], m_Elements[i].FileName[5], m_Elements[i].FileName[6], m_Elements[i].FileName[7]);
			return szFilename;
		}
	}
	return "unk";
}



// this classes must be created by the CNANDContentManager
class CNANDContentLoader : public INANDContentLoader
{
public:

	CNANDContentLoader(const std::string& _rName);

	virtual ~CNANDContentLoader();

	bool IsValid() const	{ return m_Valid; }
	u64 GetTitleID() const  { return m_TitleID; }
	u16 GetIosVersion() const { return m_IosVersion; }
	u32 GetBootIndex() const  { return m_BootIndex; }
	size_t GetContentSize() const { return m_Content.size(); }
	const SNANDContent* GetContentByIndex(int _Index) const;
	const u8* GetTicketView() const { return m_TicketView; }
	const u8* GetTmdHeader() const { return m_TmdHeader; }

	const std::vector<SNANDContent>& GetContent() const { return m_Content; }

	const u16 GetTitleVersion() const {return m_TileVersion;}
	const u16 GetNumEntries() const {return m_numEntries;}
	const DiscIO::IVolume::ECountry GetCountry() const;

private:

	bool m_Valid;
	u64 m_TitleID;
	u16 m_IosVersion;
	u32 m_BootIndex;
	u16 m_numEntries;
	u16 m_TileVersion;
	u8 m_TicketView[TICKET_VIEW_SIZE];
	u8 m_TmdHeader[TMD_HEADER_SIZE];

	std::vector<SNANDContent> m_Content;


	bool CreateFromDirectory(const std::string& _rPath);
	bool CreateFromWAD(const std::string& _rName);

	void AESDecode(u8* _pKey, u8* _IV, u8* _pSrc, u32 _Size, u8* _pDest);

	void GetKeyFromTicket(u8* pTicket, u8* pTicketKey);

	bool ParseTMD(u8* pDataApp, u32 pDataAppSize, u8* pTicket, u8* pTMD);
};




CNANDContentLoader::CNANDContentLoader(const std::string& _rName)
	: m_Valid(false)
	, m_TitleID(-1)
	, m_IosVersion(0x09)
	, m_BootIndex(-1)
{
	if (File::IsDirectory(_rName.c_str()))
	{
		m_Valid = CreateFromDirectory(_rName);
	}
	else if (File::Exists(_rName.c_str()))
	{
		m_Valid = CreateFromWAD(_rName);
	}
	else
	{
//		_dbg_assert_msg_(BOOT, 0, "CNANDContentLoader loads neither folder nor file");
	}
}

CNANDContentLoader::~CNANDContentLoader()
{
	for (size_t i=0; i<m_Content.size(); i++)
	{
		delete [] m_Content[i].m_pData;
	}
	m_Content.clear();
}

const SNANDContent* CNANDContentLoader::GetContentByIndex(int _Index) const
{
	for (size_t i=0; i<m_Content.size(); i++)
	{
		if (m_Content[i].m_Index == _Index)
			return &m_Content[i];
	}
	return NULL;
}

bool CNANDContentLoader::CreateFromWAD(const std::string& _rName)
{
	WiiWAD Wad(_rName);
	if (!Wad.IsValid())
		return false;

	return ParseTMD(Wad.GetDataApp(), Wad.GetDataAppSize(), Wad.GetTicket(), Wad.GetTMD());
}

bool CNANDContentLoader::CreateFromDirectory(const std::string& _rPath)
{
	std::string TMDFileName(_rPath);
	TMDFileName += "/title.tmd";

	FILE* pTMDFile = fopen(TMDFileName.c_str(), "rb");
	if (pTMDFile == NULL) {
		ERROR_LOG(DISCIO, "CreateFromDirectory: error opening %s", 
				  TMDFileName.c_str());
		return false;
	}
	u64 Size = File::GetSize(TMDFileName.c_str());
	u8* pTMD = new u8[(u32)Size];
	fread(pTMD, (size_t)Size, 1, pTMDFile);
	fclose(pTMDFile);

	memcpy(m_TicketView, pTMD + 0x180, TICKET_VIEW_SIZE);
	memcpy(m_TmdHeader, pTMD, TMD_HEADER_SIZE);

	//////
	m_TileVersion = Common::swap16(pTMD + 0x01dc);
	m_numEntries = Common::swap16(pTMD + 0x01de);
	m_BootIndex = Common::swap16(pTMD + 0x01e0);
	m_TitleID = Common::swap64(pTMD + 0x018c);
	m_IosVersion = Common::swap16(pTMD + 0x018a);

	m_Content.resize(m_numEntries);

	for (u32 i = 0; i < m_numEntries; i++) 
	{
		SNANDContent& rContent = m_Content[i];

		rContent.m_ContentID = Common::swap32(pTMD + 0x01e4 + 0x24*i);
		rContent.m_Index = Common::swap16(pTMD + 0x01e8 + 0x24*i);
		rContent.m_Type = Common::swap16(pTMD + 0x01ea + 0x24*i);
		rContent.m_Size = (u32)Common::swap64(pTMD + 0x01ec + 0x24*i);
		memcpy(rContent.m_SHA1Hash, pTMD + 0x01f4 + 0x24*i, 20);
		memcpy(rContent.m_Header, pTMD + 0x01e4 + 0x24*i, 36);

		rContent.m_pData = NULL;		 
		char szFilename[1024];

		if (rContent.m_Type & 0x8000)  // shared app
		{
			std::string Filename = CSharedContent::AccessInstance().GetFilenameFromSHA1(rContent.m_SHA1Hash);
			strcpy(szFilename, Filename.c_str());
		}
		else
		{
			sprintf(szFilename, "%s/%08x.app", _rPath.c_str(), rContent.m_ContentID);
		}

		INFO_LOG(DISCIO, "NANDContentLoader: load %s", szFilename);

		FILE* pFile = fopen(szFilename, "rb");
		if (pFile != NULL)
		{
			u64 ContentSize = File::GetSize(szFilename);
			rContent.m_pData = new u8[(u32)ContentSize];

			_dbg_assert_msg_(BOOT, rContent.m_Size==ContentSize, "TMDLoader: Filesize doesnt fit (%s %i)... prolly you have a bad dump", szFilename, i);

			fread(rContent.m_pData, (size_t)ContentSize, 1, pFile);
			fclose(pFile);
		} 
		else 
		{
			ERROR_LOG(DISCIO, "NANDContentLoader: error opening %s", szFilename);
		}
	}

	return true;
}

void CNANDContentLoader::AESDecode(u8* _pKey, u8* _IV, u8* _pSrc, u32 _Size, u8* _pDest)
{
	AES_KEY AESKey;

	AES_set_decrypt_key(_pKey, 128, &AESKey);
	AES_cbc_encrypt(_pSrc, _pDest, _Size, &AESKey, _IV, AES_DECRYPT);
}

void CNANDContentLoader::GetKeyFromTicket(u8* pTicket, u8* pTicketKey)
{
	u8 CommonKey[16] = {0xeb,0xe4,0x2a,0x22,0x5e,0x85,0x93,0xe4,0x48,0xd9,0xc5,0x45,0x73,0x81,0xaa,0xf7};	
	u8 IV[16];
	memset(IV, 0, sizeof IV);
	memcpy(IV, pTicket + 0x01dc, 8);
	AESDecode(CommonKey, IV, pTicket + 0x01bf, 16, pTicketKey);
}

bool CNANDContentLoader::ParseTMD(u8* pDataApp, u32 pDataAppSize, u8* pTicket, u8* pTMD)
{
	u8 DecryptTitleKey[16];
	u8 IV[16];

	GetKeyFromTicket(pTicket, DecryptTitleKey);

	memcpy(m_TicketView, pTMD + 0x180, TICKET_VIEW_SIZE);
	memcpy(m_TmdHeader, pTMD, TMD_HEADER_SIZE);
	
	m_TileVersion = Common::swap16(pTMD + 0x01dc);
	m_numEntries = Common::swap16(pTMD + 0x01de);
	m_BootIndex = Common::swap16(pTMD + 0x01e0);
	m_TitleID = Common::swap64(pTMD + 0x018c);
	m_IosVersion = Common::swap16(pTMD + 0x018a);

	u8* p = pDataApp;

	m_Content.resize(m_numEntries);

	for (u32 i=0; i<m_numEntries; i++) 
	{
		SNANDContent& rContent = m_Content[i];
				
		rContent.m_ContentID = Common::swap32(pTMD + 0x01e4 + 0x24*i);
		rContent.m_Index = Common::swap16(pTMD + 0x01e8 + 0x24*i);
		rContent.m_Type = Common::swap16(pTMD + 0x01ea + 0x24*i);
		rContent.m_Size= (u32)Common::swap64(pTMD + 0x01ec + 0x24*i);
		memcpy(rContent.m_SHA1Hash, pTMD + 0x01f4 + 0x24*i, 20);
		memcpy(rContent.m_Header, pTMD + 0x01e4 + 0x24*i, 36);

		u32 RoundedSize = ROUND_UP(rContent.m_Size, 0x40);
		rContent.m_pData = new u8[RoundedSize];
		
		memset(IV, 0, sizeof IV);
		memcpy(IV, pTMD + 0x01e8 + 0x24*i, 2);
		AESDecode(DecryptTitleKey, IV, p, RoundedSize, rContent.m_pData);

		p += RoundedSize;
	}

	return true;
}

const DiscIO::IVolume::ECountry CNANDContentLoader::GetCountry() const
{
	if (!IsValid())
		return DiscIO::IVolume::COUNTRY_UNKNOWN;

	u64 TitleID = GetTitleID();
	char* pTitleID = (char*)&TitleID;

	return CountrySwitch((u8)pTitleID[0]);


}



CNANDContentManager CNANDContentManager::m_Instance;


CNANDContentManager::~CNANDContentManager()
{
	CNANDContentMap::iterator itr = m_Map.begin();
	while (itr != m_Map.end())
	{
		delete itr->second;
		itr++;
	}
	m_Map.clear();
}

const INANDContentLoader& CNANDContentManager::GetNANDLoader(const std::string& _rName)
{
	std::string KeyString(_rName);

	std::transform(KeyString.begin(), KeyString.end(), KeyString.begin(),
		(int(*)(int)) toupper);


	CNANDContentMap::iterator itr = m_Map.find(KeyString);
	if (itr != m_Map.end())
		return *itr->second;

	m_Map[KeyString] = new CNANDContentLoader(KeyString);
	return *m_Map[KeyString];
}

} // namespace end

