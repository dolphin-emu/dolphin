// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <QSize>

#include "Common/FileUtil.h"
#include "Core/ConfigManager.h"
#include "DolphinQt2/Settings.h"

static QString GetSettingsPath()
{
	return QString::fromStdString(File::GetUserPath(D_CONFIG_IDX)) + QStringLiteral("/UI.ini");
}

Settings::Settings(QObject* parent)
	: QSettings(GetSettingsPath(), QSettings::IniFormat, parent)
{
}

QString Settings::GetThemeDir() const
{
	QString theme_name = value(QStringLiteral("Theme"), QStringLiteral("Clean")).toString();
	return QString::fromStdString(File::GetThemeDir(theme_name.toStdString()));
}

QString Settings::GetLastGame() const
{
	return value(QStringLiteral("GameList/LastGame")).toString();
}

void Settings::SetLastGame(const QString& path)
{
	setValue(QStringLiteral("GameList/LastGame"), path);
}

QStringList Settings::GetPaths() const
{
	return value(QStringLiteral("GameList/Paths")).toStringList();
}

void Settings::SetPaths(const QStringList& paths)
{
	setValue(QStringLiteral("GameList/Paths"), paths);
}

void Settings::RemovePath(int i)
{
	QStringList paths = GetPaths();
	paths.removeAt(i);
	SetPaths(paths);
}

QString Settings::GetDefaultGame() const
{
	return QString::fromStdString(SConfig::GetInstance().m_strDefaultISO);
}

void Settings::SetDefaultGame(const QString& path)
{
	SConfig::GetInstance().m_strDefaultISO = path.toStdString();
	SConfig::GetInstance().SaveSettings();
}

QString Settings::GetDVDRoot() const
{
	return QString::fromStdString(SConfig::GetInstance().m_strDVDRoot);
}

void Settings::SetDVDRoot(const QString& path)
{
	SConfig::GetInstance().m_strDVDRoot = path.toStdString();
	SConfig::GetInstance().SaveSettings();
}

QString Settings::GetApploader() const
{
	return QString::fromStdString(SConfig::GetInstance().m_strApploader);
}

void Settings::SetApploader(const QString& path)
{
	SConfig::GetInstance().m_strApploader = path.toStdString();
	SConfig::GetInstance().SaveSettings();
}

QString Settings::GetWiiNAND() const
{
	return QString::fromStdString(SConfig::GetInstance().m_NANDPath);
}

void Settings::SetWiiNAND(const QString& path)
{
	SConfig::GetInstance().m_NANDPath = path.toStdString();
	SConfig::GetInstance().SaveSettings();
}

DiscIO::IVolume::ELanguage Settings::GetWiiSystemLanguage() const
{
	return SConfig::GetInstance().GetCurrentLanguage(true);
}

DiscIO::IVolume::ELanguage Settings::GetGCSystemLanguage() const
{
	return SConfig::GetInstance().GetCurrentLanguage(false);
}

bool Settings::GetConfirmStop() const
{
	return value(QStringLiteral("Emulation/ConfirmStop"), true).toBool();
}

bool Settings::GetRenderToMain() const
{
	return value(QStringLiteral("Graphics/RenderToMain"), false).toBool();
}

bool Settings::GetFullScreen() const
{
	return value(QStringLiteral("Graphics/FullScreen"), false).toBool();
}

QSize Settings::GetRenderWindowSize() const
{
	return value(QStringLiteral("Graphics/RenderWindowSize"), QSize(640, 480)).toSize();
}
