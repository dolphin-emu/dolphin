// Copyright 2015 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/Settings.h"

#include <atomic>
#include <memory>

#include <QApplication>
#include <QColor>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFontDatabase>
#include <QPalette>
#include <QSize>
#include <QStyle>
#include <QWidget>

#ifdef _WIN32
#include <winrt/Windows.UI.ViewManagement.h>

#include <QToolButton>
#endif

#include "AudioCommon/AudioCommon.h"

#include "Common/Config/Config.h"
#include "Common/FileUtil.h"

#include "Core/AchievementManager.h"
#include "Core/Config/GraphicsSettings.h"
#include "Core/Config/MainSettings.h"
#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/NetPlayClient.h"
#include "Core/NetPlayServer.h"
#include "Core/System.h"

#include "DolphinQt/QtUtils/QueueOnObject.h"

#include "InputCommon/ControllerInterface/ControllerInterface.h"

#include "VideoCommon/NetPlayChatUI.h"
#include "VideoCommon/NetPlayGolfUI.h"

static bool s_system_dark = false;
static std::unique_ptr<QPalette> s_default_palette;

Settings::Settings()
{
  qRegisterMetaType<Core::State>();
  Core::AddOnStateChangedCallback([this](Core::State new_state) {
    QueueOnObject(this, [this, new_state] {
      // Avoid signal spam while continuously frame stepping. Will still send a signal for the first
      // and last framestep.
      if (!m_continuously_frame_stepping)
        emit EmulationStateChanged(new_state);
    });
  });

  Config::AddConfigChangedCallback([this] {
    static std::atomic do_once{true};
    if (do_once.exchange(false))
    {
      // Calling ConfigChanged() with a "delay" can have risks, for example, if from
      // code we change some configs that result in Qt greying out some setting, we could
      // end up editing that setting before its greyed out, sending out an event,
      // which might not be expected or handled by the code, potentially crashing.
      // The only safe option would be to wait on the Qt thread to have finished executing this.
      QueueOnObject(this, [this] {
        do_once = true;
        emit ConfigChanged();
      });
    }
  });

  m_hotplug_callback_handle = g_controller_interface.RegisterDevicesChangedCallback([this] {
    if (Core::IsHostThread())
    {
      emit DevicesChanged();
    }
    else
    {
      // Any device shared_ptr in the host thread needs to be released immediately as otherwise
      // they'd continue living until the queued event has run, but some devices can't be recreated
      // until they are destroyed.
      // This is safe from any thread. Devices will be refreshed and re-acquired and in
      // DevicesChanged(). Calling it without queueing shouldn't cause any deadlocks but is slow.
      emit ReleaseDevices();

      QueueOnObject(this, [this] { emit DevicesChanged(); });
    }
  });
}

Settings::~Settings() = default;

void Settings::UnregisterDevicesChangedCallback() const
{
  g_controller_interface.UnregisterDevicesChangedCallback(m_hotplug_callback_handle);
}

Settings& Settings::Instance()
{
  static Settings settings;
  return settings;
}

QSettings& Settings::GetQSettings()
{
  static QSettings settings(
      QStringLiteral("%1/Qt.ini").arg(QString::fromStdString(File::GetUserPath(D_CONFIG_IDX))),
      QSettings::IniFormat);
  return settings;
}

void Settings::TriggerThemeChanged()
{
  emit ThemeChanged();
}

QString Settings::GetUserStyleName()
{
  if (GetQSettings().contains(QStringLiteral("userstyle/name")))
    return GetQSettings().value(QStringLiteral("userstyle/name")).toString();

  // Migration code for the old way of storing this setting
  return QFileInfo(GetQSettings().value(QStringLiteral("userstyle/path")).toString()).fileName();
}

void Settings::SetUserStyleName(const QString& stylesheet_name)
{
  GetQSettings().setValue(QStringLiteral("userstyle/name"), stylesheet_name);
}

void Settings::InitDefaultPalette()
{
  s_default_palette = std::make_unique<QPalette>(qApp->palette());
}

