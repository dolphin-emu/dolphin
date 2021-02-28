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
  bool init_mod(Game game, Region region) override;

private:
  // ------------------------------
  // -----Active Mod Functions-----
  // ------------------------------
  void calculate_pitch_delta();
  void calculate_pitch_locked(Game game, Region region);
  void calculate_pitch_to_target(float target_pitch);
  float calculate_yaw_vel();
  void handle_beam_visor_switch(std::array<int, 4> const &beams,
                                std::array<std::tuple<int, int>, 4> const& visors);
  void mp3_handle_cursor(bool lock);
  void mp3_handle_lasso(u32 cplayer_address);

  void run_mod_menu(Game game, Region region);
  void run_mod_mp1(Region region);
  void run_mod_mp2(Region region);
  void run_mod_mp3(Game game, Region region);
  void run_mod_mp1_gc();
  void run_mod_mp2_gc();

  void CheckBeamVisorSetting(Game game);

  // ------------------------
  // -----Init Functions-----
  // ------------------------
  void add_beam_change_code_mp1(u32 start_point);
  void add_beam_change_code_mp2(u32 start_point);
  void add_grapple_slide_code_mp3(u32 start_point);
  void add_grapple_lasso_code_mp3(u32 func1, u32 func2, u32 func3);
  void add_control_state_hook_mp3(u32 start_point, Region region);
  // Very large code, apologies for anyone who reads this
  // corresponding assembly is in comments :)
  void add_strafe_code_mp1_ntsc();
  void add_strafe_code_mp1_pal();

  void init_mod_menu(Game game, Region region);
  void init_mod_mp1(Region region);
  void init_mod_mp2(Region region);
  void init_mod_mp3(Region region);
  void init_mod_mp1_gc(Region region);
  void init_mod_mp2_gc(Region region);
  void init_mod_mp3_standalone(Region region);

  // All 3 of these games have this in common (MP3 just ignores beams)
  u32 active_visor_offset;
  u32 powerups_ptr_address;
  u32 powerups_ptr_offset;
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
      u32 object_list_ptr_address;
      u32 camera_uid_address;
      u32 beamvisor_menu_address;
      u32 beamvisor_menu_offset;
      u32 cursor_base_address;
      u32 cursor_offset;
    } mp1_static;

    struct {
      u32 yaw_vel_address;
      u32 pitch_address;
      u32 angvel_max_address;
      u32 orbit_state_address;
      u32 tweak_player_address;
      u32 cplayer_address;
      u32 camera_uid_address;
      u32 state_mgr_address;
      u32 crosshair_color_address;
    } mp1_gc_static;

    struct {
      u32 cplayer_ptr_address;
      u32 load_state_address;
      u32 lockon_address;
      u32 beamvisor_menu_address;
      u32 beamvisor_menu_offset;
      u32 cursor_base_address;
      u32 cursor_offset;
      u32 tweak_player_offset;
      u32 object_list_ptr_address;
      u32 camera_uid_address;
    } mp2_static;

    struct {
      u32 state_mgr_address;
      u32 tweak_player_offset;
      u32 crosshair_color_address;
    } mp2_gc_static;

    struct {
      u32 cplayer_ptr_address;
      u32 cursor_dlg_enabled_address;
      u32 cursor_ptr_address;
      u32 cursor_offset;
      u32 boss_info_address;
      u32 lockon_address;
      u32 gun_lag_toc_offset;
      u32 beamvisor_menu_address;
      u32 beamvisor_menu_offset;
      u32 cplayer_pitch_offset;
      u32 cam_uid_ptr_address;
      u32 state_mgr_ptr_address;
    } mp3_static;
  };

  // We store our pitch value interally to have full control over it
  float pitch;

  // For interpolating the camera pitch to centre when in MP3 context sensitive mode.
  float start_pitch;
  int delta = 0;

  // Prime 3 Grapple Lasso
  u32 grapple_initial_cooldown = 0;
  bool grapple_button_down, grapple_tugging, grapple_swap_axis = false;
  float grapple_hand_pos, grapple_force;

  // Check when to reset the cursor position
  bool menu_open = true;
};
}
