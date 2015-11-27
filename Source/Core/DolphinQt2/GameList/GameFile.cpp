// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <QCryptographicHash>
#include <QDataStream>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QSharedPointer>

#include "Common/FileUtil.h"
#include "Core/ConfigManager.h"
#include "DiscIO/VolumeCreator.h"
#include "DolphinQt2/Resources.h"
#include "DolphinQt2/GameList/GameFile.h"

static const int CACHE_VERSION = 13; // Last changed in PR #3261
static const int DATASTREAM_VERSION = QDataStream::Qt_5_5;

static QMap<DiscIO::IVolume::ELanguage, QString> ConvertLanguageMap(
		const std::map<DiscIO::IVolume::ELanguage, std::string>& map)
{
	QMap<DiscIO::IVolume::ELanguage, QString> result;
	for (auto entry : map)
		result.insert(entry.first, QString::fromStdString(entry.second).trimmed());
	return result;
}

GameFile::GameFile(QString path) : m_path(path)
{
	m_valid = false;

	if (!LoadFileInfo(path))
		return;

	if (!TryLoadCache())
	{
		if (TryLoadVolume())
		{
			LoadState();
		}
		else if (!TryLoadElfDol())
		{
			return;
		}
	}

	m_valid = true;
}

DiscIO::IVolume::ELanguage GameFile::GetDefaultLanguage() const
{
	bool wii = m_platform != DiscIO::IVolume::GAMECUBE_DISC;
	return SConfig::GetInstance().GetCurrentLanguage(wii);
}

QString GameFile::GetCacheFileName() const
{
	QString folder = QString::fromStdString(File::GetUserPath(D_CACHE_IDX));
	// Append a hash of the full path to prevent name clashes between
	// files with the same names in different folders.
	QString hash = QString::fromUtf8(
			QCryptographicHash::hash(m_path.toUtf8(),
			QCryptographicHash::Md5).toHex());
	return folder + m_file_name + hash;
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
	else
		m_banner = Resources::GetMisc(Resources::BANNER_MISSING);
}

bool GameFile::LoadFileInfo(QString path)
{
	QFileInfo info(path);
	if (!info.exists() || !info.isReadable())
		return false;

	m_file_name = info.fileName();
	m_extension = info.suffix();
	m_folder = info.dir().dirName();
	m_last_modified = info.lastModified();
	m_size = info.size();

	return true;
}

void GameFile::LoadState()
{
	IniFile ini = SConfig::LoadGameIni(m_unique_id.toStdString(), m_revision);
	std::string issues_temp;
	ini.GetIfExists("EmuState", "EmulationStateId", &m_rating);
	ini.GetIfExists("EmuState", "EmulationIssues", &issues_temp);
	m_issues = QString::fromStdString(issues_temp);
}

bool GameFile::IsElfOrDol()
{
	return m_extension == QStringLiteral("elf") ||
		   m_extension == QStringLiteral("dol");
}

bool GameFile::TryLoadCache()
{
	QFile cache(GetCacheFileName());
	if (!cache.exists())
		return false;
	if (!cache.open(QIODevice::ReadOnly))
		return false;
	if (QFileInfo(cache).lastModified() < m_last_modified)
		return false;

	QDataStream in(&cache);
	in.setVersion(DATASTREAM_VERSION);

	int cache_version;
	in >> cache_version;
	if (cache_version != CACHE_VERSION)
		return false;

	return false;
}

bool GameFile::TryLoadVolume()
{
	QSharedPointer<DiscIO::IVolume> volume(DiscIO::CreateVolumeFromFilename(m_path.toStdString()));
	if (volume == nullptr)
		return false;

	m_unique_id = QString::fromStdString(volume->GetUniqueID());
	m_maker_id = QString::fromStdString(volume->GetMakerID());
	m_revision = volume->GetRevision();
	m_internal_name = QString::fromStdString(volume->GetInternalName());
	m_short_names = ConvertLanguageMap(volume->GetNames(false));
	m_long_names = ConvertLanguageMap(volume->GetNames(true));
	m_descriptions = ConvertLanguageMap(volume->GetDescriptions());
	m_company = QString::fromStdString(volume->GetCompany());
	m_disc_number = volume->GetDiscNumber();
	m_platform = volume->GetVolumeType();
	m_country = volume->GetCountry();
	m_blob_type = volume->GetBlobType();
	m_raw_size = volume->GetRawSize();

	if (m_company.isEmpty() && m_unique_id.size() >= 6)
		m_company = QString::fromStdString(
				DiscIO::GetCompanyFromID(m_unique_id.mid(4, 2).toStdString()));

	ReadBanner(*volume);

	SaveCache();
	return true;
}

bool GameFile::TryLoadElfDol()
{
	if (!IsElfOrDol())
		return false;

	m_revision = 0;
	m_long_names[DiscIO::IVolume::LANGUAGE_ENGLISH] = m_file_name;
	m_platform = DiscIO::IVolume::ELF_DOL;
	m_country = DiscIO::IVolume::COUNTRY_UNKNOWN;
	m_blob_type = DiscIO::BlobType::DIRECTORY;
	m_raw_size = m_size;
	m_banner = Resources::GetMisc(Resources::BANNER_MISSING);
	m_rating = 0;

	return true;
}
void GameFile::SaveCache()
{
	// TODO
}

QString GameFile::GetLanguageString(QMap<DiscIO::IVolume::ELanguage, QString> m) const
{
	// Try the settings language, then English, then just pick one.
	if (m.isEmpty())
		return QString();
	DiscIO::IVolume::ELanguage current_lang = GetDefaultLanguage();
	if (m.contains(current_lang))
		return m[current_lang];
	if (m.contains(DiscIO::IVolume::LANGUAGE_ENGLISH))
		return m[DiscIO::IVolume::LANGUAGE_ENGLISH];
	return m.first();
}
