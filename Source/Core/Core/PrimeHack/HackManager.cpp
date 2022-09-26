#include "Core/PrimeHack/HackManager.h"

#include "Core/PrimeHack/HackConfig.h"
#include "Core/PrimeHack/PrimeUtils.h"

#include "Core/PowerPC/PowerPC.h"
#include "Core/PowerPC/MMU.h"

#include "Core/ConfigManager.h"
#include "Core/Config/MainSettings.h"

#include "InputCommon/GenericMouse.h"

namespace prime {
HackManager::HackManager()
  : active_game(Game::INVALID_GAME),
    active_region(Region::INVALID_REGION),
    last_game(Game::INVALID_GAME),
    last_region(Region::INVALID_REGION) {}

#define FOURCC(a, b, c, d) (((static_cast<u32>(a) << 24) & 0xff000000) | \
                            ((static_cast<u32>(b) << 16) & 0x00ff0000) | \
                            ((static_cast<u32>(c) << 8) & 0x0000ff00) | \
                            (static_cast<u32>(d) & 0x000000ff))

void HackManager::run_active_mods() {
  // When launching a new title, the EH being 0 is a good sign
  // that the game isn't done loading yet
  u32 exception_hook = read32(0x80000048);
  if (exception_hook == 0) {
    return;
  }
  u32 game_sig = PowerPC::HostRead_Instruction(0x8046d340);
  switch (game_sig)
  {
  case 0x38000018:
    active_game = Game::MENU;
    active_region = Region::NTSC_U;
    break;
  case 0x806ddaec:
    active_game = Game::MENU_PRIME_1;
    active_region = Region::NTSC_J;
    break;
  case 0x801e0000:
    active_game = Game::MENU_PRIME_2;
    active_region = Region::NTSC_J;
    break;
  case 0x7c0000d0:
    active_game = Game::MENU;
    active_region = Region::PAL;
    break;
  case 0x4e800020:
    active_game = Game::PRIME_1;
    active_region = Region::NTSC_U;
    break;
  case 0x53687566:
    active_game = Game::PRIME_1;
    active_region = Region::NTSC_J;
    break;
  case 0x7c962378:
    active_game = Game::PRIME_1;
    active_region = Region::PAL;
    break;
  case 0x4bff64e1:
    active_game = Game::PRIME_2;
    active_region = Region::NTSC_U;
    break;
  case 0x936daabc:
    active_game = Game::PRIME_2;
    active_region = Region::NTSC_J;
    break;
  case 0x80830000:
    active_game = Game::PRIME_2;
    active_region = Region::PAL;
    break;
  case 0x80010070:
    if (PowerPC::HostRead_U32(0x80576ae8) == 0x7d415378) {
      active_game = Game::PRIME_3;
      active_region = Region::NTSC_U;
    }
    else {
      active_game = Game::INVALID_GAME;
      active_region = Region::INVALID_REGION;
    }
    break;
  case 0x3a800000:
    if (PowerPC::HostRead_U32(0x805795a4) == 0x7d415378) {
      active_game = Game::PRIME_3;
      active_region = Region::PAL;
    }
    else {
      active_game = Game::INVALID_GAME;
      active_region = Region::INVALID_REGION;
    }
    break;
  default:
    u32 region_code = PowerPC::HostRead_U32(0x80000000);
    if (region_code == FOURCC('G', 'M', '8', 'E')) {
      active_region = Region::NTSC_U;

      const u8 version = read8(0x80000007);
      switch (version) {
        case 0:
          active_game = Game::PRIME_1_GCN;
          break;

        case 1:
          active_game = Game::PRIME_1_GCN_R1;
          break;

        case 2:
          active_game = Game::PRIME_1_GCN_R2;
          break;

        default:
          active_game = Game::INVALID_GAME;
          active_region = Region::INVALID_REGION;
          break;
      }
    }
    else if (region_code == FOURCC('G', 'M', '8', 'P')) {
      active_game = Game::PRIME_1_GCN;
      active_region = Region::PAL;
    }
    else if (region_code == FOURCC('G', '2', 'M', 'E')) {
      active_game = Game::PRIME_2_GCN;
      active_region = Region::NTSC_U;
    }
    else if (region_code == FOURCC('G', '2', 'M', 'J')) {
      active_game = Game::PRIME_2_GCN;
      active_region = Region::NTSC_J;
    }
    else if (region_code == FOURCC('G', '2', 'M', 'P')) {
      active_game = Game::PRIME_2_GCN;
      active_region = Region::PAL;
    }
    else if (region_code == FOURCC('R', 'M', '3', 'E')) {
      active_game = Game::PRIME_3_STANDALONE;
      active_region = Region::NTSC_U;
    }
    else if (region_code == FOURCC('R', 'M', '3', 'J')) {
      active_game = Game::PRIME_3_STANDALONE;
      active_region = Region::NTSC_J;
    }
    else if (region_code == FOURCC('R', 'M', '3', 'P')) {
      active_game = Game::PRIME_3_STANDALONE;
      active_region = Region::PAL;
    }
    else {
      active_game = Game::INVALID_GAME;
      active_region = Region::INVALID_REGION;
    }
    break;
  }

  if (active_game != last_game || active_region != last_region) {
    for (auto& mod : mods) {
      mod.second->reset_mod();
    }
    GetVariableManager()->reset_variables();
  }

  ClrDevInfo(); // Clear the dev info stream before the mods print again.

  update_mod_states();

  if (active_game != Game::INVALID_GAME && active_region != Region::INVALID_REGION) {
    for (auto& mod : mods) {
      if (!mod.second->is_initialized()) {
        bool was_init = mod.second->init_mod(active_game, active_region);
        if (was_init) {
          mod.second->mark_initialized();
        }
      }
      if (mod.second->should_apply_changes()) {
        mod.second->update_original_instructions();
        mod.second->apply_instruction_changes();
      }
    }

    last_game = active_game;
    last_region = active_region;
    for (auto& mod : mods) {
      if (mod.second->mod_state() == ModState::ENABLED ||
          mod.second->mod_state() == ModState::CODE_DISABLED) {
        mod.second->run_mod(active_game, active_region);
      }
    }
  }
  prime::g_mouse_input->ResetDeltas();
}

