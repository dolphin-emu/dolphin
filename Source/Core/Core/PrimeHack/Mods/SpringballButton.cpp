#include "Core/PrimeHack/Mods/SpringballButton.h"

#include "Core/PrimeHack/PrimeUtils.h"

namespace prime {

void SpringballButton::run_mod(Game game, Region region) {  
  u32 actual_cplayer_address;
  switch (game) {
  case Game::PRIME_1:
    springball_check(cplayer_address + 0x2f4, cplayer_address + 0x25c);
    DevInfo("CPlayer", "%08X", cplayer_address);
    break;
  case Game::PRIME_2:
    actual_cplayer_address = read32(cplayer_address);
    springball_check(actual_cplayer_address + 0x374, actual_cplayer_address + 0x2c4);
    DevInfo("CPlayer", "%08X", cplayer_address);
    break;
  case Game::PRIME_3:
    actual_cplayer_address = read32(read32(read32(cplayer_address) + 0x04) + 0x2184);
    springball_check(actual_cplayer_address + 0x358, actual_cplayer_address + 0x29c);
    DevInfo("CPlayer", "%08X", cplayer_address);
    break;
  default:
    break;
  }
}

void SpringballButton::init_mod(Game game, Region region) {
  switch (game) {
  case Game::PRIME_1:
    if (region == Region::NTSC) {
      cplayer_address = 0x804d3c20;
      springball_code(0x801476d0);
    }
    else if (region == Region::PAL) {
      cplayer_address = 0x804d7b60;
      springball_code(0x80147820);
    }
    break;
  case Game::PRIME_2:
    if (region == Region::NTSC) {
      cplayer_address = 0x804e87dc;
      springball_code(0x8010bd98);
    }
    else if (region == Region::PAL) {
      cplayer_address = 0x804efc2c;
      springball_code(0x8010d440);
    }
    break;
  case Game::PRIME_3:
    if (region == Region::NTSC) {
      cplayer_address = 0x805c6c6c;
      springball_code(0x801077d4);
    }
    else if (region == Region::PAL) {
      cplayer_address = 0x805ca0ec;
      springball_code(0x80107120);
    }
    break;
  default:
    break;
  }
  initialized = true;
}

void SpringballButton::springball_code(u32 start_point) {
  code_changes.emplace_back(start_point + 0x00, 0x3C808000);  // lis r4, 0x8000
  code_changes.emplace_back(start_point + 0x04, 0x60844164);  // ori r4, r4, 0x4164
  code_changes.emplace_back(start_point + 0x08, 0x88640000);  // lbz r3, 0(r4)
  code_changes.emplace_back(start_point + 0x0c, 0x38A00000);  // li r5, 0
  code_changes.emplace_back(start_point + 0x10, 0x98A40000);  // stb r5, 0(r4)
  code_changes.emplace_back(start_point + 0x14, 0x2C030000);  // cmpwi r3, 0
}

void SpringballButton::springball_check(u32 ball_address, u32 movement_address) {
  if (CheckSpringBallCtl())
  {             
    u32 ball_state = read32(ball_address);
    u32 movement_state = read32(movement_address);

    if ((ball_state == 1 || ball_state == 2) && movement_state == 0)
      write8(1, 0x80004164);
  }
}

}