void Settings::UpdateSystemDark()
{
#ifdef _WIN32
  // Check if the system is set to dark mode so we can set the default theme and window
  // decorations accordingly.
  {
    using namespace winrt::Windows::UI::ViewManagement;
    const UISettings settings;
    const auto& [_A, R, G, B] = settings.GetColorValue(UIColorType::Foreground);

    const bool is_system_dark = 5 * G + 2 * R + B > 8 * 128;
    Instance().SetSystemDark(is_system_dark);
  }
#endif
}

void Settings::SetSystemDark(const bool dark)
{
  s_system_dark = dark;
}

bool Settings::IsSystemDark()
{
  return s_system_dark;
}

bool Settings::IsThemeDark()
{
  return qApp->palette().color(QPalette::Base).valueF() < 0.5;
}

// Calling this before the main window has been created breaks the style of some widgets.
void Settings::ApplyStyle()
{
  const StyleType style_type = GetStyleType();
  const QString stylesheet_name = GetUserStyleName();
  QString stylesheet_contents;

  // If we haven't found one, we continue with an empty (default) style
  if (!stylesheet_name.isEmpty() && style_type == StyleType::User)
  {
    // Load custom user stylesheet
    auto directory = QDir(QString::fromStdString(File::GetUserPath(D_STYLES_IDX)));
    QFile stylesheet(directory.filePath(stylesheet_name));

    if (stylesheet.open(QFile::ReadOnly))
      stylesheet_contents = QString::fromUtf8(stylesheet.readAll().data());
  }

#ifdef _WIN32
  if (stylesheet_contents.isEmpty())
  {
    // No theme selected or found. Usually we would just fallthrough and set an empty stylesheet
    // which would select Qt's default theme, but unlike other OSes we don't automatically get a
    // default dark theme on Windows when the user has selected dark mode in the Windows settings.
    // So manually check if the user wants dark mode and, if yes, load our embedded dark theme.
    if (style_type == StyleType::Dark || (style_type != StyleType::Light && IsSystemDark()))
    {
      QFile file(QStringLiteral(":/dolphin_dark_win/dark.qss"));
      if (file.open(QFile::ReadOnly))
        stylesheet_contents = QString::fromUtf8(file.readAll().data());

      QPalette palette = qApp->style()->standardPalette();
      palette.setColor(QPalette::Window, QColor(32, 32, 32));
      palette.setColor(QPalette::WindowText, QColor(220, 220, 220));
      palette.setColor(QPalette::Base, QColor(32, 32, 32));
      palette.setColor(QPalette::AlternateBase, QColor(48, 48, 48));
      palette.setColor(QPalette::PlaceholderText, QColor(126, 126, 126));
      palette.setColor(QPalette::Text, QColor(220, 220, 220));
      palette.setColor(QPalette::Button, QColor(48, 48, 48));
      palette.setColor(QPalette::ButtonText, QColor(220, 220, 220));
      palette.setColor(QPalette::BrightText, QColor(255, 255, 255));
      palette.setColor(QPalette::Highlight, QColor(0, 120, 215));
      palette.setColor(QPalette::HighlightedText, QColor(255, 255, 255));
      palette.setColor(QPalette::Link, QColor(100, 160, 220));
      palette.setColor(QPalette::LinkVisited, QColor(100, 160, 220));
      qApp->setPalette(palette);
    }
    else
    {
      // reset any palette changes that may exist from a previously set dark mode
      if (s_default_palette)
        qApp->setPalette(*s_default_palette);
    }
  }
#endif

  // Define tooltips style if not already defined
  if (!stylesheet_contents.contains(QStringLiteral("QToolTip"), Qt::CaseSensitive))
  {
    const QPalette& palette = qApp->palette();
    QColor window_color;
    QColor text_color;
    QColor unused_text_emphasis_color;
    QColor border_color;
    GetToolTipStyle(window_color, text_color, unused_text_emphasis_color, border_color, palette,
                    palette);

    const auto tooltip_stylesheet =
        QStringLiteral("QToolTip { background-color: #%1; color: #%2; padding: 8px; "
                       "border: 1px; border-style: solid; border-color: #%3; }")
            .arg(window_color.rgba(), 0, 16)
            .arg(text_color.rgba(), 0, 16)
            .arg(border_color.rgba(), 0, 16);
    stylesheet_contents.append(QStringLiteral("%1").arg(tooltip_stylesheet));
  }

  qApp->setStyleSheet(stylesheet_contents);
}

