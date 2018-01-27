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

void Settings::SetThemeName(const QString& theme_name)
{
  SConfig::GetInstance().theme_name = theme_name.toStdString();
  emit ThemeChanged();
}

QString Settings::GetLastGame() const
{
	// TODO Carl: fix this
	//return value(QStringLiteral("GameList/LastGame")).toString();
	return QStringLiteral("");
}

void Settings::SetLastGame(const QString& path)
{
  // TODO Carl: fix this
  // setValue(QStringLiteral("GameList/LastGame"), path);
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

bool Settings::GetPreferredView() const
{
  return QSettings().value(QStringLiteral("PreferredView"), true).toBool();
}

void Settings::SetPreferredView(bool list)
{
  QSettings().setValue(QStringLiteral("PreferredView"), list);
}

int Settings::GetStateSlot() const
{
  return QSettings().value(QStringLiteral("Emulation/StateSlot"), 1).toInt();
}

void Settings::SetStateSlot(int slot)
{
  QSettings().setValue(QStringLiteral("Emulation/StateSlot"), slot);
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
  return QSettings().value(QStringLiteral("logging/logvisible")).toBool();
}

void Settings::SetLogVisible(bool visible)
{
  if (IsLogVisible() != visible)
  {
    QSettings().setValue(QStringLiteral("logging/logvisible"), visible);
    emit LogVisibilityChanged(visible);
  }
}

bool Settings::IsLogConfigVisible() const
{
  return QSettings().value(QStringLiteral("logging/logconfigvisible")).toBool();
}

void Settings::SetLogConfigVisible(bool visible)
{
  if (IsLogConfigVisible() != visible)
  {
    QSettings().setValue(QStringLiteral("logging/logconfigvisible"), visible);
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
