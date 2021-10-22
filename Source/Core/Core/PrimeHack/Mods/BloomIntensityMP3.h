#pragma once

#include "Core/PrimeHack/PrimeMod.h"

namespace prime {
class BloomIntensityMP3 : public PrimeMod {
  float slider_val;
public:
  void run_mod(Game game, Region region) override;
  bool init_mod(Game game, Region region) override;
  void on_state_change(ModState old_state) override {}

  void replace_start_bloom_func(u32 start_point, u32 bloom_func);
};

}  // namespace prime
