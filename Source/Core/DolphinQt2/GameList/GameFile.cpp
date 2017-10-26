// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <QDir>
#include <QFileInfo>
#include <QImage>
#include <QSharedPointer>

#include "Common/Assert.h"
#include "Common/FileUtil.h"
#include "Common/NandPaths.h"
#include "Common/StringUtil.h"
#include "Core/ConfigManager.h"
#include "Core/HW/WiiSaveCrypted.h"
#include "Core/IOS/ES/ES.h"
#include "Core/IOS/IOS.h"
#include "Core/WiiUtils.h"
#include "DiscIO/Blob.h"
#include "DiscIO/Enums.h"
#include "DiscIO/Volume.h"
#include "DolphinQt2/GameList/GameFile.h"
#include "DolphinQt2/Resources.h"
#include "DolphinQt2/Settings.h"

QList<DiscIO::Language> GameFile::GetAvailableLanguages() const
{
  return m_long_names.keys();
}

static QMap<DiscIO::Language, QString>
ConvertLanguageMap(const std::map<DiscIO::Language, std::string>& map)
{
  QMap<DiscIO::Language, QString> result;
  for (auto entry : map)
    result.insert(entry.first, QString::fromStdString(entry.second).trimmed());
  return result;
}

GameFile::GameFile()
{
  m_valid = false;
}

GameFile::GameFile(const QString& path) : m_path(path)
{
  m_valid = false;

  if (!LoadFileInfo(path))
    return;

  if (TryLoadVolume())
  {
    LoadState();
  }
  else if (!TryLoadElfDol())
  {
    return;
  }

  m_valid = true;
}

bool GameFile::IsValid() const
{
  if (!m_valid)
    return false;

  if (m_platform == DiscIO::Platform::WII_WAD && !IOS::ES::IsChannel(m_title_id))
    return false;

  return true;
}

void GameFile::ReadBanner(const DiscIO::Volume& volume)
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
}

bool GameFile::LoadFileInfo(const QString& path)
{
  QFileInfo info(path);
  if (!info.exists() || !info.isReadable())
    return false;

  m_last_modified = info.lastModified();
  m_size = info.size();

  return true;
}

void GameFile::LoadState()
{
  IniFile ini = SConfig::LoadGameIni(m_game_id.toStdString(), m_revision);
  std::string issues_temp;
  ini.GetIfExists("EmuState", "EmulationStateId", &m_rating);
  ini.GetIfExists("EmuState", "EmulationIssues", &issues_temp);
  m_issues = QString::fromStdString(issues_temp);
}

bool GameFile::IsElfOrDol()
{
  QString extension = GetFileExtension();
  return extension == QStringLiteral("elf") || extension == QStringLiteral("dol");
}

bool GameFile::TryLoadVolume()
{
  QSharedPointer<DiscIO::Volume> volume(
      DiscIO::CreateVolumeFromFilename(m_path.toStdString()).release());
  if (volume == nullptr)
    return false;

  m_game_id = QString::fromStdString(volume->GetGameID());
  std::string maker_id = volume->GetMakerID();
  m_title_id = volume->GetTitleID().value_or(0);
  m_maker = QString::fromStdString(DiscIO::GetCompanyFromID(maker_id));
  m_maker_id = QString::fromStdString(maker_id);
  m_revision = volume->GetRevision().value_or(0);
  m_internal_name = QString::fromStdString(volume->GetInternalName());
  m_short_names = ConvertLanguageMap(volume->GetShortNames());
  m_long_names = ConvertLanguageMap(volume->GetLongNames());
  m_short_makers = ConvertLanguageMap(volume->GetShortMakers());
  m_long_makers = ConvertLanguageMap(volume->GetLongMakers());
  m_descriptions = ConvertLanguageMap(volume->GetDescriptions());
  m_disc_number = volume->GetDiscNumber().value_or(0);
  m_platform = volume->GetVolumeType();
  m_region = volume->GetRegion();
  m_country = volume->GetCountry();
  m_blob_type = volume->GetBlobType();
  m_raw_size = volume->GetRawSize();
  m_apploader_date = QString::fromStdString(volume->GetApploaderDate());

  ReadBanner(*volume);

  return true;
}

bool GameFile::TryLoadElfDol()
{
  if (!IsElfOrDol())
    return false;

  m_revision = 0;
  m_platform = DiscIO::Platform::ELF_DOL;
  m_region = DiscIO::Region::UNKNOWN_REGION;
  m_country = DiscIO::Country::COUNTRY_UNKNOWN;
  m_blob_type = DiscIO::BlobType::DIRECTORY;
  m_raw_size = m_size;
  m_rating = 0;

  return true;
}

QString GameFile::GetFileName() const
{
  return QFileInfo(m_path).fileName();
}

QString GameFile::GetFileExtension() const
{
  return QFileInfo(m_path).suffix();
}

