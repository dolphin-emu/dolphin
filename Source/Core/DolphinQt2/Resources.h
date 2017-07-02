// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <QList>

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

  static QIcon GetScaledIcon(const std::string& name);
  static QIcon GetScaledThemeIcon(const std::string& name);

  static QPixmap GetScaledPixmap(const std::string& name);
  static QPixmap GetScaledThemePixmap(const std::string& name);

private:
  Resources() {}
  static void InitThemeIcons();
  static QIcon GetIcon(const QString& name, const QString& dir);
  static QPixmap GetPixmap(const QString& name, const QString& dir);

  static QList<QPixmap> m_platforms;
  static QList<QPixmap> m_countries;
  static QList<QPixmap> m_ratings;
  static QList<QPixmap> m_misc;
};
