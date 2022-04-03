#pragma once

#include "Core/PrimeHack/PrimeMod.h"
#include "Core/PrimeHack/Transform.h"

namespace prime {

class Noclip : public PrimeMod {
public:
  void run_mod(Game game, Region region) override;
  bool init_mod(Game game, Region region) override;
  void on_state_change(ModState old_state) override;

private:
  vec3 get_movement_vec(u32 camera_tf_addr);
  void run_mod_mp1(bool has_control);
  void run_mod_mp1_gc(bool has_control);
  void run_mod_mp2(bool has_control);
  void run_mod_mp2_gc(bool has_control);
  void run_mod_mp3(bool has_control);

  void noclip_code_mp1(u32 cplayer_address, u32 start_point, u32 return_location);
  void noclip_code_mp1_gc(u32 cplayer_address, u32 start_point, u32 return_location);
  void noclip_code_mp2(u32 cplayer_address, u32 start_point, u32 return_location);
  void noclip_code_mp2_gc(u32 cplayer_address, u32 start_point, u32 return_location);
  void noclip_code_mp3(u32 cplayer_address, u32 start_point, u32 return_location);

  bool has_control_mp1_gc();
  bool has_control_mp2();
  bool has_control_mp2_gc();
  bool has_control_mp3();

  u64 old_matexclude_list;

  Transform player_transform;
  vec3 player_vec;

  bool had_control = true;
};

}
