#pragma once

#include "Core/PrimeHack/PrimeMod.h"

namespace prime {
// FOV scaling for PrimeHack. Does the following:
//  - Provides full control of camera movement to the device outputting to
//    GetHorizontalAxis and GetVerticalAxis
//  - Locks the reticle center-screen
//  - Unlocks the reticle when needed (MP3 ridley fight, cursor control)
//  - Forces freelook during grapple hook/slide
//  - Forces freelook when on ship (MP3)
//  - Gives control of visor & beam switching to the device outputting to
//    CheckVisorCtl and CheckBeamCtl
//  - Removes arm cannon "damped movement" in MP2 & MP3
class ViewModifier : public PrimeMod {
public:
  void run_mod(Game game, Region region) override;
  bool init_mod(Game game, Region region) override;
  void on_state_change(ModState old_state) override {}

private:
  void adjust_viewmodel(float fov, u32 arm_address, u32 znear_address, u32 znear_value);
  void adjust_fov_mp3(float fov, u16 camera_id);
  static void on_camera_change(u32);

  void run_mod_mp1();
  void run_mod_mp1_gc();
  void run_mod_mp2();
  void run_mod_mp2_gc();
  void run_mod_mp3();

  void init_mod_mp1(Region region);
  void init_mod_mp1_gc(Region region);
  void init_mod_mp1_gc_r1();
  void init_mod_mp1_gc_r2();
  void init_mod_mp2(Region region);
  void init_mod_mp2_gc(Region region);
  void init_mod_mp3(Region region);
  void init_mod_mp3_standalone(Region region);
};

}
