// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <QCryptographicHash>
#include <QDataStream>
#include <QDir>
#include <QImage>
#include <QSharedPointer>

#include "Common/FileUtil.h"
#include "Core/ConfigManager.h"
#include "DiscIO/Enums.h"
#include "DiscIO/Volume.h"
#include "DiscIO/VolumeCreator.h"
#include "DolphinQt2/GameList/GameFile.h"
#include "DolphinQt2/Resources.h"
#include "DolphinQt2/Settings.h"

static const int CACHE_VERSION = 13;  // Last changed in PR #3261
static const int DATASTREAM_VERSION = QDataStream::Qt_5_5;

QList<DiscIO::ELanguage> GameFile::GetAvailableLanguages() const
{
  return m_long_names.keys();
}

static QMap<DiscIO::ELanguage, QString>
ConvertLanguageMap(const std::map<DiscIO::ELanguage, std::string>& map)
{
  QMap<DiscIO::ELanguage, QString> result;
  for (auto entry : map)
    result.insert(entry.first, QString::fromStdString(entry.second).trimmed());
  return result;
}

GameFile::GameFile(const QString& path) : m_path(path)
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

QString GameFile::GetCacheFileName() const
{
  QString folder = QString::fromStdString(File::GetUserPath(D_CACHE_IDX));
  // Append a hash of the full path to prevent name clashes between
  // files with the same names in different folders.
  QString hash =
      QString::fromUtf8(QCryptographicHash::hash(m_path.toUtf8(), QCryptographicHash::Md5).toHex());
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
    banner.setPixel(x, y, qRgb((buffer[i] & 0xFF0000) >> 16, (buffer[i] & 0x00FF00) >> 8,
                               (buffer[i] & 0x0000FF) >> 0));
  }

  if (!banner.isNull())
    m_banner = QPixmap::fromImage(banner);
  else
    m_banner = Resources::GetMisc(Resources::BANNER_MISSING);
}

bool GameFile::LoadFileInfo(const QString& path)
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
  return m_extension == QStringLiteral("elf") || m_extension == QStringLiteral("dol");
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
  QSharedPointer<DiscIO::IVolume> volume(
      DiscIO::CreateVolumeFromFilename(m_path.toStdString()).release());
  if (volume == nullptr)
    return false;

  m_unique_id = QString::fromStdString(volume->GetUniqueID());
  std::string maker_id = volume->GetMakerID();
  m_maker = QString::fromStdString(DiscIO::GetCompanyFromID(maker_id));
  m_maker_id = QString::fromStdString(maker_id);
  m_revision = volume->GetRevision();
  m_internal_name = QString::fromStdString(volume->GetInternalName());
  m_short_names = ConvertLanguageMap(volume->GetShortNames());
  m_long_names = ConvertLanguageMap(volume->GetLongNames());
  m_short_makers = ConvertLanguageMap(volume->GetShortMakers());
  m_long_makers = ConvertLanguageMap(volume->GetLongMakers());
  m_descriptions = ConvertLanguageMap(volume->GetDescriptions());
  m_disc_number = volume->GetDiscNumber();
  m_platform = volume->GetVolumeType();
  m_country = volume->GetCountry();
  m_blob_type = volume->GetBlobType();
  m_raw_size = volume->GetRawSize();
  m_apploader_date = QString::fromStdString(volume->GetApploaderDate());

  ReadBanner(*volume);

  SaveCache();
  return true;
}

