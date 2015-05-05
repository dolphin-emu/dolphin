// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include <cinttypes>
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>
#include <utility>
#include <vector>
#include <wx/app.h>
#include <wx/bitmap.h>
#include <wx/filefn.h>
#include <wx/gdicmn.h>
#include <wx/image.h>
#include <wx/string.h>
#include <wx/window.h>

#include "Common/ChunkFile.h"
#include "Common/CommonPaths.h"
#include "Common/CommonTypes.h"
#include "Common/FileUtil.h"
#include "Common/Hash.h"
#include "Common/IniFile.h"
#include "Common/StringUtil.h"

#include "Core/ConfigManager.h"
#include "Core/CoreParameter.h"
#include "Core/Boot/Boot.h"

#include "DiscIO/CompressedBlob.h"
#include "DiscIO/Filesystem.h"
#include "DiscIO/Volume.h"
#include "DiscIO/VolumeCreator.h"

#include "DolphinWX/ISOFile.h"
#include "DolphinWX/WxUtils.h"

static const u32 CACHE_REVISION = 0x123;

#define DVD_BANNER_WIDTH 96
#define DVD_BANNER_HEIGHT 32

static std::string GetLanguageString(IVolume::ELanguage language, std::map<IVolume::ELanguage, std::string> strings)
{
	auto end = strings.end();
	auto it = strings.find(language);
	if (it != end)
		return it->second;

	// English tends to be a good fallback when the requested language isn't available
	if (language != IVolume::ELanguage::LANGUAGE_ENGLISH)
	{
		it = strings.find(IVolume::ELanguage::LANGUAGE_ENGLISH);
		if (it != end)
			return it->second;
	}

	// If English isn't available either, just pick something
	if (!strings.empty())
		return strings.cbegin()->second;

	return "";
}

GameListItem::GameListItem(const std::string& _rFileName)
	: m_FileName(_rFileName)
	, m_emu_state(0)
	, m_FileSize(0)
	, m_Revision(0)
	, m_Valid(false)
	, m_BlobCompressed(false)
	, m_ImageWidth(0)
	, m_ImageHeight(0)
{
	if (LoadFromCache())
	{
		m_Valid = true;
	}
	else
	{
		DiscIO::IVolume* pVolume = DiscIO::CreateVolumeFromFilename(_rFileName);

		if (pVolume != nullptr)
		{
			if (!pVolume->IsWadFile())
				m_Platform = pVolume->IsWiiDisc() ? WII_DISC : GAMECUBE_DISC;
			else
				m_Platform = WII_WAD;

			m_names = pVolume->GetNames();
			m_descriptions = pVolume->GetDescriptions();
			m_company = pVolume->GetCompany();

			m_Country = pVolume->GetCountry();
			m_FileSize = pVolume->GetRawSize();
			m_VolumeSize = pVolume->GetSize();

			m_UniqueID = pVolume->GetUniqueID();
			m_BlobCompressed = DiscIO::IsCompressedBlob(_rFileName);
			m_IsDiscTwo = pVolume->IsDiscTwo();
			m_Revision = pVolume->GetRevision();

			std::vector<u32> Buffer = pVolume->GetBanner(&m_ImageWidth, &m_ImageHeight);
			u32* pData = Buffer.data();
			m_pImage.resize(m_ImageWidth * m_ImageHeight * 3);

			for (int i = 0; i < m_ImageWidth * m_ImageHeight; i++)
			{
				m_pImage[i * 3 + 0] = (pData[i] & 0xFF0000) >> 16;
				m_pImage[i * 3 + 1] = (pData[i] & 0x00FF00) >> 8;
				m_pImage[i * 3 + 2] = (pData[i] & 0x0000FF) >> 0;
			}

			delete pVolume;

			m_Valid = true;

			// Create a cache file only if we have an image.
			// Wii ISOs create their images after you have generated the first savegame
			if (!m_pImage.empty())
				SaveToCache();
		}
	}

	if (IsValid())
	{
		IniFile ini = SCoreStartupParameter::LoadGameIni(m_UniqueID, m_Revision);
		ini.GetIfExists("EmuState", "EmulationStateId", &m_emu_state);
		ini.GetIfExists("EmuState", "EmulationIssues", &m_issues);
	}

	if (!m_pImage.empty())
	{
		wxImage Image(m_ImageWidth, m_ImageHeight, &m_pImage[0], true);
		double Scale = wxTheApp->GetTopWindow()->GetContentScaleFactor();
		// Note: This uses nearest neighbor, which subjectively looks a lot
		// better for GC banners than smooth scaling.
		Image.Rescale(DVD_BANNER_WIDTH * Scale, DVD_BANNER_HEIGHT * Scale);
#ifdef __APPLE__
		m_Bitmap = wxBitmap(Image, -1, Scale);
#else
		m_Bitmap = wxBitmap(Image, -1);
#endif
	}
	else
	{
		// default banner
		m_Bitmap.LoadFile(StrToWxStr(File::GetThemeDir(SConfig::GetInstance().m_LocalCoreStartupParameter.theme_name)) + "nobanner.png", wxBITMAP_TYPE_PNG);
	}
}

