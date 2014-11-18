// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include <cinttypes>
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>
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

#include "DiscIO/BannerLoader.h"
#include "DiscIO/CompressedBlob.h"
#include "DiscIO/Filesystem.h"
#include "DiscIO/Volume.h"
#include "DiscIO/VolumeCreator.h"

#include "DolphinWX/ISOFile.h"
#include "DolphinWX/WxUtils.h"

static const u32 CACHE_REVISION = 0x117;

#define DVD_BANNER_WIDTH 96
#define DVD_BANNER_HEIGHT 32

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
			if (!DiscIO::IsVolumeWadFile(pVolume))
				m_Platform = DiscIO::IsVolumeWiiDisc(pVolume) ? WII_DISC : GAMECUBE_DISC;
			else
				m_Platform = WII_WAD;

			m_volume_names = pVolume->GetNames();

			m_Country  = pVolume->GetCountry();
			m_FileSize = pVolume->GetRawSize();
			m_VolumeSize = pVolume->GetSize();

			m_UniqueID = pVolume->GetUniqueID();
			m_BlobCompressed = DiscIO::IsCompressedBlob(_rFileName);
			m_IsDiscTwo = pVolume->IsDiscTwo();
			m_Revision = pVolume->GetRevision();

			// check if we can get some info from the banner file too
			DiscIO::IFileSystem* pFileSystem = DiscIO::CreateFileSystem(pVolume);

			if (pFileSystem != nullptr || m_Platform == WII_WAD)
			{
				std::unique_ptr<DiscIO::IBannerLoader> pBannerLoader(DiscIO::CreateBannerLoader(*pFileSystem, pVolume));

				if (pBannerLoader != nullptr && pBannerLoader->IsValid())
				{
					if (m_Platform != WII_WAD)
						m_banner_names = pBannerLoader->GetNames();
					m_company = pBannerLoader->GetCompany();
					m_descriptions = pBannerLoader->GetDescriptions();

					std::vector<u32> Buffer = pBannerLoader->GetBanner(&m_ImageWidth, &m_ImageHeight);
					u32* pData = &Buffer[0];
					// resize vector to image size
					m_pImage.resize(m_ImageWidth * m_ImageHeight * 3);

					for (int i = 0; i < m_ImageWidth * m_ImageHeight; i++)
					{
						m_pImage[i * 3 + 0] = (pData[i] & 0xFF0000) >> 16;
						m_pImage[i * 3 + 1] = (pData[i] & 0x00FF00) >>  8;
						m_pImage[i * 3 + 2] = (pData[i] & 0x0000FF) >>  0;
					}
				}

				delete pFileSystem;
			}

			delete pVolume;

			m_Valid = true;

			// Create a cache file only if we have an image.
			// Wii isos create their images after you have generated the first savegame
			if (!m_pImage.empty())
				SaveToCache();
		}
	}

	if (IsValid())
	{
		IniFile ini;
		ini.Load(File::GetSysDirectory() + GAMESETTINGS_DIR DIR_SEP + m_UniqueID + ".ini");
		ini.Load(File::GetUserPath(D_GAMESETTINGS_IDX) + m_UniqueID + ".ini", true);

		IniFile::Section* emu_state = ini.GetOrCreateSection("EmuState");
		emu_state->Get("EmulationStateId", &m_emu_state);
		emu_state->Get("EmulationIssues", &m_issues);
	}

	if (!m_pImage.empty())
	{
		wxImage Image(m_ImageWidth, m_ImageHeight, &m_pImage[0], true);
		double Scale = wxTheApp->GetTopWindow()->GetContentScaleFactor();
		// Note: This uses nearest neighbor, which subjectively looks a lot
		// better for GC banners than smooths caling.
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
	p.Do(m_volume_names);
	p.Do(m_company);
	p.Do(m_banner_names);
	p.Do(m_descriptions);
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
	if (m_company.empty())
		return "N/A";
	else
		return m_company;
}

// (-1 = Japanese, 0 = English, etc)?
std::string GameListItem::GetDescription(int _index) const
{
	const u32 index = _index;

	if (index < m_descriptions.size())
		return m_descriptions[index];

	if (!m_descriptions.empty())
		return m_descriptions[0];

	return "";
}

// (-1 = Japanese, 0 = English, etc)?
std::string GameListItem::GetVolumeName(int _index) const
{
	u32 const index = _index;

	if (index < m_volume_names.size() && !m_volume_names[index].empty())
		return m_volume_names[index];

	if (!m_volume_names.empty())
		return m_volume_names[0];

	return "";
}

// (-1 = Japanese, 0 = English, etc)?
std::string GameListItem::GetBannerName(int _index) const
{
	u32 const index = _index;

	if (index < m_banner_names.size() && !m_banner_names[index].empty())
		return m_banner_names[index];

	if (!m_banner_names.empty())
		return m_banner_names[0];

	return "";
}

// (-1 = Japanese, 0 = English, etc)?
std::string GameListItem::GetName(int _index) const
{
	// Prefer name from banner, fallback to name from volume, fallback to filename

	std::string name = GetBannerName(_index);

	if (name.empty())
		name = GetVolumeName(_index);

	if (name.empty())
	{
		// No usable name, return filename (better than nothing)
		SplitPath(GetFileName(), nullptr, &name, nullptr);
	}

	return name;
}

const std::string GameListItem::GetWiiFSPath() const
{
	DiscIO::IVolume *iso = DiscIO::CreateVolumeFromFilename(m_FileName);
	std::string ret;

	if (iso == nullptr)
		return ret;

	if (DiscIO::IsVolumeWiiDisc(iso) || DiscIO::IsVolumeWadFile(iso))
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

