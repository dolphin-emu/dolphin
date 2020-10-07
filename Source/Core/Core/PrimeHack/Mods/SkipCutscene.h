#pragma once

#include "Core/PrimeHack/PrimeMod.h"

namespace prime {
class SkipCutscene : public PrimeMod {
public:
  void run_mod(Game game, Region region) override {}
  void init_mod(Game game, Region region) override {
    switch (game) {
    case Game::PRIME_1:
      if (region == Region::NTSC) {
        add_return_one(0x800cf054);
      }
      else if (region == Region::PAL) {
        add_return_one(0x800cf174);
      }
      break;
    case Game::PRIME_1_GCN:
      if (region == Region::NTSC) {
        add_return_one(0x801d5528);
      }
      else if (region == Region::PAL) {
        add_return_one(0x801c6640);
      }
      break;
    case Game::PRIME_2:
      if (region == Region::NTSC) {
        add_return_one(0x800bc4d0);
      }
      else if (region == Region::PAL) {
        add_return_one(0x800bdb9c);
      }
      break;
    case Game::PRIME_2_GCN:
      if (region == Region::NTSC) {
        add_return_one(0x80142340);
      }
      else if (region == Region::PAL) {
        add_return_one(0x8014257c);
      }
      break;
    case Game::PRIME_3:
      if (region == Region::NTSC) {
        add_return_one(0x800b9f30);
      }
      else if (region == Region::PAL) {
        add_return_one(0x800b9f30);
      }
      break;
    }
    initialized = true;
  }

private:
  void add_return_one(u32 start_point) {
    code_changes.emplace_back(start_point + 0x00, 0x38600001);
    code_changes.emplace_back(start_point + 0x04, 0x4e800020);
  }
};
}
