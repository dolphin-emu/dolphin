// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <algorithm>
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

#include "DiscIO/CompressedBlob.h"
#include "DiscIO/Filesystem.h"

#include "DolphinQt/GameList/GameFile.h"
#include "DolphinQt/Utils/Utils.h"

static const u32 CACHE_REVISION = 0x00C; // Last changed in PR 2993
static const u32 DATASTREAM_REVISION = 15; // Introduced in Qt 5.2

static QMap<DiscIO::IVolume::ELanguage, QString> ConvertLocalizedStrings(std::map<DiscIO::IVolume::ELanguage, std::string> strings)
{
	QMap<DiscIO::IVolume::ELanguage, QString> result;

	for (auto entry : strings)
		result.insert(entry.first, QString::fromStdString(entry.second).trimmed());

	return result;
}

template<class to, class from>
static QMap<to, QString> CastLocalizedStrings(QMap<from, QString> strings)
{
	QMap<to, QString> result;

	auto end = strings.cend();
	for (auto it = strings.cbegin(); it != end; ++it)
		result.insert((to)it.key(), it.value());

	return result;
}

static QString GetLanguageString(DiscIO::IVolume::ELanguage language, QMap<DiscIO::IVolume::ELanguage, QString> strings)
{
	if (strings.contains(language))
		return strings.value(language);

	// English tends to be a good fallback when the requested language isn't available
	if (language != DiscIO::IVolume::ELanguage::LANGUAGE_ENGLISH)
	{
		if (strings.contains(DiscIO::IVolume::ELanguage::LANGUAGE_ENGLISH))
			return strings.value(DiscIO::IVolume::ELanguage::LANGUAGE_ENGLISH);
	}

	// If English isn't available either, just pick something
	if (!strings.empty())
		return strings.cbegin().value();

	return SL("");
}

GameFile::GameFile(const QString& fileName)
    : m_file_name(fileName)
{
	QFileInfo info(m_file_name);
	m_folder_name = info.absoluteDir().dirName();

	if (LoadFromCache())
	{
		m_valid = true;

		// Wii banners can only be read if there is a savefile,
		// so sometimes caches don't contain banners. Let's check
		// if a banner has become available after the cache was made.
		if (m_banner.isNull())
		{
			std::unique_ptr<DiscIO::IVolume> volume(DiscIO::CreateVolumeFromFilename(fileName.toStdString()));
			if (volume != nullptr)
			{
				ReadBanner(*volume);
				if (!m_banner.isNull())
					SaveToCache();
			}
		}
	}
	else
	{
		std::unique_ptr<DiscIO::IVolume> volume(DiscIO::CreateVolumeFromFilename(fileName.toStdString()));

		if (volume != nullptr)
		{
			m_platform = volume->GetVolumeType();

			m_short_names = ConvertLocalizedStrings(volume->GetNames(false));
			m_long_names = ConvertLocalizedStrings(volume->GetNames(true));
			m_descriptions = ConvertLocalizedStrings(volume->GetDescriptions());
			m_company = QString::fromStdString(volume->GetCompany());

			m_country = volume->GetCountry();
			m_file_size = volume->GetRawSize();
			m_volume_size = volume->GetSize();

			m_unique_id = QString::fromStdString(volume->GetUniqueID());
			m_compressed = DiscIO::IsCompressedBlob(fileName.toStdString());
			m_disc_number = volume->GetDiscNumber();
			m_revision = volume->GetRevision();

			ReadBanner(*volume);

			m_valid = true;
			SaveToCache();
		}
	}

	if (m_company.isEmpty() && m_unique_id.size() >= 6)
		m_company = QString::fromStdString(DiscIO::GetCompanyFromID(m_unique_id.mid(4, 2).toStdString()));

	if (m_valid)
	{
		IniFile ini = SConfig::LoadGameIni(m_unique_id.toStdString(), m_revision);
		std::string issues_temp;
		ini.GetIfExists("EmuState", "EmulationStateId", &m_emu_state);
		ini.GetIfExists("EmuState", "EmulationIssues", &issues_temp);
		m_issues = QString::fromStdString(issues_temp);
	}

	if (!IsValid() && IsElfOrDol())
	{
		m_valid = true;
		m_file_size = info.size();
		m_platform = DiscIO::IVolume::ELF_DOL;
	}
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

	// Increment CACHE_REVISION if the code below is modified (GameFile.cpp)
	QDataStream stream(&file);
	stream.setVersion(DATASTREAM_REVISION);

	u32 cache_rev;
	stream >> cache_rev;
	if (cache_rev != CACHE_REVISION)
		return false;

	u32 country;
	u32 platform;
	QMap<u8, QString> short_names;
	QMap<u8, QString> long_names;
	QMap<u8, QString> descriptions;
	stream >> short_names
	       >> long_names
	       >> descriptions
	       >> m_company
	       >> m_unique_id
	       >> m_file_size
	       >> m_volume_size
	       >> country
	       >> m_banner
	       >> m_compressed
	       >> platform
	       >> m_disc_number
	       >> m_revision;
	m_country = (DiscIO::IVolume::ECountry)country;
	m_platform = (DiscIO::IVolume::EPlatform)platform;
	m_short_names = CastLocalizedStrings<DiscIO::IVolume::ELanguage>(short_names);
	m_long_names = CastLocalizedStrings<DiscIO::IVolume::ELanguage>(long_names);
	m_descriptions = CastLocalizedStrings<DiscIO::IVolume::ELanguage>(descriptions);
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

	// Increment CACHE_REVISION if the code below is modified (GameFile.cpp)
	QDataStream stream(&file);
	stream.setVersion(DATASTREAM_REVISION);
	stream << CACHE_REVISION;

	stream << CastLocalizedStrings<u8>(m_short_names)
	       << CastLocalizedStrings<u8>(m_long_names)
	       << CastLocalizedStrings<u8>(m_descriptions)
	       << m_company
	       << m_unique_id
	       << m_file_size
	       << m_volume_size
	       << (u32)m_country
	       << m_banner
	       << m_compressed
	       << (u32)m_platform
	       << m_disc_number
	       << m_revision;
}

