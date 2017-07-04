// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <QSettings>
#include <QVector>
#include <wobjectdefs.h>

#include "Common/NonCopyable.h"
#include "Core/HW/SI/SI.h"

namespace DiscIO
{
enum class Language;
}

class InputConfig;

// UI settings to be stored in the config directory.
class Settings final : public QSettings, NonCopyable
{
  W_OBJECT(Settings)

public:
  static Settings& Instance();

  // UI
  void SetThemeName(const QString& theme_name);
  QString GetThemeDir() const;
  QString GetResourcesDir() const;
  QString GetProfilesDir() const;
  QVector<QString> GetProfiles(const InputConfig* config) const;
  QString GetProfileINIPath(const InputConfig* config, const QString& name) const;
  bool IsInDevelopmentWarningEnabled() const;

  // GameList
  QStringList GetPaths() const;
  void AddPath(const QString& path);
  void SetPaths(const QStringList& paths);
  void RemovePath(const QString& path);
  QString GetDefaultGame() const;
  void SetDefaultGame(const QString& path);
  QString GetDVDRoot() const;
  void SetDVDRoot(const QString& path);
  QString GetApploader() const;
  void SetApploader(const QString& path);
  QString GetWiiNAND() const;
  void SetWiiNAND(const QString& path);
  DiscIO::Language GetWiiSystemLanguage() const;
  DiscIO::Language GetGCSystemLanguage() const;
  bool GetPreferredView() const;
  void SetPreferredView(bool table);

  // Emulation
  bool GetConfirmStop() const;
  bool IsWiiGameRunning() const;
  int GetStateSlot() const;
  void SetStateSlot(int);
  float GetEmulationSpeed() const;
  void SetEmulationSpeed(float val);
  bool GetForceNTSCJ() const;
  void SetForceNTSCJ(bool val);

  // Analytics
  bool HasAskedForAnalyticsPermission() const;
  void SetAskedForAnalyticsPermission(bool value);
  bool GetAnalyticsEnabled() const;
  void SetAnalyticsEnabled(bool val);

  // Graphics
  bool GetRenderToMain() const;
  bool GetFullScreen() const;
  QSize GetRenderWindowSize() const;
  void SetHideCursor(bool hide_cursor);
  bool GetHideCursor() const;

  // Audio
  int GetVolume() const;
  void SetVolume(int volume);
  void IncreaseVolume(int volume);
  void DecreaseVolume(int volume);

  // Columns
  bool& BannerVisible() const;
  bool& CountryVisible() const;
  bool& DescriptionVisible() const;
  bool& FilenameVisible() const;
  bool& IDVisible() const;
  bool& PlatformVisible() const;
  bool& MakerVisible() const;
  bool& SizeVisible() const;
  bool& StateVisible() const;
  bool& TitleVisible() const;

  // Input
  bool IsWiimoteSpeakerEnabled() const;
  void SetWiimoteSpeakerEnabled(bool enabled);

  bool IsBackgroundInputEnabled() const;
  void SetBackgroundInputEnabled(bool enabled);

  bool IsBluetoothPassthroughEnabled() const;
  void SetBluetoothPassthroughEnabled(bool enabled);

  SerialInterface::SIDevices GetSIDevice(size_t i) const;
  void SetSIDevice(size_t i, SerialInterface::SIDevices device);

  bool IsContinuousScanningEnabled() const;
  void SetContinuousScanningEnabled(bool enabled);

  bool IsGCAdapterRumbleEnabled(int port) const;
  void SetGCAdapterRumbleEnabled(int port, bool enabled);

  bool IsGCAdapterSimulatingDKBongos(int port) const;
  void SetGCAdapterSimulatingDKBongos(int port, bool enabled);

  void Save();

  void ThemeChanged() W_SIGNAL(ThemeChanged);
  void PathAdded(const QString& path) W_SIGNAL(PathAdded, (const QString&), path);
  void PathRemoved(const QString& path) W_SIGNAL(PathRemoved, (const QString&), path);
  void HideCursorChanged() W_SIGNAL(HideCursorChanged);
  void VolumeChanged(int volume) W_SIGNAL(VolumeChanged, (int), volume);

private:
  Settings();
};
