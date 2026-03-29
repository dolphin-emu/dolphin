// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <string>

#include "Common/IniFile.h"
#include "DolphinQt/Config/Mapping/MappingWidget.h"
#include "InputCommon/TriforcePadProfile.h"

class QComboBox;
class QGroupBox;
class QStackedWidget;
class QWidget;

class TriforceAMPadEmu final : public MappingWidget
{
  Q_OBJECT
public:
  using GameFamily = TriforcePadProfile::GameFamily;

  explicit TriforceAMPadEmu(MappingWindow* window);

  InputConfig* GetConfig() override;
  QWidget* GetExtraWidget() const override;
  std::string GetUserProfileDirectoryPath() override;
  std::string GetSysProfileDirectoryPath() override;
  void LoadProfile(const Common::IniFile& ini) override;
  void SaveProfile(Common::IniFile* ini) override;
  bool OnDialogClosing() override;

private:
  void LoadSettings() override;
  void SaveSettings() override;

  void CreateMainLayout();
  QWidget* CreateGenericTriforceWidget();
  void PersistSettings(GameFamily game_family);
  void ApplySelectedGameFamily();
  void CaptureCurrentGameFamilyConfig();
  void LoadGameFamilyConfig(GameFamily game_family);
  void LoadStoredState();
  void SaveStoredState();
  void SetSelectedGameFamily(GameFamily game_family);
  void LoadGameFamilySections(const Common::IniFile& ini, bool use_profile_sections);
  void SeedGameFamilySectionsFromCurrentConfig();
  Common::IniFile::Section* GetMutableGameFamilySection(GameFamily game_family);
  const Common::IniFile::Section* GetGameFamilySection(GameFamily game_family) const;

  void OnGameFamilyChanged(int index);
  GameFamily GetSelectedGameFamily() const;
  static GameFamily GetDefaultGameFamilySelection();

  static QString GetGameFamilyDisplayName(GameFamily game_family);
  static std::string GetProfileDirectoryPath(bool user);

  QGroupBox* m_family_box = nullptr;
  QComboBox* m_family_combo = nullptr;
  QStackedWidget* m_family_stack = nullptr;
  Common::IniFile m_family_sections;
  GameFamily m_loaded_game_family = GameFamily::GenericTriforce;
  bool m_has_loaded_game_family = false;
};
