#include "Core/PrimeHack/Mods/AutoEFB.h"

#include "Core/PrimeHack/PrimeUtils.h"

namespace prime {
void AutoEFB::run_mod(Game game, Region region) {
  if (game != Game::PRIME_2 && game != Game::PRIME_3) {
    return;
  }

  u32 visor_base;
  if (game == Game::PRIME_2) {
    visor_base = read32(read32(cplayer_ptr_address) + 0x12ec);
  }
  else {
    visor_base = read32(read32(read32(read32(cplayer_ptr_address) + 4) + 0x2184) + 0x35a8);
  }
  const bool should_use = read32(visor_base + 0x34) != (game == Game::PRIME_2 ? 2u : 1u);

  if (GetEFBTexture() != should_use) {
    SetEFBToTexture(should_use);
  }
}

void AutoEFB::init_mod(Game game, Region region) {
  switch (game) {
  case Game::PRIME_2:
    if (region == Region::NTSC) {
      cplayer_ptr_address = 0x804e87dc;
    }
    else if (region == Region::PAL) {
      cplayer_ptr_address = 0x804efc2c;
    }
    break;
  case Game::PRIME_3:
    if (region == Region::NTSC) {
      cplayer_ptr_address = 0x805c6c6c;
    }
    else if (region == Region::PAL) {
      cplayer_ptr_address = 0x805ca0ec;
    }
    break;
  default:
    break;
  }
  initialized = true;
}

}