void HackManager::update_mod_states() {
  set_mod_enabled("auto_efb", UseMPAutoEFB());
  set_mod_enabled("cut_beam_fx_mp1", GetEnableSecondaryGunFX());

  if (Config::Get(Config::MAIN_ENABLE_CHEATS)) {
    set_mod_enabled("noclip", Config::Get(Config::PRIMEHACK_NOCLIP));
    set_mod_enabled("invulnerability", Config::Get(Config::PRIMEHACK_INVULNERABILITY));
    set_mod_enabled("skip_cutscene", Config::Get(Config::PRIMEHACK_SKIPPABLE_CUTSCENES));
    set_mod_enabled("restore_dashing", Config::Get(Config::PRIMEHACK_RESTORE_SCANDASH));
    set_mod_enabled("friend_vouchers_cheat", Config::Get(Config::PRIMEHACK_FRIENDVOUCHERS));
    set_mod_enabled("portal_skip_mp2", Config::Get(Config::PRIMEHACK_SKIPMP2_PORTAL));
    set_mod_enabled("disable_hudmemo_popup", Config::Get(Config::PRIMEHACK_DISABLE_HUDMEMO));
    set_mod_enabled("unlock_hypermode", Config::Get(Config::PRIMEHACK_UNLOCK_HYPERMODE));
  }
  else {
    disable_mod("noclip");
    disable_mod("invulnerability");
    disable_mod("skip_cutscene");
    disable_mod("restore_dashing");
    disable_mod("friend_vouchers_cheat");
    disable_mod("portal_skip_mp2");
    disable_mod("unlock_hypermode");
  }

  // Disallow any PrimeHack control mods
  if (!Config::Get(Config::PRIMEHACK_ENABLE) || UsingRealWiimote())
  {
    disable_mod("fps_controls");
    disable_mod("springball_button");
    disable_mod("context_sensitive_controls");
    disable_mod("map_controller");

    return;
  } else {
    enable_mod("fps_controls");
    enable_mod("springball_button");

    if (ImprovedMotionControls()) {
      enable_mod("context_sensitive_controls");
    } else {
      auto result = mods.find("context_sensitive_controls");
      if (result == mods.end()) {
        return;
      }
      result->second->set_state(ModState::CODE_DISABLED);
    }
    if (NewMapControlsEnabled()) {
      enable_mod("map_controller");
    } else {
      disable_mod("map_controller");
    }
  }
}

void HackManager::add_mod(std::string const &name, std::unique_ptr<PrimeMod> mod) {
  mods[name] = std::move(mod);
}

void HackManager::disable_mod(std::string const& name) {
  auto result = mods.find(name);
  if (result == mods.end()) {
    return;
  }
  result->second->set_state(ModState::DISABLED);
}

void HackManager::enable_mod(std::string const& name) {
  auto result = mods.find(name);
  if (result == mods.end()) {
    return;
  }
  result->second->set_state(ModState::ENABLED);
}

void HackManager::set_mod_enabled(std::string const& name, bool enabled) {
  if (enabled) {
    enable_mod(name);
  }
  else {
    disable_mod(name);
  }
}

bool HackManager::is_mod_active(std::string const& name) {
  auto result = mods.find(name);
  if (result == mods.end()) {
    return false;
  }
  return result->second->mod_state() != ModState::DISABLED;
}

void HackManager::reset_mod(std::string const &name) {
  auto result = mods.find(name);
  if (result == mods.end()) {
    return;
  }
  result->second->reset_mod();
}

void HackManager::save_mod_states() {
  for (auto& mod : mods) {
    mod_state_backup[mod.first] = mod.second->mod_state();
  }
}

void HackManager::restore_mod_states() {
  for (auto& mod : mods) {
    auto result = mod_state_backup.find(mod.first);
    if (result == mod_state_backup.end()) {
      continue;
    }
    if (mod.second->is_initialized()) {
      mod.second->set_state(result->second);
    }
  }
}

void HackManager::revert_all_code_changes() {
  for (auto& mod : mods) {
    if (mod.second->is_initialized()) {
      mod.second->set_state(ModState::DISABLED);
      mod.second->apply_instruction_changes();
    }
  }
}

void HackManager::shutdown() {
  for (auto& mod : mods) {
    mod.second->reset_mod();
  }
  last_game = Game::INVALID_GAME;
  last_region = Region::INVALID_REGION;
}

PrimeMod *HackManager::get_mod(std::string const& name) {
  auto result = mods.find(name);
  if (result == mods.end()) {
    return nullptr;
  }
  return result->second.get();
}

}
