// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <QApplication>
#include <QFile>

#include "Common/CommonPaths.h"
#include "Common/FileUtil.h"
#include "Core/ConfigManager.h"

#include "DolphinQt/Utils/Resources.h"

QVector<QPixmap> Resources::m_platforms;
QVector<QPixmap> Resources::m_regions;
QVector<QPixmap> Resources::m_ratings;
QVector<QPixmap> Resources::m_pixmaps;

void Resources::Init()
{
	QString dir = QString::fromStdString(File::GetSysDirectory() + RESOURCES_DIR + DIR_SEP);

	m_regions.resize(DiscIO::IVolume::NUMBER_OF_COUNTRIES);
	m_regions[DiscIO::IVolume::COUNTRY_JAPAN].load(GetImageFilename("Flag_Japan", dir));
	m_regions[DiscIO::IVolume::COUNTRY_EUROPE].load(GetImageFilename("Flag_Europe", dir));
	m_regions[DiscIO::IVolume::COUNTRY_USA].load(GetImageFilename("Flag_USA", dir));

	m_regions[DiscIO::IVolume::COUNTRY_AUSTRALIA].load(GetImageFilename("Flag_Australia", dir));
	m_regions[DiscIO::IVolume::COUNTRY_FRANCE].load(GetImageFilename("Flag_France", dir));
	m_regions[DiscIO::IVolume::COUNTRY_GERMANY].load(GetImageFilename("Flag_Germany", dir));
	m_regions[DiscIO::IVolume::COUNTRY_ITALY].load(GetImageFilename("Flag_Italy", dir));
	m_regions[DiscIO::IVolume::COUNTRY_KOREA].load(GetImageFilename("Flag_Korea", dir));
	m_regions[DiscIO::IVolume::COUNTRY_NETHERLANDS].load(GetImageFilename("Flag_Netherlands", dir));
	m_regions[DiscIO::IVolume::COUNTRY_RUSSIA].load(GetImageFilename("Flag_Russia", dir));
	m_regions[DiscIO::IVolume::COUNTRY_SPAIN].load(GetImageFilename("Flag_Spain", dir));
	m_regions[DiscIO::IVolume::COUNTRY_TAIWAN].load(GetImageFilename("Flag_Taiwan", dir));
	m_regions[DiscIO::IVolume::COUNTRY_WORLD].load(GetImageFilename("Flag_International", dir));
	m_regions[DiscIO::IVolume::COUNTRY_UNKNOWN].load(GetImageFilename("Flag_Unknown", dir));

	m_platforms.resize(4);
	m_platforms[0].load(GetImageFilename("Platform_Gamecube", dir));
	m_platforms[1].load(GetImageFilename("Platform_Wii", dir));
	m_platforms[2].load(GetImageFilename("Platform_Wad", dir));
	m_platforms[3].load(GetImageFilename("Platform_File", dir));

	m_ratings.resize(6);
	m_ratings[0].load(GetImageFilename("rating0", dir));
	m_ratings[1].load(GetImageFilename("rating1", dir));
	m_ratings[2].load(GetImageFilename("rating2", dir));
	m_ratings[3].load(GetImageFilename("rating3", dir));
	m_ratings[4].load(GetImageFilename("rating4", dir));
	m_ratings[5].load(GetImageFilename("rating5", dir));

	m_pixmaps.resize(NUM_ICONS);
	m_pixmaps[DOLPHIN_LOGO].load(GetImageFilename("Dolphin", dir));
	m_pixmaps[DOLPHIN_LOGO_LARGE].load(GetImageFilename("dolphin_logo", dir));
	m_pixmaps[BANNER_MISSING].load(GetImageFilename("nobanner", dir));
	UpdatePixmaps();
}

void Resources::UpdatePixmaps()
{
	QString dir = QString::fromStdString(File::GetThemeDir(SConfig::GetInstance().theme_name));
	m_pixmaps[TOOLBAR_OPEN].load(GetImageFilename("open", dir));
	m_pixmaps[TOOLBAR_REFRESH].load(GetImageFilename("refresh", dir));
	m_pixmaps[TOOLBAR_BROWSE].load(GetImageFilename("browse", dir));
	m_pixmaps[TOOLBAR_PLAY].load(GetImageFilename("play", dir));
	m_pixmaps[TOOLBAR_STOP].load(GetImageFilename("stop", dir));
	m_pixmaps[TOOLBAR_PAUSE].load(GetImageFilename("pause", dir));
	m_pixmaps[TOOLBAR_FULLSCREEN].load(GetImageFilename("fullscreen", dir));
	m_pixmaps[TOOLBAR_SCREENSHOT].load(GetImageFilename("screenshot", dir));
	m_pixmaps[TOOLBAR_CONFIGURE].load(GetImageFilename("config", dir));
	m_pixmaps[TOOLBAR_GRAPHICS].load(GetImageFilename("graphics", dir));
	m_pixmaps[TOOLBAR_CONTROLLERS].load(GetImageFilename("classic", dir));
	m_pixmaps[TOOLBAR_HELP].load(GetImageFilename("config", dir)); // TODO
	// TODO: toolbar[MEMCARD];
	// TODO: toolbar[HOTKEYS];
}

QString Resources::GetImageFilename(const char* name, QString dir)
{
	QString fileName = QString::fromUtf8(name);
	if (qApp->devicePixelRatio() >= 2)
	{
		fileName.prepend(dir).append(QStringLiteral("@2x.png"));
		if (QFile::exists(fileName))
			return fileName;
	}
	return fileName.prepend(dir).append(QStringLiteral(".png"));
}

QPixmap& Resources::GetRegionPixmap(DiscIO::IVolume::ECountry region)
{
	return m_regions[region];
}

QPixmap& Resources::GetPlatformPixmap(int console)
{
	if (console >= m_platforms.size() || console < 0)
		return m_platforms[0];
	return m_platforms[console];
}

QPixmap& Resources::GetRatingPixmap(int rating)
{
	if (rating >= m_ratings.size() || rating < 0)
		return m_ratings[0];
	return m_ratings[rating];
}

QPixmap& Resources::GetPixmap(int id)
{
	if (id >= m_pixmaps.size() || id < 0)
		return m_pixmaps[0];
	return m_pixmaps[id];
}

QIcon Resources::GetIcon(int id)
{
	return QIcon(GetPixmap(id));
}
