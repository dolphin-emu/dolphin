#include "Core/PrimeHack/Mods/AutoEFB.h"

#include "Core/PrimeHack/PrimeUtils.h"

namespace prime {
void AutoEFB::run_mod(Game game, Region region) {
  if (game != Game::PRIME_2 &&
      game != Game::PRIME_3 &&
      game != Game::PRIME_3_STANDALONE &&
      game != Game::PRIME_2_GCN)
  {
    return;
  }

  bool should_use = false;

  if (game == Game::PRIME_2) {
    LOOKUP_DYN(active_visor);
    should_use = read32(active_visor) != 2u;
  } else if (game == Game::PRIME_3 || game == Game::PRIME_3_STANDALONE) {
    LOOKUP_DYN(active_visor);
    should_use = read32(active_visor) != 1u;
  } else if (game == Game::PRIME_2_GCN) {
    LOOKUP_DYN(player);
    should_use = !read32(player + 0x3d0);
  }

  if (GetEFBTexture() != should_use) {
    SetEFBToTexture(should_use);
  }
}

bool AutoEFB::init_mod(Game game, Region region) {
  return true;
}

}
