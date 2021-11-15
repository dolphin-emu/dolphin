#pragma once

#include "Core/PrimeHack/PrimeMod.h"
#include "Core/PrimeHack/PrimeUtils.h"

namespace prime {
  class UnlockHypermode : public PrimeMod {
  public:
    void run_mod(Game game, Region region) override {
      if (game != Game::MENU) {
        return;
      }

      u32 base = read32(GPR(13) - r13_offset);
      write8(1, base + 0x68);
      write8(1, base + 0xa0);
      write8(1, base + 0xd8);
    }

    bool init_mod(Game game, Region region) override {
      if (region == Region::NTSC_U) {
        r13_offset = 0x2f94;
      } else if (region == Region::PAL) {
        r13_offset = 0x2f34;
      }

      return true;
    }

  void on_state_change(ModState old_state) override {}

  private:
    u32 r13_offset;
  };
}
