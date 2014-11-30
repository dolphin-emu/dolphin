// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include <cstddef>
#include <cstdio>
#include <cstring>
#include <functional>
#include <map>
#include <string>
#include <utility>
#include <vector>
#include <polarssl/aes.h>

#include "Common/CommonTypes.h"
#include "Common/FileUtil.h"
#include "Common/MathUtil.h"
#include "Common/NandPaths.h"
#include "Common/StringUtil.h"
#include "Common/Logging/Log.h"

#include "DiscIO/NANDContentLoader.h"
#include "DiscIO/Volume.h"
#include "DiscIO/WiiWad.h"

namespace DiscIO
{
CSharedContent CSharedContent::m_Instance;
cUIDsys cUIDsys::m_Instance;


CSharedContent::CSharedContent()
{
	UpdateLocation();
}

void CSharedContent::UpdateLocation()
{
	m_Elements.clear();
	m_lastID = 0;
	m_contentMap = StringFromFormat("%sshared1/content.map", File::GetUserPath(D_WIIUSER_IDX).c_str());

	File::IOFile pFile(m_contentMap, "rb");
	SElement Element;
	while (pFile.ReadArray(&Element, 1))
	{
		m_Elements.push_back(Element);
		m_lastID++;
	}
}

CSharedContent::~CSharedContent()
{}

std::string CSharedContent::GetFilenameFromSHA1(const u8* _pHash)
{
	for (auto& Element : m_Elements)
	{
		if (memcmp(_pHash, Element.SHA1Hash, 20) == 0)
		{
			return StringFromFormat("%sshared1/%c%c%c%c%c%c%c%c.app", File::GetUserPath(D_WIIUSER_IDX).c_str(),
			    Element.FileName[0], Element.FileName[1], Element.FileName[2], Element.FileName[3],
			    Element.FileName[4], Element.FileName[5], Element.FileName[6], Element.FileName[7]);
		}
	}
	return "unk";
}

std::string CSharedContent::AddSharedContent(const u8* _pHash)
{
	std::string filename = GetFilenameFromSHA1(_pHash);

	if (strcasecmp(filename.c_str(), "unk") == 0)
	{
		std::string id = StringFromFormat("%08x", m_lastID);
		SElement Element;
		memcpy(Element.FileName, id.c_str(), 8);
		memcpy(Element.SHA1Hash, _pHash, 20);
		m_Elements.push_back(Element);

		File::CreateFullPath(m_contentMap);

		File::IOFile pFile(m_contentMap, "ab");
		pFile.WriteArray(&Element, 1);

		filename = StringFromFormat("%sshared1/%s.app", File::GetUserPath(D_WIIUSER_IDX).c_str(), id.c_str());
		m_lastID++;
	}

	return filename;
}


// this classes must be created by the CNANDContentManager
class CNANDContentLoader : public INANDContentLoader
{
public:
	CNANDContentLoader(const std::string& _rName);
	virtual ~CNANDContentLoader();

	bool IsValid() const override { return m_Valid; }
	void RemoveTitle() const override;
	u64 GetTitleID() const override  { return m_TitleID; }
	u16 GetIosVersion() const override { return m_IosVersion; }
	u32 GetBootIndex() const override  { return m_BootIndex; }
	size_t GetContentSize() const override { return m_Content.size(); }
	const SNANDContent* GetContentByIndex(int _Index) const override;
	const u8* GetTMDView() const override { return m_TMDView; }
	const u8* GetTMDHeader() const override { return m_TMDHeader; }
	u32 GetTIKSize() const override { return m_TIKSize; }
	const u8* GetTIK() const override { return m_TIK; }

	const std::vector<SNANDContent>& GetContent() const override { return m_Content; }

	u16 GetTitleVersion() const override {return m_TitleVersion;}
	u16 GetNumEntries() const override {return m_numEntries;}
	DiscIO::IVolume::ECountry GetCountry() const override;
	u8 GetCountryChar() const override {return m_Country; }

private:
	bool m_Valid;
	bool m_isWAD;
	std::string m_Path;
	u64 m_TitleID;
	u16 m_IosVersion;
	u32 m_BootIndex;
	u16 m_numEntries;
	u16 m_TitleVersion;
	u8 m_TMDView[TMD_VIEW_SIZE];
	u8 m_TMDHeader[TMD_HEADER_SIZE];
	u32 m_TIKSize;
	u8* m_TIK;
	u8 m_Country;

