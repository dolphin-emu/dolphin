#pragma once

#include "Core/PrimeHack/PrimeMod.h"

namespace prime {
  // Primary focus of PrimeHack. Does the following:
  //  - Provides full control of camera movement to the device outputting to
  //    GetHorizontalAxis and GetVerticalAxis
  //  - Locks the reticle center-screen
  //  - Unlocks the reticle when needed (MP3 ridley fight, cursor control)
  //  - Forces freelook during grapple hook/slide
  //  - Forces freelook when on ship (MP3)
  //  - Gives control of visor & beam switching to the device outputting to
  //    CheckVisorCtl and CheckBeamCtl
  //  - Removes arm cannon "damped movement" in MP2 & MP3
  class FpsControls : public PrimeMod {
  public:
    void run_mod(Game game, Region region) override;
    void init_mod(Game game, Region region) override;

  private:
    // ------------------------------
    // -----Active Mod Functions-----
    // ------------------------------
    void calculate_pitch_delta();
    float calculate_yaw_vel();
    void handle_beam_visor_switch(std::array<int, 4> const &beams,
      std::array<std::tuple<int, int>, 4> const &visors);
    void mp3_handle_cursor(bool lock);

    void run_mod_menu(Region region);
    void run_mod_mp1();
    void run_mod_mp2(Region region);
    void run_mod_mp3();
    void run_mod_mp1_gc();
    void run_mod_mp2_gc();

    // ------------------------
    // -----Init Functions-----
    // ------------------------
    void add_beam_change_code_mp1(u32 start_point);
    void add_beam_change_code_mp2(u32 start_point);
    void add_grapple_slide_code_mp3(u32 start_point);
    void add_control_state_hook_mp3(u32 start_point, Region region);
    // Very large code, apologies for anyone who reads this
    // corresponding assembly is in comments :)
    void add_strafe_code_mp1_ntsc();
    void add_strafe_code_mp1_pal();

    void init_mod_mp1(Region region);
    void init_mod_mp2(Region region);
    void init_mod_mp3(Region region);
    void init_mod_mp1_gc(Region region);
    void init_mod_mp2_gc(Region region);

    // All 3 of these games have this in common (MP3 just ignores beams)
    u32 active_visor_offset;
    u32 powerups_ptr_address;
    u32 powerups_size;
    u32 powerups_offset;
    u32 new_beam_address;
    u32 beamchange_flag_address;
    // Required due to MP3
    bool has_beams;
    bool fighting_ridley;

    // No particular reason to make a union
    // No particular reason not to
    union {
      struct {
        u32 yaw_vel_address;
        u32 pitch_address;
        u32 pitch_goal_address;
        u32 angvel_limiter_address;
        u32 orbit_state_address;
        u32 lockon_address;
        u32 tweak_player_address;
        u32 cplayer_address;
      } mp1_static;

      struct {
        u32 yaw_vel_address;
        u32 pitch_address;
        u32 angvel_max_address;
        u32 orbit_state_address;
        u32 tweak_player_address;
        u32 cplayer_address;
      } mp1_gc_static;

      struct {
        u32 cplayer_ptr_address;
        u32 load_state_address;
        u32 lockon_address;
      } mp2_static;

      struct {
        u32 state_mgr_address;
        u32 player_tweak_offset;
      } mp2_gc_static;

      struct {
        u32 cplayer_ptr_address;
        u32 cursor_dlg_enabled_address;
        u32 cursor_ptr_address;
        u32 cursor_offset;
        u32 boss_info_address;
        u32 lockon_address;
        u32 gun_lag_toc_offset;
        u32 motion_vtf_address;
      } mp3_static;
    };

    // We store our pitch value interally to have full control over it
    float pitch;
  };
}