GameListItem::~GameListItem()
{
}

bool GameListItem::LoadFromCache()
{
	return CChunkFileReader::Load<GameListItem>(CreateCacheFilename(), CACHE_REVISION, *this);
}

void GameListItem::SaveToCache()
{
	if (!File::IsDirectory(File::GetUserPath(D_CACHE_IDX)))
		File::CreateDir(File::GetUserPath(D_CACHE_IDX));

	CChunkFileReader::Save<GameListItem>(CreateCacheFilename(), CACHE_REVISION, *this);
}

void GameListItem::DoState(PointerWrap &p)
{
	p.Do(m_names);
	p.Do(m_descriptions);
	p.Do(m_company);
	p.Do(m_UniqueID);
	p.Do(m_FileSize);
	p.Do(m_VolumeSize);
	p.Do(m_Country);
	p.Do(m_BlobCompressed);
	p.Do(m_pImage);
	p.Do(m_ImageWidth);
	p.Do(m_ImageHeight);
	p.Do(m_Platform);
	p.Do(m_IsDiscTwo);
	p.Do(m_Revision);
}

std::string GameListItem::CreateCacheFilename()
{
	std::string Filename, LegalPathname, extension;
	SplitPath(m_FileName, &LegalPathname, &Filename, &extension);

	if (Filename.empty()) return Filename; // Disc Drive

	// Filename.extension_HashOfFolderPath_Size.cache
	// Append hash to prevent ISO name-clashing in different folders.
	Filename.append(StringFromFormat("%s_%x_%" PRIx64 ".cache",
		extension.c_str(), HashFletcher((const u8 *)LegalPathname.c_str(), LegalPathname.size()),
		File::GetSize(m_FileName)));

	std::string fullname(File::GetUserPath(D_CACHE_IDX));
	fullname += Filename;
	return fullname;
}

std::string GameListItem::GetCompany() const
{
	return m_company;
}

std::string GameListItem::GetDescription(IVolume::ELanguage language) const
{
	return GetLanguageString(language, m_descriptions);
}

std::string GameListItem::GetDescription() const
{
	return GetDescription(SConfig::GetInstance().m_LocalCoreStartupParameter.GetCurrentLanguage(m_Platform != GAMECUBE_DISC));
}

std::string GameListItem::GetName(IVolume::ELanguage language) const
{
	return GetLanguageString(language, m_names);
}

std::string GameListItem::GetName() const
{
	std::string name = GetName(SConfig::GetInstance().m_LocalCoreStartupParameter.GetCurrentLanguage(m_Platform != GAMECUBE_DISC));
	if (name.empty())
	{
		// No usable name, return filename (better than nothing)
		SplitPath(GetFileName(), nullptr, &name, nullptr);
	}
	return name;
}

std::vector<IVolume::ELanguage> GameListItem::GetLanguages() const
{
	std::vector<IVolume::ELanguage> languages;
	for (std::pair<IVolume::ELanguage, std::string> name : m_names)
		languages.push_back(name.first);
	return languages;
}

const std::string GameListItem::GetWiiFSPath() const
{
	DiscIO::IVolume *iso = DiscIO::CreateVolumeFromFilename(m_FileName);
	std::string ret;

	if (iso == nullptr)
		return ret;

	if (iso->IsWiiDisc() || iso->IsWadFile())
	{
		u64 title = 0;

		iso->GetTitleID((u8*)&title);
		title = Common::swap64(title);

		const std::string path = StringFromFormat("%stitle/%08x/%08x/data/",
				File::GetUserPath(D_WIIUSER_IDX).c_str(), (u32)(title>>32), (u32)title);

		if (!File::Exists(path))
			File::CreateFullPath(path);

		if (path[0] == '.')
			ret = WxStrToStr(wxGetCwd()) + path.substr(strlen(ROOT_DIR));
		else
			ret = path;
	}
	delete iso;

	return ret;
}

