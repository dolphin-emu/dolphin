// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <QStringList>

#include "Common/FileUtil.h"
#include "Core/ConfigManager.h"
#include "DolphinQt/Resources.h"

QList<QPixmap> Resources::m_platforms;
QList<QPixmap> Resources::m_countries;
QList<QPixmap> Resources::m_ratings;
QList<QPixmap> Resources::m_misc;

void Resources::Init()
{
	QString sys_dir = QString::fromStdString(File::GetSysDirectory() + "Resources/");

	QStringList platforms;
	platforms << QObject::tr("Platform_Gamecube.png")
			  << QObject::tr("Platform_Wii.png")
			  << QObject::tr("Platform_Wad.png")
			  << QObject::tr("Platform_Wii.png");
	for (QString platform : platforms)
		m_platforms.append(QPixmap(platform.prepend(sys_dir)));

	QStringList countries;
	countries << QObject::tr("Flag_Europe.png")
			  << QObject::tr("Flag_Japan.png")
			  << QObject::tr("Flag_USA.png")
			  << QObject::tr("Flag_Australia.png")
			  << QObject::tr("Flag_France.png")
			  << QObject::tr("Flag_Germany.png")
			  << QObject::tr("Flag_Italy.png")
			  << QObject::tr("Flag_Korea.png")
			  << QObject::tr("Flag_Netherlands.png")
			  << QObject::tr("Flag_Russia.png")
			  << QObject::tr("Flag_Spain.png")
			  << QObject::tr("Flag_Taiwan.png")
			  << QObject::tr("Flag_International.png")
			  << QObject::tr("Flag_Unknown.png");
	for (QString country : countries)
		m_countries.append(QPixmap(country.prepend(sys_dir)));

	QStringList ratings;
	ratings << QObject::tr("rating0.png")
		    << QObject::tr("rating1.png")
		    << QObject::tr("rating2.png")
		    << QObject::tr("rating3.png")
		    << QObject::tr("rating4.png")
		    << QObject::tr("rating5.png");
	for (QString rating : ratings)
		m_ratings.append(QPixmap(rating.prepend(sys_dir)));

	QString theme_dir = QString::fromStdString(File::GetThemeDir(SConfig::GetInstance().theme_name));
	m_misc.append(QPixmap(QObject::tr("nobanner.png").prepend(theme_dir)));
	m_misc.append(QPixmap(QObject::tr("dolphin_logo.png").prepend(theme_dir)));
	m_misc.append(QPixmap(QObject::tr("Dolphin.png").prepend(theme_dir)));
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
