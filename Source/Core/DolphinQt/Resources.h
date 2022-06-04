// Copyright 2015 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QList>
#include <QPixmap>
#include <string_view>

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

  static QIcon GetScaledIcon(std::string_view name);
  static QIcon GetScaledThemeIcon(std::string_view name);
  static QIcon GetAppIcon();

  static QPixmap GetScaledPixmap(std::string_view name);

private:
  Resources() {}
  static QIcon GetIcon(std::string_view name, const QString& dir);
  static QPixmap GetPixmap(std::string_view name, const QString& dir);

  static QList<QPixmap> m_platforms;
  static QList<QPixmap> m_countries;
  static QList<QPixmap> m_misc;
};
