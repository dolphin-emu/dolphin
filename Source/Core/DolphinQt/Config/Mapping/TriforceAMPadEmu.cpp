// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/Config/Mapping/TriforceAMPadEmu.h"

#include <QComboBox>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QStackedWidget>
#include <QVBoxLayout>
#include <QWidget>

#include "Common/CommonPaths.h"
#include "Common/FileUtil.h"
#include "Common/IniFile.h"

#include "Core/HW/GCPad.h"

#include "DolphinQt/Config/Mapping/GCPadEmu.h"
#include "InputCommon/ControllerEmu/ControllerEmu.h"
#include "InputCommon/InputConfig.h"

namespace
{
constexpr const char* TRIFORCE_PROFILE_DIRECTORY = "TriforcePad";
constexpr std::string_view TRIFORCE_CONFIG_SECTION_SUFFIX = ".TriforcePad";

int GetGameFamilyStackIndex(TriforceAMPadEmu::GameFamily game_family)
{
  switch (game_family)
  {
  case TriforceAMPadEmu::GameFamily::Auto:
  case TriforceAMPadEmu::GameFamily::GenericTriforce:
  default:
    return 0;
  }
}

Common::IniFile::Section CreateNamedSection(const std::string& name,
                                            const Common::IniFile::Section& source)
{
  Common::IniFile::Section section(name);
  for (const auto& [key, value] : source.GetValues())
    section.Set(key, value);

  if (source.HasLines())
  {
    std::vector<std::string> lines;
    if (source.GetLines(&lines, false))
      section.SetLines(std::move(lines));
  }

  return section;
}

std::string GetConfigPath(const InputConfig& config)
{
  return File::GetUserPath(D_CONFIG_IDX) + config.GetIniName() + ".ini";
}

std::string GetConfigSectionPrefix(std::string_view controller_name)
{
  std::string section_name(controller_name);
  section_name += TRIFORCE_CONFIG_SECTION_SUFFIX;
  return section_name;
}

std::string GetConfigMetadataSectionName(std::string_view controller_name)
{
  return GetConfigSectionPrefix(controller_name);
}

std::string GetConfigFamilySectionName(std::string_view controller_name,
                                       TriforceAMPadEmu::GameFamily game_family)
{
  std::string section_name = GetConfigSectionPrefix(controller_name);
  section_name += '.';
  section_name += TriforcePadProfile::GetGameFamilyName(game_family);
  return section_name;
}

}  // namespace

TriforceAMPadEmu::TriforceAMPadEmu(MappingWindow* window) : MappingWidget(window)
{
  m_family_box = new QGroupBox(tr("Game Mapping"));
  auto* const family_layout = new QHBoxLayout{m_family_box};
  m_family_combo = new QComboBox{m_family_box};
  for (const GameFamily game_family : {GameFamily::Auto, GameFamily::GenericTriforce})
  {
    m_family_combo->addItem(GetGameFamilyDisplayName(game_family), static_cast<int>(game_family));
  }
  family_layout->addWidget(m_family_combo);

  CreateMainLayout();

  connect(m_family_combo, &QComboBox::currentIndexChanged, this,
          &TriforceAMPadEmu::OnGameFamilyChanged);
}

InputConfig* TriforceAMPadEmu::GetConfig()
{
  return Pad::GetConfig();
}

QWidget* TriforceAMPadEmu::GetExtraWidget() const
{
  return m_family_box;
}

std::string TriforceAMPadEmu::GetUserProfileDirectoryPath()
{
  return GetProfileDirectoryPath(true);
}

std::string TriforceAMPadEmu::GetSysProfileDirectoryPath()
{
  return GetProfileDirectoryPath(false);
}

void TriforceAMPadEmu::LoadProfile(const Common::IniFile& ini)
{
  LoadGameFamilySections(ini, true);

  const GameFamily game_family =
      TriforcePadProfile::GetSavedGameFamily(ini, GetDefaultGameFamilySelection());
  SetSelectedGameFamily(game_family);
  ApplySelectedGameFamily();
}

void TriforceAMPadEmu::SaveProfile(Common::IniFile* ini)
{
  CaptureCurrentGameFamilyConfig();

  Common::IniFile::Section* const profile_section =
      ini->GetOrCreateSection(TriforcePadProfile::PROFILE_SECTION);
  profile_section->Set(std::string(TriforcePadProfile::GAME_MAPPING_KEY),
                       std::string(TriforcePadProfile::GetGameFamilyName(GetSelectedGameFamily())));

  // TriforcePad profiles keep one real binding set per family so the dropdown swaps
  // controller state, not just display labels.
  for (const GameFamily game_family : TriforcePadProfile::REAL_GAME_FAMILIES)
  {
    const Common::IniFile::Section* const section = GetGameFamilySection(game_family);
    if (section == nullptr)
      continue;

    *ini->GetOrCreateSection(TriforcePadProfile::GetProfileSectionName(game_family)) =
        CreateNamedSection(TriforcePadProfile::GetProfileSectionName(game_family), *section);
  }
}

