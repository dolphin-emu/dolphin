// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <QList>
#include <QPixmap>

// Store for various QPixmaps that will be used repeatedly.
class Resources final
{
public:
	static void Init();

	static QPixmap GetPlatform(int platform);
	static QPixmap GetCountry(int country);
	static QPixmap GetRating(int rating);

	static QPixmap GetMisc(int id);

	enum
	{
		BANNER_MISSING,
		LOGO_LARGE,
		LOGO_SMALL
	};

private:
	Resources() {}

	static QList<QPixmap> m_platforms;
	static QList<QPixmap> m_countries;
	static QList<QPixmap> m_ratings;
	static QList<QPixmap> m_misc;
};
