// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <QDir>
#include <QSettings>
#include <QSize>

#include "AudioCommon/AudioCommon.h"
#include "Common/Config/Config.h"
#include "Common/FileUtil.h"
#include "Common/StringUtil.h"
#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "DolphinQt2/GameList/GameListModel.h"
#include "DolphinQt2/QtUtils/QueueOnObject.h"
#include "DolphinQt2/Settings.h"
#include "InputCommon/InputConfig.h"

Settings::Settings()
{
  qRegisterMetaType<Core::State>();
  Core::SetOnStateChangedCallback(
      [this](Core::State new_state) { emit EmulationStateChanged(new_state); });

  Config::AddConfigChangedCallback(
      [this] { QueueOnObject(this, [this] { emit ConfigChanged(); }); });
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

void Settings::SetThemeName(const QString& theme_name)
{
  SConfig::GetInstance().theme_name = theme_name.toStdString();
  emit ThemeChanged();
}

QStringList Settings::GetPaths() const
{
  QStringList list;
  for (const auto& path : SConfig::GetInstance().m_ISOFolder)
    list << QString::fromStdString(path);
  return list;
}

void Settings::AddPath(const QString& qpath)
{
  std::string path = qpath.toStdString();

  std::vector<std::string>& paths = SConfig::GetInstance().m_ISOFolder;
  if (std::find(paths.begin(), paths.end(), path) != paths.end())
    return;

  paths.emplace_back(path);
  emit PathAdded(qpath);
}

void Settings::RemovePath(const QString& qpath)
{
  std::string path = qpath.toStdString();
  std::vector<std::string>& paths = SConfig::GetInstance().m_ISOFolder;

  auto new_end = std::remove(paths.begin(), paths.end(), path);
  if (new_end == paths.end())
    return;

  paths.erase(new_end, paths.end());
  emit PathRemoved(qpath);
}

QString Settings::GetDefaultGame() const
{
  return QString::fromStdString(SConfig::GetInstance().m_strDefaultISO);
}

void Settings::SetDefaultGame(QString path)
{
  if (GetDefaultGame() != path)
  {
    SConfig::GetInstance().m_strDefaultISO = path.toStdString();
    emit DefaultGameChanged(path);
  }
}

bool Settings::GetPreferredView() const
{
  return GetQSettings().value(QStringLiteral("PreferredView"), true).toBool();
}

void Settings::SetPreferredView(bool list)
{
  GetQSettings().setValue(QStringLiteral("PreferredView"), list);
}

int Settings::GetStateSlot() const
{
  return GetQSettings().value(QStringLiteral("Emulation/StateSlot"), 1).toInt();
}

void Settings::SetStateSlot(int slot)
{
  GetQSettings().setValue(QStringLiteral("Emulation/StateSlot"), slot);
}

void Settings::SetHideCursor(bool hide_cursor)
{
  SConfig::GetInstance().bHideCursor = hide_cursor;
  emit HideCursorChanged();
}

bool Settings::GetHideCursor() const
{
  return SConfig::GetInstance().bHideCursor;
}

int Settings::GetVolume() const
{
  return SConfig::GetInstance().m_Volume;
}

void Settings::SetVolume(int volume)
{
  if (GetVolume() != volume)
  {
    SConfig::GetInstance().m_Volume = volume;
    emit VolumeChanged(volume);
  }
}

void Settings::IncreaseVolume(int volume)
{
  AudioCommon::IncreaseVolume(volume);
  emit VolumeChanged(GetVolume());
}

void Settings::DecreaseVolume(int volume)
{
  AudioCommon::DecreaseVolume(volume);
  emit VolumeChanged(GetVolume());
}

bool Settings::IsLogVisible() const
{
  return GetQSettings().value(QStringLiteral("logging/logvisible")).toBool();
}

void Settings::SetLogVisible(bool visible)
{
  if (IsLogVisible() != visible)
  {
    GetQSettings().setValue(QStringLiteral("logging/logvisible"), visible);
    emit LogVisibilityChanged(visible);
  }
}

bool Settings::IsLogConfigVisible() const
{
  return GetQSettings().value(QStringLiteral("logging/logconfigvisible")).toBool();
}

void Settings::SetLogConfigVisible(bool visible)
{
  if (IsLogConfigVisible() != visible)
  {
    GetQSettings().setValue(QStringLiteral("logging/logconfigvisible"), visible);
    emit LogConfigVisibilityChanged(visible);
  }
}

GameListModel* Settings::GetGameListModel() const
{
  static GameListModel* model = new GameListModel;
  return model;
}

NetPlayClient* Settings::GetNetPlayClient()
{
  return m_client.get();
}

void Settings::ResetNetPlayClient(NetPlayClient* client)
{
  m_client.reset(client);
}

NetPlayServer* Settings::GetNetPlayServer()
{
  return m_server.get();
}

void Settings::ResetNetPlayServer(NetPlayServer* server)
{
  m_server.reset(server);
}

bool Settings::GetCheatsEnabled() const
{
  return SConfig::GetInstance().bEnableCheats;
}

void Settings::SetCheatsEnabled(bool enabled)
{
  if (SConfig::GetInstance().bEnableCheats != enabled)
  {
    SConfig::GetInstance().bEnableCheats = enabled;
    emit EnableCheatsChanged(enabled);
  }
}

void Settings::SetDebugModeEnabled(bool enabled)
{
  if (IsDebugModeEnabled() != enabled)
  {
    SConfig::GetInstance().bEnableDebugging = enabled;
    emit DebugModeToggled(enabled);
  }
  if (enabled)
    SetCodeVisible(true);
}

bool Settings::IsDebugModeEnabled() const
{
  return SConfig::GetInstance().bEnableDebugging;
}

void Settings::SetRegistersVisible(bool enabled)
{
  if (IsRegistersVisible() != enabled)
  {
    GetQSettings().setValue(QStringLiteral("debugger/showregisters"), enabled);

    emit RegistersVisibilityChanged(enabled);
  }
}

bool Settings::IsRegistersVisible() const
{
  return GetQSettings().value(QStringLiteral("debugger/showregisters")).toBool();
}

void Settings::SetWatchVisible(bool enabled)
{
  if (IsWatchVisible() != enabled)
  {
    GetQSettings().setValue(QStringLiteral("debugger/showwatch"), enabled);

    emit WatchVisibilityChanged(enabled);
  }
}

bool Settings::IsWatchVisible() const
{
  return GetQSettings().value(QStringLiteral("debugger/showwatch")).toBool();
}

void Settings::SetBreakpointsVisible(bool enabled)
{
  if (IsBreakpointsVisible() != enabled)
  {
    GetQSettings().setValue(QStringLiteral("debugger/showbreakpoints"), enabled);

    emit BreakpointsVisibilityChanged(enabled);
  }
}

bool Settings::IsBreakpointsVisible() const
{
  return GetQSettings().value(QStringLiteral("debugger/showbreakpoints")).toBool();
}

bool Settings::IsControllerStateNeeded() const
{
  return m_controller_state_needed;
}

void Settings::SetControllerStateNeeded(bool needed)
{
  m_controller_state_needed = needed;
}

void Settings::SetCodeVisible(bool enabled)
{
  if (IsCodeVisible() != enabled)
  {
    GetQSettings().setValue(QStringLiteral("debugger/showcode"), enabled);

    emit CodeVisibilityChanged(enabled);
  }
}

bool Settings::IsCodeVisible() const
{
  return GetQSettings().value(QStringLiteral("debugger/showcode")).toBool();
}

void Settings::SetDebugFont(QFont font)
{
  if (GetDebugFont() != font)
  {
    GetQSettings().setValue(QStringLiteral("debugger/font"), font);

    emit DebugFontChanged(font);
  }
}

QFont Settings::GetDebugFont() const
{
  QFont default_font = QFont(QStringLiteral("Monospace"));
  default_font.setStyleHint(QFont::TypeWriter);

  return GetQSettings().value(QStringLiteral("debugger/font"), default_font).value<QFont>();
}

void Settings::SetAutoUpdateTrack(const QString& mode)
{
  if (mode == GetAutoUpdateTrack())
    return;

  SConfig::GetInstance().m_auto_update_track = mode.toStdString();

  emit AutoUpdateTrackChanged(mode);
}

QString Settings::GetAutoUpdateTrack() const
{
  return QString::fromStdString(SConfig::GetInstance().m_auto_update_track);
}

void Settings::SetAnalyticsEnabled(bool enabled)
{
  if (enabled == IsAnalyticsEnabled())
    return;

  SConfig::GetInstance().m_analytics_enabled = enabled;

  emit AnalyticsToggled(enabled);
}

bool Settings::IsAnalyticsEnabled() const
{
  return SConfig::GetInstance().m_analytics_enabled;
}
