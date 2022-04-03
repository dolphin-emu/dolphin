#pragma once

#include "Core/PrimeHack/PrimeMod.h"

namespace prime {
class SkipCutscene : public PrimeMod {
public:
  void run_mod(Game game, Region region) override { }
  bool init_mod(Game game, Region region) override {
    switch (game) {
    case Game::PRIME_1:
      if (region == Region::NTSC_U) {
        add_return_one(0x800cf054);
      } else if (region == Region::PAL) {
        add_return_one(0x800cf174);
      } else if (region == Region::NTSC_J) {
        add_return_one(0x800cf3e4);
      }
      break;
    case Game::PRIME_1_GCN:
      if (region == Region::NTSC_U) {
        add_return_one(0x801d5528);
      } else if (region == Region::PAL) {
        add_return_one(0x801c6640);
      }
      break;
    case Game::PRIME_1_GCN_R1:
      add_return_one(0x8015204c);
      break;
    case Game::PRIME_1_GCN_R2:
      add_return_one(0x801d5d78);
      break;
    case Game::PRIME_2:
      if (region == Region::NTSC_U) {
        add_return_one(0x800bc4d0);
      } else if (region == Region::PAL) {
        add_return_one(0x800bdb9c);
      } else if (region == Region::NTSC_J) {
        add_return_one(0x800bbb68);
      }
      break;
    case Game::PRIME_2_GCN:
      if (region == Region::NTSC_U) {
        add_return_one(0x80142340);
      } else if (region == Region::NTSC_J) {
        add_return_one(0x80143330);
      } else if (region == Region::PAL) {
        add_return_one(0x8014257c);
      }
      break;
    case Game::PRIME_3:
      if (region == Region::NTSC_U) {
        add_return_one(0x800b9f30);
      } else if (region == Region::PAL) {
        add_return_one(0x800b9f30);
      }
      break;
    case Game::PRIME_3_STANDALONE:
      if (region == Region::NTSC_U) {
        add_return_one(0x800bb930);
      } else if (region == Region::PAL) {
        add_return_one(0x800bbd24);
      } else if (region == Region::NTSC_J) {
        add_return_one(0x800bc10c);
      }
      break;
    }
    return true;
  }
  void on_state_change(ModState old_state) override {}

private:
  void add_return_one(u32 start_point) {
    add_code_change(start_point + 0x00, 0x38600001);
    add_code_change(start_point + 0x04, 0x4e800020);
  }
};
}
