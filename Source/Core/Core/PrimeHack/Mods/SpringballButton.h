#pragma once

#include "Core/PrimeHack/PrimeMod.h"

namespace prime {
class SpringballButton : public PrimeMod {
public:
  void run_mod(Game game, Region region) override;
  bool init_mod(Game game, Region region) override;
  void on_state_change(ModState old_state) override {}

private:
  void springball_code(u32 start_point);
  void springball_check(u32 ball_address, u32 movement_address);
};
}
