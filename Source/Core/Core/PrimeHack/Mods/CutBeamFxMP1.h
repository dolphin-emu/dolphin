#pragma once

#include "Core/PrimeHack/PrimeMod.h"

namespace prime {

class CutBeamFxMP1 : public PrimeMod {
public:
  void run_mod(Game game, Region region) override {}
  bool init_mod(Game game, Region region) override;
  void on_state_change(ModState old_state) override {}
};

}