bool GameFile::TryLoadElfDol()
{
  if (!IsElfOrDol())
    return false;

  m_revision = 0;
  m_long_names[DiscIO::ELanguage::LANGUAGE_ENGLISH] = m_file_name;
  m_platform = DiscIO::EPlatform::ELF_DOL;
  m_country = DiscIO::ECountry::COUNTRY_UNKNOWN;
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

QString GameFile::GetBannerString(const QMap<DiscIO::ELanguage, QString>& m) const
{
  // Try the settings language, then English, then just pick one.
  if (m.isEmpty())
    return QString();

  bool wii = m_platform != DiscIO::EPlatform::GAMECUBE_DISC;
  DiscIO::ELanguage current_lang;
  if (wii)
    current_lang = Settings().GetWiiSystemLanguage();
  else
    current_lang = Settings().GetGCSystemLanguage();

  if (m.contains(current_lang))
    return m[current_lang];
  if (m.contains(DiscIO::ELanguage::LANGUAGE_ENGLISH))
    return m[DiscIO::ELanguage::LANGUAGE_ENGLISH];
  return m.first();
}

QString GameFile::GetPlatform() const
{
  switch (m_platform)
  {
  case DiscIO::EPlatform::GAMECUBE_DISC:
    return QObject::tr("GameCube");
  case DiscIO::EPlatform::WII_DISC:
    return QObject::tr("Wii");
  case DiscIO::EPlatform::WII_WAD:
    return QObject::tr("Wii Channel");
  case DiscIO::EPlatform::ELF_DOL:
    return QObject::tr("ELF/DOL");
  default:
    return QObject::tr("Unknown");
  }
}

QString GameFile::GetCountry() const
{
  switch (m_country)
  {
  case DiscIO::ECountry::COUNTRY_EUROPE:
    return QObject::tr("Europe");
  case DiscIO::ECountry::COUNTRY_JAPAN:
    return QObject::tr("Japan");
  case DiscIO::ECountry::COUNTRY_USA:
    return QObject::tr("USA");
  case DiscIO::ECountry::COUNTRY_AUSTRALIA:
    return QObject::tr("Australia");
  case DiscIO::ECountry::COUNTRY_FRANCE:
    return QObject::tr("France");
  case DiscIO::ECountry::COUNTRY_GERMANY:
    return QObject::tr("Germany");
  case DiscIO::ECountry::COUNTRY_ITALY:
    return QObject::tr("Italy");
  case DiscIO::ECountry::COUNTRY_KOREA:
    return QObject::tr("Korea");
  case DiscIO::ECountry::COUNTRY_NETHERLANDS:
    return QObject::tr("Netherlands");
  case DiscIO::ECountry::COUNTRY_RUSSIA:
    return QObject::tr("Russia");
  case DiscIO::ECountry::COUNTRY_SPAIN:
    return QObject::tr("Spain");
  case DiscIO::ECountry::COUNTRY_TAIWAN:
    return QObject::tr("Taiwan");
  case DiscIO::ECountry::COUNTRY_WORLD:
    return QObject::tr("World");
  default:
    return QObject::tr("Unknown");
  }
}

QString GameFile::GetLanguage(DiscIO::ELanguage lang) const
{
  switch (lang)
  {
  case DiscIO::ELanguage::LANGUAGE_JAPANESE:
    return QObject::tr("Japanese");
  case DiscIO::ELanguage::LANGUAGE_ENGLISH:
    return QObject::tr("English");
  case DiscIO::ELanguage::LANGUAGE_GERMAN:
    return QObject::tr("German");
  case DiscIO::ELanguage::LANGUAGE_FRENCH:
    return QObject::tr("French");
  case DiscIO::ELanguage::LANGUAGE_SPANISH:
    return QObject::tr("Spanish");
  case DiscIO::ELanguage::LANGUAGE_ITALIAN:
    return QObject::tr("Italian");
  case DiscIO::ELanguage::LANGUAGE_DUTCH:
    return QObject::tr("Dutch");
  case DiscIO::ELanguage::LANGUAGE_SIMPLIFIED_CHINESE:
    return QObject::tr("Simplified Chinese");
  case DiscIO::ELanguage::LANGUAGE_TRADITIONAL_CHINESE:
    return QObject::tr("Traditional Chinese");
  case DiscIO::ELanguage::LANGUAGE_KOREAN:
    return QObject::tr("Korean");
  default:
    return QObject::tr("Unknown");
  }
}

// Convert an integer size to a friendly string representation.
QString FormatSize(qint64 size)
{
  QStringList units{QStringLiteral("KB"), QStringLiteral("MB"), QStringLiteral("GB"),
                    QStringLiteral("TB")};
  QStringListIterator i(units);
  QString unit = QStringLiteral("B");
  double num = (double)size;
  while (num > 1024.0 && i.hasNext())
  {
    unit = i.next();
    num /= 1024.0;
  }
  return QStringLiteral("%1 %2").arg(QString::number(num, 'f', 1)).arg(unit);
}
