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
    const u32 visor_base = read32(read32(cplayer_ptr_address) + 0x12ec);
    should_use = read32(visor_base + 0x34) != 2u;
  }
  else if (game == Game::PRIME_3) {
    const u32 visor_base = read32(read32(read32(read32(cplayer_ptr_address) + 4) + 0x2184) + 0x35a8);
    should_use = read32(visor_base + 0x34) != 1u;
  }
  else if (game == Game::PRIME_3_STANDALONE) {
    if (region == Region::NTSC_U)
    {
      const u32 visor_base = read32(read32(read32(cplayer_ptr_address) + 0x2184) + 0x35a0);
      should_use = read32(visor_base + 0x34) != 1u;
    }
    else if (region == Region::NTSC_J)
    {
      const u32 visor_base = read32(read32(read32(read32(cplayer_ptr_address) + 4) + 0x2184) + 0x35a8);
      should_use = read32(visor_base + 0x34) != 1u;
    }
    else if (region == Region::PAL)
    {
      const u32 visor_base = read32(read32(read32(read32(cplayer_ptr_address) + 4) + 0x2184) + 0x35a0);
      should_use = read32(visor_base + 0x34) != 1u;
    }
  }
  else { // if (game == Game::PRIME_2_GCN)
    should_use = !read32(read32(cplayer_ptr_address) + 0x3d0);
  }

  if (GetEFBTexture() != should_use) {
    SetEFBToTexture(should_use);
  }
}

bool AutoEFB::init_mod(Game game, Region region) {
  switch (game) {
  case Game::PRIME_2:
    if (region == Region::NTSC_U) {
      cplayer_ptr_address = 0x804e87dc;
    }
    else if (region == Region::NTSC_J) {
      cplayer_ptr_address = 0x804e8fcc;
    }
    else if (region == Region::PAL) {
      cplayer_ptr_address = 0x804efc2c;
    }
    break;
  case Game::PRIME_2_GCN:
    if (region == Region::NTSC_U) {
      cplayer_ptr_address = 0x803dcbdc;
    }
    else if (region == Region::PAL) {
      cplayer_ptr_address = 0x803dddfc;
    }
    break;
  case Game::PRIME_3:
    if (region == Region::NTSC_U) {
      cplayer_ptr_address = 0x805c6c6c;
    }
    else if (region == Region::PAL) {
      cplayer_ptr_address = 0x805ca0ec;
    }
    break;
  case Game::PRIME_3_STANDALONE:
    if (region == Region::NTSC_U)
    {
      cplayer_ptr_address = 0x805c4f98;
    }
    else if (region == Region::NTSC_J)
    {
      cplayer_ptr_address = 0x805caa5c;
    }
    else if (region == Region::PAL)
    {
      cplayer_ptr_address = 0x805c759c;
    }
    break;
  default:
    break;
  }
  
  return true;
}

}
