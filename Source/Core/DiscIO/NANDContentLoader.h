// Copyright 2009 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <cstddef>
#include <string>
#include <unordered_map>
#include <vector>

#include "Common/CommonTypes.h"
#include "Common/NandPaths.h"
#include "DiscIO/Volume.h"

namespace DiscIO
{
	bool Add_Ticket(u64 TitleID, const u8 *p_tik, u32 tikSize);
struct SNANDContent
{
	u32 m_ContentID;
	u16 m_Index;
	u16 m_Type;
	u32 m_Size;
	u8 m_SHA1Hash[20];
	u8 m_Header[36]; //all of the above

	std::string m_Filename;
	u8* m_pData;
};

// Instances of this class must be created by CNANDContentManager
class CNANDContentLoader final
{
public:
	CNANDContentLoader(const std::string& _rName);
	virtual ~CNANDContentLoader();

	bool IsValid() const { return m_Valid; }
	void RemoveTitle() const;
	u64 GetTitleID() const  { return m_TitleID; }
	u16 GetIosVersion() const { return m_IosVersion; }
	u32 GetBootIndex() const  { return m_BootIndex; }
	size_t GetContentSize() const { return m_Content.size(); }
	const SNANDContent* GetContentByIndex(int _Index) const;
	const u8* GetTMDView() const { return m_TMDView; }
	const u8* GetTMDHeader() const { return m_TMDHeader; }
	u32 GetTIKSize() const { return m_TIKSize; }
	const u8* GetTIK() const { return m_TIK; }

	const std::vector<SNANDContent>& GetContent() const { return m_Content; }

	u16 GetTitleVersion() const {return m_TitleVersion;}
	u16 GetNumEntries() const {return m_numEntries;}
	DiscIO::IVolume::ECountry GetCountry() const;
	u8 GetCountryChar() const {return m_Country; }

	enum
	{
		TMD_VIEW_SIZE       = 0x58,
		TMD_HEADER_SIZE     = 0x1E4,
		CONTENT_HEADER_SIZE = 0x24,
		TICKET_SIZE         = 0x2A4
	};

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


// we open the NAND Content files too often... let's cache them
class CNANDContentManager
{
public:
	static CNANDContentManager& Access() { static CNANDContentManager instance; return instance; }
	u64 Install_WiiWAD(const std::string& fileName);

	const CNANDContentLoader& GetNANDLoader(const std::string& content_path);
	const CNANDContentLoader& GetNANDLoader(u64 title_id, Common::FromWhichRoot from);
	bool RemoveTitle(u64 titl_id, Common::FromWhichRoot from);
	void ClearCache();

private:
	CNANDContentManager() {}
	~CNANDContentManager();

	CNANDContentManager(CNANDContentManager const&) = delete;
	void operator=(CNANDContentManager const&) = delete;

	std::unordered_map<std::string, std::unique_ptr<CNANDContentLoader>> m_map;
};

class CSharedContent
{
public:
	static CSharedContent& AccessInstance() { static CSharedContent instance; return instance; }

	std::string GetFilenameFromSHA1(const u8* _pHash);
	std::string AddSharedContent(const u8* _pHash);
	void UpdateLocation();

private:
	CSharedContent();
	virtual ~CSharedContent();

	CSharedContent(CSharedContent const&) = delete;
	void operator=(CSharedContent const&) = delete;

#pragma pack(push,1)
	struct SElement
	{
		u8 FileName[8];
		u8 SHA1Hash[20];
	};
#pragma pack(pop)

	u32 m_lastID;
	std::string m_contentMap;
	std::vector<SElement> m_Elements;
};

class cUIDsys
{
public:
	static cUIDsys& AccessInstance() { static cUIDsys instance; return instance; }

	u32 GetUIDFromTitle(u64 _Title);
	void AddTitle(u64 _Title);
	void GetTitleIDs(std::vector<u64>& _TitleIDs, bool _owned = false);
	void UpdateLocation();

private:
	cUIDsys();
	virtual ~cUIDsys();

	cUIDsys(cUIDsys const&) = delete;
	void operator=(cUIDsys const&) = delete;

#pragma pack(push,1)
	struct SElement
	{
		u8 titleID[8];
		u8 UID[4];
	};
#pragma pack(pop)

	u32 m_lastUID;
	std::string m_uidSys;
	std::vector<SElement> m_Elements;
};

}