	std::vector<SNANDContent> m_Content;


	bool Initialize(const std::string& _rName);

	void AESDecode(u8* _pKey, u8* _IV, u8* _pSrc, u32 _Size, u8* _pDest);

	void GetKeyFromTicket(u8* pTicket, u8* pTicketKey);
};


CNANDContentLoader::CNANDContentLoader(const std::string& _rName)
	: m_Valid(false)
	, m_isWAD(false)
	, m_TitleID(-1)
	, m_IosVersion(0x09)
	, m_BootIndex(-1)
	, m_TIKSize(0)
	, m_TIK(nullptr)
{
	m_Valid = Initialize(_rName);
}

CNANDContentLoader::~CNANDContentLoader()
{
	for (auto& content : m_Content)
	{
		delete [] content.m_pData;
	}
	m_Content.clear();
	if (m_TIK)
	{
		delete []m_TIK;
		m_TIK = nullptr;
	}
}

const SNANDContent* CNANDContentLoader::GetContentByIndex(int _Index) const
{
	for (auto& Content : m_Content)
	{
		if (Content.m_Index == _Index)
		{
			return &Content;
		}
	}
	return nullptr;
}

bool CNANDContentLoader::Initialize(const std::string& _rName)
{
	if (_rName.empty())
		return false;
	m_Path = _rName;
	WiiWAD Wad(_rName);
	u8* pDataApp = nullptr;
	u8* pTMD = nullptr;
	u8 DecryptTitleKey[16];
	u8 IV[16];
	if (Wad.IsValid())
	{
		m_isWAD = true;
		m_TIKSize = Wad.GetTicketSize();
		m_TIK = new u8[m_TIKSize];
		memcpy(m_TIK, Wad.GetTicket(), m_TIKSize);
		GetKeyFromTicket(m_TIK, DecryptTitleKey);
		u32 pTMDSize = Wad.GetTMDSize();
		pTMD = new u8[pTMDSize];
		memcpy(pTMD, Wad.GetTMD(), pTMDSize);
		pDataApp = Wad.GetDataApp();
	}
	else
	{
		std::string TMDFileName(m_Path);

		if ('/' == *TMDFileName.rbegin())
			TMDFileName += "title.tmd";
		else
			m_Path = TMDFileName.substr(0, TMDFileName.find("title.tmd"));

		File::IOFile pTMDFile(TMDFileName, "rb");
		if (!pTMDFile)
		{
			WARN_LOG(DISCIO, "CreateFromDirectory: error opening %s", TMDFileName.c_str());
			return false;
		}
		u32 pTMDSize = (u32)File::GetSize(TMDFileName);
		pTMD = new u8[pTMDSize];
		pTMDFile.ReadBytes(pTMD, (size_t)pTMDSize);
		pTMDFile.Close();
	}

	memcpy(m_TMDView, pTMD + 0x180, TMD_VIEW_SIZE);
	memcpy(m_TMDHeader, pTMD, TMD_HEADER_SIZE);


	m_TitleVersion = Common::swap16(pTMD + 0x01dc);
	m_numEntries = Common::swap16(pTMD + 0x01de);
	m_BootIndex = Common::swap16(pTMD + 0x01e0);
	m_TitleID = Common::swap64(pTMD + 0x018c);
	m_IosVersion = Common::swap16(pTMD + 0x018a);
	m_Country = *(u8*)&m_TitleID;
	if (m_Country == 2) // SYSMENU
		m_Country = GetSysMenuRegion(m_TitleVersion);

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

		if (m_isWAD)
		{
			u32 RoundedSize = ROUND_UP(rContent.m_Size, 0x40);
			rContent.m_pData = new u8[RoundedSize];

			memset(IV, 0, sizeof IV);
			memcpy(IV, pTMD + 0x01e8 + 0x24*i, 2);
			AESDecode(DecryptTitleKey, IV, pDataApp, RoundedSize, rContent.m_pData);

			pDataApp += RoundedSize;
			continue;
		}

		rContent.m_pData = nullptr;

		if (rContent.m_Type & 0x8000)  // shared app
			rContent.m_Filename = CSharedContent::AccessInstance().GetFilenameFromSHA1(rContent.m_SHA1Hash);
		else
			rContent.m_Filename = StringFromFormat("%s/%08x.app", m_Path.c_str(), rContent.m_ContentID);

		// Be graceful about incorrect tmds.
		if (File::Exists(rContent.m_Filename))
			rContent.m_Size = (u32) File::GetSize(rContent.m_Filename);
	}

