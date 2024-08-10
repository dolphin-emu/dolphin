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
  void ShowLog();
  void HideLog();
  static bool IsLogConfigVisible();
  void ShowLogConfig();
  void HideLogConfig();
  void ShowToolBar();
  void HideToolBar();
  static bool IsToolBarVisible();
  void LockWidgets();
  void UnlockWidgets();
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
  void EnableAutoRefresh();
  void DisableAutoRefresh();

  // Emulation
  static int GetStateSlot();
  static void SetStateSlot(int);
  bool IsBatchModeEnabled() const;
  void DisableBatchMode();
  void EnableBatchMode();

  static bool IsSDCardInserted();
  void InsertSDCard();
  void EjectSDCard();
  static bool IsUSBKeyboardConnected();
  void ConnectUSBKeyboard();
  void DisconnectUSBKeyboard();

  void EnableContinuousFrameStepping();
  void DisableContinuousFrameStepping();
  bool GetIsContinuouslyFrameStepping() const;

  // Graphics
  static Config::ShowCursor GetCursorVisibility();
  static bool GetLockCursor();
  void SetKeepWindowOnTop(bool top);
  static bool IsKeepWindowOnTopEnabled();
  static bool GetGraphicModsEnabled();
  void EnableGraphicMods();
  void DisableGraphicMods();

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
  void EnableDebugMode();
  void DisableDebugMode();
  static bool IsDebugModeEnabled();
  void ShowRegisters();
  void HideRegisters();
  static bool IsRegistersVisible();
  void ShowThreads();
  void HideThreads();
  static bool IsThreadsVisible();
  void ShowWatch();
  void HideWatch();
  static bool IsWatchVisible();
  void ShowBreakpoints();
  void HideBreakpoints();
  static bool IsBreakpointsVisible();
  void ShowCode();
  void HideCode();
  static bool IsCodeVisible();
  void ShowMemory();
  void HideMemory();
  static bool IsMemoryVisible();
  void ShowNetwork();
  void HideNetwork();
  static bool IsNetworkVisible();
  void ShowJIT();
  void HideJIT();
  static bool IsJITVisible();
  void ShowAssembler();
  void HideAssembler();
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
  void EnableAnalytics();
  void DisableAnalytics();

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