QString GameFile::GetFileFolder() const
{
  return QFileInfo(m_path).dir().dirName();
}

QString GameFile::GetBannerString(const QMap<DiscIO::Language, QString>& m) const
{
  // Try the settings language, then English, then just pick one.
  if (m.isEmpty())
    return QString();

  bool wii = m_platform != DiscIO::Platform::GAMECUBE_DISC;
  DiscIO::Language current_lang = SConfig::GetInstance().GetCurrentLanguage(wii);

  if (m.contains(current_lang))
    return m[current_lang];
  if (m.contains(DiscIO::Language::LANGUAGE_ENGLISH))
    return m[DiscIO::Language::LANGUAGE_ENGLISH];
  return m.first();
}

QString GameFile::GetPlatform() const
{
  switch (m_platform)
  {
  case DiscIO::Platform::GAMECUBE_DISC:
    return QObject::tr("GameCube");
  case DiscIO::Platform::WII_DISC:
    return QObject::tr("Wii");
  case DiscIO::Platform::WII_WAD:
    return QObject::tr("Wii Channel");
  case DiscIO::Platform::ELF_DOL:
    return QObject::tr("ELF/DOL");
  default:
    return QObject::tr("Unknown");
  }
}

QString GameFile::GetCountry() const
{
  switch (m_country)
  {
  case DiscIO::Country::COUNTRY_EUROPE:
    return QObject::tr("Europe");
  case DiscIO::Country::COUNTRY_JAPAN:
    return QObject::tr("Japan");
  case DiscIO::Country::COUNTRY_USA:
    return QObject::tr("USA");
  case DiscIO::Country::COUNTRY_AUSTRALIA:
    return QObject::tr("Australia");
  case DiscIO::Country::COUNTRY_FRANCE:
    return QObject::tr("France");
  case DiscIO::Country::COUNTRY_GERMANY:
    return QObject::tr("Germany");
  case DiscIO::Country::COUNTRY_ITALY:
    return QObject::tr("Italy");
  case DiscIO::Country::COUNTRY_KOREA:
    return QObject::tr("Korea");
  case DiscIO::Country::COUNTRY_NETHERLANDS:
    return QObject::tr("Netherlands");
  case DiscIO::Country::COUNTRY_RUSSIA:
    return QObject::tr("Russia");
  case DiscIO::Country::COUNTRY_SPAIN:
    return QObject::tr("Spain");
  case DiscIO::Country::COUNTRY_TAIWAN:
    return QObject::tr("Taiwan");
  case DiscIO::Country::COUNTRY_WORLD:
    return QObject::tr("World");
  default:
    return QObject::tr("Unknown");
  }
}

QString GameFile::GetLanguage(DiscIO::Language lang) const
{
  switch (lang)
  {
  case DiscIO::Language::LANGUAGE_JAPANESE:
    return QObject::tr("Japanese");
  case DiscIO::Language::LANGUAGE_ENGLISH:
    return QObject::tr("English");
  case DiscIO::Language::LANGUAGE_GERMAN:
    return QObject::tr("German");
  case DiscIO::Language::LANGUAGE_FRENCH:
    return QObject::tr("French");
  case DiscIO::Language::LANGUAGE_SPANISH:
    return QObject::tr("Spanish");
  case DiscIO::Language::LANGUAGE_ITALIAN:
    return QObject::tr("Italian");
  case DiscIO::Language::LANGUAGE_DUTCH:
    return QObject::tr("Dutch");
  case DiscIO::Language::LANGUAGE_SIMPLIFIED_CHINESE:
    return QObject::tr("Simplified Chinese");
  case DiscIO::Language::LANGUAGE_TRADITIONAL_CHINESE:
    return QObject::tr("Traditional Chinese");
  case DiscIO::Language::LANGUAGE_KOREAN:
    return QObject::tr("Korean");
  default:
    return QObject::tr("Unknown");
  }
}

QString GameFile::GetUniqueID() const
{
  std::vector<std::string> info;
  if (!GetGameID().isEmpty())
    info.push_back(GetGameID().toStdString());

  if (GetRevision() != 0)
  {
    info.push_back("Revision " + std::to_string(GetRevision()));
  }

  std::string name = m_long_names[DiscIO::Language::LANGUAGE_ENGLISH].toStdString();

  if (name.empty())
  {
    if (!m_long_names.isEmpty())
      name = m_long_names.begin().value().toStdString();
    else
    {
      std::string filename, extension;
      name = SplitPath(m_path.toStdString(), nullptr, &filename, &extension);
      name = filename + extension;
    }
  }

  int disc_number = GetDiscNumber() + 1;

  std::string lower_name = name;
  std::transform(lower_name.begin(), lower_name.end(), lower_name.begin(), ::tolower);
  if (disc_number > 1 &&
      lower_name.find(std::string("disc ") + std::to_string(disc_number)) == std::string::npos &&
      lower_name.find(std::string("disc") + std::to_string(disc_number)) == std::string::npos)
  {
    info.push_back("Disc " + std::to_string(disc_number));
  }

  if (info.empty())
    return QString::fromStdString(name);

  std::ostringstream ss;
  std::copy(info.begin(), info.end() - 1, std::ostream_iterator<std::string>(ss, ", "));
  ss << info.back();
  return QString::fromStdString(name + " (" + ss.str() + ")");
}