	delete [] pTMD;
	return true;
}
void CNANDContentLoader::AESDecode(u8* _pKey, u8* _IV, u8* _pSrc, u32 _Size, u8* _pDest)
{
	aes_context AES_ctx;

	aes_setkey_dec(&AES_ctx, _pKey, 128);
	aes_crypt_cbc(&AES_ctx, AES_DECRYPT, _Size, _IV, _pSrc, _pDest);
}

void CNANDContentLoader::GetKeyFromTicket(u8* pTicket, u8* pTicketKey)
{
	u8 CommonKey[16] = {0xeb,0xe4,0x2a,0x22,0x5e,0x85,0x93,0xe4,0x48,0xd9,0xc5,0x45,0x73,0x81,0xaa,0xf7};
	u8 IV[16];
	memset(IV, 0, sizeof IV);
	memcpy(IV, pTicket + 0x01dc, 8);
	AESDecode(CommonKey, IV, pTicket + 0x01bf, 16, pTicketKey);
}


DiscIO::IVolume::ECountry CNANDContentLoader::GetCountry() const
{
	if (!IsValid())
		return DiscIO::IVolume::COUNTRY_UNKNOWN;

	return CountrySwitch(m_Country);
}


CNANDContentManager CNANDContentManager::m_Instance;


CNANDContentManager::~CNANDContentManager()
{
	for (auto& entry : m_Map)
	{
		delete entry.second;
	}
	m_Map.clear();
}

const INANDContentLoader& CNANDContentManager::GetNANDLoader(const std::string& _rName, bool forceReload)
{
	CNANDContentMap::iterator lb = m_Map.lower_bound(_rName);

	if (lb == m_Map.end() || (m_Map.key_comp()(_rName, lb->first)))
	{
		m_Map.insert(lb, CNANDContentMap::value_type(_rName, new CNANDContentLoader(_rName)));
	}
	else
	{
		if (!lb->second->IsValid() || forceReload)
		{
			delete lb->second;
			lb->second = new CNANDContentLoader(_rName);
		}
	}
	return *m_Map[_rName];
}

const INANDContentLoader& CNANDContentManager::GetNANDLoader(u64 _titleId, bool forceReload)
{
	std::string _rName = Common::GetTitleContentPath(_titleId);
	return GetNANDLoader(_rName, forceReload);
}
bool CNANDContentManager::RemoveTitle(u64 _titleID)
{
	if (!GetNANDLoader(_titleID).IsValid())
		return false;
	GetNANDLoader(_titleID).RemoveTitle();
	return GetNANDLoader(_titleID, true).IsValid();
}

void CNANDContentLoader::RemoveTitle() const
{
	INFO_LOG(DISCIO, "RemoveTitle %08x/%08x", (u32)(m_TitleID >> 32), (u32)m_TitleID);
	if (IsValid())
	{
		// remove tmd?
		for (u32 i = 0; i < m_numEntries; i++)
		{
			if (!(m_Content[i].m_Type & 0x8000)) // skip shared apps
			{
				std::string filename = StringFromFormat("%s%08x.app", Common::GetTitleContentPath(m_TitleID).c_str(), m_Content[i].m_ContentID);
				INFO_LOG(DISCIO, "Delete %s", filename.c_str());
				File::Delete(filename);
			}
		}
	}
}

cUIDsys::cUIDsys()
{
	UpdateLocation();
}

void cUIDsys::UpdateLocation()
{
	m_Elements.clear();
	m_lastUID = 0x00001000;
	m_uidSys = StringFromFormat("%ssys/uid.sys", File::GetUserPath(D_WIIUSER_IDX).c_str());

	File::IOFile pFile(m_uidSys, "rb");
	SElement Element;
	while (pFile.ReadArray(&Element, 1))
	{
		*(u32*)&(Element.UID) = Common::swap32(m_lastUID++);
		m_Elements.push_back(Element);
	}
	pFile.Close();

	if (m_Elements.empty())
	{
		*(u64*)&(Element.titleID) = Common::swap64(TITLEID_SYSMENU);
		*(u32*)&(Element.UID) = Common::swap32(m_lastUID++);

		File::CreateFullPath(m_uidSys);
		pFile.Open(m_uidSys, "wb");
		if (!pFile.WriteArray(&Element, 1))
			ERROR_LOG(DISCIO, "Failed to write to %s", m_uidSys.c_str());
	}
}