Settings::StyleType Settings::GetStyleType()
{
  if (GetQSettings().contains(QStringLiteral("userstyle/styletype")))
  {
    bool ok = false;
    const int type_int = GetQSettings().value(QStringLiteral("userstyle/styletype")).toInt(&ok);
    if (ok && type_int >= static_cast<int>(StyleType::MinValue) &&
        type_int <= static_cast<int>(StyleType::MaxValue))
    {
      return static_cast<StyleType>(type_int);
    }
  }

  // if the style type is unset or invalid, try the old enabled flag instead
  const bool enabled = GetQSettings().value(QStringLiteral("userstyle/enabled"), false).toBool();
  return enabled ? StyleType::User : StyleType::System;
}

void Settings::SetStyleType(StyleType type)
{
  GetQSettings().setValue(QStringLiteral("userstyle/styletype"), static_cast<int>(type));

  // also set the old setting so that the config is correctly intepreted by older Dolphin builds
  GetQSettings().setValue(QStringLiteral("userstyle/enabled"), type == StyleType::User);
}

void Settings::GetToolTipStyle(QColor& window_color, QColor& text_color,
                               QColor& emphasis_text_color, QColor& border_color,
                               const QPalette& palette, const QPalette& high_contrast_palette)
{
  const auto theme_window_color = palette.color(QPalette::Base);
  const auto theme_window_hsv = theme_window_color.toHsv();
  const auto brightness = theme_window_hsv.value();
  const bool brightness_over_threshold = brightness > 128;
  const QColor emphasis_text_color_1 = Qt::yellow;
  const QColor emphasis_text_color_2 = QColor(QStringLiteral("#0090ff"));  // ~light blue
  if (Get(Config::MAIN_USE_HIGH_CONTRAST_TOOLTIPS))
  {
    window_color = brightness_over_threshold ? QColor(72, 72, 72) : Qt::white;
    text_color = brightness_over_threshold ? Qt::white : Qt::black;
    emphasis_text_color = brightness_over_threshold ? emphasis_text_color_1 : emphasis_text_color_2;
    border_color = high_contrast_palette.color(QPalette::Window).darker(160);
  }
  else
  {
    window_color = palette.color(QPalette::Window);
    text_color = palette.color(QPalette::Text);
    emphasis_text_color = brightness_over_threshold ? emphasis_text_color_2 : emphasis_text_color_1;
    border_color = palette.color(QPalette::Text);
  }
}

QStringList Settings::GetPaths()
{
  QStringList list;
  for (const auto& path : Config::GetIsoPaths())
    list << QString::fromStdString(path);
  return list;
}

void Settings::AddPath(const QString& qpath)
{
  std::string path = qpath.toStdString();
  std::vector<std::string> paths = Config::GetIsoPaths();

  if (std::ranges::find(paths, path) != paths.end())
    return;

  paths.emplace_back(path);
  Config::SetIsoPaths(paths);
  emit PathAdded(qpath);
}

void Settings::RemovePath(const QString& qpath)
{
  const std::string path = qpath.toStdString();
  std::vector<std::string> paths = Config::GetIsoPaths();

  const auto new_end = std::ranges::remove(paths, path).begin();
  if (new_end == paths.end())
    return;

  paths.erase(new_end, paths.end());
  Config::SetIsoPaths(paths);
  emit PathRemoved(qpath);
}

void Settings::RefreshGameList()
{
  emit GameListRefreshRequested();
}

void Settings::NotifyRefreshGameListStarted()
{
  emit GameListRefreshStarted();
}

void Settings::NotifyRefreshGameListComplete()
{
  emit GameListRefreshCompleted();
}

void Settings::NotifyMetadataRefreshComplete()
{
  emit MetadataRefreshCompleted();
}

