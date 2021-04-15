#pragma once

#include "Core/PrimeHack/PrimeMod.h"
#include <set>

namespace prime {

class ContextSensitiveControls : public PrimeMod {
public:
  void run_mod(Game game, Region region) override;
  bool init_mod(Game game, Region region) override;
  void on_state_change(ModState old_state) override {}

private:
  u32 motion_vtf_address;
  u32 cplayer_ptr_address;

  const std::set<u32> radio_editor_ids = {
    // Ship Radio IDs
    0x180263, 0x0D0853,
    0x1003F0, 0x02072F,
    0x0202AB, 0x2105F8,
    0x020428, 0x02027E,
    0x1202E7, 0x020208,
    0x1C0004, 0x02029C,
    0x0202AF, 0x080305,
    0x0202F7
  };
};
}
