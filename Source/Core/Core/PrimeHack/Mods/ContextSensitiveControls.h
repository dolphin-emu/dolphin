#pragma once

#include "Core/PrimeHack/PrimeMod.h"

namespace prime {

class ContextSensitiveControls : public PrimeMod {
public:
  void run_mod(Game game, Region region) override;
  bool init_mod(Game game, Region region) override;

private:
  u32 motion_vtf_address;
  u32 cplayer_ptr_address;
};
}
