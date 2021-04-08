#pragma once

#include "Core/PrimeHack/PrimeMod.h"

#include <map>
#include <vector>

namespace prime {
  
struct PortalControl {
  u32 scan_eid;
  u32 seq_timer_eid;
  u32 rip_fx_gen_eid;
};

class PortalSkipMP2 : public PrimeMod {
public:
  void run_mod(Game game, Region region) override;
  bool init_mod(Game game, Region region) override;
  void on_state_change(ModState old_state) override {}

  void fix_portal_terminal_layer_bits();

private:
  std::map<u32, std::vector<PortalControl>> portal_control_map;
};

}
