// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <memory>

#include <QObject>
#include <QVector>

#include "Core/NetPlayClient.h"
#include "Core/NetPlayServer.h"

namespace DiscIO
{
enum class Language;
}

class GameListModel;
class InputConfig;

// UI settings to be stored in the config directory.
class Settings final : public QObject
{
  Q_OBJECT

public:
  Settings(const Settings&) = delete;
  Settings& operator=(const Settings&) = delete;
  Settings(Settings&&) = delete;
  Settings& operator=(Settings&&) = delete;

  static Settings& Instance();

  // UI
  void SetThemeName(const QString& theme_name);
  QString GetProfilesDir() const;
  QVector<QString> GetProfiles(const InputConfig* config) const;
  QString GetProfileINIPath(const InputConfig* config, const QString& name) const;

  bool IsInDevelopmentWarningEnabled() const;
  bool IsLogVisible() const;
  void SetLogVisible(bool visible);
  bool IsLogConfigVisible() const;
  void SetLogConfigVisible(bool visible);

  // GameList
  QStringList GetPaths() const;
  void AddPath(const QString& path);
  void RemovePath(const QString& path);
  bool GetPreferredView() const;
  void SetPreferredView(bool list);

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

  // NetPlay
  NetPlayClient* GetNetPlayClient();
  void ResetNetPlayClient(NetPlayClient* client = nullptr);
  NetPlayServer* GetNetPlayServer();
  void ResetNetPlayServer(NetPlayServer* server = nullptr);

  // Other
  GameListModel* GetGameListModel() const;

signals:
  void ThemeChanged();
  void PathAdded(const QString&);
  void PathRemoved(const QString&);
  void HideCursorChanged();
  void VolumeChanged(int volume);
  void NANDRefresh();
  void LogVisibilityChanged(bool visible);
  void LogConfigVisibilityChanged(bool visible);

private:
  std::unique_ptr<NetPlayClient> m_client;
  std::unique_ptr<NetPlayServer> m_server;
  Settings();
};
