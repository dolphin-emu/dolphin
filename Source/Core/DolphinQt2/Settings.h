// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <memory>

#include <QFont>
#include <QObject>
#include <QSettings>
#include <QVector>

#include "Core/NetPlayClient.h"
#include "Core/NetPlayServer.h"

namespace Core
{
enum class State;
}

namespace DiscIO
{
enum class Language;
}

class GameListModel;
class InputConfig;
class QFont;

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
  static QSettings& GetQSettings();

  // UI
  void SetThemeName(const QString& theme_name);

  bool IsInDevelopmentWarningEnabled() const;
  bool IsLogVisible() const;
  void SetLogVisible(bool visible);
  bool IsLogConfigVisible() const;
  void SetLogConfigVisible(bool visible);
  bool IsControllerStateNeeded() const;
  void SetControllerStateNeeded(bool needed);

  // GameList
  QStringList GetPaths() const;
  void AddPath(const QString& path);
  void RemovePath(const QString& path);
  bool GetPreferredView() const;
  void SetPreferredView(bool list);
  QString GetDefaultGame() const;
  void SetDefaultGame(QString path);

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

  // Cheats
  bool GetCheatsEnabled() const;
  void SetCheatsEnabled(bool enabled);

  // Debug
  void SetDebugModeEnabled(bool enabled);
  bool IsDebugModeEnabled() const;
  void SetRegistersVisible(bool enabled);
  bool IsRegistersVisible() const;
  void SetWatchVisible(bool enabled);
  bool IsWatchVisible() const;
  void SetBreakpointsVisible(bool enabled);
  bool IsBreakpointsVisible() const;
  void SetCodeVisible(bool enabled);
  bool IsCodeVisible() const;
  QFont GetDebugFont() const;
  void SetDebugFont(QFont font);

  // Auto-Update
  QString GetAutoUpdateTrack() const;
  void SetAutoUpdateTrack(const QString& mode);

  // Analytics
  bool IsAnalyticsEnabled() const;
  void SetAnalyticsEnabled(bool enabled);

  // Other
  GameListModel* GetGameListModel() const;
signals:
  void ConfigChanged();
  void EmulationStateChanged(Core::State new_state);
  void ThemeChanged();
  void PathAdded(const QString&);
  void PathRemoved(const QString&);
  void DefaultGameChanged(const QString&);
  void HideCursorChanged();
  void VolumeChanged(int volume);
  void NANDRefresh();
  void RegistersVisibilityChanged(bool visible);
  void LogVisibilityChanged(bool visible);
  void LogConfigVisibilityChanged(bool visible);
  void EnableCheatsChanged(bool enabled);
  void WatchVisibilityChanged(bool visible);
  void BreakpointsVisibilityChanged(bool visible);
  void CodeVisibilityChanged(bool visible);
  void DebugModeToggled(bool enabled);
  void DebugFontChanged(QFont font);
  void AutoUpdateTrackChanged(const QString& mode);
  void AnalyticsToggled(bool enabled);

private:
  bool m_controller_state_needed = false;
  std::unique_ptr<NetPlayClient> m_client;
  std::unique_ptr<NetPlayServer> m_server;
  Settings();
};

Q_DECLARE_METATYPE(Core::State);
