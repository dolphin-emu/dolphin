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
  case Game::PRIME_3_STANDALONE:
    switch (region)
    {
    case Region::NTSC_U:
      actual_cplayer_address = read32(read32(cplayer_address) + 0x2184);
      springball_check(actual_cplayer_address + 0x358, actual_cplayer_address + 0x29c);
      DevInfo("CPlayer", "%08X", cplayer_address);
      break;
    case Region::NTSC_J:
    case Region::PAL:
      actual_cplayer_address = read32(read32(read32(cplayer_address) + 0x04) + 0x2184);
      springball_check(actual_cplayer_address + 0x358, actual_cplayer_address + 0x29c);
      DevInfo("CPlayer", "%08X", cplayer_address);
      break;
    }
    break;
  default:
    break;
  }
}

bool SpringballButton::init_mod(Game game, Region region) {
  switch (game) {
  case Game::PRIME_1:
    if (region == Region::NTSC_U) {
      cplayer_address = 0x804d3c20;
      springball_code(0x801476d0);
    }
    else if (region == Region::PAL) {
      cplayer_address = 0x804d7b60;
      springball_code(0x80147820);
    }
    else { // region == Region::NTSC_J
      cplayer_address = 0x804d3ea0;
      springball_code(0x80147cd0);
    }
    break;
  case Game::PRIME_2:
    if (region == Region::NTSC_U) {
      cplayer_address = 0x804e87dc;
      springball_code(0x8010bd98);
    }
    else if (region == Region::NTSC_J) {
      cplayer_address = 0x804e8fcc;
      springball_code(0x8010b368);
    }
    else if (region == Region::PAL) {
      cplayer_address = 0x804efc2c;
      springball_code(0x8010d440);
    }
    break;
  case Game::PRIME_3:
    if (region == Region::NTSC_U) {
      cplayer_address = 0x805c6c6c;
      springball_code(0x801077d4);
    }
    else if (region == Region::PAL) {
      cplayer_address = 0x805ca0ec;
      springball_code(0x80107120);
    }
    break;
  case Game::PRIME_3_STANDALONE:
    if (region == Region::NTSC_U) {
      cplayer_address = 0x805c4f98;
      springball_code(0x8010c984);
    }
    else if (region == Region::NTSC_J) {
      cplayer_address = 0x805caa5c;
      springball_code(0x8010d49c);
    }
    else if (region == Region::PAL) {
      cplayer_address = 0x805c759c;
      springball_code(0x8010ced4);
    }
    break;
  default:
    break;
  }
  return true;
}

void SpringballButton::springball_code(u32 start_point) {
  u32 lis, ori;
  std::tie<u32, u32>(lis, ori) = prime::GetVariableManager()->make_lis_ori(4, "springball_trigger");

  add_code_change(start_point + 0x00, lis);  // lis r4, springball_trigger_upper
  add_code_change(start_point + 0x04, ori);  // ori r4, r4, springball_trigger_lower
  add_code_change(start_point + 0x08, 0x88640000);  // lbz r3, 0(r4)
  add_code_change(start_point + 0x0c, 0x38A00000);  // li r5, 0
  add_code_change(start_point + 0x10, 0x98A40000);  // stb r5, 0(r4)
  add_code_change(start_point + 0x14, 0x2C030000);  // cmpwi r3, 0
}

void SpringballButton::springball_check(u32 ball_address, u32 movement_address) {
  if (CheckSpringBallCtl())
  {
    u32 ball_state = read32(ball_address);
    u32 movement_state = read32(movement_address);

    if ((ball_state == 1 || ball_state == 2) && movement_state == 0)
      prime::GetVariableManager()->set_variable("springball_trigger", (u8) 1);
  }
}
}
