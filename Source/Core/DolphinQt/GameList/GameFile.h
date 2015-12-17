// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <QMap>
#include <QPixmap>
#include <QString>

#include <string>

#include "DiscIO/Blob.h"
#include "DiscIO/Volume.h"

#include "DolphinQt/Utils/Resources.h"

class GameFile final
{
public:
	GameFile(const QString& fileName);
	GameFile(const std::string& fileName) : GameFile(QString::fromStdString(fileName)) {}

	bool IsValid() const { return m_valid; }
	QString GetFileName() { return m_file_name; }
	QString GetFolderName() { return m_folder_name; }
	QString GetName(bool prefer_long, DiscIO::IVolume::ELanguage language) const;
	QString GetName(bool prefer_long) const;
	QString GetDescription(DiscIO::IVolume::ELanguage language) const;
	QString GetDescription() const;
	QString GetCompany() const { return m_company; }
	u16 GetRevision() const { return m_revision; }
	const QString GetUniqueID() const { return m_unique_id; }
	const QString GetWiiFSPath() const;
	DiscIO::IVolume::ECountry GetCountry() const { return m_country; }
	DiscIO::IVolume::EPlatform GetPlatform() const { return m_platform; }
	DiscIO::BlobType GetBlobType() const { return m_blob_type; }
	const QString GetIssues() const { return m_issues; }
	int GetEmuState() const { return m_emu_state; }
	bool IsCompressed() const
	{
		return m_blob_type == DiscIO::BlobType::GCZ || m_blob_type == DiscIO::BlobType::CISO ||
		       m_blob_type == DiscIO::BlobType::WBFS;
	}
	u64 GetFileSize() const { return m_file_size; }
	u64 GetVolumeSize() const { return m_volume_size; }
	// 0 is the first disc, 1 is the second disc
	u8 GetDiscNumber() const { return m_disc_number; }
	const QPixmap GetBitmap() const
	{
		if (m_banner.isNull())
			return Resources::GetPixmap(Resources::BANNER_MISSING);

		return m_banner;
	}

private:
	QString m_file_name;
	QString m_folder_name;

	QMap<DiscIO::IVolume::ELanguage, QString> m_short_names;
	QMap<DiscIO::IVolume::ELanguage, QString> m_long_names;
	QMap<DiscIO::IVolume::ELanguage, QString> m_descriptions;
	QString m_company;

	QString m_unique_id;
	quint64 m_title_id;

	QString m_issues;
	int m_emu_state = 0;

	quint64 m_file_size = 0;
	quint64 m_volume_size = 0;

	DiscIO::IVolume::ECountry m_country = DiscIO::IVolume::COUNTRY_UNKNOWN;
	DiscIO::IVolume::EPlatform m_platform;
	DiscIO::BlobType m_blob_type;
	u16 m_revision = 0;

	QPixmap m_banner;
	bool m_valid = false;
	u8 m_disc_number = 0;

	bool LoadFromCache();
	void SaveToCache();

	bool IsElfOrDol() const;
	QString CreateCacheFilename() const;

	// Outputs to m_banner
	void ReadBanner(const std::vector<u32>& buffer, int width, int height);
	// Outputs to m_short_names, m_long_names, m_descriptions, m_company.
	// Returns whether a file was found, not whether it contained useful data.
	bool ReadXML(const QString& file_path);
};