cUIDsys::~cUIDsys()
{}

u32 cUIDsys::GetUIDFromTitle(u64 _Title)
{
	for (auto& Element : m_Elements)
	{
		if (Common::swap64(_Title) == *(u64*)&(Element.titleID))
		{
			return Common::swap32(Element.UID);
		}
	}
	return 0;
}

void cUIDsys::AddTitle(u64 _TitleID)
{
	if (GetUIDFromTitle(_TitleID))
	{
		INFO_LOG(DISCIO, "Title %08x%08x, already exists in uid.sys", (u32)(_TitleID >> 32), (u32)_TitleID);
		return;
	}

	SElement Element;
	*(u64*)&(Element.titleID) = Common::swap64(_TitleID);
	*(u32*)&(Element.UID) = Common::swap32(m_lastUID++);
	m_Elements.push_back(Element);

	File::CreateFullPath(m_uidSys);
	File::IOFile pFile(m_uidSys, "ab");

	if (!pFile.WriteArray(&Element, 1))
		ERROR_LOG(DISCIO, "fwrite failed");
}

void cUIDsys::GetTitleIDs(std::vector<u64>& _TitleIDs, bool _owned)
{
	for (auto& Element : m_Elements)
	{
		if ((_owned && Common::CheckTitleTIK(Common::swap64(Element.titleID)))  ||
			(!_owned && Common::CheckTitleTMD(Common::swap64(Element.titleID))))
			_TitleIDs.push_back(Common::swap64(Element.titleID));
	}
}

u64 CNANDContentManager::Install_WiiWAD(std::string &fileName)
{
	if (fileName.find(".wad") == std::string::npos)
		return 0;
	const INANDContentLoader& ContentLoader = GetNANDLoader(fileName);
	if (ContentLoader.IsValid() == false)
		return 0;

	u64 TitleID = ContentLoader.GetTitleID();

	//copy WAD's tmd header and contents to content directory

	std::string ContentPath(Common::GetTitleContentPath(TitleID));
	std::string TMDFileName(Common::GetTMDFileName(TitleID));
	File::CreateFullPath(TMDFileName);

	File::IOFile pTMDFile(TMDFileName, "wb");
	if (!pTMDFile)
	{
		PanicAlertT("WAD installation failed: error creating %s", TMDFileName.c_str());
		return 0;
	}

	pTMDFile.WriteBytes(ContentLoader.GetTMDHeader(), INANDContentLoader::TMD_HEADER_SIZE);

	for (u32 i = 0; i < ContentLoader.GetContentSize(); i++)
	{
		const SNANDContent& Content = ContentLoader.GetContent()[i];

		pTMDFile.WriteBytes(Content.m_Header, INANDContentLoader::CONTENT_HEADER_SIZE);

		std::string APPFileName;
		if (Content.m_Type & 0x8000) //shared
			APPFileName = CSharedContent::AccessInstance().AddSharedContent(Content.m_SHA1Hash);
		else
			APPFileName = StringFromFormat("%s%08x.app", ContentPath.c_str(), Content.m_ContentID);

		if (!File::Exists(APPFileName))
		{
			File::CreateFullPath(APPFileName);
			File::IOFile pAPPFile(APPFileName, "wb");
			if (!pAPPFile)
			{
				PanicAlertT("WAD installation failed: error creating %s", APPFileName.c_str());
				return 0;
			}

			pAPPFile.WriteBytes(Content.m_pData, Content.m_Size);
		}
		else
		{
			INFO_LOG(DISCIO, "Content %s already exists.", APPFileName.c_str());
		}
	}

	//Extract and copy WAD's ticket to ticket directory
	if (!Add_Ticket(TitleID, ContentLoader.GetTIK(), ContentLoader.GetTIKSize()))
	{
		PanicAlertT("WAD installation failed: error creating ticket");
		return 0;
	}

	cUIDsys::AccessInstance().AddTitle(TitleID);

	return TitleID;
}

bool Add_Ticket(u64 TitleID, const u8* p_tik, u32 tikSize)
{
	std::string TicketFileName = Common::GetTicketFileName(TitleID);
	File::CreateFullPath(TicketFileName);
	File::IOFile pTicketFile(TicketFileName, "wb");
	if (!pTicketFile || !p_tik)
	{
		//PanicAlertT("WAD installation failed: error creating %s", TicketFileName.c_str());
		return false;
	}
	return pTicketFile.WriteBytes(p_tik, tikSize);
}

} // namespace end

