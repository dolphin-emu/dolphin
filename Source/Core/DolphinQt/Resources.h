// Copyright 2015 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QIcon>
#include <QList>
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
    Logo,
  };

  static void Init();

  static QIcon GetPlatform(DiscIO::Platform platform);
  static QIcon GetCountry(DiscIO::Country country);

  static QIcon GetMisc(MiscID id);

  static QIcon GetResourceIcon(std::string_view name);
  static QIcon GetThemeIcon(std::string_view name);
  static QIcon GetAppIcon();

private:
  Resources() {}
  static QIcon LoadNamedIcon(std::string_view name, const QString& dir);

  static bool m_svg_supported;
  static QList<QIcon> m_platforms;
  static QList<QIcon> m_countries;
  static QList<QIcon> m_misc;
};
