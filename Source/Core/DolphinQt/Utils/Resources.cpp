// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2
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

void Resources::Init()
{
	QString dir = QString::fromStdString(File::GetSysDirectory() + "Resources/");

	m_regions.resize(DiscIO::IVolume::NUMBER_OF_COUNTRIES);
	m_regions[DiscIO::IVolume::COUNTRY_EUROPE].load(dir + SL("Flag_Europe.png"));
	m_regions[DiscIO::IVolume::COUNTRY_FRANCE].load(dir + SL("Flag_France.png"));
	m_regions[DiscIO::IVolume::COUNTRY_RUSSIA].load(dir + SL("Flag_Unknown.png")); // TODO
	m_regions[DiscIO::IVolume::COUNTRY_USA].load(dir + SL("Flag_USA.png"));
	m_regions[DiscIO::IVolume::COUNTRY_JAPAN].load(dir + SL("Flag_Japan.png"));
	m_regions[DiscIO::IVolume::COUNTRY_KOREA].load(dir + SL("Flag_Korea.png"));
	m_regions[DiscIO::IVolume::COUNTRY_ITALY].load(dir + SL("Flag_Italy.png"));
	m_regions[DiscIO::IVolume::COUNTRY_TAIWAN].load(dir + SL("Flag_Taiwan.png"));
	m_regions[DiscIO::IVolume::COUNTRY_SDK].load(dir + SL("Flag_SDK.png"));
	m_regions[DiscIO::IVolume::COUNTRY_UNKNOWN].load(dir + SL("Flag_Unknown.png"));

	m_platforms.resize(3);
	m_platforms[0].load(dir + SL("Platform_Gamecube.png"));
	m_platforms[1].load(dir + SL("Platform_Wii.png"));
	m_platforms[2].load(dir + SL("Platform_Wad.png"));

	m_ratings.resize(6);
	m_ratings[0].load(dir + SL("rating0.png"));
	m_ratings[1].load(dir + SL("rating1.png"));
	m_ratings[2].load(dir + SL("rating2.png"));
	m_ratings[3].load(dir + SL("rating3.png"));
	m_ratings[4].load(dir + SL("rating4.png"));
	m_ratings[5].load(dir + SL("rating5.png"));

	m_pixmaps.resize(NUM_ICONS);
	m_pixmaps[DOLPHIN_LOGO].load(dir + SL("Dolphin.png"));
	UpdatePixmaps();
}

// Wrapper for GetImageFilename() so you don't have to to call it directly
#define GIFN(file) GetImageFilename(SL(file), dir)

void Resources::UpdatePixmaps()
{
	QString dir = QString::fromStdString(File::GetThemeDir(SConfig::GetInstance().m_LocalCoreStartupParameter.theme_name));
	m_pixmaps[TOOLBAR_OPEN].load(GIFN("open"));
	m_pixmaps[TOOLBAR_REFRESH].load(GIFN("refresh"));
	m_pixmaps[TOOLBAR_BROWSE].load(GIFN("browse"));
	m_pixmaps[TOOLBAR_PLAY].load(GIFN("play"));
	m_pixmaps[TOOLBAR_STOP].load(GIFN("stop"));
	m_pixmaps[TOOLBAR_PAUSE].load(GIFN("pause"));
	m_pixmaps[TOOLBAR_FULLSCREEN].load(GIFN("fullscreen"));
	m_pixmaps[TOOLBAR_SCREENSHOT].load(GIFN("screenshot"));
	m_pixmaps[TOOLBAR_CONFIGURE].load(GIFN("config"));
	m_pixmaps[TOOLBAR_PLUGIN_GFX].load(GIFN("graphics"));
	m_pixmaps[TOOLBAR_PLUGIN_DSP].load(GIFN("dsp"));
	m_pixmaps[TOOLBAR_PLUGIN_GCPAD].load(GIFN("gcpad"));
	m_pixmaps[TOOLBAR_PLUGIN_WIIMOTE].load(GIFN("wiimote"));
	m_pixmaps[TOOLBAR_HELP].load(GIFN("nobanner")); // TODO
	// TODO: toolbar[MEMCARD];
	// TODO: toolbar[HOTKEYS];
	m_pixmaps[BANNER_MISSING].load(GIFN("nobanner"));
}

QString Resources::GetImageFilename(QString name, QString dir)
{
	if (qApp->devicePixelRatio() >= 2)
	{
		QString fileName = name.prepend(dir).append(SL("@2x.png"));
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
