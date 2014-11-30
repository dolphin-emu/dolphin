// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include <memory>

#include <QDataStream>
#include <QDir>
#include <QFile>
#include <QFileInfo>

#include "Common/Common.h"
#include "Common/CommonPaths.h"
#include "Common/FileUtil.h"
#include "Common/Hash.h"
#include "Common/IniFile.h"
#include "Common/StringUtil.h"

#include "Core/ConfigManager.h"

#include "DiscIO/BannerLoader.h"
#include "DiscIO/CompressedBlob.h"
#include "DiscIO/Filesystem.h"

#include "DolphinQt/GameList/GameFile.h"
#include "DolphinQt/Utils/Resources.h"
#include "DolphinQt/Utils/Utils.h"

static const u32 CACHE_REVISION = 0x003;
static const u32 DATASTREAM_REVISION = 15; // Introduced in Qt 5.2

static QStringList VectorToStringList(std::vector<std::string> vec, bool trim = false)
{
	QStringList result;
	if (trim)
	{
		for (const std::string& member : vec)
			result.append(QString::fromStdString(member).trimmed());
	}
	else
	{
		for (const std::string& member : vec)
			result.append(QString::fromStdString(member));
	}
	return result;
}

GameFile::GameFile(const QString& fileName)
    : m_file_name(fileName)
{
	bool hasBanner = false;

	if (LoadFromCache())
	{
		m_valid = true;
		hasBanner = true;
	}
	else
	{
		DiscIO::IVolume* volume = DiscIO::CreateVolumeFromFilename(fileName.toStdString());

		if (volume != nullptr)
		{
			if (!DiscIO::IsVolumeWadFile(volume))
				m_platform = DiscIO::IsVolumeWiiDisc(volume) ? WII_DISC : GAMECUBE_DISC;
			else
				m_platform = WII_WAD;

			m_volume_names = VectorToStringList(volume->GetNames());

			m_country  = volume->GetCountry();
			m_file_size = volume->GetRawSize();
			m_volume_size = volume->GetSize();

			m_unique_id = QString::fromStdString(volume->GetUniqueID());
			m_compressed = DiscIO::IsCompressedBlob(fileName.toStdString());
			m_is_disc_two = volume->IsDiscTwo();
			m_revision = volume->GetRevision();

			QFileInfo info(m_file_name);
			m_folder_name = info.absoluteDir().dirName();

			// check if we can get some info from the banner file too
			DiscIO::IFileSystem* fileSystem = DiscIO::CreateFileSystem(volume);

			if (fileSystem != nullptr || m_platform == WII_WAD)
			{
				std::unique_ptr<DiscIO::IBannerLoader> bannerLoader(DiscIO::CreateBannerLoader(*fileSystem, volume));

				if (bannerLoader != nullptr)
				{
					if (bannerLoader->IsValid())
					{
						if (m_platform != WII_WAD)
							m_names = VectorToStringList(bannerLoader->GetNames());
						m_company = QString::fromStdString(bannerLoader->GetCompany());
						m_descriptions = VectorToStringList(bannerLoader->GetDescriptions(), true);

						int width, height;
						std::vector<u32> buffer = bannerLoader->GetBanner(&width, &height);
						QImage banner(width, height, QImage::Format_RGB888);
						for (int i = 0; i < width * height; i++)
						{
							int x = i % width, y = i / width;
							banner.setPixel(x, y, qRgb((buffer[i] & 0xFF0000) >> 16,
						                    (buffer[i] & 0x00FF00) >>  8,
						                    (buffer[i] & 0x0000FF) >>  0));
						}

						if (!banner.isNull())
						{
							hasBanner = true;
							m_banner = QPixmap::fromImage(banner);
						}
					}
				}
				delete fileSystem;
			}
			delete volume;

			m_valid = true;
			if (hasBanner)
				SaveToCache();
		}
	}

	if (m_valid)
	{
		IniFile ini;
		ini.Load(File::GetSysDirectory() + GAMESETTINGS_DIR DIR_SEP + m_unique_id.toStdString() + ".ini");
		ini.Load(File::GetUserPath(D_GAMESETTINGS_IDX) + m_unique_id.toStdString() + ".ini", true);

		std::string issues_temp;
		ini.GetIfExists("EmuState", "EmulationStateId", &m_emu_state);
		ini.GetIfExists("EmuState", "EmulationIssues", &issues_temp);
		m_issues = QString::fromStdString(issues_temp);
	}

	if (!hasBanner)
		m_banner = Resources::GetPixmap(Resources::BANNER_MISSING);
}

