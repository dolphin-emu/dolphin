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
#include "DolphinQt/Resources.h"
#include "DolphinQt/GameList/GameFile.h"

static const int CACHE_VERSION = 1;
static const int DATASTREAM_VERSION = QDataStream::Qt_5_5;

static QMap<DiscIO::IVolume::ELanguage, QString> ConvertLanguageMap(
		const std::map<DiscIO::IVolume::ELanguage, std::string>& map)
{
	QMap<DiscIO::IVolume::ELanguage, QString> result;
	for (auto entry : map)
		result.insert(entry.first, QString::fromStdString(entry.second).trimmed());
	return result;
}

GameFile::GameFile(QString path)
{
	m_valid = false;

	QFileInfo info(path);
	if (!info.exists() || !info.isReadable())
		return;

	m_path = info.canonicalFilePath();
	m_file_name = info.fileName();
	m_folder = info.dir().dirName();
	m_last_modified = info.lastModified();

	if (!TryLoadCache())
		if (!TryLoadFile())
			return;

	if (m_banner.isNull())
		m_banner = Resources::GetMisc(Resources::BANNER_MISSING);

	IniFile ini = SConfig::LoadGameIni(m_unique_id.toStdString(), m_revision);
	std::string issues_temp;
	ini.GetIfExists("EmuState", "EmulationStateId", &m_rating);
	ini.GetIfExists("EmuState", "EmulationIssues", &issues_temp);
	m_issues = QString::fromStdString(issues_temp);

	m_valid = true;
}

DiscIO::IVolume::ELanguage GameFile::GetLanguage() const
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

	// TODO
	return false;
}

bool GameFile::TryLoadFile()
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

void GameFile::SaveCache()
{
	// TODO
}

