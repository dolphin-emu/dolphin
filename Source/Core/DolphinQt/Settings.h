// Copyright 2015 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <memory>

#include <QFont>
#include <QObject>
#include <QRadioButton>
#include <QSettings>

#include "Core/Config/MainSettings.h"
#include "DiscIO/Enums.h"
#include "InputCommon/ControllerInterface/ControllerInterface.h"

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

  void UnregisterDevicesChangedCallback();

  static Settings& Instance();
  static QSettings& GetQSettings();

  // UI
  void TriggerThemeChanged();
  void InitDefaultPalette();
  void UpdateSystemDark();
  void SetSystemDark(bool dark);
  bool IsSystemDark();
  bool IsThemeDark();

  void SetUserStyleName(const QString& stylesheet_name);
  QString GetUserStyleName() const;

  enum class StyleType : int
  {
    System = 0,
    Light = 1,
    Dark = 2,
    User = 3,

    MinValue = 0,
    MaxValue = 3,
  };

  void SetStyleType(StyleType type);
  StyleType GetStyleType() const;

  // this evaluates the current stylesheet settings and refreshes the GUI with them
  void ApplyStyle();

  void GetToolTipStyle(QColor& window_color, QColor& text_color, QColor& emphasis_text_color,
                       QColor& border_color, const QPalette& palette,
                       const QPalette& high_contrast_palette) const;

  bool IsLogVisible() const;
  void SetLogVisible(bool visible);
  bool IsLogConfigVisible() const;
  void SetLogConfigVisible(bool visible);
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
  void NotifyRefreshGameListStarted();
  void NotifyRefreshGameListComplete();
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
  Config::ShowCursor GetCursorVisibility() const;
  bool GetLockCursor() const;
  void SetKeepWindowOnTop(bool top);
  bool IsKeepWindowOnTopEnabled() const;
  bool GetGraphicModsEnabled() const;
  void SetGraphicModsEnabled(bool enabled);

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
  void SetThreadsVisible(bool enabled);
  bool IsThreadsVisible() const;
  void SetWatchVisible(bool enabled);
  bool IsWatchVisible() const;
  void SetBreakpointsVisible(bool enabled);
  bool IsBreakpointsVisible() const;
  void SetCodeVisible(bool enabled);
  bool IsCodeVisible() const;
  void SetMemoryVisible(bool enabled);
  bool IsMemoryVisible() const;
  void SetNetworkVisible(bool enabled);
  bool IsNetworkVisible() const;
  void SetJITVisible(bool enabled);
  bool IsJITVisible() const;
  void SetAssemblerVisible(bool enabled);
  bool IsAssemblerVisible() const;
  QFont GetDebugFont() const;
  void SetDebugFont(QFont font);

  // Auto-Update
  QString GetAutoUpdateTrack() const;
  void SetAutoUpdateTrack(const QString& mode);

  // Fallback Region
  DiscIO::Region GetFallbackRegion() const;
  void SetFallbackRegion(const DiscIO::Region& region);

  // Analytics
  bool IsAnalyticsEnabled() const;
  void SetAnalyticsEnabled(bool enabled);

signals:
  void ConfigChanged();
  void EmulationStateChanged(Core::State new_state);
  void ThemeChanged();
  void PathAdded(const QString&);
  void PathRemoved(const QString&);
  void DefaultGameChanged(const QString&);
  void GameListRefreshRequested();
  void GameListRefreshStarted();
  void GameListRefreshCompleted();
  void TitleDBReloadRequested();
  void MetadataRefreshRequested();
  void MetadataRefreshCompleted();
  void AutoRefreshToggled(bool enabled);
  void CursorVisibilityChanged();
  void LockCursorChanged();
  void KeepWindowOnTopChanged(bool top);
  void VolumeChanged(int volume);
  void NANDRefresh();
  void RegistersVisibilityChanged(bool visible);
  void ThreadsVisibilityChanged(bool visible);
  void LogVisibilityChanged(bool visible);
  void LogConfigVisibilityChanged(bool visible);
  void ToolBarVisibilityChanged(bool visible);
  void WidgetLockChanged(bool locked);
  void EnableCheatsChanged(bool enabled);
  void WatchVisibilityChanged(bool visible);
  void BreakpointsVisibilityChanged(bool visible);
  void CodeVisibilityChanged(bool visible);
  void MemoryVisibilityChanged(bool visible);
  void NetworkVisibilityChanged(bool visible);
  void JITVisibilityChanged(bool visible);
  void AssemblerVisibilityChanged(bool visible);
  void DebugModeToggled(bool enabled);
  void DebugFontChanged(const QFont& font);
  void AutoUpdateTrackChanged(const QString& mode);
  void FallbackRegionChanged(const DiscIO::Region& region);
  void AnalyticsToggled(bool enabled);
  void ReleaseDevices();
  void DevicesChanged();
  void SDCardInsertionChanged(bool inserted);
  void USBKeyboardConnectionChanged(bool connected);
  void EnableGfxModsChanged(bool enabled);
  void HardcoreStateChanged();

private:
  Settings();

  bool m_batch = false;
  std::shared_ptr<NetPlay::NetPlayClient> m_client;
  std::shared_ptr<NetPlay::NetPlayServer> m_server;
  ControllerInterface::HotplugCallbackHandle m_hotplug_callback_handle;
};

Q_DECLARE_METATYPE(Core::State);