void Settings::ReloadTitleDB()
{
  emit TitleDBReloadRequested();
}

bool Settings::IsAutoRefreshEnabled()
{
  return GetQSettings().value(QStringLiteral("gamelist/autorefresh"), true).toBool();
}

void Settings::EnableAutoRefresh()
{
  if (IsAutoRefreshEnabled())
    return;

  GetQSettings().setValue(QStringLiteral("gamelist/autorefresh"), true);
  emit AutoRefreshToggled(true);
}

void Settings::DisableAutoRefresh()
{
  if (!IsAutoRefreshEnabled())
    return;

  GetQSettings().setValue(QStringLiteral("gamelist/autorefresh"), false);
  emit AutoRefreshToggled(false);
}

QString Settings::GetDefaultGame()
{
  return QString::fromStdString(Get(Config::MAIN_DEFAULT_ISO));
}

void Settings::SetDefaultGame(const QString& path)
{
  if (GetDefaultGame() != path)
  {
    SetBase(Config::MAIN_DEFAULT_ISO, path.toStdString());
    emit DefaultGameChanged(path);
  }
}

bool Settings::GetPreferredView()
{
  return GetQSettings().value(QStringLiteral("PreferredView"), true).toBool();
}

void Settings::SetPreferredView(const bool list)
{
  GetQSettings().setValue(QStringLiteral("PreferredView"), list);
}

int Settings::GetStateSlot()
{
  return GetQSettings().value(QStringLiteral("Emulation/StateSlot"), 1).toInt();
}

void Settings::SetStateSlot(const int slot)
{
  GetQSettings().setValue(QStringLiteral("Emulation/StateSlot"), slot);
}

Config::ShowCursor Settings::GetCursorVisibility()
{
  return Get(Config::MAIN_SHOW_CURSOR);
}

bool Settings::GetLockCursor()
{
  return Get(Config::MAIN_LOCK_CURSOR);
}

void Settings::SetKeepWindowOnTop(const bool top)
{
  if (IsKeepWindowOnTopEnabled() == top)
    return;

  emit KeepWindowOnTopChanged(top);
}

bool Settings::IsKeepWindowOnTopEnabled()
{
  return Get(Config::MAIN_KEEP_WINDOW_ON_TOP);
}

bool Settings::GetGraphicModsEnabled()
{
  return Get(Config::GFX_MODS_ENABLE);
}

void Settings::EnableGraphicMods()
{
  if (GetGraphicModsEnabled())
    return;

  SetBaseOrCurrent(Config::GFX_MODS_ENABLE, true);
  emit EnableGfxModsChanged(true);
}

void Settings::DisableGraphicMods()
{
  if (!GetGraphicModsEnabled())
    return;

  SetBaseOrCurrent(Config::GFX_MODS_ENABLE, false);
  emit EnableGfxModsChanged(false);
}

int Settings::GetVolume()
{
  return Get(Config::MAIN_AUDIO_VOLUME);
}

void Settings::SetVolume(const int volume)
{
  if (GetVolume() != volume)
  {
    SetBaseOrCurrent(Config::MAIN_AUDIO_VOLUME, volume);
    emit VolumeChanged(volume);
  }
}

void Settings::IncreaseVolume(const int volume)
{
  AudioCommon::IncreaseVolume(Core::System::GetInstance(), volume);
  emit VolumeChanged(GetVolume());
}

void Settings::DecreaseVolume(const int volume)
{
  AudioCommon::DecreaseVolume(Core::System::GetInstance(), volume);
  emit VolumeChanged(GetVolume());
}

bool Settings::IsLogVisible()
{
  return GetQSettings().value(QStringLiteral("logging/logvisible")).toBool();
}

void Settings::ShowLog()
{
  if (IsLogVisible())
    return;

  GetQSettings().setValue(QStringLiteral("logging/logvisible"), true);
  emit LogVisibilityChanged(true);
}

void Settings::HideLog()
{
  if (!IsLogVisible())
    return;

  GetQSettings().setValue(QStringLiteral("logging/logvisible"), false);
  emit LogVisibilityChanged(false);
}

