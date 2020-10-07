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
    void init_mod(Game game, Region region) override;

  private:
    void disable_culling(u32 start_point);
    void adjust_viewmodel(float fov, u32 arm_address, u32 znear_address, u32 znear_value);

    void run_mod_mp1();
    void run_mod_mp1_gc();
    void run_mod_mp2();
    void run_mod_mp2_gc();
    void run_mod_mp3();

    void init_mod_mp1(Region region);
    void init_mod_mp1_gc(Region region);
    void init_mod_mp2(Region region);
    void init_mod_mp2_gc(Region region);
    void init_mod_mp3(Region region);

    union {
      struct {
        u32 camera_ptr_address;
        u32 active_camera_offset_address;
        u32 global_fov1_address;
        u32 global_fov2_address;
        u32 gun_pos_address;
        u32 culling_address;
      } mp1_static;

      struct {
        u32 camera_ptr_address;
        u32 camera_offset_address;
        u32 tweakgun_ptr_address;
        u32 culling_address;
        u32 load_state_address;
      } mp2_static;

      struct {
        u32 camera_ptr_address;
        u32 tweakgun_address;
        u32 culling_address;
      } mp3_static;

      struct {
        u32 camera_mgr_address;
        u32 object_list_address;
        // global first & third person FOV
        u32 global_fov1_table_off;
        u32 global_fov2_table_off;
        u32 gun_pos_address;
        u32 culling_address;
      } mp1_gc_static;

      struct {
        u32 state_mgr_address;
        u32 gun_tweak_offset;
        u32 culling_address;
      } mp2_gc_static;
    };
  };

}
