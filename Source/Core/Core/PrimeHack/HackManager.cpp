#include "Core/PrimeHack/HackManager.h"

#include "Core/PrimeHack/HackConfig.h"
#include "Core/PowerPC/PowerPC.h"
#include "InputCommon/GenericMouse.h"

namespace prime {
  HackManager::HackManager()
    : active_game(Game::INVALID_GAME),
      active_region(Region::INVALID_REGION),
      last_game(Game::INVALID_GAME),
      last_region(Region::INVALID_REGION) {}

  void HackManager::run_active_mods() {
    u32 game_sig = PowerPC::HostRead_Instruction(0x80074000);
    switch (game_sig)
    {
    case 0x90010024:
      active_game = Game::MENU;
      active_region = Region::NTSC;
      break;
    case 0x93FD0008:
      active_game = Game::MENU;
      active_region = Region::PAL;
      break;
    case 0x480008D1:
      active_game = Game::PRIME_1;
      active_region = Region::NTSC;
      break;
    case 0x7EE3BB78:
      active_game = Game::PRIME_1;
      active_region = Region::PAL;
      break;
    case 0x7C6F1B78:
      active_game = Game::PRIME_2;
      active_region = Region::NTSC;
      break;
    case 0x90030028:
      active_game = Game::PRIME_2;
      active_region = Region::PAL;
      break;
    case 0x90010020:
      active_game = Game::PRIME_3;
      {
        u32 region_diff = PowerPC::HostRead_U32(0x800CC000);
        if (region_diff == 0x981D005E)
        {
          active_region = Region::NTSC;
        }
        else if (region_diff == 0x8803005D)
        {
          active_region = Region::PAL;
        }
        else
        {
          active_game = Game::INVALID_GAME;
          active_region = Region::INVALID_REGION;
        }
      }
      break;
    default:
      active_game = Game::INVALID_GAME;
      active_region = Region::INVALID_REGION;
    }

    if (active_game != Game::INVALID_GAME && active_region != Region::INVALID_REGION) {
      std::vector<std::unique_ptr<PrimeMod>>& active_mods = mod_list[static_cast<int>(active_game)][static_cast<int>(active_region)];
      if (active_game != last_game || active_region != last_region) {
        prime::RefreshControlDevices();
      }
      for (std::size_t i = 0; i < active_mods.size(); i++) {
        if (active_mods[i]->should_apply_changes()) {
          auto const& changes = active_mods[i]->get_instruction_changes();
          for (CodeChange const& change : changes) {
            PowerPC::HostWrite_U32(change.var, change.address);
            PowerPC::ScheduleInvalidateCacheThreadSafe(change.address);
          }
        }
      }
      
      last_game = active_game;
      last_region = active_region;
      for (std::size_t i = 0; i < active_mods.size(); i++) {
        active_mods[i]->run_mod();
      }
    }
    prime::g_mouse_input->ResetDeltas();
  }

  void HackManager::add_mod(std::unique_ptr<PrimeMod> mod) {
    if (mod->region() != Region::INVALID_REGION && mod->game() != Game::INVALID_GAME) {
      mod_list[static_cast<int>(mod->game())][static_cast<int>(mod->region())].emplace_back(std::move(mod));
    }
  }
  
  void HackManager::remove_mod(std::unique_ptr<PrimeMod> mod)
  {
    if (mod_list.size() != 0)
    {

    }
  }
}