bool GameFile::IsInstalled() const
{
  _assert_(m_platform == DiscIO::Platform::WII_WAD);

  const std::string content_dir =
      Common::GetTitleContentPath(m_title_id, Common::FromWhichRoot::FROM_CONFIGURED_ROOT);

  if (!File::IsDirectory(content_dir))
    return false;

  // Since this isn't IOS and we only need a simple way to figure out if a title is installed,
  // we make the (reasonable) assumption that having more than just the TMD in the content
  // directory means that the title is installed.
  const auto entries = File::ScanDirectoryTree(content_dir, false);
  return std::any_of(entries.children.begin(), entries.children.end(),
                     [](const auto& file) { return file.virtualName != "title.tmd"; });
}

bool GameFile::Install()
{
  _assert_(m_platform == DiscIO::Platform::WII_WAD);

  bool installed = WiiUtils::InstallWAD(m_path.toStdString());

  if (installed)
    Settings::Instance().NANDRefresh();

  return installed;
}

bool GameFile::Uninstall()
{
  _assert_(m_platform == DiscIO::Platform::WII_WAD);
  IOS::HLE::Kernel ios;
  return ios.GetES()->DeleteTitleContent(m_title_id) == IOS::HLE::IPC_SUCCESS;
}

bool GameFile::ExportWiiSave()
{
  return CWiiSaveCrypted::ExportWiiSave(m_title_id);
}

QString GameFile::GetWiiFSPath() const
{
  _assert_(m_platform == DiscIO::Platform::WII_DISC || m_platform == DiscIO::Platform::WII_WAD);

  const std::string path = Common::GetTitleDataPath(m_title_id, Common::FROM_CONFIGURED_ROOT);

  return QString::fromStdString(path);
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

template <typename T, typename U = std::enable_if_t<std::is_enum<T>::value>>
QDataStream& operator<<(QDataStream& out, const T& enum_value)
{
  out << static_cast<std::underlying_type_t<T>>(enum_value);
  return out;
}

template <typename T, typename U = std::enable_if_t<std::is_enum<T>::value>>
QDataStream& operator>>(QDataStream& in, T& enum_value)
{
  std::underlying_type_t<T> tmp;
  in >> tmp;
  enum_value = static_cast<T>(tmp);
  return in;
}

// Some C++ implementations define uint64_t as an 'unsigned long', but QDataStream only has built-in
// overloads for quint64, which is an 'unsigned long long' on Unix
QDataStream& operator<<(QDataStream& out, const unsigned long& integer)
{
  out << static_cast<quint64>(integer);
  return out;
}
QDataStream& operator>>(QDataStream& in, unsigned long& integer)
{
  quint64 tmp;
  in >> tmp;
  integer = static_cast<unsigned long>(tmp);
  return in;
}

QDataStream& operator<<(QDataStream& out, const GameFile& file)
{
  out << file.m_last_modified;
  out << file.m_path;
  out << file.m_title_id;
  out << file.m_game_id;
  out << file.m_maker_id;
  out << file.m_maker;
  out << file.m_long_makers;
  out << file.m_short_makers;
  out << file.m_internal_name;
  out << file.m_long_names;
  out << file.m_short_names;
  out << file.m_platform;
  out << file.m_region;
  out << file.m_country;
  out << file.m_blob_type;
  out << file.m_size;
  out << file.m_raw_size;
  out << file.m_descriptions;
  out << file.m_revision;
  out << file.m_disc_number;
  out << file.m_issues;
  out << file.m_rating;
  out << file.m_apploader_date;
  out << file.m_banner;

  return out;
}

QDataStream& operator>>(QDataStream& in, GameFile& file)
{
  in >> file.m_last_modified;
  in >> file.m_path;
  in >> file.m_title_id;
  in >> file.m_game_id;
  in >> file.m_maker_id;
  in >> file.m_maker;
  in >> file.m_long_makers;
  in >> file.m_short_makers;
  in >> file.m_internal_name;
  in >> file.m_long_names;
  in >> file.m_short_names;
  in >> file.m_platform;
  in >> file.m_region;
  in >> file.m_country;
  in >> file.m_blob_type;
  in >> file.m_size;
  in >> file.m_raw_size;
  in >> file.m_descriptions;
  in >> file.m_revision;
  in >> file.m_disc_number;
  in >> file.m_issues;
  in >> file.m_rating;
  in >> file.m_apploader_date;
  in >> file.m_banner;

  file.m_valid = true;

  return in;
}