bool GameFile::IsElfOrDol() const
{
	const std::string name = m_file_name.toStdString();
	const size_t pos = name.rfind('.');

	if (pos != std::string::npos)
	{
		std::string ext = name.substr(pos);
		std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

		return ext == ".elf" || ext == ".dol";
	}
	return false;
}

QString GameFile::CreateCacheFilename() const
{
	std::string filename, pathname, extension;
	SplitPath(m_file_name.toStdString(), &pathname, &filename, &extension);

	if (filename.empty())
		return SL(""); // must be a disc drive

	// Filename.extension_HashOfFolderPath_Size.cache
	// Append hash to prevent ISO name-clashing in different folders.
	filename.append(StringFromFormat("%s_%x_%llx.qcache",
		extension.c_str(), HashFletcher((const u8*)pathname.c_str(), pathname.size()),
		(unsigned long long)File::GetSize(m_file_name.toStdString())));

	QString fullname = QString::fromStdString(File::GetUserPath(D_CACHE_IDX));
	fullname += QString::fromStdString(filename);
	return fullname;
}

void GameFile::ReadBanner(const DiscIO::IVolume& volume)
{
	int width, height;
	std::vector<u32> buffer = volume.GetBanner(&width, &height);
	QImage banner(width, height, QImage::Format_RGB888);
	for (int i = 0; i < width * height; i++)
	{
		int x = i % width, y = i / width;
		banner.setPixel(x, y, qRgb((buffer[i] & 0xFF0000) >> 16,
		                           (buffer[i] & 0x00FF00) >> 8,
		                           (buffer[i] & 0x0000FF) >> 0));
	}

	if (!banner.isNull())
		m_banner = QPixmap::fromImage(banner);
}

QString GameFile::GetDescription(DiscIO::IVolume::ELanguage language) const
{
	return GetLanguageString(language, m_descriptions);
}

QString GameFile::GetDescription() const
{
	bool wii = m_platform != DiscIO::IVolume::GAMECUBE_DISC;
	return GetDescription(SConfig::GetInstance().GetCurrentLanguage(wii));
}

QString GameFile::GetName(bool prefer_long, DiscIO::IVolume::ELanguage language) const
{
	return GetLanguageString(language, prefer_long ? m_long_names : m_short_names);
}

QString GameFile::GetName(bool prefer_long) const
{
	bool wii = m_platform != DiscIO::IVolume::GAMECUBE_DISC;
	QString name = GetName(prefer_long, SConfig::GetInstance().GetCurrentLanguage(wii));
	if (name.isEmpty())
	{
		// No usable name, return filename (better than nothing)
		std::string name_temp, extension;
		SplitPath(m_file_name.toStdString(), nullptr, &name_temp, &extension);
		name = QString::fromStdString(name_temp + extension);
	}
	return name;
}

const QString GameFile::GetWiiFSPath() const
{
	std::unique_ptr<DiscIO::IVolume> volume(DiscIO::CreateVolumeFromFilename(m_file_name.toStdString()));
	QString ret;

	if (volume == nullptr)
		return ret;

	if (volume->GetVolumeType() != DiscIO::IVolume::GAMECUBE_DISC)
	{
		std::string path;
		u64 title;

		volume->GetTitleID((u8*)&title);
		title = Common::swap64(title);

		path = StringFromFormat("%s/title/%08x/%08x/data/", File::GetUserPath(D_WIIROOT_IDX).c_str(), (u32)(title >> 32), (u32)title);

		if (!File::Exists(path))
			File::CreateFullPath(path);

		if (path[0] == '.')
			ret = QDir::currentPath() + QString::fromStdString(path).mid((int)strlen(ROOT_DIR));
		else
			ret = QString::fromStdString(path);
	}

	return ret;
}
