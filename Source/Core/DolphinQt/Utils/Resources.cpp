// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <QApplication>
#include <QFile>

#include "Common/FileUtil.h"
#include "Core/ConfigManager.h"

#include "DolphinQt/Utils/Resources.h"
#include "DolphinQt/Utils/Utils.h"

QVector<QPixmap> Resources::m_platforms;
QVector<QPixmap> Resources::m_regions;
QVector<QPixmap> Resources::m_ratings;
QVector<QPixmap> Resources::m_pixmaps;

// Wrapper for GetImageFilename() so you don't have to to call it directly
#define GIFN(file) GetImageFilename(SL(file), dir)

void Resources::Init()
{
	QString dir = QString::fromStdString(File::GetSysDirectory() + "Resources/");

	m_regions.resize(DiscIO::IVolume::NUMBER_OF_COUNTRIES);
	m_regions[DiscIO::IVolume::COUNTRY_JAPAN].load(GIFN("Flag_Japan"));
	m_regions[DiscIO::IVolume::COUNTRY_EUROPE].load(GIFN("Flag_Europe"));
	m_regions[DiscIO::IVolume::COUNTRY_USA].load(GIFN("Flag_USA"));

	m_regions[DiscIO::IVolume::COUNTRY_AUSTRALIA].load(GIFN("Flag_Australia"));
	m_regions[DiscIO::IVolume::COUNTRY_FRANCE].load(GIFN("Flag_France"));
	m_regions[DiscIO::IVolume::COUNTRY_GERMANY].load(GIFN("Flag_Germany"));
	m_regions[DiscIO::IVolume::COUNTRY_ITALY].load(GIFN("Flag_Italy"));
	m_regions[DiscIO::IVolume::COUNTRY_KOREA].load(GIFN("Flag_Korea"));
	m_regions[DiscIO::IVolume::COUNTRY_NETHERLANDS].load(GIFN("Flag_Netherlands"));
	m_regions[DiscIO::IVolume::COUNTRY_RUSSIA].load(GIFN("Flag_Russia"));
	m_regions[DiscIO::IVolume::COUNTRY_SPAIN].load(GIFN("Flag_Spain"));
	m_regions[DiscIO::IVolume::COUNTRY_TAIWAN].load(GIFN("Flag_Taiwan"));
	m_regions[DiscIO::IVolume::COUNTRY_WORLD].load(GIFN("Flag_Europe")); // Uses European flag as a placeholder
	m_regions[DiscIO::IVolume::COUNTRY_UNKNOWN].load(GIFN("Flag_Unknown"));

	m_platforms.resize(3);
	m_platforms[0].load(GIFN("Platform_Gamecube"));
	m_platforms[1].load(GIFN("Platform_Wii"));
	m_platforms[2].load(GIFN("Platform_Wad"));

	m_ratings.resize(6);
	m_ratings[0].load(GIFN("rating0"));
	m_ratings[1].load(GIFN("rating1"));
	m_ratings[2].load(GIFN("rating2"));
	m_ratings[3].load(GIFN("rating3"));
	m_ratings[4].load(GIFN("rating4"));
	m_ratings[5].load(GIFN("rating5"));

	m_pixmaps.resize(NUM_ICONS);
	m_pixmaps[DOLPHIN_LOGO].load(GIFN("Dolphin"));
	m_pixmaps[DOLPHIN_LOGO_LARGE].load(GIFN("dolphin_logo"));
	UpdatePixmaps();
}

void Resources::UpdatePixmaps()
{
	QString dir = QString::fromStdString(File::GetThemeDir(SConfig::GetInstance().theme_name));
	m_pixmaps[TOOLBAR_OPEN].load(GIFN("open"));
	m_pixmaps[TOOLBAR_REFRESH].load(GIFN("refresh"));
	m_pixmaps[TOOLBAR_BROWSE].load(GIFN("browse"));
	m_pixmaps[TOOLBAR_PLAY].load(GIFN("play"));
	m_pixmaps[TOOLBAR_STOP].load(GIFN("stop"));
	m_pixmaps[TOOLBAR_PAUSE].load(GIFN("pause"));
	m_pixmaps[TOOLBAR_FULLSCREEN].load(GIFN("fullscreen"));
	m_pixmaps[TOOLBAR_SCREENSHOT].load(GIFN("screenshot"));
	m_pixmaps[TOOLBAR_CONFIGURE].load(GIFN("config"));
	m_pixmaps[TOOLBAR_GRAPHICS].load(GIFN("graphics"));
	m_pixmaps[TOOLBAR_CONTROLLERS].load(GIFN("classic"));
	m_pixmaps[TOOLBAR_HELP].load(GIFN("nobanner")); // TODO
	// TODO: toolbar[MEMCARD];
	// TODO: toolbar[HOTKEYS];
	m_pixmaps[BANNER_MISSING].load(GIFN("nobanner"));
}

QString Resources::GetImageFilename(QString name, QString dir)
{
	if (qApp->devicePixelRatio() >= 2)
	{
		QString fileName = name;
		fileName.prepend(dir).append(SL("@2x.png"));
		if (QFile::exists(fileName))
			return fileName;
	}
	return name.prepend(dir).append(SL(".png"));
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