bool GameFile::LoadFromCache()
{
	QString filename = CreateCacheFilename();
	if (filename.isEmpty())
		return false;

	QFile file(filename);
	if (!file.exists())
		return false;
	if (!file.open(QFile::ReadOnly))
		return false;

	// If you modify the code below, you MUST bump the CACHE_REVISION!
	QDataStream stream(&file);
	stream.setVersion(DATASTREAM_REVISION);

	u32 cache_rev;
	stream >> cache_rev;
	if (cache_rev != CACHE_REVISION)
		return false;

	int country;
	stream >> m_folder_name
	       >> m_volume_names
	       >> m_company
	       >> m_descriptions
	       >> m_unique_id
	       >> m_file_size
	       >> m_volume_size
	       >> country
	       >> m_banner
	       >> m_compressed
	       >> m_platform
	       >> m_is_disc_two
	       >> m_revision;
	m_country = (DiscIO::IVolume::ECountry)country;
	file.close();
	return true;
}

void GameFile::SaveToCache()
{
	if (!File::IsDirectory(File::GetUserPath(D_CACHE_IDX)))
		File::CreateDir(File::GetUserPath(D_CACHE_IDX));

	QString filename = CreateCacheFilename();
	if (filename.isEmpty())
		return;
	if (QFile::exists(filename))
		QFile::remove(filename);

	QFile file(filename);
	if (!file.open(QFile::WriteOnly))
		return;

	// If you modify the code below, you MUST bump the CACHE_REVISION!
	QDataStream stream(&file);
	stream.setVersion(DATASTREAM_REVISION);
	stream << CACHE_REVISION;

	stream << m_folder_name
	       << m_volume_names
	       << m_company
	       << m_descriptions
	       << m_unique_id
	       << m_file_size
	       << m_volume_size
	       << (int)m_country
	       << m_banner
	       << m_compressed
	       << m_platform
	       << m_is_disc_two
	       << m_revision;
}

QString GameFile::CreateCacheFilename()
{
	std::string filename, pathname, extension;
	SplitPath(m_file_name.toStdString(), &pathname, &filename, &extension);

	if (filename.empty())
		return SL(""); // must be a disc drive

	// Filename.extension_HashOfFolderPath_Size.cache
	// Append hash to prevent ISO name-clashing in different folders.
	filename.append(StringFromFormat("%s_%x_%lx.qcache",
		extension.c_str(), HashFletcher((const u8*)pathname.c_str(), pathname.size()),
		File::GetSize(m_file_name.toStdString())));

	QString fullname = QString::fromStdString(File::GetUserPath(D_CACHE_IDX));
	fullname += QString::fromStdString(filename);
	return fullname;
}

QString GameFile::GetCompany() const
{
	if (m_company.isEmpty())
		return QObject::tr("N/A");
	else
		return m_company;
}

// For all of the following functions that accept an "index" parameter,
// (-1 = Japanese, 0 = English, etc)?

QString GameFile::GetDescription(int index) const
{
	if (index < m_descriptions.size())
		return m_descriptions[index];

	if (!m_descriptions.empty())
		return m_descriptions[0];

	return SL("");
}

QString GameFile::GetVolumeName(int index) const
{
	if (index < m_volume_names.size() && !m_volume_names[index].isEmpty())
		return m_volume_names[index];

	if (!m_volume_names.isEmpty())
		return m_volume_names[0];

	return SL("");
}

QString GameFile::GetBannerName(int index) const
{
	if (index < m_names.size() && !m_names[index].isEmpty())
		return m_names[index];

	if (!m_names.isEmpty())
		return m_names[0];

	return SL("");
}

QString GameFile::GetName(int index) const
{
	// Prefer name from banner, fallback to name from volume, fallback to filename
	QString name = GetBannerName(index);

	if (name.isEmpty())
		name = GetVolumeName(index);

	if (name.isEmpty())
	{
		// No usable name, return filename (better than nothing)
		std::string nametemp;
		SplitPath(m_file_name.toStdString(), nullptr, &nametemp, nullptr);
		name = QString::fromStdString(nametemp);
	}

	return name;
}

const QString GameFile::GetWiiFSPath() const
{
	DiscIO::IVolume* volume = DiscIO::CreateVolumeFromFilename(m_file_name.toStdString());
	QString ret;

	if (volume == nullptr)
		return ret;

	if (DiscIO::IsVolumeWiiDisc(volume) || DiscIO::IsVolumeWadFile(volume))
	{
		std::string path;
		u64 title;

		volume->GetTitleID((u8*)&title);
		title = Common::swap64(title);

		path = StringFromFormat("%stitle/%08x/%08x/data/", File::GetUserPath(D_WIIUSER_IDX).c_str(), (u32)(title >> 32), (u32)title);

		if (!File::Exists(path))
			File::CreateFullPath(path);

		if (path[0] == '.')
			ret = QDir::currentPath() + QString::fromStdString(path).mid((int)strlen(ROOT_DIR));
		else
			ret = QString::fromStdString(path);
	}
	delete volume;

	return ret;
}
