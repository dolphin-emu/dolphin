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

  void UnregisterDevicesChangedCallback() const;

  static Settings& Instance();
  static QSettings& GetQSettings();

  // UI
  void TriggerThemeChanged();
  static void InitDefaultPalette();
  static void UpdateSystemDark();
  static void SetSystemDark(bool dark);
  static bool IsSystemDark();
  static bool IsThemeDark();

  static void SetUserStyleName(const QString& stylesheet_name);
  static QString GetUserStyleName();

  enum class StyleType : int
  {
    System = 0,
    Light = 1,
    Dark = 2,
    User = 3,

    MinValue = 0,
    MaxValue = 3,
  };

  static void SetStyleType(StyleType type);
  static StyleType GetStyleType();

  // this evaluates the current stylesheet settings and refreshes the GUI with them
  static void ApplyStyle();

  static void GetToolTipStyle(QColor& window_color, QColor& text_color, QColor& emphasis_text_color,
                              QColor& border_color, const QPalette& palette,
                              const QPalette& high_contrast_palette);

  static bool IsLogVisible();
  void SetLogVisible(bool visible);
  static bool IsLogConfigVisible();
  void SetLogConfigVisible(bool visible);
  void SetToolBarVisible(bool visible);
  static bool IsToolBarVisible();
  void SetWidgetsLocked(bool visible);
  static bool AreWidgetsLocked();

  void RefreshWidgetVisibility();

  // GameList
  static QStringList GetPaths();
  void AddPath(const QString& path);
  void RemovePath(const QString& path);
  static bool GetPreferredView();
  static void SetPreferredView(bool list);
  static QString GetDefaultGame();
  void SetDefaultGame(const QString& path);
  void RefreshGameList();
  void NotifyRefreshGameListStarted();
  void NotifyRefreshGameListComplete();
  void NotifyMetadataRefreshComplete();
  void ReloadTitleDB();
  static bool IsAutoRefreshEnabled();
  void SetAutoRefreshEnabled(bool enabled);

  // Emulation
  static int GetStateSlot();
  static void SetStateSlot(int);
  bool IsBatchModeEnabled() const;
  void SetBatchModeEnabled(bool batch);

  static bool IsSDCardInserted();
  void SetSDCardInserted(bool inserted);
  static bool IsUSBKeyboardConnected();
  void SetUSBKeyboardConnected(bool connected);

  void SetIsContinuouslyFrameStepping(bool is_stepping);
  bool GetIsContinuouslyFrameStepping() const;

  // Graphics
  static Config::ShowCursor GetCursorVisibility();
  static bool GetLockCursor();
  void SetKeepWindowOnTop(bool top);
  static bool IsKeepWindowOnTopEnabled();
  static bool GetGraphicModsEnabled();
  void SetGraphicModsEnabled(bool enabled);

  // Audio
  static int GetVolume();
  void SetVolume(int volume);
  void IncreaseVolume(int volume);
  void DecreaseVolume(int volume);

  // NetPlay
  std::shared_ptr<NetPlay::NetPlayClient> GetNetPlayClient();
  void ResetNetPlayClient(NetPlay::NetPlayClient* client = nullptr);
  std::shared_ptr<NetPlay::NetPlayServer> GetNetPlayServer();
  void ResetNetPlayServer(NetPlay::NetPlayServer* server = nullptr);

  // Cheats
  static bool GetCheatsEnabled();

  // Debug
  void SetDebugModeEnabled(bool enabled);
  static bool IsDebugModeEnabled();
  void SetRegistersVisible(bool enabled);
  static bool IsRegistersVisible();
  void SetThreadsVisible(bool enabled);
  static bool IsThreadsVisible();
  void SetWatchVisible(bool enabled);
  static bool IsWatchVisible();
  void SetBreakpointsVisible(bool enabled);
  static bool IsBreakpointsVisible();
  void SetCodeVisible(bool enabled);
  static bool IsCodeVisible();
  void SetMemoryVisible(bool enabled);
  static bool IsMemoryVisible();
  void SetNetworkVisible(bool enabled);
  static bool IsNetworkVisible();
  void SetJITVisible(bool enabled);
  static bool IsJITVisible();
  void SetAssemblerVisible(bool enabled);
  static bool IsAssemblerVisible();
  static QFont GetDebugFont();
  void SetDebugFont(const QFont& font);

  // Auto-Update
  static QString GetAutoUpdateTrack();
  void SetAutoUpdateTrack(const QString& mode);

  // Fallback Region
  static DiscIO::Region GetFallbackRegion();
  void SetFallbackRegion(const DiscIO::Region& region);

  // Analytics
  static bool IsAnalyticsEnabled();
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
  std::atomic<bool> m_continuously_frame_stepping = false;

  std::shared_ptr<NetPlay::NetPlayClient> m_client;
  std::shared_ptr<NetPlay::NetPlayServer> m_server;
  ControllerInterface::HotplugCallbackHandle m_hotplug_callback_handle;
};

Q_DECLARE_METATYPE(Core::State);
