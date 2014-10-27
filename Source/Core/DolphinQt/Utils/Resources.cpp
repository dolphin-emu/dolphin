// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

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

void Resources::UpdatePixmaps()
{
	QString dir = QString::fromStdString(File::GetThemeDir(SConfig::GetInstance().m_LocalCoreStartupParameter.theme_name));
	m_pixmaps[TOOLBAR_OPEN].load(dir + SL("open.png"));
	m_pixmaps[TOOLBAR_REFRESH].load(dir + SL("refresh.png"));
	m_pixmaps[TOOLBAR_BROWSE].load(dir + SL("browse.png"));
	m_pixmaps[TOOLBAR_PLAY].load(dir + SL("play.png"));
	m_pixmaps[TOOLBAR_STOP].load(dir + SL("stop.png"));
	m_pixmaps[TOOLBAR_PAUSE].load(dir + SL("pause.png"));
	m_pixmaps[TOOLBAR_FULLSCREEN].load(dir + SL("fullscreen.png"));
	m_pixmaps[TOOLBAR_SCREENSHOT].load(dir + SL("screenshot.png"));
	m_pixmaps[TOOLBAR_CONFIGURE].load(dir + SL("config.png"));
	m_pixmaps[TOOLBAR_PLUGIN_GFX].load(dir + SL("graphics.png"));
	m_pixmaps[TOOLBAR_PLUGIN_DSP].load(dir + SL("dsp.png"));
	m_pixmaps[TOOLBAR_PLUGIN_GCPAD].load(dir + SL("gcpad.png"));
	m_pixmaps[TOOLBAR_PLUGIN_WIIMOTE].load(dir + SL("wiimote.png"));
	m_pixmaps[TOOLBAR_HELP].load(dir + SL("nobanner.png")); // TODO
	// TODO: toolbar[MEMCARD];
	// TODO: toolbar[HOTKEYS];
	m_pixmaps[BANNER_MISSING].load(dir + SL("nobanner.png"));
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
