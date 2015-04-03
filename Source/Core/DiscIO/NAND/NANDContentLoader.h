// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include <cstddef>
#include <map>
#include <string>
#include <vector>

#include "Common/CommonTypes.h"
#include "DiscIO/Volume/Volume.h"

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

// pure virtual interface so just the NANDContentManager can create these files only
class INANDContentLoader
{
public:

	INANDContentLoader() {}

	virtual ~INANDContentLoader()  {}

	virtual bool IsValid() const = 0;
	virtual void RemoveTitle() const = 0;
	virtual u64 GetTitleID() const = 0;
	virtual u16 GetIosVersion() const = 0;
	virtual u32 GetBootIndex() const = 0;
	virtual size_t GetContentSize() const = 0;
	virtual const SNANDContent* GetContentByIndex(int _Index) const = 0;
	virtual const u8* GetTMDView() const = 0;
	virtual const u8* GetTMDHeader() const = 0;
	virtual u32 GetTIKSize() const = 0;
	virtual const u8* GetTIK() const = 0;
	virtual const std::vector<SNANDContent>& GetContent() const = 0;
	virtual u16 GetTitleVersion() const = 0;
	virtual u16 GetNumEntries() const = 0;
	virtual DiscIO::IVolume::ECountry GetCountry() const = 0;
	virtual u8 GetCountryChar() const = 0;

	enum
	{
		TMD_VIEW_SIZE       = 0x58,
		TMD_HEADER_SIZE     = 0x1E4,
		CONTENT_HEADER_SIZE = 0x24,
		TICKET_SIZE         = 0x2A4
	};
};


// we open the NAND Content files too often... let's cache them
class CNANDContentManager
{
public:
	static CNANDContentManager& Access() { static CNANDContentManager instance; return instance; }
	u64 Install_WiiWAD(std::string &fileName);

	const INANDContentLoader& GetNANDLoader(const std::string& _rName, bool forceReload = false);
	const INANDContentLoader& GetNANDLoader(u64 _titleId, bool forceReload = false);
	bool RemoveTitle(u64 _titleID);

private:
	CNANDContentManager() {}
	~CNANDContentManager();

	CNANDContentManager(CNANDContentManager const&) = delete;
	void operator=(CNANDContentManager const&) = delete;

	typedef std::map<std::string, INANDContentLoader*> CNANDContentMap;
	CNANDContentMap m_Map;
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
