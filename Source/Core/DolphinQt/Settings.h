// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <memory>

#include <QFont>
#include <QObject>
#include <QSettings>

namespace Core
{
enum class State;
}

namespace DiscIO
{
enum class Language;
}

namespace NetPlay
{
class NetPlayClient;
class NetPlayServer;
}  // namespace NetPlay

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

  ~Settings();

  static Settings& Instance();
  static QSettings& GetQSettings();

  // UI
  void SetThemeName(const QString& theme_name);
  void SetCurrentUserStyle(const QString& stylesheet_path);
  QString GetCurrentUserStyle() const;

  void SetUserStylesEnabled(bool enabled);
  bool AreUserStylesEnabled() const;

  bool IsLogVisible() const;
  void SetLogVisible(bool visible);
  bool IsLogConfigVisible() const;
  void SetLogConfigVisible(bool visible);
  bool IsControllerStateNeeded() const;
  void SetControllerStateNeeded(bool needed);
  void SetToolBarVisible(bool visible);
  bool IsToolBarVisible() const;
  void SetWidgetsLocked(bool visible);
  bool AreWidgetsLocked() const;

  void RefreshWidgetVisibility();

  // GameList
  QStringList GetPaths() const;
  void AddPath(const QString& path);
  void RemovePath(const QString& path);
  bool GetPreferredView() const;
  void SetPreferredView(bool list);
  QString GetDefaultGame() const;
  void SetDefaultGame(QString path);
  void RefreshGameList();
  void RefreshMetadata();
  void NotifyMetadataRefreshComplete();
  void ReloadTitleDB();
  bool IsAutoRefreshEnabled() const;
  void SetAutoRefreshEnabled(bool enabled);

  // Emulation
  int GetStateSlot() const;
  void SetStateSlot(int);
  bool IsBatchModeEnabled() const;
  void SetBatchModeEnabled(bool batch);

  bool IsSDCardInserted() const;
  void SetSDCardInserted(bool inserted);
  bool IsUSBKeyboardConnected() const;
  void SetUSBKeyboardConnected(bool connected);

  // Graphics
  void SetHideCursor(bool hide_cursor);
  bool GetHideCursor() const;
  void SetKeepWindowOnTop(bool top);
  bool IsKeepWindowOnTopEnabled() const;

  // Audio
  int GetVolume() const;
  void SetVolume(int volume);
  void IncreaseVolume(int volume);
  void DecreaseVolume(int volume);

  // NetPlay
  std::shared_ptr<NetPlay::NetPlayClient> GetNetPlayClient();
  void ResetNetPlayClient(NetPlay::NetPlayClient* client = nullptr);
  std::shared_ptr<NetPlay::NetPlayServer> GetNetPlayServer();
  void ResetNetPlayServer(NetPlay::NetPlayServer* server = nullptr);

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
  void SetMemoryVisible(bool enabled);
  bool IsMemoryVisible() const;
  void SetJITVisible(bool enabled);
  bool IsJITVisible() const;
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
  void GameListRefreshRequested();
  void TitleDBReloadRequested();
  void MetadataRefreshRequested();
  void MetadataRefreshCompleted();
  void AutoRefreshToggled(bool enabled);
  void HideCursorChanged();
  void KeepWindowOnTopChanged(bool top);
  void VolumeChanged(int volume);
  void NANDRefresh();
  void RegistersVisibilityChanged(bool visible);
  void LogVisibilityChanged(bool visible);
  void LogConfigVisibilityChanged(bool visible);
  void ToolBarVisibilityChanged(bool visible);
  void WidgetLockChanged(bool locked);
  void EnableCheatsChanged(bool enabled);
  void WatchVisibilityChanged(bool visible);
  void BreakpointsVisibilityChanged(bool visible);
  void CodeVisibilityChanged(bool visible);
  void MemoryVisibilityChanged(bool visible);
  void JITVisibilityChanged(bool visible);
  void DebugModeToggled(bool enabled);
  void DebugFontChanged(QFont font);
  void AutoUpdateTrackChanged(const QString& mode);
  void AnalyticsToggled(bool enabled);
  void DevicesChanged();
  void SDCardInsertionChanged(bool inserted);
  void USBKeyboardConnectionChanged(bool connected);

private:
  bool m_batch = false;
  bool m_controller_state_needed = false;
  std::shared_ptr<NetPlay::NetPlayClient> m_client;
  std::shared_ptr<NetPlay::NetPlayServer> m_server;
  Settings();
};

Q_DECLARE_METATYPE(Core::State);