bool Settings::IsLogConfigVisible()
{
  return GetQSettings().value(QStringLiteral("logging/logconfigvisible")).toBool();
}

void Settings::ShowLogConfig()
{
  if (IsLogConfigVisible())
    return;

  GetQSettings().setValue(QStringLiteral("logging/logconfigvisible"), true);
  emit LogConfigVisibilityChanged(true);
}

void Settings::HideLogConfig()
{
  if (!IsLogConfigVisible())
    return;

  GetQSettings().setValue(QStringLiteral("logging/logconfigvisible"), false);
  emit LogConfigVisibilityChanged(false);
}

std::shared_ptr<NetPlay::NetPlayClient> Settings::GetNetPlayClient()
{
  return m_client;
}

void Settings::ResetNetPlayClient(NetPlay::NetPlayClient* client)
{
  m_client.reset(client);

  g_netplay_chat_ui.reset();
  g_netplay_golf_ui.reset();
}

std::shared_ptr<NetPlay::NetPlayServer> Settings::GetNetPlayServer()
{
  return m_server;
}

void Settings::ResetNetPlayServer(NetPlay::NetPlayServer* server)
{
  m_server.reset(server);
}

bool Settings::GetCheatsEnabled()
{
  return Get(Config::MAIN_ENABLE_CHEATS);
}

void Settings::EnableDebugMode()
{
  const bool enabled = !AchievementManager::GetInstance().IsHardcoreModeActive();

  if (IsDebugModeEnabled() == enabled)
    return;

  SetBaseOrCurrent(Config::MAIN_ENABLE_DEBUGGING, enabled);
  emit DebugModeToggled(enabled);
  if (enabled)
    ShowCode();
}

void Settings::DisableDebugMode()
{
  if (IsDebugModeEnabled())
    return;

  SetBaseOrCurrent(Config::MAIN_ENABLE_DEBUGGING, false);
  emit DebugModeToggled(false);
}

bool Settings::IsDebugModeEnabled()
{
  return Get(Config::MAIN_ENABLE_DEBUGGING);
}

void Settings::ShowRegisters()
{
  if (IsRegistersVisible())
    return;

  GetQSettings().setValue(QStringLiteral("debugger/showregisters"), true);
  emit RegistersVisibilityChanged(true);
}

void Settings::HideRegisters()
{
  if (!IsRegistersVisible())
    return;

  GetQSettings().setValue(QStringLiteral("debugger/showregisters"), false);
  emit RegistersVisibilityChanged(false);
}

bool Settings::IsThreadsVisible()
{
  return GetQSettings().value(QStringLiteral("debugger/showthreads")).toBool();
}

void Settings::ShowThreads()
{
  if (IsThreadsVisible())
    return;

  GetQSettings().setValue(QStringLiteral("debugger/showthreads"), true);
  emit ThreadsVisibilityChanged(true);
}

void Settings::HideThreads()
{
  if (!IsThreadsVisible())
    return;

  GetQSettings().setValue(QStringLiteral("debugger/showthreads"), false);
  emit ThreadsVisibilityChanged(false);
}

bool Settings::IsRegistersVisible()
{
  return GetQSettings().value(QStringLiteral("debugger/showregisters")).toBool();
}

void Settings::ShowWatch()
{
  if (IsWatchVisible())
    return;

  GetQSettings().setValue(QStringLiteral("debugger/showwatch"), true);
  emit WatchVisibilityChanged(true);
}

void Settings::HideWatch()
{
  if (!IsWatchVisible())
    return;

  GetQSettings().setValue(QStringLiteral("debugger/showwatch"), false);
  emit WatchVisibilityChanged(false);
}

bool Settings::IsWatchVisible()
{
  return GetQSettings().value(QStringLiteral("debugger/showwatch")).toBool();
}

void Settings::ShowBreakpoints()
{
  if (IsBreakpointsVisible())
    return;

  GetQSettings().setValue(QStringLiteral("debugger/showbreakpoints"), true);
  emit BreakpointsVisibilityChanged(true);
}

