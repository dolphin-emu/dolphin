#pragma once

#include "Core/PrimeHack/PrimeMod.h"
#include "Core/PrimeHack/PrimeUtils.h"

namespace prime {
  class FriendVouchers : public PrimeMod {
  public:
    void run_mod(Game game, Region region) override {
      if (game != Game::MENU)
        return;

      for (int i = 0; i < 105; i++) {
        u32 vouchers_cost_addr = extras_arr_addr + (i * 0xC) + 0x4;
        if (read8(vouchers_cost_addr) != 0) {
          write32(read32(vouchers_cost_addr) & ~0x000000FF, vouchers_cost_addr);
        }
      }

      // "Screen-shot Tool" is handled weirdly. We manually set the price here.
      write32(0x03000102, extras_arr_addr + 0x430);
      write32(0x00000002, extras_arr_addr + 0x434);
    }

    bool init_mod(Game game, Region region) override {
      if (region == Region::NTSC_U) {
        extras_arr_addr = 0x8052DBB0;
      } else if (region == Region::PAL) {
        extras_arr_addr = 0x805320A0;
      }

      return true;
    }
    
  void on_state_change(ModState old_state) override {}

  private:
    u32 extras_arr_addr;
  };
}
