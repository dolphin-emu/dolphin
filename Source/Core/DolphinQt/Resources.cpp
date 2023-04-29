// Copyright 2015 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/Resources.h"

#include <QFileInfo>
#include <QIcon>
#include <QImageReader>
#include <QPixmap>

#include "Common/Assert.h"
#include "Common/FileUtil.h"

#include "Core/Config/MainSettings.h"

#include "DolphinQt/Settings.h"

bool Resources::m_svg_supported;
QList<QIcon> Resources::m_platforms;
QList<QIcon> Resources::m_countries;
QList<QIcon> Resources::m_misc;

QIcon Resources::LoadNamedIcon(std::string_view name, const QString& dir)
{
  const QString base_path = dir + QLatin1Char{'/'} + QString::fromLatin1(name);
  const QString svg_path = base_path + QStringLiteral(".svg");

  // Prefer svg
  if (m_svg_supported && QFileInfo(svg_path).exists())
    return QIcon(svg_path);

  QIcon icon;

  auto load_png = [&](int scale) {
    QString suffix = QStringLiteral(".png");
    if (scale > 1)
      suffix = QString::fromLatin1("@%1x.png").arg(scale);

    QPixmap pixmap(base_path + suffix);
    if (!pixmap.isNull())
    {
      pixmap.setDevicePixelRatio(scale);
      icon.addPixmap(pixmap);
    }
  };

  // Since we are caching the files, we need to try and load all known sizes up-front.
  // Otherwise, a dynamic change of devicePixelRatio could result in use of non-ideal image from
  // cache while a better one exists on disk.
  for (auto scale : {1, 2, 4})
    load_png(scale);

  ASSERT(icon.availableSizes().size() > 0);

  return icon;
}

static QString GetCurrentThemeDir()
{
  return QString::fromStdString(File::GetThemeDir(Config::Get(Config::MAIN_THEME_NAME)));
}

static QString GetResourcesDir()
{
  return QString::fromStdString(File::GetSysDirectory() + "Resources");
}

QIcon Resources::GetResourceIcon(std::string_view name)
{
  return LoadNamedIcon(name, GetResourcesDir());
}

QIcon Resources::GetThemeIcon(std::string_view name)
{
  return LoadNamedIcon(name, GetCurrentThemeDir());
}

void Resources::Init()
{
  m_svg_supported = QImageReader::supportedImageFormats().contains("svg");

  for (std::string_view platform :
       {"Platform_Gamecube", "Platform_Wii", "Platform_Wad", "Platform_File"})
  {
    m_platforms.append(GetResourceIcon(platform));
  }

  for (std::string_view country :
       {"Flag_Europe", "Flag_Japan", "Flag_USA", "Flag_Australia", "Flag_France", "Flag_Germany",
        "Flag_Italy", "Flag_Korea", "Flag_Netherlands", "Flag_Russia", "Flag_Spain", "Flag_Taiwan",
        "Flag_International", "Flag_Unknown"})
  {
    m_countries.append(GetResourceIcon(country));
  }

  m_misc.append(GetResourceIcon("nobanner"));
  m_misc.append(GetResourceIcon("dolphin_logo"));
}

QIcon Resources::GetPlatform(DiscIO::Platform platform)
{
  return m_platforms[static_cast<int>(platform)];
}

QIcon Resources::GetCountry(DiscIO::Country country)
{
  return m_countries[static_cast<int>(country)];
}

QIcon Resources::GetMisc(MiscID id)
{
  return m_misc[static_cast<int>(id)];
}

QIcon Resources::GetAppIcon()
{
  return GetMisc(MiscID::Logo);
}
