#pragma once

#include "Core/PrimeHack/PrimeMod.h"
#include "Core/PrimeHack/HackConfig.h"

namespace prime {

class AutoEFB : public PrimeMod {
public:
  void run_mod(Game game, Region region) override;
  bool init_mod(Game game, Region region) override;
  void on_state_change(ModState old_state) override {}
};

}
