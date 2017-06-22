// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <QGuiApplication>
#include <QIcon>
#include <QPixmap>
#include <QScreen>
#include <QStringList>

#include "Common/CommonPaths.h"
#include "Common/FileUtil.h"
#include "Core/ConfigManager.h"
#include "DolphinQt2/Resources.h"
#include "DolphinQt2/Settings.h"

QList<QPixmap> Resources::m_platforms;
QList<QPixmap> Resources::m_countries;
QList<QPixmap> Resources::m_ratings;
QList<QPixmap> Resources::m_misc;

QIcon Resources::GetIcon(const QString& name, const QString& dir)
{
  QString base_path = dir + QStringLiteral("/") + name;

  const auto dpr = QGuiApplication::primaryScreen()->devicePixelRatio();

  QIcon icon(base_path.append(QStringLiteral(".png")));

  if (dpr > 2)
  {
    QPixmap pixmap(base_path.append(QStringLiteral("@4x.png")));
    if (!pixmap.isNull())
    {
      pixmap.setDevicePixelRatio(4.0);
      icon.addPixmap(pixmap);
    }
  }

  return icon;
}

QPixmap Resources::GetPixmap(const QString& name, const QString& dir)
{
  const auto icon = GetIcon(name, dir);
  return icon.pixmap(icon.availableSizes()[0]);
}

static QString GetCurrentThemeDir()
{
  return QString::fromStdString(File::GetThemeDir(SConfig::GetInstance().theme_name));
}

static QString GetResourcesDir()
{
  return QString::fromStdString(File::GetSysDirectory() + "Resources");
}

QIcon Resources::GetScaledIcon(const std::string& name)
{
  return GetIcon(QString::fromStdString(name), GetResourcesDir());
}

QIcon Resources::GetScaledThemeIcon(const std::string& name)
{
  return GetIcon(QString::fromStdString(name), GetCurrentThemeDir());
}

QPixmap Resources::GetScaledPixmap(const std::string& name)
{
  return GetPixmap(QString::fromStdString(name), GetResourcesDir());
}

QPixmap Resources::GetScaledThemePixmap(const std::string& name)
{
  return GetPixmap(QString::fromStdString(name), GetCurrentThemeDir());
}

void Resources::Init()
{
  QString sys_dir = QString::fromStdString(File::GetSysDirectory() + RESOURCES_DIR + DIR_SEP);

  for (const std::string& platform :
       {"Platform_Gamecube", "Platform_Wii", "Platform_Wad", "Platform_File"})
  {
    m_platforms.append(GetScaledPixmap(platform));
  }

  for (const std::string& country :
       {"Flag_Europe", "Flag_Japan", "Flag_USA", "Flag_Australia", "Flag_France", "Flag_Germany",
        "Flag_Italy", "Flag_Korea", "Flag_Netherlands", "Flag_Russia", "Flag_Spain", "Flag_Taiwan",
        "Flag_International", "Flag_Unknown"})
  {
    m_countries.append(GetScaledPixmap(country));
  }

  m_misc.append(GetScaledPixmap("nobanner"));
  m_misc.append(GetScaledPixmap("dolphin_logo"));
  m_misc.append(GetScaledPixmap("Dolphin"));

  QObject::connect(&Settings::Instance(), &Settings::ThemeChanged, Resources::InitThemeIcons);
  InitThemeIcons();
}

void Resources::InitThemeIcons()
{
  m_ratings = {GetScaledThemePixmap("rating0"), GetScaledThemePixmap("rating1"),
               GetScaledThemePixmap("rating2"), GetScaledThemePixmap("rating3"),
               GetScaledThemePixmap("rating4"), GetScaledThemePixmap("rating5")};
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
