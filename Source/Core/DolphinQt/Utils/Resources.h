// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include <QIcon>
#include <QPixmap>
#include <QVector>

#include "DiscIO/Volume.h"

class Resources
{
public:
	static void Init();
	static void UpdatePixmaps();

	static QPixmap& GetPlatformPixmap(int console);
	static QPixmap& GetRegionPixmap(DiscIO::IVolume::ECountry region);
	static QPixmap& GetRatingPixmap(int rating);
	static QPixmap& GetPixmap(int id);
	static QIcon GetIcon(int id);

	enum
	{
		TOOLBAR_OPEN = 0,
		TOOLBAR_REFRESH,
		TOOLBAR_BROWSE,
		TOOLBAR_PLAY,
		TOOLBAR_STOP,
		TOOLBAR_PAUSE,
		TOOLBAR_FULLSCREEN,
		TOOLBAR_SCREENSHOT,
		TOOLBAR_CONFIGURE,
		TOOLBAR_PLUGIN_GFX,
		TOOLBAR_PLUGIN_DSP,
		TOOLBAR_PLUGIN_GCPAD,
		TOOLBAR_PLUGIN_WIIMOTE,
		TOOLBAR_HELP,
		MEMCARD,
		HOTKEYS,
		DOLPHIN_LOGO,
		BANNER_MISSING,
		NUM_ICONS
	};

private:
	static QVector<QPixmap> m_platforms;
	static QVector<QPixmap> m_regions;
	static QVector<QPixmap> m_ratings;
	static QVector<QPixmap> m_pixmaps;
};
