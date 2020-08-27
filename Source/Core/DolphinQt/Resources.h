// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <QList>
#include <QPixmap>

namespace DiscIO
{
enum class Country;
enum class Platform;
}  // namespace DiscIO

// Store for various QPixmaps that will be used repeatedly.
class Resources final
{
public:
  enum class MiscID
  {
    BannerMissing,
    LogoLarge,
    LogoSmall
  };

  static void Init();

  static QPixmap GetPlatform(DiscIO::Platform platform);
  static QPixmap GetCountry(DiscIO::Country country);

  static QPixmap GetMisc(MiscID id);

  static QIcon GetScaledIcon(const std::string& name);
  static QIcon GetScaledThemeIcon(const std::string& name);
  static QIcon GetAppIcon();

  static QPixmap GetScaledPixmap(const std::string& name);

private:
  Resources() {}
  static QIcon GetIcon(const QString& name, const QString& dir);
  static QPixmap GetPixmap(const QString& name, const QString& dir);

  static QList<QPixmap> m_platforms;
  static QList<QPixmap> m_countries;
  static QList<QPixmap> m_misc;
};
