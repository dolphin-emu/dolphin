#pragma once

#include "Core/PrimeHack/PrimeMod.h"

namespace prime {

class DisableBloom : public PrimeMod {
public:
  void run_mod(Game game, Region region) override {}
  // BLR writing machine
  void init_mod(Game game, Region region) override {
    switch (game) {
    case Game::PRIME_1:
      if (region == Region::NTSC) {
        code_changes.emplace_back(0x80290edc, 0x4e800020);
      }
      else if (region == Region::PAL) {
        code_changes.emplace_back(0x80291258, 0x4e800020);
      }
      break;
    case Game::PRIME_2:
      if (region == Region::NTSC) {
        code_changes.emplace_back(0x80292204, 0x4e800020);
      }
      else if (region == Region::PAL) {
        code_changes.emplace_back(0x80294a40, 0x4e800020);
      }
      break;
    case Game::PRIME_3:
      if (region == Region::NTSC) {
        code_changes.emplace_back(0x804852cc, 0x4e800020);
      }
      else if (region == Region::PAL) {
        code_changes.emplace_back(0x804849e8, 0x4e800020);
      }
      break;
    case Game::PRIME_3_WII:
      if (region == Region::NTSC)
      {
        code_changes.emplace_back(0x80486880, 0x4e800020);
      }
      else if (region == Region::PAL)
      {
        code_changes.emplace_back(0x804885a4, 0x4e800020);
      }
      break;
    default:
      break;
    }
    initialized = true;
  }
// Might as well be a functor...
};
}
