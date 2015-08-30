// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <QMap>
#include <QPixmap>
#include <QString>

#include <string>

#include "DiscIO/Volume.h"
#include "DiscIO/VolumeCreator.h"

class GameFile final
{
public:
	GameFile(const QString& fileName);
	GameFile(const std::string& fileName) : GameFile(QString::fromStdString(fileName)) {}

	bool IsValid() const { return m_valid; }
	QString GetFileName() { return m_file_name; }
	QString GetFolderName() { return m_folder_name; }
	QString GetName(DiscIO::IVolume::ELanguage language) const;
	QString GetName() const;
	QString GetDescription(DiscIO::IVolume::ELanguage language) const;
	QString GetDescription() const;
	QString GetCompany() const;
	int GetRevision() const { return m_revision; }
	const QString GetUniqueID() const { return m_unique_id; }
	const QString GetWiiFSPath() const;
	DiscIO::IVolume::ECountry GetCountry() const { return m_country; }
	int GetPlatform() const { return m_platform; }
	const QString GetIssues() const { return m_issues; }
	int GetEmuState() const { return m_emu_state; }
	bool IsCompressed() const { return m_compressed; }
	u64 GetFileSize() const { return m_file_size; }
	u64 GetVolumeSize() const { return m_volume_size; }
	bool IsDiscTwo() const { return m_is_disc_two; }
	const QPixmap GetBitmap() const { return m_banner; }

	enum
	{
		GAMECUBE_DISC = 0,
		WII_DISC,
		WII_WAD,
		NUMBER_OF_PLATFORMS
	};

private:
	QString m_file_name;
	QString m_folder_name;

	QMap<DiscIO::IVolume::ELanguage, QString> m_names;
	QMap<DiscIO::IVolume::ELanguage, QString> m_descriptions;
	QString m_company;

	QString m_unique_id;

	QString m_issues;
	int m_emu_state = 0;

	quint64 m_file_size = 0;
	quint64 m_volume_size = 0;

	DiscIO::IVolume::ECountry m_country;
	int m_platform;
	int m_revision = 0;

	QPixmap m_banner;
	bool m_valid = false;
	bool m_compressed = false;
	bool m_is_disc_two = false;

	bool LoadFromCache();
	void SaveToCache();

	QString CreateCacheFilename();
};
