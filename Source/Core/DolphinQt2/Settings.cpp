// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <QDir>
#include <QSize>

#include "Common/FileUtil.h"
#include "Core/ConfigManager.h"
#include "DolphinQt2/Settings.h"

static QString GetSettingsPath()
{
  return QString::fromStdString(File::GetUserPath(D_CONFIG_IDX)) + QStringLiteral("/UI.ini");
}

Settings::Settings(QObject* parent) : QSettings(GetSettingsPath(), QSettings::IniFormat, parent)
{
}

QString Settings::GetThemeDir() const
{
  QString theme_name = value(QStringLiteral("Theme"), QStringLiteral("Clean")).toString();
  return QString::fromStdString(File::GetThemeDir(theme_name.toStdString()));
}

QString Settings::GetResourcesDir() const
{
  return QString::fromStdString(File::GetSysDirectory().append("Resources"))
      .append(QDir::separator());
}

bool Settings::IsInDevelopmentWarningEnabled() const
{
  // There's intentionally no way to set this from the UI.
  // Add it to your INI manually instead.
  return value(QStringLiteral("ShowDevelopmentWarning"), true).toBool();
}

QString Settings::GetLastGame() const
{
  return value(QStringLiteral("GameList/LastGame")).toString();
}

void Settings::SetLastGame(const QString& path)
{
  setValue(QStringLiteral("GameList/LastGame"), path);
}

QStringList Settings::GetPaths() const
{
  return value(QStringLiteral("GameList/Paths")).toStringList();
}

void Settings::SetPaths(const QStringList& paths)
{
  setValue(QStringLiteral("GameList/Paths"), paths);
}

void Settings::RemovePath(int i)
{
  QStringList paths = GetPaths();
  paths.removeAt(i);
  SetPaths(paths);
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

void Settings::Save()
{
  return SConfig::GetInstance().SaveSettings();
}
