// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <QDateTime>
#include <QMap>
#include <QPixmap>
#include <QString>

#include "DiscIO/Volume.h"

// TODO replace this with a struct and some static functions.
// TODO cache
// TODO elf/dol and Homebrew XML files
class GameFile final
{
public:
	explicit GameFile(QString path);

	bool IsValid()            const { return m_valid; }

	// These will be properly initialized before we try to load the file.
	QString GetPath()         const { return m_path; }
	QString GetFileName()     const { return m_file_name; }
	QString GetFolder()       const { return m_folder; }

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

	DiscIO::IVolume::ELanguage GetLanguage() const;
	QString GetShortName() const { return GetShortName(GetLanguage()); }
	QString GetShortName(DiscIO::IVolume::ELanguage lang) const
	{
		return m_short_names[lang];
	}

	QString GetLongName() const { return GetLongName(GetLanguage()); }
	QString GetLongName(DiscIO::IVolume::ELanguage lang) const
	{
		return m_long_names[lang];
	}

	QString GetDescription() const { return GetDescription(GetLanguage()); }
	QString GetDescription(DiscIO::IVolume::ELanguage lang) const
	{
		return m_descriptions[lang];
	}

private:
	QString GetCacheFileName() const;
	void ReadBanner(const DiscIO::IVolume& volume);
	bool TryLoadCache();
	bool TryLoadFile();
	void SaveCache();

	bool m_valid;
	QString m_path;
	QString m_file_name;
	QString m_folder;
	QDateTime m_last_modified;

	QString m_unique_id;
	QString m_maker_id;
	u16 m_revision;
	QString m_internal_name;
	QMap<DiscIO::IVolume::ELanguage, QString> m_short_names;
	QMap<DiscIO::IVolume::ELanguage, QString> m_long_names;
	QMap<DiscIO::IVolume::ELanguage, QString> m_descriptions;
	QString m_company;
	u8 m_disc_number;
	DiscIO::IVolume::EPlatform m_platform;
	DiscIO::IVolume::ECountry m_country;
	DiscIO::BlobType m_blob_type;
	u64 m_raw_size;
	QPixmap m_banner;
	QString m_issues;
	int m_rating;
};
