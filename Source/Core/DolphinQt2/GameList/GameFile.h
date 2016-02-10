// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <QDateTime>
#include <QMap>
#include <QPixmap>
#include <QString>

#include "DiscIO/Volume.h"

// TODO cache
class GameFile final
{
public:
	explicit GameFile(const QString& path);

	bool IsValid()            const { return m_valid; }

	// These will be properly initialized before we try to load the file.
	QString GetPath()         const { return m_path; }
	QString GetFileName()     const { return m_file_name; }
	QString GetExtension()    const { return m_extension; }
	QString GetFolder()       const { return m_folder; }
	qint64  GetFileSize()     const { return m_size; }

	// The rest will not.
	QString GetUniqueID()     const { return m_unique_id; }
	QString GetMakerID()      const { return m_maker_id; }
	u16 GetRevision()         const { return m_revision; }
	QString GetInternalName() const { return m_internal_name; }
	QString GetCompany()      const { return m_company; }
	u8 GetDiscNumber()        const { return m_disc_number; }
	u64 GetRawSize()          const { return m_raw_size; }
	QPixmap GetBanner()       const { return m_banner; }
	QString GetIssues()       const { return m_issues; }
	int GetRating()           const { return m_rating; }

	DiscIO::IVolume::EPlatform GetPlatform() const { return m_platform; }
	DiscIO::IVolume::ECountry GetCountry()   const { return m_country; }
	DiscIO::BlobType GetBlobType()           const { return m_blob_type; }

	QString GetShortName() const { return GetLanguageString(m_short_names); }
	QString GetShortName(DiscIO::IVolume::ELanguage lang) const
	{
		return m_short_names[lang];
	}

	QString GetLongName() const { return GetLanguageString(m_long_names); }
	QString GetLongName(DiscIO::IVolume::ELanguage lang) const
	{
		return m_long_names[lang];
	}

	QString GetDescription() const { return GetLanguageString(m_descriptions); }
	QString GetDescription(DiscIO::IVolume::ELanguage lang) const
	{
		return m_descriptions[lang];
	}

private:
	DiscIO::IVolume::ELanguage GetDefaultLanguage() const;
	QString GetLanguageString(const QMap<DiscIO::IVolume::ELanguage, QString>& m) const;

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

	QString m_unique_id;
	QString m_maker_id;
	u16 m_revision = 0;
	QString m_internal_name;
	QMap<DiscIO::IVolume::ELanguage, QString> m_short_names;
	QMap<DiscIO::IVolume::ELanguage, QString> m_long_names;
	QMap<DiscIO::IVolume::ELanguage, QString> m_descriptions;
	QString m_company;
	u8 m_disc_number = 0;
	DiscIO::IVolume::EPlatform m_platform;
	DiscIO::IVolume::ECountry m_country;
	DiscIO::BlobType m_blob_type;
	u64 m_raw_size = 0;
	QPixmap m_banner;
	QString m_issues;
	int m_rating = 0;
};
