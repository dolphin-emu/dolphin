#pragma once

#include "Core/PrimeHack/PrimeMod.h"
#include "Core/PrimeHack/Quaternion.h"

namespace prime {

class MapController : public PrimeMod {
public:
  void run_mod(Game game, Region region) override;
  bool init_mod(Game game, Region region) override;
  void on_state_change(ModState old_state) override {}
  void reset_rotation(float horizontal, float vertical);
  quat compute_orientation();
  float get_player_yaw() const;

private:
  void init_mod_mp1_gc(Game game, Region region);
  void init_mod_mp1(Region region);
  void init_mod_mp2_gc(Region region);
  void init_mod_mp2(Region region);

  float frame_dx, frame_dy;
  float x_rot, y_rot;
};

}
