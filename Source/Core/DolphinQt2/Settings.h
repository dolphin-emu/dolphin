// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <QObject>
#include <QVector>

#include "Common/NonCopyable.h"

namespace DiscIO
{
enum class Language;
}

class InputConfig;

// UI settings to be stored in the config directory.
class Settings final : public QObject, NonCopyable
{
  Q_OBJECT

public:
  static Settings& Instance();

  // UI
  void SetThemeName(const QString& theme_name);
  QString GetProfilesDir() const;
  QVector<QString> GetProfiles(const InputConfig* config) const;
  QString GetProfileINIPath(const InputConfig* config, const QString& name) const;

  // GameList
  QStringList GetPaths() const;
  void AddPath(const QString& path);
  void RemovePath(const QString& path);
  bool GetPreferredView() const;
  void SetPreferredView(bool table);

  // Emulation
  int GetStateSlot() const;
  void SetStateSlot(int);

  // Graphics
  void SetHideCursor(bool hide_cursor);
  bool GetHideCursor() const;

  // Audio
  int GetVolume() const;
  void SetVolume(int volume);
  void IncreaseVolume(int volume);
  void DecreaseVolume(int volume);

signals:
  void ThemeChanged();
  void PathAdded(const QString&);
  void PathRemoved(const QString&);
  void HideCursorChanged();
  void VolumeChanged(int volume);
  void NANDRefresh();

private:
  Settings();
};
