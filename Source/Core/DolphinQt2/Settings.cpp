// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <QDir>
#include <QSize>

#include "AudioCommon/AudioCommon.h"
#include "Common/FileSearch.h"
#include "Common/FileUtil.h"
#include "Common/StringUtil.h"
#include "Core/ConfigManager.h"
#include "DolphinQt2/Settings.h"
#include "InputCommon/InputConfig.h"

static QString GetSettingsPath()
{
  return QString::fromStdString(File::GetUserPath(D_CONFIG_IDX)) + QStringLiteral("/UI.ini");
}

Settings::Settings() : QSettings(GetSettingsPath(), QSettings::IniFormat)
{
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

QString Settings::GetThemeDir() const
{
  return QString::fromStdString(File::GetThemeDir(SConfig::GetInstance().theme_name));
}

QString Settings::GetResourcesDir() const
{
  return QString::fromStdString(File::GetSysDirectory().append("Resources"))
      .append(QDir::separator());
}

QString Settings::GetProfilesDir() const
{
  return QString::fromStdString(File::GetUserPath(D_CONFIG_IDX) + "Profiles/");
}

QString Settings::GetProfileINIPath(const InputConfig* config, const QString& name) const
{
  return GetProfilesDir() + QString::fromStdString(config->GetProfileName()) + QDir::separator() +
         name + QStringLiteral(".ini");
}

bool Settings::IsInDevelopmentWarningEnabled() const
{
  // There's intentionally no way to set this from the UI.
  // Add it to your INI manually instead.
  return value(QStringLiteral("ShowDevelopmentWarning"), true).toBool();
}

QStringList Settings::GetPaths() const
{
  return value(QStringLiteral("GameList/Paths")).toStringList();
}

void Settings::AddPath(const QString& path)
{
  QStringList game_folders = Settings::Instance().GetPaths();
  if (!game_folders.contains(path))
  {
    game_folders << path;
    Settings::Instance().SetPaths(game_folders);
    emit PathAdded(path);
  }
}

void Settings::SetPaths(const QStringList& paths)
{
  setValue(QStringLiteral("GameList/Paths"), paths);
}

void Settings::RemovePath(const QString& path)
{
  QStringList paths = GetPaths();
  int i = paths.indexOf(path);
  if (i < 0)
    return;

  paths.removeAt(i);
  SetPaths(paths);
  emit PathRemoved(path);
}

QString Settings::GetDefaultGame() const
{
  return QString::fromStdString(SConfig::GetInstance().m_strDefaultISO);
}

void Settings::SetDefaultGame(const QString& path)
{
  SConfig::GetInstance().m_strDefaultISO = path.toStdString();
  SConfig::GetInstance().SaveSettings();
}

QString Settings::GetDVDRoot() const
{
  return QString::fromStdString(SConfig::GetInstance().m_strDVDRoot);
}

void Settings::SetDVDRoot(const QString& path)
{
  SConfig::GetInstance().m_strDVDRoot = path.toStdString();
  SConfig::GetInstance().SaveSettings();
}

QString Settings::GetApploader() const
{
  return QString::fromStdString(SConfig::GetInstance().m_strApploader);
}

void Settings::SetApploader(const QString& path)
{
  SConfig::GetInstance().m_strApploader = path.toStdString();
  SConfig::GetInstance().SaveSettings();
}

QString Settings::GetWiiNAND() const
{
  return QString::fromStdString(SConfig::GetInstance().m_NANDPath);
}

void Settings::SetWiiNAND(const QString& path)
{
  SConfig::GetInstance().m_NANDPath = path.toStdString();
  SConfig::GetInstance().SaveSettings();
}

float Settings::GetEmulationSpeed() const
{
  return SConfig::GetInstance().m_EmulationSpeed;
}

void Settings::SetEmulationSpeed(float val)
{
  SConfig::GetInstance().m_EmulationSpeed = val;
}

bool Settings::GetForceNTSCJ() const
{
  return SConfig::GetInstance().bForceNTSCJ;
}

void Settings::SetForceNTSCJ(bool val)
{
  SConfig::GetInstance().bForceNTSCJ = val;
}

bool Settings::GetAnalyticsEnabled() const
{
  return SConfig::GetInstance().m_analytics_enabled;
}

void Settings::SetAnalyticsEnabled(bool val)
{
  SConfig::GetInstance().m_analytics_enabled = val;
}

DiscIO::Language Settings::GetWiiSystemLanguage() const
{
  return SConfig::GetInstance().GetCurrentLanguage(true);
}

DiscIO::Language Settings::GetGCSystemLanguage() const
{
  return SConfig::GetInstance().GetCurrentLanguage(false);
}

bool Settings::GetPreferredView() const
{
  return value(QStringLiteral("PreferredView"), true).toBool();
}

void Settings::SetPreferredView(bool table)
{
  setValue(QStringLiteral("PreferredView"), table);
}

bool Settings::GetConfirmStop() const
{
  return value(QStringLiteral("Emulation/ConfirmStop"), true).toBool();
}

int Settings::GetStateSlot() const
{
  return value(QStringLiteral("Emulation/StateSlot"), 1).toInt();
}

void Settings::SetStateSlot(int slot)
{
  setValue(QStringLiteral("Emulation/StateSlot"), slot);
}

bool Settings::GetRenderToMain() const
{
  return value(QStringLiteral("Graphics/RenderToMain"), false).toBool();
}

bool Settings::GetFullScreen() const
{
  return value(QStringLiteral("Graphics/FullScreen"), false).toBool();
}

QSize Settings::GetRenderWindowSize() const
{
  return value(QStringLiteral("Graphics/RenderWindowSize"), QSize(640, 480)).toSize();
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

bool& Settings::BannerVisible() const
{
  return SConfig::GetInstance().m_showBannerColumn;
}

bool& Settings::CountryVisible() const
{
  return SConfig::GetInstance().m_showRegionColumn;
}

bool& Settings::DescriptionVisible() const
{
  return SConfig::GetInstance().m_showDescriptionColumn;
}

bool& Settings::FilenameVisible() const
{
  return SConfig::GetInstance().m_showFileNameColumn;
}

bool& Settings::IDVisible() const
{
  return SConfig::GetInstance().m_showIDColumn;
}

bool& Settings::MakerVisible() const
{
  return SConfig::GetInstance().m_showMakerColumn;
}

bool& Settings::PlatformVisible() const
{
  return SConfig::GetInstance().m_showSystemColumn;
}

bool& Settings::TitleVisible() const
{
  return SConfig::GetInstance().m_showTitleColumn;
}

bool& Settings::SizeVisible() const
{
  return SConfig::GetInstance().m_showSizeColumn;
}

bool& Settings::StateVisible() const
{
  return SConfig::GetInstance().m_showStateColumn;
}

bool Settings::IsBluetoothPassthroughEnabled() const
{
  return SConfig::GetInstance().m_bt_passthrough_enabled;
}

void Settings::SetBluetoothPassthroughEnabled(bool enabled)
{
  SConfig::GetInstance().m_bt_passthrough_enabled = enabled;
}

bool Settings::IsContinuousScanningEnabled() const
{
  return SConfig::GetInstance().m_WiimoteContinuousScanning;
}

void Settings::SetContinuousScanningEnabled(bool enabled)
{
  SConfig::GetInstance().m_WiimoteContinuousScanning = enabled;
}

bool Settings::IsBackgroundInputEnabled() const
{
  return SConfig::GetInstance().m_BackgroundInput;
}

void Settings::SetBackgroundInputEnabled(bool enabled)
{
  SConfig::GetInstance().m_BackgroundInput = enabled;
}

bool Settings::IsWiimoteSpeakerEnabled() const
{
  return SConfig::GetInstance().m_WiimoteEnableSpeaker;
}

void Settings::SetWiimoteSpeakerEnabled(bool enabled)
{
  SConfig::GetInstance().m_WiimoteEnableSpeaker = enabled;
}

SerialInterface::SIDevices Settings::GetSIDevice(size_t i) const
{
  return SConfig::GetInstance().m_SIDevice[i];
}

void Settings::SetSIDevice(size_t i, SerialInterface::SIDevices device)
{
  SConfig::GetInstance().m_SIDevice[i] = device;
}

bool Settings::IsWiiGameRunning() const
{
  return SConfig::GetInstance().bWii;
}

bool Settings::IsGCAdapterRumbleEnabled(int port) const
{
  return SConfig::GetInstance().m_AdapterRumble[port];
}

void Settings::SetGCAdapterRumbleEnabled(int port, bool enabled)
{
  SConfig::GetInstance().m_AdapterRumble[port] = enabled;
}

bool Settings::IsGCAdapterSimulatingDKBongos(int port) const
{
  return SConfig::GetInstance().m_AdapterKonga[port];
}

void Settings::SetGCAdapterSimulatingDKBongos(int port, bool enabled)
{
  SConfig::GetInstance().m_AdapterKonga[port] = enabled;
}

void Settings::Save()
{
  return SConfig::GetInstance().SaveSettings();
}

QVector<QString> Settings::GetProfiles(const InputConfig* config) const
{
  const std::string path = GetProfilesDir().toStdString() + config->GetProfileName();
  QVector<QString> vec;

  for (const auto& file : Common::DoFileSearch({path}, {".ini"}))
  {
    std::string basename;
    SplitPath(file, nullptr, &basename, nullptr);
    vec.push_back(QString::fromStdString(basename));
  }

  return vec;
}

bool Settings::HasAskedForAnalyticsPermission() const
{
  return SConfig::GetInstance().m_analytics_permission_asked;
}

void Settings::SetAskedForAnalyticsPermission(bool value)
{
  SConfig::GetInstance().m_analytics_permission_asked = value;
}
