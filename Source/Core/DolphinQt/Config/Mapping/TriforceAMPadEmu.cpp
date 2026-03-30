// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/Config/Mapping/TriforceAMPadEmu.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QPushButton>
#include <QStackedWidget>
#include <QVBoxLayout>
#include <QWidget>

#include "Common/CommonPaths.h"
#include "Common/FileUtil.h"
#include "Common/IniFile.h"

#include "Core/HW/GCPad.h"
#include "Core/HW/PadGroups.h"

#include "DolphinQt/Config/Mapping/GCPadEmu.h"
#include "InputCommon/ControllerEmu/ControllerEmu.h"
#include "InputCommon/ControllerEmu/ControlGroup/ControlGroup.h"
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
    return 0;
  case TriforceAMPadEmu::GameFamily::VirtuaStriker:
    return 1;
  default:
    return 0;
  }
}

QString GetGameFamilyLabel(TriforceAMPadEmu::GameFamily game_family)
{
  switch (game_family)
  {
  case TriforceAMPadEmu::GameFamily::Auto:
    return TriforceAMPadEmu::tr("Auto");
  case TriforceAMPadEmu::GameFamily::GenericTriforce:
    return TriforceAMPadEmu::tr("Generic Triforce");
  case TriforceAMPadEmu::GameFamily::VirtuaStriker:
    return TriforceAMPadEmu::tr("Virtua Striker");
  default:
    return {};
  }
}

QString GetFamilyMismatchText(TriforceAMPadEmu::GameFamily detected_game_family,
                              TriforceAMPadEmu::GameFamily selected_game_family)
{
  return TriforceAMPadEmu::tr("The running game matches %1, but Game Mapping is set to %2.")
      .arg(GetGameFamilyLabel(detected_game_family), GetGameFamilyLabel(selected_game_family));
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

void SeedMissingGameFamilySections(Common::IniFile* family_sections)
{
  const Common::IniFile::Section* fallback = family_sections->GetSection(
      TriforcePadProfile::GetProfileSectionName(TriforceAMPadEmu::GameFamily::GenericTriforce));
  if (fallback == nullptr && !family_sections->GetSections().empty())
    fallback = &family_sections->GetSections().front();

  if (fallback == nullptr)
    return;

  for (const auto game_family : TriforcePadProfile::REAL_GAME_FAMILIES)
  {
    if (family_sections->Exists(TriforcePadProfile::GetProfileSectionName(game_family)))
      continue;

    *family_sections->GetOrCreateSection(TriforcePadProfile::GetProfileSectionName(game_family)) =
        CreateNamedSection(TriforcePadProfile::GetProfileSectionName(game_family), *fallback);
  }
}

}  // namespace

TriforceAMPadEmu::TriforceAMPadEmu(MappingWindow* window) : MappingWidget(window)
{
  m_family_box = new QGroupBox(tr("Game Mapping"));
  auto* const family_layout = new QHBoxLayout{m_family_box};
  m_family_combo = new QComboBox{m_family_box};
  for (const GameFamily game_family :
       {GameFamily::Auto, GameFamily::GenericTriforce, GameFamily::VirtuaStriker})
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
  GameFamily game_family = GetSelectedGameFamily();
  if (!ResolveGameFamilyMismatchOnClose(&game_family))
    return false;

  PersistSettings(game_family);
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
  m_family_stack->addWidget(CreateVirtuaStrikerWidget());
  main_layout->addWidget(m_family_stack);
}

QWidget* TriforceAMPadEmu::CreateGenericTriforceWidget()
{
  // Keep the stock AM baseboard page intact for the generic fallback layout.
  return CreateAMBaseboardMappingWidget(GetParent());
}

QWidget* TriforceAMPadEmu::CreateVirtuaStrikerWidget()
{
  auto* const widget = new QWidget(this);
  auto* const layout = new QGridLayout(widget);

  layout->addWidget(
      CreateAliasedControlsBox(
          tr("Buttons"), Pad::GetGroup(GetPort(), PadGroup::Buttons),
          {{"Short Pass", 0}, {"Shoot", 1}, {"Long Cross", 2}, {"Dash", 3}, {"Start", 5}}),
      0, 0);
  layout->addWidget(
      CreateAliasedControlsBox(tr("Tactics"), Pad::GetGroup(GetPort(), PadGroup::DPad),
                               {{"Tactics (U)", 2}, {"Tactics (M)", 0}, {"Tactics (D)", 3}}),
      1, 0);
  layout->addWidget(CreateGroupBox(tr("Triforce"), Pad::GetGroup(GetPort(), PadGroup::Triforce)), 2,
                    0);
  layout->addWidget(
      CreateGroupBox(tr("Control Stick"), Pad::GetGroup(GetPort(), PadGroup::MainStick)), 0, 1, 3,
      1);
  layout->addWidget(CreateGroupBox(tr("Options"), Pad::GetGroup(GetPort(), PadGroup::Options)), 2,
                    2);

  return widget;
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
  {
    SeedGameFamilySectionsFromCurrentConfig();
    return;
  }

  SeedMissingGameFamilySections(&m_family_sections);
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

QGroupBox* TriforceAMPadEmu::CreateAliasedControlsBox(const QString& name,
                                                      ControllerEmu::ControlGroup* group,
                                                      std::initializer_list<ControlAlias> controls)
{
  auto* const group_box = new QGroupBox(name);
  auto* const layout = new QFormLayout(group_box);

  for (const ControlAlias control : controls)
    CreateControl(tr(control.label), group->controls[control.index].get(), layout, true);

  return group_box;
}

bool TriforceAMPadEmu::ResolveGameFamilyMismatchOnClose(GameFamily* game_family)
{
  if (*game_family == GameFamily::Auto)
    return true;

  const std::optional<GameFamily> detected_game_family =
      TriforcePadProfile::DetectRunningGameFamily();
  if (!detected_game_family.has_value() || detected_game_family == *game_family)
    return true;

  QMessageBox prompt(this);
  prompt.setIcon(QMessageBox::Warning);
  prompt.setWindowTitle(tr("Game Mapping Mismatch"));
  prompt.setText(GetFamilyMismatchText(*detected_game_family, *game_family));

  QCheckBox* const switch_to_auto = new QCheckBox(tr("Switch to Auto"), &prompt);
  prompt.setCheckBox(switch_to_auto);

  QPushButton* const use_detected = prompt.addButton(tr("Use Detected"), QMessageBox::AcceptRole);
  QPushButton* const keep_manual =
      prompt.addButton(tr("Keep Manual"), QMessageBox::DestructiveRole);
  QPushButton* const keep_editing = prompt.addButton(tr("Keep Editing"), QMessageBox::RejectRole);
  Q_UNUSED(keep_manual);

  prompt.exec();

  if (prompt.clickedButton() == keep_editing)
    return false;

  if (prompt.clickedButton() == use_detected)
  {
    *game_family = switch_to_auto->isChecked() ? GameFamily::Auto : *detected_game_family;
  }

  return true;
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
  return GetGameFamilyLabel(game_family);
}

std::string TriforceAMPadEmu::GetProfileDirectoryPath(bool user)
{
  const std::string& base_path = user ? File::GetUserPath(D_CONFIG_IDX) : File::GetSysDirectory();
  return base_path + "Profiles/" + TRIFORCE_PROFILE_DIRECTORY + "/";
}
