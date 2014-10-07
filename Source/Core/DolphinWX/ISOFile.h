// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include <string>
#include <vector>

#include "Common/CommonTypes.h"
#include "DiscIO/Volume.h"

#if defined(HAVE_WX) && HAVE_WX
#include <wx/image.h>
#endif

class PointerWrap;
class GameListItem : NonCopyable
{
public:
	GameListItem(const std::string& _rFileName);
	~GameListItem();

	bool IsValid() const {return m_Valid;}
	const std::string& GetFileName() const {return m_FileName;}
	std::string GetBannerName(int index) const;
	std::string GetVolumeName(int index) const;
	std::string GetName(int index) const;
	std::string GetCompany() const;
	std::string GetDescription(int index = 0) const;
	int GetRevision() const { return m_Revision; }
	const std::string& GetUniqueID() const {return m_UniqueID;}
	const std::string GetWiiFSPath() const;
	DiscIO::IVolume::ECountry GetCountry() const {return m_Country;}
	int GetPlatform() const {return m_Platform;}
	const std::string& GetIssues() const { return m_issues; }
	int GetEmuState() const { return m_emu_state; }
	bool IsCompressed() const {return m_BlobCompressed;}
	u64 GetFileSize() const {return m_FileSize;}
	u64 GetVolumeSize() const {return m_VolumeSize;}
	bool IsDiscTwo() const {return m_IsDiscTwo;}
#if defined(HAVE_WX) && HAVE_WX
	const wxBitmap& GetBitmap() const {return m_Bitmap;}
#endif

	void DoState(PointerWrap &p);

	enum
	{
		GAMECUBE_DISC = 0,
		WII_DISC,
		WII_WAD,
		NUMBER_OF_PLATFORMS
	};

private:
	std::string m_FileName;

	// TODO: eliminate this and overwrite with names from banner when available?
	std::vector<std::string> m_volume_names;

	// Stuff from banner
	std::string m_company;
	std::vector<std::string> m_banner_names;
	std::vector<std::string> m_descriptions;

	std::string m_UniqueID;

	std::string m_issues;
	int m_emu_state;

	u64 m_FileSize;
	u64 m_VolumeSize;

	DiscIO::IVolume::ECountry m_Country;
	int m_Platform;
	int m_Revision;

#if defined(HAVE_WX) && HAVE_WX
	wxBitmap m_Bitmap;
#endif
	bool m_Valid;
	bool m_BlobCompressed;
	std::vector<u8> m_pImage;
	int m_ImageWidth, m_ImageHeight;
	bool m_IsDiscTwo;

	bool LoadFromCache();
	void SaveToCache();

	std::string CreateCacheFilename();
};
