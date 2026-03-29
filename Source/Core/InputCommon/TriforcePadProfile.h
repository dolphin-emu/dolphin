// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>
#include <optional>
#include <string>
#include <string_view>

#include "Common/IniFile.h"
#include "Core/Core.h"
#include "Core/System.h"

namespace TriforcePadProfile
{
// Shared Triforce game-family/profile helpers used by the Qt editor, runtime profile loading,
// and game-INI profile expansion. Keeping the game-family selection logic here makes all three
// paths pick the same game-family section from a TriforcePad profile.
// These game families are broader than any eventual game-specific classification and also carry
// UI-only values like Auto and GenericTriforce.
enum class GameFamily
{
  Auto,
  GenericTriforce,
};

constexpr std::string_view PROFILE_SECTION = "Profile";
constexpr std::string_view GAME_MAPPING_KEY = "GameMapping";
constexpr std::array<GameFamily, 1> REAL_GAME_FAMILIES = {GameFamily::GenericTriforce};

inline std::string_view GetGameFamilyName(GameFamily game_family)
{
  switch (game_family)
  {
  case GameFamily::Auto:
    return "Auto";
  case GameFamily::GenericTriforce:
    return "GenericTriforce";
  }

  return "Auto";
}

inline GameFamily ParseGameFamilyName(std::string_view name, GameFamily fallback = GameFamily::Auto)
{
  for (const GameFamily game_family : REAL_GAME_FAMILIES)
  {
    if (name == GetGameFamilyName(game_family))
      return game_family;
  }

  return name == GetGameFamilyName(GameFamily::Auto) ? GameFamily::Auto : fallback;
}

inline std::string GetProfileSectionName(GameFamily game_family)
{
  std::string section_name(PROFILE_SECTION);
  section_name += '.';
  section_name += GetGameFamilyName(game_family);
  return section_name;
}

inline std::optional<GameFamily> DetectRunningGameFamily()
{
  Core::System& system = Core::System::GetInstance();
  if (Core::GetState(system) == Core::State::Uninitialized || !system.IsTriforce())
    return std::nullopt;

  return GameFamily::GenericTriforce;
}

inline GameFamily GetEffectiveGameFamily(GameFamily game_family)
{
  if (game_family != GameFamily::Auto)
    return game_family;

  return DetectRunningGameFamily().value_or(GameFamily::GenericTriforce);
}

inline GameFamily GetSavedGameFamily(const Common::IniFile& ini, GameFamily fallback)
{
  const Common::IniFile::Section* const profile = ini.GetSection(PROFILE_SECTION);
  if (profile == nullptr)
    return fallback;

  std::string stored_family;
  if (!profile->Get(GAME_MAPPING_KEY, &stored_family))
    return fallback;

  return ParseGameFamilyName(stored_family, fallback);
}

inline bool HasProfileSections(const Common::IniFile& ini)
{
  for (const GameFamily game_family : REAL_GAME_FAMILIES)
  {
    if (ini.Exists(GetProfileSectionName(game_family)))
      return true;
  }

  return false;
}

inline const Common::IniFile::Section* GetEffectiveProfileSection(const Common::IniFile& ini)
{
  if (!HasProfileSections(ini))
    return ini.GetSection(PROFILE_SECTION);

  // Family-aware TriforcePad profiles keep one section per real game family and a
  // separate GameMapping selector that decides which one should be active.
  const GameFamily selected_game_family = GetSavedGameFamily(ini, GameFamily::Auto);
  const Common::IniFile::Section* const family_section =
      ini.GetSection(GetProfileSectionName(GetEffectiveGameFamily(selected_game_family)));

  return family_section != nullptr ? family_section : ini.GetSection(PROFILE_SECTION);
}
}  // namespace TriforcePadProfile