void TriforceAMPadEmu::LoadSettings()
{
  Pad::LoadConfig();
  LoadStoredState();
}

void TriforceAMPadEmu::SaveSettings()
{
  CaptureCurrentGameFamilyConfig();
  PersistSettings(GetSelectedGameFamily());
}

bool TriforceAMPadEmu::OnDialogClosing()
{
  CaptureCurrentGameFamilyConfig();
  PersistSettings(GetSelectedGameFamily());
  return true;
}

void TriforceAMPadEmu::PersistSettings(GameFamily game_family)
{
  if (game_family != GetSelectedGameFamily())
    SetSelectedGameFamily(game_family);

  const GameFamily effective_game_family = TriforcePadProfile::GetEffectiveGameFamily(game_family);
  if (!m_has_loaded_game_family || effective_game_family != m_loaded_game_family)
    LoadGameFamilyConfig(effective_game_family);

  Pad::GetConfig()->SaveConfig();
  SaveStoredState();
}

void TriforceAMPadEmu::LoadStoredState()
{
  Common::IniFile ini;
  ini.Load(GetConfigPath(*GetConfig()));

  const std::string controller_name = GetController()->GetName();
  bool has_saved_sections = false;
  for (const GameFamily game_family : TriforcePadProfile::REAL_GAME_FAMILIES)
  {
    if (ini.Exists(GetConfigFamilySectionName(controller_name, game_family)))
    {
      has_saved_sections = true;
      break;
    }
  }

  if (has_saved_sections)
  {
    LoadGameFamilySections(ini, false);
  }
  else
  {
    // Older configs only have the flat GC pad section. Seed every Triforce family
    // from that state so existing setups migrate without losing their current bindings.
    SeedGameFamilySectionsFromCurrentConfig();
  }

  GameFamily game_family = GetDefaultGameFamilySelection();
  if (const Common::IniFile::Section* const metadata =
          ini.GetSection(GetConfigMetadataSectionName(controller_name)))
  {
    std::string stored_family;
    if (metadata->Get(TriforcePadProfile::GAME_MAPPING_KEY, &stored_family))
      game_family = TriforcePadProfile::ParseGameFamilyName(stored_family, game_family);
  }

  SetSelectedGameFamily(game_family);
  ApplySelectedGameFamily();
}

void TriforceAMPadEmu::CreateMainLayout()
{
  auto* const main_layout = new QVBoxLayout{this};
  m_family_stack = new QStackedWidget{this};
  m_family_stack->addWidget(CreateGenericTriforceWidget());
  main_layout->addWidget(m_family_stack);
}

QWidget* TriforceAMPadEmu::CreateGenericTriforceWidget()
{
  // Keep the stock AM baseboard page intact for the generic fallback layout.
  return CreateAMBaseboardMappingWidget(GetParent());
}

void TriforceAMPadEmu::ApplySelectedGameFamily()
{
  const GameFamily effective_game_family =
      TriforcePadProfile::GetEffectiveGameFamily(GetSelectedGameFamily());
  if (!m_has_loaded_game_family || m_loaded_game_family != effective_game_family)
    LoadGameFamilyConfig(effective_game_family);

  m_family_stack->setCurrentIndex(GetGameFamilyStackIndex(effective_game_family));
}

void TriforceAMPadEmu::CaptureCurrentGameFamilyConfig()
{
  if (!m_has_loaded_game_family)
    return;

  GetController()->SaveConfig(GetMutableGameFamilySection(m_loaded_game_family));
}

void TriforceAMPadEmu::LoadGameFamilyConfig(GameFamily game_family)
{
  const Common::IniFile::Section* const section = GetGameFamilySection(game_family);
  if (section == nullptr)
    return;

  Common::IniFile::Section copy = *section;
  GetController()->LoadConfig(&copy);
  GetController()->UpdateReferences(g_controller_interface);

  Common::IniFile texture_ini;
  *texture_ini.GetOrCreateSection(GetController()->GetName()) =
      CreateNamedSection(GetController()->GetName(), copy);
  GetController()->GetConfig()->GenerateControllerTextures(texture_ini);

  m_loaded_game_family = game_family;
  m_has_loaded_game_family = true;

  const auto lock = GetController()->GetStateLock();
  emit ConfigChanged();
}

