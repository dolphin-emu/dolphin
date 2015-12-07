// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <QStringList>

#include "Common/CommonPaths.h"
#include "Common/FileUtil.h"
#include "Core/ConfigManager.h"
#include "DolphinQt2/Resources.h"

QList<QPixmap> Resources::m_platforms;
QList<QPixmap> Resources::m_countries;
QList<QPixmap> Resources::m_ratings;
QList<QPixmap> Resources::m_misc;

void Resources::Init()
{
	QString sys_dir = QString::fromStdString(File::GetSysDirectory() + RESOURCES_DIR + DIR_SEP);

	QStringList platforms{
		QStringLiteral("Platform_Gamecube.png"),
		QStringLiteral("Platform_Wii.png"),
		QStringLiteral("Platform_Wad.png"),
		QStringLiteral("Platform_File.png")
	};
	for (QString platform : platforms)
		m_platforms.append(QPixmap(platform.prepend(sys_dir)));

	QStringList countries{
		QStringLiteral("Flag_Europe.png"),
		QStringLiteral("Flag_Japan.png"),
		QStringLiteral("Flag_USA.png"),
		QStringLiteral("Flag_Australia.png"),
		QStringLiteral("Flag_France.png"),
		QStringLiteral("Flag_Germany.png"),
		QStringLiteral("Flag_Italy.png"),
		QStringLiteral("Flag_Korea.png"),
		QStringLiteral("Flag_Netherlands.png"),
		QStringLiteral("Flag_Russia.png"),
		QStringLiteral("Flag_Spain.png"),
		QStringLiteral("Flag_Taiwan.png"),
		QStringLiteral("Flag_International.png"),
		QStringLiteral("Flag_Unknown.png")
	};
	for (QString country : countries)
		m_countries.append(QPixmap(country.prepend(sys_dir)));

	QStringList ratings{
		QStringLiteral("rating0.png"),
		QStringLiteral("rating1.png"),
		QStringLiteral("rating2.png"),
		QStringLiteral("rating3.png"),
		QStringLiteral("rating4.png"),
		QStringLiteral("rating5.png")
	};
	for (QString rating : ratings)
		m_ratings.append(QPixmap(rating.prepend(sys_dir)));

	m_misc.append(QPixmap(QStringLiteral("nobanner.png").prepend(sys_dir)));
	m_misc.append(QPixmap(QStringLiteral("dolphin_logo.png").prepend(sys_dir)));
	m_misc.append(QPixmap(QStringLiteral("Dolphin.png").prepend(sys_dir)));
}

QPixmap Resources::GetPlatform(int platform)
{
	return m_platforms[platform];
}

QPixmap Resources::GetCountry(int country)
{
	return m_countries[country];
}

QPixmap Resources::GetRating(int rating)
{
	return m_ratings[rating];
}

QPixmap Resources::GetMisc(int id)
{
	return m_misc[id];
}