void Settings::HideBreakpoints()
{
  if (!IsBreakpointsVisible())
    return;

  GetQSettings().setValue(QStringLiteral("debugger/showbreakpoints"), false);
  emit BreakpointsVisibilityChanged(false);
}

bool Settings::IsBreakpointsVisible()
{
  return GetQSettings().value(QStringLiteral("debugger/showbreakpoints")).toBool();
}

void Settings::ShowCode()
{
  if (IsCodeVisible())
    return;

  GetQSettings().setValue(QStringLiteral("debugger/showcode"), true);

  emit CodeVisibilityChanged(true);
}

void Settings::HideCode()
{
  if (!IsCodeVisible())
    return;

  GetQSettings().setValue(QStringLiteral("debugger/showcode"), false);

  emit CodeVisibilityChanged(false);
}

bool Settings::IsCodeVisible()
{
  return GetQSettings().value(QStringLiteral("debugger/showcode")).toBool();
}

void Settings::ShowMemory()
{
  if (IsMemoryVisible())
    return;

  QSettings().setValue(QStringLiteral("debugger/showmemory"), true);
  emit MemoryVisibilityChanged(true);
}

void Settings::HideMemory()
{
  if (!IsMemoryVisible())
    return;

  QSettings().setValue(QStringLiteral("debugger/showmemory"), false);
  emit MemoryVisibilityChanged(false);
}

bool Settings::IsMemoryVisible()
{
  return QSettings().value(QStringLiteral("debugger/showmemory")).toBool();
}

void Settings::ShowNetwork()
{
  if (IsNetworkVisible())
    return;

  GetQSettings().setValue(QStringLiteral("debugger/shownetwork"), true);
  emit NetworkVisibilityChanged(true);
}

void Settings::HideNetwork()
{
  if (!IsNetworkVisible())
    return;

  GetQSettings().setValue(QStringLiteral("debugger/shownetwork"), false);
  emit NetworkVisibilityChanged(false);
}

bool Settings::IsNetworkVisible()
{
  return GetQSettings().value(QStringLiteral("debugger/shownetwork")).toBool();
}

void Settings::ShowJIT()
{
  if (IsJITVisible())
    return;

  QSettings().setValue(QStringLiteral("debugger/showjit"), true);
  emit JITVisibilityChanged(true);
}

void Settings::HideJIT()
{
  if (!IsJITVisible())
    return;

  QSettings().setValue(QStringLiteral("debugger/showjit"), false);
  emit JITVisibilityChanged(false);
}

bool Settings::IsJITVisible()
{
  return QSettings().value(QStringLiteral("debugger/showjit")).toBool();
}

void Settings::ShowAssembler()
{
  if (IsAssemblerVisible())
    return;

  QSettings().setValue(QStringLiteral("debugger/showassembler"), true);
  emit AssemblerVisibilityChanged(true);
}

void Settings::HideAssembler()
{
  if (!IsAssemblerVisible())
    return;

  QSettings().setValue(QStringLiteral("debugger/showassembler"), false);
  emit AssemblerVisibilityChanged(false);
}

bool Settings::IsAssemblerVisible()
{
  return QSettings().value(QStringLiteral("debugger/showassembler")).toBool();
}

void Settings::RefreshWidgetVisibility()
{
  emit DebugModeToggled(IsDebugModeEnabled());
  emit LogVisibilityChanged(IsLogVisible());
  emit LogConfigVisibilityChanged(IsLogConfigVisible());
}

void Settings::SetDebugFont(const QFont& font)
{
  if (GetDebugFont() != font)
  {
    GetQSettings().setValue(QStringLiteral("debugger/font"), font);
    emit DebugFontChanged(font);
  }
}

QFont Settings::GetDebugFont()
{
  auto default_font = QFont(QFontDatabase::systemFont(QFontDatabase::FixedFont).family());
  default_font.setPointSizeF(9.0);

  return GetQSettings().value(QStringLiteral("debugger/font"), default_font).value<QFont>();
}

void Settings::SetAutoUpdateTrack(const QString& mode)
{
  if (mode == GetAutoUpdateTrack())
    return;

  SetBase(Config::MAIN_AUTOUPDATE_UPDATE_TRACK, mode.toStdString());

  emit AutoUpdateTrackChanged(mode);
}