void TriforceAMPadEmu::SaveStoredState()
{
  Common::IniFile ini;
  const std::string config_path = GetConfigPath(*GetConfig());
  ini.Load(config_path);

  const std::string controller_name = GetController()->GetName();
  Common::IniFile::Section* const metadata =
      ini.GetOrCreateSection(GetConfigMetadataSectionName(controller_name));
  metadata->Set(std::string(TriforcePadProfile::GAME_MAPPING_KEY),
                std::string(TriforcePadProfile::GetGameFamilyName(GetSelectedGameFamily())));

  for (const GameFamily game_family : TriforcePadProfile::REAL_GAME_FAMILIES)
  {
    const Common::IniFile::Section* const section = GetGameFamilySection(game_family);
    if (section == nullptr)
      continue;

    *ini.GetOrCreateSection(GetConfigFamilySectionName(controller_name, game_family)) =
        CreateNamedSection(GetConfigFamilySectionName(controller_name, game_family), *section);
  }

  ini.Save(config_path);
}

void TriforceAMPadEmu::SetSelectedGameFamily(GameFamily game_family)
{
  const QSignalBlocker blocker(m_family_combo);
  m_family_combo->setCurrentIndex(m_family_combo->findData(static_cast<int>(game_family)));
}

void TriforceAMPadEmu::LoadGameFamilySections(const Common::IniFile& ini, bool use_profile_sections)
{
  m_family_sections = Common::IniFile{};

  const Common::IniFile::Section* legacy_section = nullptr;
  if (use_profile_sections && !TriforcePadProfile::HasProfileSections(ini))
    legacy_section = ini.GetSection(TriforcePadProfile::PROFILE_SECTION);

  const std::string controller_name = GetController()->GetName();
  for (const GameFamily game_family : TriforcePadProfile::REAL_GAME_FAMILIES)
  {
    const Common::IniFile::Section* source = nullptr;
    if (use_profile_sections)
      source = ini.GetSection(TriforcePadProfile::GetProfileSectionName(game_family));
    else
      source = ini.GetSection(GetConfigFamilySectionName(controller_name, game_family));

    if (source == nullptr)
      source = legacy_section;

    if (source == nullptr)
      continue;

    *GetMutableGameFamilySection(game_family) =
        CreateNamedSection(TriforcePadProfile::GetProfileSectionName(game_family), *source);
  }

  if (m_family_sections.GetSections().empty())
    SeedGameFamilySectionsFromCurrentConfig();
}

void TriforceAMPadEmu::SeedGameFamilySectionsFromCurrentConfig()
{
  Common::IniFile::Section section;
  GetController()->SaveConfig(&section);

  m_family_sections = Common::IniFile{};
  for (const GameFamily game_family : TriforcePadProfile::REAL_GAME_FAMILIES)
    *GetMutableGameFamilySection(game_family) =
        CreateNamedSection(TriforcePadProfile::GetProfileSectionName(game_family), section);
}

Common::IniFile::Section* TriforceAMPadEmu::GetMutableGameFamilySection(GameFamily game_family)
{
  return m_family_sections.GetOrCreateSection(
      TriforcePadProfile::GetProfileSectionName(game_family));
}

const Common::IniFile::Section* TriforceAMPadEmu::GetGameFamilySection(GameFamily game_family) const
{
  return m_family_sections.GetSection(TriforcePadProfile::GetProfileSectionName(game_family));
}

void TriforceAMPadEmu::OnGameFamilyChanged(int)
{
  CaptureCurrentGameFamilyConfig();
  ApplySelectedGameFamily();
}

TriforceAMPadEmu::GameFamily TriforceAMPadEmu::GetSelectedGameFamily() const
{
  return static_cast<GameFamily>(m_family_combo->currentData().toInt());
}

TriforceAMPadEmu::GameFamily TriforceAMPadEmu::GetDefaultGameFamilySelection()
{
  return TriforcePadProfile::DetectRunningGameFamily().has_value() ? GameFamily::Auto :
                                                                     GameFamily::GenericTriforce;
}

QString TriforceAMPadEmu::GetGameFamilyDisplayName(GameFamily game_family)
{
  switch (game_family)
  {
  case GameFamily::Auto:
    return tr("Auto");
  case GameFamily::GenericTriforce:
    return tr("Generic Triforce");
  default:
    return {};
  }
}

std::string TriforceAMPadEmu::GetProfileDirectoryPath(bool user)
{
  const std::string& base_path = user ? File::GetUserPath(D_CONFIG_IDX) : File::GetSysDirectory();
  return base_path + "Profiles/" + TRIFORCE_PROFILE_DIRECTORY + "/";
}
