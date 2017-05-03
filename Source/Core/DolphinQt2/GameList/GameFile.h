// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <QDateTime>
#include <QMap>
#include <QPixmap>
#include <QString>

#include "Common/CommonTypes.h"

namespace DiscIO
{
enum class BlobType;
enum class Country;
enum class Language;
enum class Region;
enum class Platform;
class IVolume;
}

// TODO cache
class GameFile final
{
public:
  explicit GameFile(const QString& path);

  bool IsValid() const { return m_valid; }
  // These will be properly initialized before we try to load the file.
  QString GetFilePath() const { return m_path; }
  QString GetFileName() const { return m_file_name; }
  QString GetFileExtension() const { return m_extension; }
  QString GetFileFolder() const { return m_folder; }
  qint64 GetFileSize() const { return m_size; }
  // The rest will not.
  QString GetGameID() const { return m_game_id; }
  QString GetMakerID() const { return m_maker_id; }
  QString GetMaker() const { return m_maker; }
  u16 GetRevision() const { return m_revision; }
  QString GetInternalName() const { return m_internal_name; }
  u8 GetDiscNumber() const { return m_disc_number; }
  u64 GetRawSize() const { return m_raw_size; }
  QPixmap GetBanner() const { return m_banner; }
  QString GetIssues() const { return m_issues; }
  int GetRating() const { return m_rating; }
  QString GetApploaderDate() const { return m_apploader_date; }
  DiscIO::Platform GetPlatformID() const { return m_platform; }
  QString GetPlatform() const;
  DiscIO::Region GetRegion() const { return m_region; }
  DiscIO::Country GetCountryID() const { return m_country; }
  QString GetCountry() const;
  DiscIO::BlobType GetBlobType() const { return m_blob_type; }
  // Banner details
  QString GetLanguage(DiscIO::Language lang) const;
  QList<DiscIO::Language> GetAvailableLanguages() const;
  QString GetShortName() const { return GetBannerString(m_short_names); }
  QString GetShortMaker() const { return GetBannerString(m_short_makers); }
  QString GetLongName() const { return GetBannerString(m_long_names); }
  QString GetLongMaker() const { return GetBannerString(m_long_makers); }
  QString GetDescription() const { return GetBannerString(m_descriptions); }
  QString GetShortName(DiscIO::Language lang) const { return m_short_names[lang]; }
  QString GetShortMaker(DiscIO::Language lang) const { return m_short_makers[lang]; }
  QString GetLongName(DiscIO::Language lang) const { return m_long_names[lang]; }
  QString GetLongMaker(DiscIO::Language lang) const { return m_long_makers[lang]; }
  QString GetDescription(DiscIO::Language lang) const { return m_descriptions[lang]; }
private:
  QString GetBannerString(const QMap<DiscIO::Language, QString>& m) const;

  QString GetCacheFileName() const;
  void ReadBanner(const DiscIO::IVolume& volume);
  bool LoadFileInfo(const QString& path);
  void LoadState();
  bool IsElfOrDol();
  bool TryLoadElfDol();
  bool TryLoadCache();
  bool TryLoadVolume();
  void SaveCache();

  bool m_valid;
  QString m_path;
  QString m_file_name;
  QString m_extension;
  QString m_folder;
  QDateTime m_last_modified;
  qint64 m_size = 0;

  QString m_game_id;
  QString m_maker;
  QString m_maker_id;
  u16 m_revision = 0;
  QString m_internal_name;
  QMap<DiscIO::Language, QString> m_short_names;
  QMap<DiscIO::Language, QString> m_long_names;
  QMap<DiscIO::Language, QString> m_short_makers;
  QMap<DiscIO::Language, QString> m_long_makers;
  QMap<DiscIO::Language, QString> m_descriptions;
  QString m_company;
  u8 m_disc_number = 0;
  DiscIO::Region m_region;
  DiscIO::Platform m_platform;
  DiscIO::Country m_country;
  DiscIO::BlobType m_blob_type;
  u64 m_raw_size = 0;
  QPixmap m_banner;
  QString m_issues;
  int m_rating = 0;
  QString m_apploader_date;
};

QString FormatSize(qint64 size);