QString Settings::GetAutoUpdateTrack()
{
  return QString::fromStdString(Get(Config::MAIN_AUTOUPDATE_UPDATE_TRACK));
}

void Settings::SetFallbackRegion(const DiscIO::Region& region)
{
  if (region == GetFallbackRegion())
    return;

  SetBase(Config::MAIN_FALLBACK_REGION, region);

  emit FallbackRegionChanged(region);
}

DiscIO::Region Settings::GetFallbackRegion()
{
  return Get(Config::MAIN_FALLBACK_REGION);
}

void Settings::EnableAnalytics()
{
  if (IsAnalyticsEnabled())
    return;

  SetBase(Config::MAIN_ANALYTICS_ENABLED, true);

  emit AnalyticsToggled(true);
}

void Settings::DisableAnalytics()
{
  if (!IsAnalyticsEnabled())
    return;

  SetBase(Config::MAIN_ANALYTICS_ENABLED, false);

  emit AnalyticsToggled(false);
}

bool Settings::IsAnalyticsEnabled()
{
  return Get(Config::MAIN_ANALYTICS_ENABLED);
}

void Settings::ShowToolBar()
{
  if (IsToolBarVisible())
    return;

  GetQSettings().setValue(QStringLiteral("toolbar/visible"), true);
  emit ToolBarVisibilityChanged(true);
}

void Settings::HideToolBar()
{
  if (!IsToolBarVisible())
    return;

  GetQSettings().setValue(QStringLiteral("toolbar/visible"), false);
  emit ToolBarVisibilityChanged(false);
}

bool Settings::IsToolBarVisible()
{
  return GetQSettings().value(QStringLiteral("toolbar/visible"), true).toBool();
}

void Settings::LockWidgets()
{
  if (AreWidgetsLocked())
    return;

  GetQSettings().setValue(QStringLiteral("widgets/locked"), true);
  emit WidgetLockChanged(true);
}

void Settings::UnlockWidgets()
{
  if (!AreWidgetsLocked())
    return;

  GetQSettings().setValue(QStringLiteral("widgets/locked"), false);
  emit WidgetLockChanged(false);
}

bool Settings::AreWidgetsLocked()
{
  return GetQSettings().value(QStringLiteral("widgets/locked"), true).toBool();
}

bool Settings::IsBatchModeEnabled() const
{
  return m_batch;
}

void Settings::EnableBatchMode()
{
  m_batch = true;
}

void Settings::DisableBatchMode()
{
  m_batch = false;
}

bool Settings::IsSDCardInserted()
{
  return Get(Config::MAIN_WII_SD_CARD);
}

void Settings::InsertSDCard()
{
  if (IsSDCardInserted())
    return;

  SetBaseOrCurrent(Config::MAIN_WII_SD_CARD, true);
  emit SDCardInsertionChanged(true);
}

void Settings::EjectSDCard()
{
  if (!IsSDCardInserted())
    return;

  SetBaseOrCurrent(Config::MAIN_WII_SD_CARD, false);
  emit SDCardInsertionChanged(false);
}

bool Settings::IsUSBKeyboardConnected()
{
  return Get(Config::MAIN_WII_KEYBOARD);
}

void Settings::ConnectUSBKeyboard()
{
  if (IsUSBKeyboardConnected())
    return;

  SetBaseOrCurrent(Config::MAIN_WII_KEYBOARD, true);
  emit USBKeyboardConnectionChanged(true);
}

void Settings::DisconnectUSBKeyboard()
{
  if (!IsUSBKeyboardConnected())
    return;

  SetBaseOrCurrent(Config::MAIN_WII_KEYBOARD, false);
  emit USBKeyboardConnectionChanged(false);
}

void Settings::EnableContinuousFrameStepping()
{
  m_continuously_frame_stepping = true;
}

void Settings::DisableContinuousFrameStepping()
{
  m_continuously_frame_stepping = false;
}

bool Settings::GetIsContinuouslyFrameStepping() const
{
  return m_continuously_frame_stepping;
}
