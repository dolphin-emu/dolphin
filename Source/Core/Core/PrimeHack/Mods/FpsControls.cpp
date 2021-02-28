#include "Core/PrimeHack/Mods/FpsControls.h"
#include "Core/PrimeHack/Mods/ContextSensitiveControls.h"

#include "Core/PrimeHack/PrimeUtils.h"

#include "Common/Timer.h"

namespace prime {
namespace {  
const std::array<int, 4> prime_one_beams = {0, 2, 1, 3};
const std::array<int, 4> prime_two_beams = {0, 1, 2, 3};

const std::array<std::tuple<int, int>, 4> prime_one_visors = {
  std::make_tuple<int, int>(0, 0x11), std::make_tuple<int, int>(2, 0x05),
  std::make_tuple<int, int>(3, 0x09), std::make_tuple<int, int>(1, 0x0d)};
const std::array<std::tuple<int, int>, 4> prime_two_visors = {
  std::make_tuple<int, int>(0, 0x08), std::make_tuple<int, int>(2, 0x09),
  std::make_tuple<int, int>(3, 0x0a), std::make_tuple<int, int>(1, 0x0b)};
const std::array<std::tuple<int, int>, 4> prime_three_visors = {
  std::make_tuple<int, int>(0, 0x0b), std::make_tuple<int, int>(1, 0x0c),
  std::make_tuple<int, int>(2, 0x0d), std::make_tuple<int, int>(3, 0x0e)};

constexpr u32 ORBIT_STATE_GRAPPLE = 5;

bool is_string_ridley(u32 string_base) {
  if (string_base == 0) {
    return false;
  }

  const char ridley_str[] = "Meta Ridley";
  constexpr auto str_len = sizeof(ridley_str) - 1;
  int str_idx = 0;

  while (read16(string_base) != 0 && str_idx < str_len) {
    if (static_cast<char>(read16(string_base)) != ridley_str[str_idx]) {
      return false;
    }
    str_idx++;
    string_base += 2;
  }
  return str_idx == str_len && read16(string_base) == 0;
}
}
void FpsControls::run_mod(Game game, Region region) {
  switch (game) {
  case Game::MENU:
  case Game::MENU_PRIME_1:
  case Game::MENU_PRIME_2:
    run_mod_menu(game, region);
    break;
  case Game::PRIME_1:
    run_mod_mp1(region);
    break;
  case Game::PRIME_2:
    run_mod_mp2(region);
    break;
  case Game::PRIME_3:
    run_mod_mp3(game, region);
    break;
  case Game::PRIME_1_GCN:
    run_mod_mp1_gc();
    break;
  case Game::PRIME_2_GCN:
    run_mod_mp2_gc();
    break;
  case Game::PRIME_3_STANDALONE:
    run_mod_mp3(game, region);
    break;
  default:
    break;
  }
}

void FpsControls::calculate_pitch_delta() {
  const float compensated_sens = GetSensitivity() * TURNRATE_RATIO / 60.f;

  if (CheckPitchRecentre()) {
    calculate_pitch_to_target(0.f);
    return;
  }

  pitch += static_cast<float>(GetVerticalAxis()) * compensated_sens *
    (InvertedY() ? 1.f : -1.f);
  pitch = std::clamp(pitch, -1.52f, 1.52f);
}

void FpsControls::calculate_pitch_locked(Game game, Region region) {
  // Calculate the pitch based on the XF matrix to allow us to write out the pitch
  // even while locked onto a target, the pitch will be written to match the lock
  // angle throughout the entire lock-on. The very first frame when the lock is
  // released (and before this mod has run again) the game will still render the
  // correct angle. If we stop writing the angle during lock-on then a 1-frame snap
  // occurs immediately after releasing the lock, due to the mod running after the
  // frame has already been rendered.
  u32 camera_tf_addr = 0;

  switch (game) {
    case Game::PRIME_1: {
      const u32 camera_ptr = read32(mp1_static.object_list_ptr_address);
      const u32 camera_offset = (((read32(mp1_static.camera_uid_address) + 10) >> 16) & 0x3ff) << 3;
      camera_tf_addr = read32(camera_ptr + camera_offset + 4) + 0x2c;

      break;
    }
    case Game::PRIME_1_GCN: {
      const u16 camera_uid = read16(mp1_gc_static.camera_uid_address);
      if (camera_uid == -1) {
        return;
      }
      u32 camera_offset = (camera_uid & 0x3ff) << 3;
      const u32 camera_address =
          read32(read32(mp1_gc_static.state_mgr_address + 0x810) + 4 + camera_offset);
      camera_tf_addr = camera_address + 0x34;

      break;
    }
    case Game::PRIME_2: {
      u32 object_list_address = read32(mp2_static.object_list_ptr_address);
      if (!mem_check(object_list_address)) {
          return;
      }

      const u16 camera_uid = read16(mp2_static.camera_uid_address);
      if (camera_uid == -1) {
          return;
      }

      const u32 camera_offset = (camera_uid & 0x3ff) << 3;
      camera_tf_addr = read32(object_list_address + 4 + camera_offset) + 0x20;
      break;
    }
    case Game::PRIME_2_GCN: {
      u32 object_list_address = read32(mp2_gc_static.state_mgr_address + 0x810);
      if (!mem_check(object_list_address)) {
        return;
      }
      const u32 camera_mgr = read32(mp2_gc_static.state_mgr_address + 0x151c);
      if (!mem_check(camera_mgr)) {
        return;
      }

      const u16 camera_uid = read16(camera_mgr + 0x14);
      if (camera_uid == 0xffff) {
        return;
      }
      const u32 camera_offset = (camera_uid & 0x3ff) << 3;
      u32 camera_address = read32(object_list_address + 4 + camera_offset);
      camera_tf_addr = camera_address + 0x24;
      break;
    }
    case Game::PRIME_3: {
      u32 offset = 0x28;
      if (game == Game::PRIME_3_STANDALONE && region == Region::NTSC_U)
      offset += 4;

      u32 object_list_address = read32(read32(mp3_static.state_mgr_ptr_address + offset) + 0x1010);
      if (!mem_check(object_list_address)) {
          return;
      }

      const u16 camera_id = read16(read32(read32(mp3_static.cam_uid_ptr_address) + 0xc) + 0x16);
      if (camera_id == 0xffff) {
          return;
      }

      camera_tf_addr = read32(object_list_address + 4 + (8 * camera_id)) + 0x3c;
    }
  }
  
  if (camera_tf_addr == 0) {
     return;
  } 

  Transform camera_tf(camera_tf_addr);
  pitch = asin(camera_tf.fwd().z);
  pitch = std::clamp(pitch, -1.52f, 1.52f);
}

float FpsControls::calculate_yaw_vel() {
  return GetHorizontalAxis() * GetSensitivity() * (InvertedX() ? 1.f : -1.f);;
}

void FpsControls::calculate_pitch_to_target(float target_pitch)
{
  // Smoothly transitions pitch to target through interpolation

  const float margin = 0.05f;
  if (pitch > (target_pitch - margin) && pitch < (target_pitch + margin)) {
    delta = 0;
    pitch = target_pitch;
    return;
  }

  if (delta == 0) {
    start_pitch = pitch;
  }

  pitch = MathUtil::Lerp(start_pitch, target_pitch, delta / 15.f);
  pitch = std::clamp(pitch, -1.52f, 1.52f);

  delta++;

  return;
}

void FpsControls::handle_beam_visor_switch(std::array<int, 4> const &beams,
  std::array<std::tuple<int, int>, 4> const &visors) {
  // Global array of all powerups (measured in "ammunition"
  // even for things like visors/beams)
  u32 powerups_array_base;
  powerups_array_base = read32(powerups_ptr_address);

  // We copy out the ownership status of beams and visors to our own array for
  // get_beam_switch and get_visor_switch
  for (int i = 0; i < 4; i++) {
    const bool visor_owned =
      read32(powerups_array_base + std::get<1>(visors[i]) *
        powerups_size + powerups_offset) ? true : false;
    set_visor_owned(i, visor_owned);
    if (has_beams) {
      const bool beam_owned =
        read32(powerups_array_base + beams[i] *
          powerups_size + powerups_offset) ? true : false;
      set_beam_owned(i, beam_owned);
    }
  }

  if (has_beams) {
    const int beam_id = get_beam_switch(beams);
    if (beam_id != -1) {
      write32(beam_id, new_beam_address);
      write32(1, beamchange_flag_address);
    }
  }

  int visor_id, visor_off;
  std::tie(visor_id, visor_off) = get_visor_switch(visors,
    read32(powerups_array_base + active_visor_offset) == 0);

  if (visor_id != -1) {
    if (read32(powerups_array_base + (visor_off * powerups_size) + powerups_offset)) {
      write32(visor_id, powerups_array_base + active_visor_offset);
    }
  }

  DevInfo("Powerups_Base", "%08X", powerups_array_base);
}

void FpsControls::run_mod_menu(Game game, Region region) {
  if (region == Region::NTSC_U) {
    handle_cursor(0x80913c9c, 0x80913d5c, 0.95f, 0.90f);
  }
  else if (region == Region::NTSC_J) {
    if (game == Game::MENU_PRIME_1) {
      handle_cursor(0x805a7da8, 0x805a7dac, 0.95f, 0.90f);
    }
    if (game == Game::MENU_PRIME_2) {
      handle_cursor(0x805a7ba8, 0x805a7bac, 0.95f, 0.90f);
    }
  }
  else if (region == Region::PAL) {
    u32 cursor_address = PowerPC::HostRead_U32(0x80621ffc);
    handle_cursor(cursor_address + 0xdc, cursor_address + 0x19c, 0.95f, 0.90f);
  }
}

void FpsControls::run_mod_mp1(Region region) {
  handle_beam_visor_switch(prime_one_beams, prime_one_visors);
  CheckBeamVisorSetting(Game::PRIME_1);

  // Is beam/visor menu showing on screen
  bool beamvisor_menu = read32(read32(mp1_static.beamvisor_menu_address) + mp1_static.beamvisor_menu_offset) == 1;

  // Allows freelook in grapple, otherwise we are orbiting (locked on) to something
  bool locked = (read32(mp1_static.orbit_state_address) != ORBIT_STATE_GRAPPLE &&
    read8(mp1_static.lockon_address) || beamvisor_menu);

  u32 cursor_base = read32(read32(mp1_static.cursor_base_address) + mp1_static.cursor_offset);

  if (locked) {
    write32(0, mp1_static.yaw_vel_address);
    calculate_pitch_locked(Game::PRIME_1, region);
    writef32(pitch, mp1_static.pitch_address);
    writef32(pitch, mp1_static.pitch_goal_address);

    if (beamvisor_menu) {
      u32 mode = read32(read32(mp1_static.beamvisor_menu_address) + mp1_static.cursor_offset + 8);

      // if the menu id is not null
      if (mode != 0xFFFFFFFF) {
        if (menu_open == false) {
          change_code_group_state("beam_change", ModState::DISABLED);
        }

        handle_cursor(cursor_base + 0x9c, cursor_base + 0x15c, 0.95f, 0.90f);
        menu_open = true;
      }
    } else if (HandleReticleLockOn()) {  // If we handle menus, this doesn't need to be ran
      handle_cursor(cursor_base + 0x9c, cursor_base + 0x15c, 0.95f, 0.90f);
    }
  } else {
    if (menu_open) {
      change_code_group_state("beam_change", ModState::ENABLED);

      menu_open = false;
    }

    set_cursor_pos(0, 0);
    write32(0, cursor_base + 0x9c);
    write32(0, cursor_base + 0x15c);

    calculate_pitch_delta();
    writef32(pitch, mp1_static.pitch_address);
    writef32(pitch, mp1_static.pitch_goal_address);

    // Max pitch angle, as abs val (any higher = gimbal lock)
    writef32(1.52f, mp1_static.tweak_player_address + 0x134);

    // Setting this to 0 allows yaw velocity (Z component of an angular momentum
    // vector) to affect the transform matrix freely
    write32(0, mp1_static.angvel_limiter_address);

    u32 ball_state = read32(mp1_static.cplayer_address + 0x2f4);
    if (ball_state == 0) {
      // Actual angular velocity Z address amazing
      writef32(calculate_yaw_vel(), mp1_static.yaw_vel_address);
    }
  }
}

void FpsControls::run_mod_mp1_gc() {
  u8 version = read8(0x80000007);
  const bool show_crosshair = GetShowGCCrosshair();
  const u32 crosshair_color = show_crosshair ? GetGCCrosshairColor() : 0x4b7ea331;

  if (version != 0) {
    return;
  }

  change_code_group_state("show_crosshair", show_crosshair ? ModState::ENABLED : ModState::DISABLED);

  const u32 orbit_state = read32(mp1_gc_static.orbit_state_address);
  if (orbit_state != ORBIT_STATE_GRAPPLE &&
    orbit_state != 0) {
    calculate_pitch_locked(Game::PRIME_1_GCN, GetHackManager()->get_active_region());
    writef32(pitch, mp1_gc_static.pitch_address);
    return;
  }

  if (show_crosshair) {
    write32(crosshair_color, mp1_gc_static.crosshair_color_address);
  }

  calculate_pitch_delta();
  writef32(pitch, mp1_gc_static.pitch_address);
  writef32(1.52f, mp1_gc_static.tweak_player_address + 0x134);

  u32 ball_state = read32(mp1_gc_static.cplayer_address + 0x2f4);
  if (ball_state == 0) {
    // Actual angular velocity Z address amazing
    writef32(calculate_yaw_vel() / 200.f, mp1_gc_static.yaw_vel_address);
  }
    
  for (int i = 0; i < 8; i++) {
    writef32(100000000.f, mp1_gc_static.angvel_max_address + i * 4);
    writef32(1.f, mp1_gc_static.angvel_max_address + i * 4 - 32);
  }
}

void FpsControls::run_mod_mp2(Region region) {
  CheckBeamVisorSetting(Game::PRIME_2);

  // VERY similar to mp1, this time CPlayer isn't TOneStatic (presumably because
  // of multiplayer mode in the GCN version?)
  u32 cplayer_address = read32(mp2_static.cplayer_ptr_address);
  if (!mem_check(cplayer_address)) {
    return;
  }

  if (read32(mp2_static.load_state_address) != 1) {
    return;
  }

  // HACK ooo
  powerups_ptr_address = cplayer_address + 0x12ec;
  handle_beam_visor_switch(prime_two_beams, prime_two_visors);

  // Is beam/visor menu showing on screen
  bool beamvisor_menu = read32(read32(mp2_static.beamvisor_menu_address) + mp2_static.beamvisor_menu_offset) == 1;

  // Allows freelook in grapple, otherwise we are orbiting (locked on) to something
  bool locked = (read32(cplayer_address + 0x390) != ORBIT_STATE_GRAPPLE &&
    read32(mp2_static.lockon_address) || beamvisor_menu);

  u32 cursor_base = read32(read32(mp2_static.cursor_base_address) + mp2_static.cursor_offset);

  if (locked) {
    // Angular velocity (not really, but momentum) is being messed with like mp1
    // just being accessed relative to cplayer
    write32(0, cplayer_address + 0x178);
    calculate_pitch_locked(Game::PRIME_2, region);
    writef32(pitch, cplayer_address + 0x5f0);

    if (beamvisor_menu) {
      u32 mode = read32(read32(mp2_static.beamvisor_menu_address) + mp2_static.beamvisor_menu_offset + 0xc);

      // if the menu id is not null
      if (mode != 0xFFFFFFFF) {
        if (menu_open == false) {
          change_code_group_state("beam_change", ModState::DISABLED);
        }

        handle_cursor(cursor_base + 0x9c, cursor_base + 0x15c, 0.95f, 0.90f);
        menu_open = true;
      }
    } else if (HandleReticleLockOn()) {
      handle_cursor(cursor_base + 0x9c, cursor_base + 0x15c, 0.95f, 0.90f);
    }
  } else {
    if (menu_open) {
      change_code_group_state("beam_change", ModState::ENABLED);

      menu_open = false;
    }

    set_cursor_pos(0, 0);
    write32(0, cursor_base + 0x9c);
    write32(0, cursor_base + 0x15c);

    calculate_pitch_delta();
    // Grab the arm cannon address, go to its transform field (NOT the
    // Actor's xf @ 0x30!!)
    writef32(pitch, cplayer_address + 0x5f0);
    u32 arm_cannon_model_matrix = read32(cplayer_address + 0xea8) + 0x3b0;
    // For whatever god forsaken reason, writing pitch to the z component of the
    // right vector for this xf makes the gun not lag. Won't fix what ain't broken
    writef32(pitch, arm_cannon_model_matrix + 0x24);
    u32 tweak_player_address = read32(read32(GPR(13) - mp2_static.tweak_player_offset));
    if (mem_check(tweak_player_address)) {
      // This one's stored as degrees instead of radians
      writef32(87.0896f, tweak_player_address + 0x180);
    }

    u32 ball_state = read32(cplayer_address + 0x374);

    if (ball_state == 0) {
      writef32(calculate_yaw_vel(), cplayer_address + 0x178);
    }

    // Nothing new here
    write32(0, cplayer_address + 0x174 + 0x18);
  }
}

void FpsControls::run_mod_mp2_gc() {
  const bool show_crosshair = GetShowGCCrosshair();
  const u32 crosshair_color = show_crosshair ? GetGCCrosshairColor() : 0x4b7ea331;
  const u32 world_address = read32(mp2_gc_static.state_mgr_address + 0x1604);

  change_code_group_state("show_crosshair", show_crosshair ? ModState::ENABLED : ModState::DISABLED);

  if (!mem_check(world_address)) {
    return;
  }
  // World loading phase == 4 -> complete
  if (read32(world_address + 0x4) != 4) {
    return;
  }

  const u32 cplayer_address = read32(mp2_gc_static.state_mgr_address + 0x14fc);
  if (!mem_check(cplayer_address)) {
    return;
  }

  const u32 orbit_state = read32(cplayer_address + 0x3a4);
  if (orbit_state != ORBIT_STATE_GRAPPLE &&
    orbit_state != 0) {
    calculate_pitch_locked(Game::PRIME_2_GCN, GetHackManager()->get_active_region());
    writef32(pitch, cplayer_address + 0x604);
    return;
  }

  if (show_crosshair) {
    write32(crosshair_color, mp2_gc_static.crosshair_color_address);
  }

  const u32 tweak_player_address = read32(read32(GPR(13) - mp2_gc_static.tweak_player_offset));
  if (mem_check(tweak_player_address)) {
    // Freelook rotation speed tweak
    write32(0x4f800000, tweak_player_address + 0x188);
    // Freelook pitch half-angle range tweak
    writef32(87.0896f, tweak_player_address + 0x184);
    // Air translational friction changes to make diagonal strafe match normal speed
    writef32(0.25f, tweak_player_address + 0x88);
    for (int i = 0; i < 8; i++) {
      writef32(100000000.f, tweak_player_address + 0xc4 + i * 4);
      writef32(1.f, tweak_player_address + 0xa4 + i * 4);
    }
  }

  calculate_pitch_delta();
  writef32(pitch, cplayer_address + 0x604);

  const u32 ball_state = read32(cplayer_address + 0x38c);

  if (ball_state == 0) {
    // Forgot to note this in MP1 GC, in trilogy we were using angular momentum
    // whereas we're using angvel here, so divide out Samus' mass (200)
    writef32(calculate_yaw_vel() / 200.f, cplayer_address + 0x1bc);
  }
}

void FpsControls::mp3_handle_cursor(bool lock) {
  u32 cursor_base = read32(read32(mp3_static.cursor_ptr_address) + mp3_static.cursor_offset);
  if (lock) {
    write32(0, cursor_base + 0x9c);
    write32(0, cursor_base + 0x15c);
  }
  else {
    handle_cursor(cursor_base + 0x9c, cursor_base + 0x15c, 0.95f, 0.90f);
  }
}

void FpsControls::mp3_handle_lasso(u32 cplayer_address)
{
  if (GrappleCtlBound()) {
    change_code_group_state("grapple_lasso", ModState::ENABLED);

    // Disable animation code changes if trying to use grapple voltage.
    change_code_group_state("grapple_lasso_animation",
      CheckForward() || CheckBack() ? ModState::DISABLED : ModState::ENABLED);
  }
  else {
    change_code_group_state("grapple_lasso", ModState::DISABLED);
    change_code_group_state("grapple_lasso_animation", ModState::DISABLED);
  }

  // If currently locked onto a grapple point. This must be seperate from lock-on for grapple swing.
  if (read8(cplayer_address + 0x378)) {
    if (grapple_initial_cooldown == 0) {
      grapple_initial_cooldown = Common::Timer::GetTimeMs();
    }
    else if (Common::Timer::GetTimeMs() > grapple_initial_cooldown + 800) {
      if (prime::CheckGrappleCtl()) {
        grapple_button_down = true;
      } else if (grapple_button_down) {
        grapple_tugging = true;
        grapple_button_down = false;

        grapple_swap_axis = !grapple_swap_axis;

        // 0.45 for repeated taps. 1 will instantly complete the grapple.
        grapple_force += GrappleTappingMode() ? 0.45f : 1.0f;
      }

      if (grapple_tugging) {
        constexpr float force_delta = 0.045f;

        // Use tapping/force method
        if (grapple_force > 0) {
          grapple_hand_pos += force_delta;
          grapple_force -= force_delta;
        }
        else {
          grapple_hand_pos -= 0.050f;
          grapple_force = 0;
        }

        grapple_hand_pos = std::clamp(grapple_hand_pos, 0.f, 1.0f);

        prime::GetVariableManager()->set_variable("grapple_hand_x", grapple_hand_pos);
        prime::GetVariableManager()->set_variable("grapple_hand_y", grapple_hand_pos / 4);

        if (grapple_hand_pos == 1.0f) {
          // State 4 completes the grapple (e.g pull door from frame)
          prime::GetVariableManager()->set_variable("grapple_lasso_state", (u32) 4);
        } else {
          // State 2 "holds" the grapple for lasso/voltage.
          prime::GetVariableManager()->set_variable("grapple_lasso_state", (u32) 2);
        }
      }
    } 
  } else {
    prime::GetVariableManager()->set_variable("grapple_hand_x", 0.f);
    prime::GetVariableManager()->set_variable("grapple_hand_y", 0.f);
    prime::GetVariableManager()->set_variable("grapple_lasso_state", (u32) 0);

    grapple_initial_cooldown = 0;
    grapple_hand_pos = 0;
    grapple_force = 0;
    grapple_button_down, grapple_tugging = false;
  }
}

// this game is
void FpsControls::run_mod_mp3(Game active_game, Region active_region) {
  CheckBeamVisorSetting(active_game);

  u32 cplayer_address = 0;

  // Handles menu screen cursor
  if (read8(mp3_static.cursor_dlg_enabled_address)) {
    mp3_handle_cursor(false);
    return;
  }

  // In NTSC-J version there is a quiz to select the difficulty
  // This checks if we are ingame
  if (active_region == Region::NTSC_J && read32(mp3_static.cplayer_ptr_address + 0x298) == 0xFFFFFFFF) {
    mp3_handle_cursor(false);
    return;
  }

  if (active_game == Game::PRIME_3_STANDALONE && active_region == Region::NTSC_U) {
    cplayer_address = read32(read32(mp3_static.cplayer_ptr_address) + 0x2184);
  } else {
    cplayer_address = read32(read32(read32(mp3_static.cplayer_ptr_address) + 0x04) + 0x2184);
  }

  if (!mem_check(cplayer_address)) {
    return;
  }

  // HACK ooo
  powerups_ptr_address = cplayer_address + powerups_ptr_offset;
  handle_beam_visor_switch({}, prime_three_visors);

  u32 boss_name_str = read32(read32(read32(read32(mp3_static.boss_info_address) + 0x6e0) + 0x24) + 0x150);
  bool is_boss_metaridley = is_string_ridley(boss_name_str);

  // Compare based on boss name string, Meta Ridley only appears once
  if (is_boss_metaridley) {
    if (!fighting_ridley) {
      fighting_ridley = true;
      set_state(ModState::CODE_DISABLED);
    }
    mp3_handle_cursor(false);
    return;
  } else {
    if (fighting_ridley) {
      fighting_ridley = false;
      set_state(ModState::ENABLED);
    }
    mp3_handle_cursor(true);
  }

  // Set flag to grapple when possible if the button is being held.
  prime::GetVariableManager()->set_variable("trigger_grapple", prime::CheckGrappleCtl() ? (u32) 1 : (u32) 0);

  u32 pitch_address = cplayer_address + mp3_static.cplayer_pitch_offset;

  bool beamvisor_menu = read32(read32(mp3_static.beamvisor_menu_address) + mp3_static.beamvisor_menu_offset) == 3;

  if (!read8(cplayer_address + 0x378) && read8(mp3_static.lockon_address) || beamvisor_menu) {
    write32(0, cplayer_address + 0x174);
    calculate_pitch_locked(Game::PRIME_3, active_region);

    if (HandleReticleLockOn() || beamvisor_menu) {
      mp3_handle_cursor(false);
    }

    writef32(pitch, pitch_address);

    return;
  }

  // Handle grapple lasso bind
  mp3_handle_lasso(cplayer_address);

  // Lock Camera according to ContextSensitiveControls and interpolate to pitch 0
  if (prime::GetLockCamera() != Unlocked) {
    const float target_pitch = Centre ? 0.f : 0.23f;

    if (pitch == target_pitch) {
      writef32(target_pitch, pitch_address);
      mp3_handle_cursor(false);

      return;
    }

    calculate_pitch_to_target(target_pitch);
    writef32(pitch, pitch_address);

    return;
  }

  mp3_handle_cursor(true);
  set_cursor_pos(0, 0);

  calculate_pitch_delta();
  // Gun damping uses its own TOC value, so screw it (I checked the binary)
  u32 rtoc_gun_damp = GPR(2) - mp3_static.gun_lag_toc_offset;
  write32(0, rtoc_gun_damp);
  writef32(pitch, pitch_address);

  u32 ball_state = read32(cplayer_address + 0x358);

  if (ball_state == 0) {
    writef32(calculate_yaw_vel(), cplayer_address + 0x174);
  }

  // Nothing new here
  write32(0, cplayer_address + 0x174 + 0x18);
}

void FpsControls::CheckBeamVisorSetting(Game game)
{
  bool beam,visor;
  std::tie<bool, bool>(beam, visor) = GetMenuOptions();

  switch (game) {
  case Game::PRIME_1:
  case Game::PRIME_2:
    change_code_group_state("beam_menu", beam ? ModState::DISABLED : ModState::ENABLED);
  case Game::PRIME_3_STANDALONE: 
  case Game::PRIME_3:
    change_code_group_state("visor_menu", visor ? ModState::DISABLED : ModState::ENABLED);
    break;
  }
}

bool FpsControls::init_mod(Game game, Region region) {
  switch (game) {
  case Game::MENU_PRIME_1:
  case Game::MENU_PRIME_2:
    init_mod_menu(game, region);
    break;
  case Game::PRIME_1:
    init_mod_mp1(region);
    break;
  case Game::PRIME_1_GCN:
    init_mod_mp1_gc(region);
    break;
  case Game::PRIME_2:
    init_mod_mp2(region);
    break;
  case Game::PRIME_2_GCN:
    init_mod_mp2_gc(region);
    break;
  case Game::PRIME_3:
    init_mod_mp3(region);
    break;
  case Game::PRIME_3_STANDALONE:
    init_mod_mp3_standalone(region);
    break;
  }
  return true;
}

void FpsControls::add_beam_change_code_mp1(u32 start_point) {
  add_code_change(start_point + 0x00, 0x3c80804a, "beam_change");  // lis    r4, 0x804a      ; set r4 to beam change base address
  add_code_change(start_point + 0x04, 0x388479f0, "beam_change");  // addi   r4, r4, 0x79f0  ; (0x804a79f0)
  add_code_change(start_point + 0x08, 0x80640000, "beam_change");  // lwz    r3, 0(r4)       ; grab flag
  add_code_change(start_point + 0x0c, 0x2c030000, "beam_change");  // cmpwi  r3, 0           ; check if beam should change
  add_code_change(start_point + 0x10, 0x41820058, "beam_change");  // beq    0x58            ; don't attempt beam change if 0
  add_code_change(start_point + 0x14, 0x83440004, "beam_change");  // lwz    r26, 4(r4)      ; get expected beam (r25, r26 used to assign beam)
  add_code_change(start_point + 0x18, 0x7f59d378, "beam_change");  // mr     r25, r26        ; copy expected beam to other reg
  add_code_change(start_point + 0x1c, 0x38600000, "beam_change");  // li     r3, 0           ; reset flag
  add_code_change(start_point + 0x20, 0x90640000, "beam_change");  // stw    r3, 0(r4)       ;
  add_code_change(start_point + 0x24, 0x48000044, "beam_change");  // b      0x44            ; jump forward to beam assign
}

void FpsControls::add_beam_change_code_mp2(u32 start_point) {
  add_code_change(start_point + 0x00, 0x3c80804d, "beam_change");  // lis    r4, 0x804d      ; set r4 to beam change base address
  add_code_change(start_point + 0x04, 0x3884d250, "beam_change");  // subi   r4, r4, 0x2db0  ; (0x804cd250)
  add_code_change(start_point + 0x08, 0x80640000, "beam_change");  // lwz    r3, 0(r4)       ; grab flag
  add_code_change(start_point + 0x0c, 0x2c030000, "beam_change");  // cmpwi  r3, 0           ; check if beam should change
  add_code_change(start_point + 0x10, 0x4182005c, "beam_change");  // beq    0x5c            ; don't attempt beam change if 0
  add_code_change(start_point + 0x14, 0x83e40004, "beam_change");  // lwz    r31, 4(r4)      ; get expected beam (r31, r30 used to assign beam)
  add_code_change(start_point + 0x18, 0x7ffefb78, "beam_change");  // mr     r30, r31        ; copy expected beam to other reg
  add_code_change(start_point + 0x1c, 0x38600000, "beam_change");  // li     r3, 0           ; reset flag
  add_code_change(start_point + 0x20, 0x90640000, "beam_change");  // stw    r3, 0(r4)       ;
  add_code_change(start_point + 0x24, 0x48000048, "beam_change");  // b      0x48            ; jump forward to beam assign
}

void FpsControls::add_grapple_slide_code_mp3(u32 start_point) {
  add_code_change(start_point + 0x00, 0x60000000);  // nop                  ; trashed because useless
  add_code_change(start_point + 0x04, 0x60000000);  // nop                  ; trashed because useless
  add_code_change(start_point + 0x08, 0x60000000);  // nop                  ; trashed because useless
  add_code_change(start_point + 0x0c, 0xc0010240);  // lfs  f0, 0x240(r1)   ; grab the x component of new origin
  add_code_change(start_point + 0x10, 0xd01f0048);  // stfs f0, 0x48(r31)   ; store it into player's xform x origin (CTransform + 0x0c)
  add_code_change(start_point + 0x14, 0xc0010244);  // lfs  f0, 0x244(r1)   ; grab the y component of new origin
  add_code_change(start_point + 0x18, 0xd01f0058);  // stfs f0, 0x58(r31)   ; store it into player's xform y origin (CTransform + 0x1c)
  add_code_change(start_point + 0x1c, 0xc0010248);  // lfs  f0, 0x248(r1)   ; grab the z component of new origin
  add_code_change(start_point + 0x20, 0xd01f0068);  // stfs f0, 0x68(r31)   ; store it into player's xform z origin (CTransform + 0xcc)
  add_code_change(start_point + 0x28, 0x389f003c);  // addi r4, r31, 0x3c   ; next sub call is SetTransform, so set player's transform
                                                              // ; to their own transform (safety no-op, does other updating too)
}

void FpsControls::add_grapple_lasso_code_mp3(u32 func1, u32 func2, u32 func3) {
  u32 lis_x, ori_x, lis_y, ori_y;

  std::tie<u32, u32>(lis_x, ori_x) = prime::GetVariableManager()->make_lis_ori(11, "grapple_hand_x");
  std::tie<u32, u32>(lis_y, ori_y) = prime::GetVariableManager()->make_lis_ori(11, "grapple_hand_y");

  // Controls how tight the lasso is and if to turn yellow.
  add_code_change(func1, lis_x, "grapple_lasso");
  add_code_change(func1 + 0x4, ori_x, "grapple_lasso");
  add_code_change(func1 + 0xC, 0xC04B0000, "grapple_lasso");  // lfs f2, 0(r11)

                                                              // Skips the game's checks and lets us control grapple lasso ourselves.
  add_code_change(func1 + 0x8, 0x40800160, "grapple_lasso"); // first conditional branch changed to jmp to end
  add_code_change(func1 + 0x18, 0x40810148, "grapple_lasso"); // second conditional branch changed to jmp to end
  add_code_change(func1 + 0x54, 0x4800010C, "grapple_lasso"); // end of 'yellow' segment jmp to end

  // Controls the pulling animation.
  add_code_change(func2, lis_x, "grapple_lasso_animation");
  add_code_change(func2 + 0x4, ori_x, "grapple_lasso_animation");
  add_code_change(func2 + 0x8, 0xC00B0000, "grapple_lasso_animation");  // lfs f0, 0(r11)
  add_code_change(func2 + 0xC, lis_y, "grapple_lasso_animation");
  add_code_change(func2 + 0x10, ori_y, "grapple_lasso_animation");
  add_code_change(func2 + 0x14, 0xC04B0000, "grapple_lasso_animation");  // lfs f2, 0(r11)
  add_code_change(func2 + 0x18, 0xD05701C8, "grapple_lasso_animation");  // stfs f2, 0x1C8(r23)

  u32 lis, ori;
  // Controls the return value of the "ProcessGrappleLasso" function.
  std::tie<u32, u32>(lis, ori) = prime::GetVariableManager()->make_lis_ori(30, "grapple_lasso_state");
  add_code_change(func1 + 0x160, lis, "grapple_lasso");
  add_code_change(func1 + 0x164, ori, "grapple_lasso");
  add_code_change(func1 + 0x168, 0x83DE0000, "grapple_lasso"); // lwz r30, 0(r30)

  // Triggers grapple.
  std::tie<u32, u32>(lis, ori) = prime::GetVariableManager()->make_lis_ori(3, "trigger_grapple");
  add_code_change(func3 + 0x0, lis, "grapple_lasso");
  add_code_change(func3 + 0x4, ori, "grapple_lasso");
  add_code_change(func3 + 0x8, 0x80630000, "grapple_lasso"); // lwz r3, 0(r3)
  add_code_change(func3 + 0xC, 0x4E800020, "grapple_lasso"); // blr
}

void FpsControls::add_control_state_hook_mp3(u32 start_point, Region region) {
  Game active_game = GetHackManager()->get_active_game();
  if (region == Region::NTSC_U) {
    if (active_game == Game::PRIME_3) {
      add_code_change(start_point + 0x00, 0x3c60805c);  // lis  r3, 0x805c
      add_code_change(start_point + 0x04, 0x38636c40);  // addi r3, r3, 0x6c40
    } else {
      add_code_change(start_point + 0x00, 0x3c60805c);  // lis  r3, 0x805c
      add_code_change(start_point + 0x04, 0x38634f6c);  // addi r3, r3, 0x4f6c
    }
  } else if (region == Region::NTSC_J) {
    if (active_game == Game::PRIME_3_STANDALONE)
    {
      add_code_change(start_point + 0x00, 0x3c60805d);  // lis  r3, 0x805d
      add_code_change(start_point + 0x04, 0x3863aa30);  // subi r3, r3, 0x55d0
    }
  } else if (region == Region::PAL) {
    if (active_game == Game::PRIME_3) {
      add_code_change(start_point + 0x00, 0x3c60805d);  // lis  r3, 0x805d
      add_code_change(start_point + 0x04, 0x3863a0c0);  // subi r3, r3, 0x5f40
    }
    else {
      add_code_change(start_point + 0x00, 0x3c60805c);  // lis  r3, 0x805c
      add_code_change(start_point + 0x04, 0x38637570);  // addi r3, r3, 0x7570
    }
  }
  add_code_change(start_point + 0x08, 0x8063002c);  // lwz  r3, 0x2c(r3)
  if (active_game == Game::PRIME_3_STANDALONE && region == Region::NTSC_U) {
    add_code_change(start_point + 0x0c, 0x60000000);  // nop
  } else {
    add_code_change(start_point + 0x0c, 0x80630004);  // lwz  r3, 0x04(r3)
  }
  add_code_change(start_point + 0x10, 0x80632184);  // lwz  r3, 0x2184(r3)
  add_code_change(start_point + 0x14, 0x7c03f800);  // cmpw r3, r31
  add_code_change(start_point + 0x18, 0x4d820020);  // beqlr
  add_code_change(start_point + 0x1c, 0x7fe3fb78);  // mr   r3, r31
  add_code_change(start_point + 0x20, 0x90c30078);  // stw  r6, 0x78(r3)
  add_code_change(start_point + 0x24, 0x4e800020);  // blr
}

// Truly cursed
void FpsControls::add_strafe_code_mp1_ntsc() {
  // calculate side movement @ 805afc00
  // stwu r1, 0x18(r1) 
  // mfspr r0, LR
  // stw r0, 0x1c(r1) 
  // lwz r5, -0x5ee8(r13) 
  // lwz r4, 0x2b0(r29) 
  // cmpwi r4, 2 
  // li r4, 4 
  // bne 0x8 
  // lwz r4, 0x2ac(r29) 
  // slwi r4, r4, 2 
  // add r3, r4, r5
  // lfs f1, 0x44(r3) 
  // lfs f2, 0x4(r3) 
  // fmuls f3, f2, f27
  // lfs f0, 0xe8(r29)
  // fmuls f1, f1, f0
  // fdivs f1, f1, f3
  // lfs f0, 0xa4(r3) 
  // stfs f0, 0x10(r1)
  // fmuls f1, f1, f0 
  // lfs f0, -0x4260(r2) 
  // fcmpo cr0, f30, f0
  // lfs f0, -0x4238(r2) 
  // ble 0x8
  // lfs f0, -0x4280(r2) 
  // fmuls f0, f0, f1
  // lfs f3, 0x10(r1) 
  // fsubs f3, f3, f1
  // fmuls f3, f3, f30
  // fadds f0, f0, f3 
  // stfs f0, 0x18(r1)
  // stfs f2, 0x14(r1)
  // addi r3, r1, 0x4
  // addi r4, r29, 0x34
  // addi r5, r29, 0x138
  // bl 0xFFD62D98
  // lfs f0, 0x18(r1)
  // lfs f1, 0x4(r1)
  // fsubs f0, f0, f1
  // lfs f1, 0x10(r1) 
  // fdivs f0, f0, f1
  // lfs f1, -0x4238(r2) 
  // fcmpo cr0, f0, f1
  // bge 0xc
  // fmr f0, f1
  // b 0x14
  // lfs f1, -0x4280(r2)
  // fcmpo cr0, f0, f1
  // ble 0x8
  // fmr f0, f1
  // lfs f1, 0x14(r1)
  // fmuls f1, f0, f1
  // lwz r0, 0x1c(r1)
  // mtspr LR, r0
  // addi r1, r1, -0x18
  // blr
  add_code_change(0x805afc00, 0x94210018);
  add_code_change(0x805afc04, 0x7c0802a6);
  add_code_change(0x805afc08, 0x9001001c);
  add_code_change(0x805afc0c, 0x80ada118);
  add_code_change(0x805afc10, 0x809d02b0);
  add_code_change(0x805afc14, 0x2c040002);
  add_code_change(0x805afc18, 0x38800004);
  add_code_change(0x805afc1c, 0x40820008);
  add_code_change(0x805afc20, 0x809d02ac);
  add_code_change(0x805afc24, 0x5484103a);
  add_code_change(0x805afc28, 0x7c642a14);
  add_code_change(0x805afc2c, 0xc0230044);
  add_code_change(0x805afc30, 0xc0430004);
  add_code_change(0x805afc34, 0xec6206f2);
  add_code_change(0x805afc38, 0xc01d00e8);
  add_code_change(0x805afc3c, 0xec210032);
  add_code_change(0x805afc40, 0xec211824);
  add_code_change(0x805afc44, 0xc00300a4);
  add_code_change(0x805afc48, 0xd0010010);
  add_code_change(0x805afc4c, 0xec210032);
  add_code_change(0x805afc50, 0xc002bda0);
  add_code_change(0x805afc54, 0xfc1e0040);
  add_code_change(0x805afc58, 0xc002bdc8);
  add_code_change(0x805afc5c, 0x40810008);
  add_code_change(0x805afc60, 0xc002bd80);
  add_code_change(0x805afc64, 0xec000072);
  add_code_change(0x805afc68, 0xc0610010);
  add_code_change(0x805afc6c, 0xec630828);
  add_code_change(0x805afc70, 0xec6307b2);
  add_code_change(0x805afc74, 0xec00182a);
  add_code_change(0x805afc78, 0xd0010018);
  add_code_change(0x805afc7c, 0xd0410014);
  add_code_change(0x805afc80, 0x38610004);
  add_code_change(0x805afc84, 0x389d0034);
  add_code_change(0x805afc88, 0x38bd0138);
  add_code_change(0x805afc8c, 0x4bd62d99);
  add_code_change(0x805afc90, 0xc0010018);
  add_code_change(0x805afc94, 0xc0210004);
  add_code_change(0x805afc98, 0xec000828);
  add_code_change(0x805afc9c, 0xc0210010);
  add_code_change(0x805afca0, 0xec000824);
  add_code_change(0x805afca4, 0xc022bdc8);
  add_code_change(0x805afca8, 0xfc000840);
  add_code_change(0x805afcac, 0x4080000c);
  add_code_change(0x805afcb0, 0xfc000890);
  add_code_change(0x805afcb4, 0x48000014);
  add_code_change(0x805afcb8, 0xc022bd80);
  add_code_change(0x805afcbc, 0xfc000840);
  add_code_change(0x805afcc0, 0x40810008);
  add_code_change(0x805afcc4, 0xfc000890);
  add_code_change(0x805afcc8, 0xc0210014);
  add_code_change(0x805afccc, 0xec200072);
  add_code_change(0x805afcd0, 0x8001001c);
  add_code_change(0x805afcd4, 0x7c0803a6);
  add_code_change(0x805afcd8, 0x3821ffe8);
  add_code_change(0x805afcdc, 0x4e800020);

  // Apply strafe force instead of torque @ 802875c4
  // lfs f1, -0x4260(r2)
  // lfs f0, -0x41bc(r2)
  // fsubs f1, f30, f1
  // fabs f1, f1
  // fcmpo cr0, f1, f0
  // ble 0x2c
  // bl 0x328624
  // bl 0xFFD93F54
  // mr r5, r3
  // mr r3, r29
  // lfs f0, -0x4260(r2)
  // stfs f1, 0x10(r1)
  // stfs f0, 0x14(r1)
  // stfs f0, 0x18(r1)
  // addi r4, r1, 0x10
  add_code_change(0x802875c4, 0xc022bda0);
  add_code_change(0x802875c8, 0xc002be44);
  add_code_change(0x802875cc, 0xec3e0828);
  add_code_change(0x802875d0, 0xfc200a10);
  add_code_change(0x802875d4, 0xfc010040);
  add_code_change(0x802875d8, 0x4081002c);
  add_code_change(0x802875dc, 0x48328625);
  add_code_change(0x802875e0, 0x4bd93f55);
  add_code_change(0x802875e4, 0x7c651b78);
  add_code_change(0x802875e8, 0x7fa3eb78);
  add_code_change(0x802875ec, 0xc002bda0);
  add_code_change(0x802875f0, 0xd0210010);
  add_code_change(0x802875f4, 0xd0010014);
  add_code_change(0x802875f8, 0xd0010018);
  add_code_change(0x802875fc, 0x38810010);

  // disable rotation on LR analog
  add_code_change(0x80286fe0, 0x4bfffc71);
  add_code_change(0x80286c88, 0x4800000c);
  add_code_change(0x8028739c, 0x60000000);
  add_code_change(0x802873e0, 0x60000000);
  add_code_change(0x8028707c, 0x60000000);
  add_code_change(0x802871bc, 0x60000000);
  add_code_change(0x80287288, 0x60000000);

  // Clamp current xy velocity @ 802872A4
  // lfs f1, -0x7ec0(r2)
  // fmuls f0, f30, f30
  // fcmpo cr0, f0, f1
  // ble 0x134
  // fmuls f0, f31, f31m
  // fcmpo cr0, f0, f1
  // ble 0x128
  // lfs f0, 0x138(r29)
  // lfs f1, 0x13c(r29)
  // fmuls f0, f0, f0
  // fmadds f1, f1, f1, f0
  // frsqrte f1, f1
  // fres f1, f1
  // addi r3, r2, -0x2040
  // slwi r0, r0, 2
  // add r3, r0, r3
  // lfs f0, 0(r3)
  // fcmpo cr0, f1, f0
  // ble 0xf8
  // lfs f3, 0xe8(r29)
  // lfs f2, 0x138(r29)
  // fdivs f2, f2, f1
  // fmuls f2, f0, f2
  // stfs f2, 0x138(r29)
  // fmuls f2, f3, f2
  // stfs f2, 0xfc(r29)
  // lfs f2, 0x13c(r29)
  // fdivs f2, f2, f1
  // fmuls f2, f0, f2
  // stfs f2, 0x13c(r29)
  // fmuls f2, f3, f2
  // stfs f2, 0x100(r29)
  // b 0xc0
  add_code_change(0x802872a4, 0xc0228140);
  add_code_change(0x802872a8, 0xec1e07b2);
  add_code_change(0x802872ac, 0xfc000840);
  add_code_change(0x802872b0, 0x40810134);
  add_code_change(0x802872b4, 0xec1f07f2);
  add_code_change(0x802872b8, 0xfc000840);
  add_code_change(0x802872bc, 0x40810128);
  add_code_change(0x802872c0, 0xc01d0138);
  add_code_change(0x802872c4, 0xc03d013c);
  add_code_change(0x802872c8, 0xec000032);
  add_code_change(0x802872cc, 0xec21007a);
  add_code_change(0x802872d0, 0xfc200834);
  add_code_change(0x802872d4, 0xec200830);
  add_code_change(0x802872d8, 0x3862dfc0);
  add_code_change(0x802872dc, 0x5400103a);
  add_code_change(0x802872e0, 0x7c601a14);
  add_code_change(0x802872e4, 0xc0030000);
  add_code_change(0x802872e8, 0xfc010040);
  add_code_change(0x802872ec, 0x408100f8);
  add_code_change(0x802872f0, 0xc07d00e8);
  add_code_change(0x802872f4, 0xc05d0138);
  add_code_change(0x802872f8, 0xec420824);
  add_code_change(0x802872fc, 0xec4000b2);
  add_code_change(0x80287300, 0xd05d0138);
  add_code_change(0x80287304, 0xec4300b2);
  add_code_change(0x80287308, 0xd05d00fc);
  add_code_change(0x8028730c, 0xc05d013c);
  add_code_change(0x80287310, 0xec420824);
  add_code_change(0x80287314, 0xec4000b2);
  add_code_change(0x80287318, 0xd05d013c);
  add_code_change(0x8028731c, 0xec4300b2);
  add_code_change(0x80287320, 0xd05d0100);
  add_code_change(0x80287324, 0x480000c0);

  // max speed values table @ 805afce0
  add_code_change(0x805afce0, 0x41480000);
  add_code_change(0x805afce4, 0x41480000);
  add_code_change(0x805afce8, 0x41480000);
  add_code_change(0x805afcec, 0x41480000);
  add_code_change(0x805afcf0, 0x41480000);
  add_code_change(0x805afcf4, 0x41480000);
  add_code_change(0x805afcf8, 0x41480000);
  add_code_change(0x805afcfc, 0x41480000);
}

void FpsControls::add_strafe_code_mp1_pal() {
  // calculate side movement @ 80471c00
  // stwu r1, 0x18(r1) 
  // mfspr r0, LR
  // stw r0, 0x1c(r1) 
  // lwz r5, -0x5e70(r13)
  // lwz r4, 0x2c0(r29) 
  // cmpwi r4, 2
  // li r4, 4
  // bne 0x8
  // lwz r4, 0x2bc(r29) 
  // slwi r4, r4, 2 
  // add r3, r4, r5
  // lfs f1, 0x44(r3)
  // lfs f2, 0x4(r3) 
  // fmuls f3, f2, f27
  // lfs f0, 0xf8(r29)
  // fmuls f1, f1, f0
  // fdivs f1, f1, f3
  // lfs f0, 0xa4(r3) 
  // stfs f0, 0x10(r1)
  // fmuls f1, f1, f0 
  // lfs f0, -0x4180(r2)
  // fcmpo cr0, f30, f0
  // lfs f0, -0x4158(r2)
  // ble 0x8
  // lfs f0, -0x41A0(r2)
  // fmuls f0, f0, f1
  // lfs f3, 0x10(r1) 
  // fsubs f3, f3, f1
  // fmuls f3, f3, f30
  // fadds f0, f0, f3 
  // stfs f0, 0x18(r1)
  // stfs f2, 0x14(r1)
  // addi r3, r1, 0x4
  // addi r4, r29, 0x34
  // addi r5, r29, 0x148
  // bl 0xffe89b68
  // lfs f0, 0x18(r1)
  // lfs f1, 0x4(r1)
  // fsubs f0, f0, f1
  // lfs f1, 0x10(r1) 
  // fdivs f0, f0, f1
  // lfs f1, -0x4158(r2)
  // fcmpo cr0, f0, f1
  // bge 0xc
  // fmr f0, f1
  // b 0x14
  // lfs f1, -0x41A0(r2)
  // fcmpo cr0, f0, f1
  // ble 0x8
  // fmr f0, f1
  // lfs f1, 0x14(r1)
  // fmuls f1, f0, f1
  // lwz r0, 0x1c(r1)
  // mtspr LR, r0
  // addi r1, r1, -0x18
  // blr
  add_code_change(0x80471c00, 0x94210018);
  add_code_change(0x80471c04, 0x7c0802a6);
  add_code_change(0x80471c08, 0x9001001c);
  add_code_change(0x80471c0c, 0x80ada190);
  add_code_change(0x80471c10, 0x809d02c0);
  add_code_change(0x80471c14, 0x2c040002);
  add_code_change(0x80471c18, 0x38800004);
  add_code_change(0x80471c1c, 0x40820008);
  add_code_change(0x80471c20, 0x809d02bc);
  add_code_change(0x80471c24, 0x5484103a);
  add_code_change(0x80471c28, 0x7c642a14);
  add_code_change(0x80471c2c, 0xc0230044);
  add_code_change(0x80471c30, 0xc0430004);
  add_code_change(0x80471c34, 0xec6206f2);
  add_code_change(0x80471c38, 0xc01d00f8);
  add_code_change(0x80471c3c, 0xec210032);
  add_code_change(0x80471c40, 0xec211824);
  add_code_change(0x80471c44, 0xc00300a4);
  add_code_change(0x80471c48, 0xd0010010);
  add_code_change(0x80471c4c, 0xec210032);
  add_code_change(0x80471c50, 0xc002be80);
  add_code_change(0x80471c54, 0xfc1e0040);
  add_code_change(0x80471c58, 0xc002bea8);
  add_code_change(0x80471c5c, 0x40810008);
  add_code_change(0x80471c60, 0xc002be60);
  add_code_change(0x80471c64, 0xec000072);
  add_code_change(0x80471c68, 0xc0610010);
  add_code_change(0x80471c6c, 0xec630828);
  add_code_change(0x80471c70, 0xec6307b2);
  add_code_change(0x80471c74, 0xec00182a);
  add_code_change(0x80471c78, 0xd0010018);
  add_code_change(0x80471c7c, 0xd0410014);
  add_code_change(0x80471c80, 0x38610004);
  add_code_change(0x80471c84, 0x389d0034);
  add_code_change(0x80471c88, 0x38bd0148);
  add_code_change(0x80471c8c, 0x4be89b69);
  add_code_change(0x80471c90, 0xc0010018);
  add_code_change(0x80471c94, 0xc0210004);
  add_code_change(0x80471c98, 0xec000828);
  add_code_change(0x80471c9c, 0xc0210010);
  add_code_change(0x80471ca0, 0xec000824);
  add_code_change(0x80471ca4, 0xc022bea8);
  add_code_change(0x80471ca8, 0xfc000840);
  add_code_change(0x80471cac, 0x4080000c);
  add_code_change(0x80471cb0, 0xfc000890);
  add_code_change(0x80471cb4, 0x48000014);
  add_code_change(0x80471cb8, 0xc022be60);
  add_code_change(0x80471cbc, 0xfc000840);
  add_code_change(0x80471cc0, 0x40810008);
  add_code_change(0x80471cc4, 0xfc000890);
  add_code_change(0x80471cc8, 0xc0210014);
  add_code_change(0x80471ccc, 0xec200072);
  add_code_change(0x80471cd0, 0x8001001c);
  add_code_change(0x80471cd4, 0x7c0803a6);
  add_code_change(0x80471cd8, 0x3821ffe8);
  add_code_change(0x80471cdc, 0x4e800020);

  // Apply strafe force instead of torque @ 802749a8
  // lfs f1, -0x4180(r2)
  // lfs f0, -0x40DC(r2)
  // fsubs f1, f30, f1
  // fabs f1, f1
  // fcmpo cr0, f1, f0
  // ble 0x2c
  // bl 0x1fd240
  // bl 0xffda74e0
  // mr r5, r3
  // mr r3, r29
  // lfs f0, -0x4180(r2)
  // stfs f1, 0x10(r1)
  // stfs f0, 0x14(r1)
  // stfs f0, 0x18(r1)
  // addi r4, r1, 0x10
  add_code_change(0x802749a8, 0xc022be80);
  add_code_change(0x802749ac, 0xc002bf24);
  add_code_change(0x802749b0, 0xec3e0828);
  add_code_change(0x802749b4, 0xfc200a10);
  add_code_change(0x802749b8, 0xfc010040);
  add_code_change(0x802749bc, 0x4081002c);
  add_code_change(0x802749c0, 0x481fd241);
  add_code_change(0x802749c4, 0x4bda74e1);
  add_code_change(0x802749c8, 0x7c651b78);
  add_code_change(0x802749cc, 0x7fa3eb78);
  add_code_change(0x802749d0, 0xc002be80);
  add_code_change(0x802749d4, 0xd0210010);
  add_code_change(0x802749d8, 0xd0010014);
  add_code_change(0x802749dc, 0xd0010018);
  add_code_change(0x802749e0, 0x38810010);

  // disable rotation on LR analog
  add_code_change(0x802743C4, 0x4bfffc71); // jump/address updated
  add_code_change(0x8027406C, 0x4800000c); // updated following addresses
  add_code_change(0x80274780, 0x60000000);
  add_code_change(0x802747C4, 0x60000000);
  add_code_change(0x80274460, 0x60000000);
  add_code_change(0x802745A0, 0x60000000);
  add_code_change(0x8027466C, 0x60000000);

  // Clamp current xy velocity @ 80274688
  // lfs f1, -0x7ec0(r2)
  // fmuls f0, f30, f30
  // fcmpo cr0, f0, f1
  // ble 0x134
  // fmuls f0, f31, f31
  // fcmpo cr0, f0, f1
  // ble 0x128
  // lfs f0, 0x148(r29)
  // lfs f1, 0x14c(r29)
  // fmuls f0, f0, f0
  // fmadds f1, f1, f1, f0
  // frsqrte f1, f1
  // fres f1, f1
  // addi r3, r2, -0x2180
  // slwi r0, r0, 2
  // add r3, r0, r3
  // lfs f0, 0(r3)
  // fcmpo cr0, f1, f0
  // ble 0xf8
  // lfs f3, 0xf8(r29)
  // lfs f2, 0x148(r29)
  // fdivs f2, f2, f1
  // fmuls f2, f0, f2
  // stfs f2, 0x148(r29)
  // fmuls f2, f3, f2
  // stfs f2, 0x10c(r29)
  // lfs f2, 0x14c(r29)
  // fdivs f2, f2, f1
  // fmuls f2, f0, f2
  // stfs f2, 0x14c(r29)
  // fmuls f2, f3, f2
  // stfs f2, 0x110(r29)
  // b 0xc0
  add_code_change(0x80274688, 0xc0228140);
  add_code_change(0x8027468c, 0xec1e07b2);
  add_code_change(0x80274690, 0xfc000840);
  add_code_change(0x80274694, 0x40810134);
  add_code_change(0x80274698, 0xec1f07f2);
  add_code_change(0x8027469c, 0xfc000840);
  add_code_change(0x802746a0, 0x40810128);
  add_code_change(0x802746a4, 0xc01d0148);
  add_code_change(0x802746a8, 0xc03d014c);
  add_code_change(0x802746ac, 0xec000032);
  add_code_change(0x802746b0, 0xec21007a);
  add_code_change(0x802746b4, 0xfc200834);
  add_code_change(0x802746b8, 0xec200830);
  add_code_change(0x802746bc, 0x3862de80);
  add_code_change(0x802746c0, 0x5400103a);
  add_code_change(0x802746c4, 0x7c601a14);
  add_code_change(0x802746c8, 0xc0030000);
  add_code_change(0x802746cc, 0xfc010040);
  add_code_change(0x802746d0, 0x408100f8);
  add_code_change(0x802746d4, 0xc07d00f8);
  add_code_change(0x802746d8, 0xc05d0148);
  add_code_change(0x802746dc, 0xec420824);
  add_code_change(0x802746e0, 0xec4000b2);
  add_code_change(0x802746e4, 0xd05d0148);
  add_code_change(0x802746e8, 0xec4300b2);
  add_code_change(0x802746ec, 0xd05d010c);
  add_code_change(0x802746f0, 0xc05d014c);
  add_code_change(0x802746f4, 0xec420824);
  add_code_change(0x802746f8, 0xec4000b2);
  add_code_change(0x802746fc, 0xd05d014c);
  add_code_change(0x80274700, 0xec4300b2);
  add_code_change(0x80274704, 0xd05d0110);
  add_code_change(0x80274708, 0x480000c0);

  // No change needed
  // max speed values table @ 80471ce0
  add_code_change(0x80471ce0, 0x41480000);
  add_code_change(0x80471ce4, 0x41480000);
  add_code_change(0x80471ce8, 0x41480000);
  add_code_change(0x80471cec, 0x41480000);
  add_code_change(0x80471cf0, 0x41480000);
  add_code_change(0x80471cf4, 0x41480000);
  add_code_change(0x80471cf8, 0x41480000);
  add_code_change(0x80471cfc, 0x41480000);
}

void FpsControls::init_mod_menu(Game game, Region region)
{
  if (region == Region::NTSC_J) {
    if (game == Game::MENU_PRIME_1) {
      // prevent wiimote pointer feedback to move the cursor
      add_code_change(0x80487160, 0x60000000);
      add_code_change(0x80487164, 0x60000000);
      // Prevent recentering the cursor on Y axis
      add_code_change(0x80487098, 0x60000000);
    }
    if (game == Game::MENU_PRIME_2) {
      // prevent wiimote pointer feedback to move the cursor
      add_code_change(0x80486fe8, 0x60000000);
      add_code_change(0x80486fec, 0x60000000);
      // Prevent recentering the cursor on X axis
      add_code_change(0x80486f18, 0x60000000);
      // Prevent recentering the cursor on Y axis
      add_code_change(0x80486f20, 0x60000000);
    }
  }
}

void FpsControls::init_mod_mp1(Region region) {
  if (region == Region::NTSC_U) {
    // This instruction change is used in all 3 games, all 3 regions. It's an update to what I believe
    // to be interpolation for player camera pitch The change is from fmuls f0, f0, f1 (0xec000072) to
    // fmuls f0, f1, f1 (0xec010072), where f0 is the output. The higher f0 is, the faster the pitch
    // *can* change in-game, and squaring f1 seems to do the job.
    add_code_change(0x80098ee4, 0xec010072);

    // NOPs for changing First Person Camera's pitch based on floor (normal? angle?)
    add_code_change(0x80099138, 0x60000000);
    add_code_change(0x80183a8c, 0x60000000);
    add_code_change(0x80183a64, 0x60000000);
    add_code_change(0x8017661c, 0x60000000);
    // Cursor location, sets to f17 (always 0 due to little use)
    add_code_change(0x802fb5b4, 0xd23f009c);
    add_code_change(0x8019fbcc, 0x60000000);

    add_code_change(0x80075f24, 0x60000000, "beam_menu");
    add_code_change(0x80075f0c, 0x60000000, "visor_menu");

    add_beam_change_code_mp1(0x8018e544);

    mp1_static.yaw_vel_address = 0x804d3d38;
    mp1_static.pitch_address = 0x804d3ffc;
    mp1_static.pitch_goal_address = 0x804c10ec;
    mp1_static.angvel_limiter_address = 0x804d3d74;
    mp1_static.orbit_state_address = 0x804d3f20;
    mp1_static.lockon_address = 0x804c00b3;
    mp1_static.tweak_player_address = 0x804ddff8;
    mp1_static.cplayer_address = 0x804d3c20;
    mp1_static.object_list_ptr_address = 0x804bfc30;
    mp1_static.camera_uid_address = 0x804c4a08;
    mp1_static.beamvisor_menu_address = 0x805c28b0;
    mp1_static.beamvisor_menu_offset = 0x32c; // 0x1a8 + 0x184
    mp1_static.cursor_base_address = 0x805c28a8;
    mp1_static.cursor_offset = 0xc54;
    powerups_ptr_address = 0x804bfcd4;
  } else if (region == Region::PAL) {
    // Same as NTSC but slightly offset
    add_code_change(0x80099068, 0xec010072);
    add_code_change(0x800992c4, 0x60000000);
    add_code_change(0x80183cfc, 0x60000000);
    add_code_change(0x80183d24, 0x60000000);
    add_code_change(0x801768b4, 0x60000000);
    add_code_change(0x802fb84c, 0xd23f009c);
    add_code_change(0x8019fe64, 0x60000000);

    add_code_change(0x80075f74, 0x60000000, "beam_menu");
    add_code_change(0x80075f8c, 0x60000000, "visor_menu");

    add_beam_change_code_mp1(0x8018e7dc);

    mp1_static.yaw_vel_address = 0x804d7c78;
    mp1_static.pitch_address = 0x804d7f3c;
    mp1_static.pitch_goal_address = 0x804c502c;
    mp1_static.angvel_limiter_address = 0x804d7cb4;
    mp1_static.orbit_state_address = 0x804d7e60;
    mp1_static.lockon_address = 0x804c3ff3;
    mp1_static.tweak_player_address = 0x804e1f38;
    mp1_static.cplayer_address = 0x804d7b60;
    mp1_static.object_list_ptr_address = 0x804c3b70;
    mp1_static.camera_uid_address = 0x804c8948;
    mp1_static.beamvisor_menu_address = 0x805c6c34;
    mp1_static.beamvisor_menu_offset = 0x32c; // 0x1a8 + 0x184
    mp1_static.cursor_base_address = 0x805c6c2c;
    mp1_static.cursor_offset = 0xd04;
    powerups_ptr_address = 0x804c3c14;
  } else { // region == Region::NTSC-J
    // Same as NTSC but slightly offset
    add_code_change(0x80099060, 0xec010072);
    add_code_change(0x800992b4, 0x60000000);
    add_code_change(0x8018460c, 0x60000000);
    add_code_change(0x801835e4, 0x60000000);
    add_code_change(0x80176ff0, 0x60000000);
    add_code_change(0x802fb234, 0xd23f009c);
    add_code_change(0x801a074c, 0x60000000);

    add_code_change(0x800760a4, 0x60000000, "beam_menu");
    add_code_change(0x8007608c, 0x60000000, "visor_menu");

    add_beam_change_code_mp1(0x8018f0c4);

    mp1_static.yaw_vel_address = 0x804d3fb8;
    mp1_static.pitch_address = 0x804d427c;
    mp1_static.pitch_goal_address = 0x804c136c;
    mp1_static.angvel_limiter_address = 0x804d3ff4;
    mp1_static.orbit_state_address = 0x804d41a0;
    mp1_static.lockon_address = 0x804c0333;
    mp1_static.tweak_player_address = 0x804de278;
    mp1_static.cplayer_address = 0x804d3ea0;
    mp1_static.object_list_ptr_address = 0x804bfeb0;
    mp1_static.camera_uid_address = 0x804c4c88;
    mp1_static.beamvisor_menu_address = 0x80642b98;
    mp1_static.beamvisor_menu_offset = 0x338; // 0x1bc + 0x184
    mp1_static.cursor_base_address = 0x80642b90;
    mp1_static.cursor_offset = 0xc54;
    powerups_ptr_address = 0x804bff54;
  }

  active_visor_offset = 0x1c;
  powerups_size = 8;
  powerups_offset = 0x30;
  // I tried matching these two between ntsc & pal for whatever reason
  new_beam_address = 0x804a79f4;
  beamchange_flag_address = 0x804a79f0;
  has_beams = true;
}

void FpsControls::init_mod_mp1_gc(Region region) {
  u8 version = read8(0x80000007);

  if (region == Region::NTSC_U) {
    if (version == 0) {
      add_code_change(0x8000f63c, 0x48000048);
      add_code_change(0x8000e538, 0x60000000);
      add_code_change(0x80014820, 0x4e800020);
      add_code_change(0x8000e73c, 0x60000000);
      add_code_change(0x8000f810, 0x48000244);
      add_code_change(0x8045c488, 0x4f800000);
      // When attached to a grapple point and spinning around it
      // the player's yaw is adjusted, this ensures only position is updated
      // Grapple point yaw fix
      add_code_change(0x8017a18c, 0x7fa3eb78);
      add_code_change(0x8017a190, 0x3881006c);
      add_code_change(0x8017a194, 0x4bed8cf9);
      // Show crosshair but don't consider pressing R button
      add_code_change(0x80016ee4, 0x3b000001, "show_crosshair"); // li r24, 1
      add_code_change(0x80016ee8, 0x8afd09c4, "show_crosshair"); // lbz r23, 0x9c4(r29)
      add_code_change(0x80016eec, 0x53173672, "show_crosshair"); // rlwimi r23, r24, 6, 25, 25 (00000001)
      add_code_change(0x80016ef0, 0x9afd09c4, "show_crosshair"); // stb r23, 0x9c4(r29)
      add_code_change(0x80016ef4, 0x4e800020, "show_crosshair"); // blr

      add_strafe_code_mp1_ntsc();

      mp1_gc_static.yaw_vel_address = 0x8046B97C + 0x14C;
      mp1_gc_static.pitch_address = 0x8046B97C + 0x3EC;
      mp1_gc_static.angvel_max_address = 0x8045c208 + 0x84;
      mp1_gc_static.orbit_state_address = 0x8046b97c + 0x304;
      mp1_gc_static.tweak_player_address = 0x8045c208;
      mp1_gc_static.cplayer_address = 0x8046B97C;
      mp1_gc_static.state_mgr_address = 0x8045a1a8;
      mp1_gc_static.camera_uid_address = 0x8045c5b4;
      mp1_gc_static.crosshair_color_address = 0x8045b678;
    }
  } else if (region == Region::PAL) {
    add_code_change(0x8000fb4c, 0x48000048);  
    add_code_change(0x8000ea60, 0x60000000);
    add_code_change(0x80015258, 0x4e800020);
    add_code_change(0x8000ec64, 0x60000000);
    add_code_change(0x8000fd20, 0x4800022c);
    add_code_change(0x803e43b4, 0x4f800000);
    // Grapple point yaw fix
    add_code_change(0x8016fc54, 0x7fa3eb78);
    add_code_change(0x8016fc58, 0x38810064); // 6c-8 = 64
    add_code_change(0x8016fc5c, 0x4bee4345); // bl 80053fa0
    // Show crosshair but don't consider pressing R button
    add_code_change(0x80017878, 0x3b000001, "show_crosshair"); // li r24, 1
    add_code_change(0x8001787c, 0x8afd09d4, "show_crosshair"); // lbz r23, 0x9d4(r29)
    add_code_change(0x80017880, 0x53173672, "show_crosshair"); // rlwimi r23, r24, 6, 25, 25 (00000001)
    add_code_change(0x80017884, 0x9afd09d4, "show_crosshair"); // stb r23, 0x9d4(r29)
    add_code_change(0x80017888, 0x4e800020, "show_crosshair"); // blr

    add_strafe_code_mp1_pal();

    mp1_gc_static.yaw_vel_address = 0x803F38B4 + 0x14C;
    mp1_gc_static.pitch_address = 0x803F38B4 + 0x3ec;
    mp1_gc_static.angvel_max_address = 0x803E4134 + 0x84;
    mp1_gc_static.orbit_state_address = 0x803F38B4 + 0x304;
    mp1_gc_static.tweak_player_address = 0x803E4134;
    mp1_gc_static.cplayer_address = 0x803F38B4;
    mp1_gc_static.state_mgr_address = 0x803e2088;
    mp1_gc_static.camera_uid_address = 0x803e44dc;
    mp1_gc_static.crosshair_color_address = 0x803e35a4;
  } else {}
}

void FpsControls::init_mod_mp2(Region region) {
  if (region == Region::NTSC_U) {
    add_code_change(0x8008ccc8, 0xc0430184);
    add_code_change(0x8008cd1c, 0x60000000);
    add_code_change(0x80147f70, 0x60000000);
    add_code_change(0x80147f98, 0x60000000);
    add_code_change(0x80135b20, 0x60000000);
    add_code_change(0x8008bb48, 0x60000000);
    add_code_change(0x8008bb18, 0x60000000);
    add_code_change(0x803054a0, 0xd23f009c);
    add_code_change(0x80169dbc, 0x60000000);
    add_code_change(0x80143d00, 0x48000050);

    add_code_change(0x8006fde0, 0x60000000, "beam_menu");
    add_code_change(0x8006fdc4, 0x60000000, "visor_menu");

    add_beam_change_code_mp2(0x8018cc88);

    mp2_static.cplayer_ptr_address = 0x804e87dc;
    mp2_static.load_state_address = 0x804e8824;
    mp2_static.lockon_address = 0x804e894f;
    mp2_static.cursor_base_address = 0x805cb2c8;
    mp2_static.cursor_offset = 0xc54;
    mp2_static.beamvisor_menu_address = 0x805cb314;
    mp2_static.beamvisor_menu_offset = 0x340; // 0x1bc + 0x184
    mp2_static.tweak_player_offset = 0x6410;
    mp2_static.object_list_ptr_address = 0x804e7af8;
    mp2_static.camera_uid_address = 0x804eb9b0;
  } else if (region == Region::PAL) {
    add_code_change(0x8008e30c, 0xc0430184);
    add_code_change(0x8008e360, 0x60000000);
    add_code_change(0x801496e4, 0x60000000);
    add_code_change(0x8014970c, 0x60000000);
    add_code_change(0x80137240, 0x60000000);
    add_code_change(0x8008d18c, 0x60000000);
    add_code_change(0x8008d15c, 0x60000000);
    add_code_change(0x80307d2c, 0xd23f009c);
    add_code_change(0x8016b534, 0x60000000);
    add_code_change(0x80145474, 0x48000050);

    add_code_change(0x80071358, 0x60000000, "beam_menu");
    add_code_change(0x8007133c, 0x60000000, "visor_menu");

    add_beam_change_code_mp2(0x8018e41c);

    mp2_static.cplayer_ptr_address = 0x804efc2c;
    mp2_static.load_state_address = 0x804efc74;
    mp2_static.lockon_address = 0x804efd9f;
    mp2_static.cursor_base_address = 0x805d2d30;
    mp2_static.cursor_offset = 0xd04;
    mp2_static.beamvisor_menu_address = 0x805d2d80;
    mp2_static.beamvisor_menu_offset = 0x340; // 0x1bc + 0x184
    mp2_static.tweak_player_offset = 0x6368;
    mp2_static.object_list_ptr_address = 0x804eef48;
    mp2_static.camera_uid_address = 0x804f2f50;
  } else if (region == Region::NTSC_J) {
    add_code_change(0x8008c944, 0xc0430184);
    add_code_change(0x8008c998, 0x60000000);
    add_code_change(0x80147578, 0x60000000);      
    add_code_change(0x801475a0, 0x60000000);
    add_code_change(0x8013511c, 0x60000000);
    add_code_change(0x8008b7c4, 0x60000000);
    add_code_change(0x8008b794, 0x60000000);
    add_code_change(0x80303ec8, 0xd23f009c);
    add_code_change(0x80169388, 0x60000000);
    add_code_change(0x8014331c, 0x48000050);

    add_code_change(0x8006fbb0, 0x60000000, "beam_menu");
    add_code_change(0x8006fb94, 0x60000000, "visor_menu");

    add_beam_change_code_mp2(0x8018c0d4);

    mp2_static.cplayer_ptr_address = 0x804e8fcc;
    mp2_static.load_state_address = 0x804e9014;
    mp2_static.lockon_address = 0x804e913f;
    mp2_static.cursor_base_address = 0x805cbaa0;
    mp2_static.cursor_offset = 0xc54;
    mp2_static.beamvisor_menu_address = 0x805cbaec;
    mp2_static.beamvisor_menu_offset = 0x340; // 0x1bc + 0x184
    mp2_static.tweak_player_offset = 0x63f8;
    mp2_static.object_list_ptr_address = 0x804e82e8;
    mp2_static.camera_uid_address = 0x804ec158;
  } else {}

  active_visor_offset = 0x34;
  powerups_size = 12;
  powerups_offset = 0x5c;
  // They match again, serendipity
  new_beam_address = 0x804cd254;
  beamchange_flag_address = 0x804cd250;
  has_beams = true;
}

void FpsControls::init_mod_mp2_gc(Region region) {
  if (region == Region::NTSC_U) {
    add_code_change(0x801b00b4, 0x48000050);
    add_code_change(0x801aef58, 0x60000000);
    add_code_change(0x800129c8, 0x4e800020);
    add_code_change(0x801af160, 0x60000000);
    add_code_change(0x801b0248, 0x48000078);
    add_code_change(0x801af450, 0x48000a34);
    add_code_change(0x8018846c, 0xc022a5b0);
    add_code_change(0x80188104, 0x4800000c);
    // Grapple point yaw fix
    add_code_change(0x8011d9c4, 0x389d0054);
    add_code_change(0x8011d9c8, 0x4bf2d1fd);
    // Show crosshair but don't consider pressing R button
    add_code_change(0x80015ed8, 0x3aa00001, "show_crosshair"); // li r21, 1
    add_code_change(0x80015edc, 0x8add1268, "show_crosshair"); // lbz r22, 0x1268(r29)
    add_code_change(0x80015ee0, 0x52b63672, "show_crosshair"); // rlwimi r22, r21, 6, 25, 25 (00000001)
    add_code_change(0x80015ee4, 0x9add1268, "show_crosshair"); // stb r22, 0x1268(r29)
    add_code_change(0x80015ee8, 0x4e800020, "show_crosshair"); // blr

    mp2_gc_static.state_mgr_address = 0x803db6e0;
    mp2_gc_static.tweak_player_offset = 0x6e3c;
    mp2_gc_static.crosshair_color_address = 0x80736d30;
  } else if (region == Region::PAL) {
    add_code_change(0x801b03c0, 0x48000050);
    add_code_change(0x801af264, 0x60000000);
    add_code_change(0x80012a2c, 0x4e800020);
    add_code_change(0x801af46c, 0x60000000);
    add_code_change(0x801b0554, 0x48000078);
    add_code_change(0x801af75c, 0x48000a34);
    add_code_change(0x80188754, 0xc022d16c);
    add_code_change(0x801883ec, 0x4800000c);
    // Grapple point yaw fix
    add_code_change(0x8011dbf8, 0x389d0054);
    add_code_change(0x8011dbfc, 0x4bf2d145);  // bl 8004ad40
    // Show crosshair but don't consider pressing R button
    add_code_change(0x80015f74, 0x3aa00001, "show_crosshair"); // li r21, 1
    add_code_change(0x80015f78, 0x8add1268, "show_crosshair"); // lbz r22, 0x1268(r29)
    add_code_change(0x80015f7c, 0x52b63672, "show_crosshair"); // rlwimi r22, r21, 6, 25, 25 (00000001)
    add_code_change(0x80015f80, 0x9add1268, "show_crosshair"); // stb r22, 0x1268(r29)
    add_code_change(0x80015f84, 0x4e800020, "show_crosshair"); // blr

    mp2_gc_static.state_mgr_address = 0x803dc900;
    mp2_gc_static.tweak_player_offset = 0x6e34;
    mp2_gc_static.crosshair_color_address = 0x80738f90;
  } else {}
}

void FpsControls::init_mod_mp3(Region region) {
  prime::GetVariableManager()->register_variable("grapple_lasso_state");
  prime::GetVariableManager()->register_variable("grapple_hand_x");
  prime::GetVariableManager()->register_variable("grapple_hand_y");
  prime::GetVariableManager()->register_variable("trigger_grapple");

  if (region == Region::NTSC_U) {
    add_code_change(0x80080ac0, 0xec010072);
    add_code_change(0x8014e094, 0x60000000);
    add_code_change(0x8014e06c, 0x60000000);
    add_code_change(0x80134328, 0x60000000);
    add_code_change(0x80133970, 0x60000000);
    add_code_change(0x8000ab58, 0x4bffad29);
    add_code_change(0x80080d44, 0x60000000);
    add_code_change(0x8007fdc8, 0x480000e4);
    add_code_change(0x8017f88c, 0x60000000);

    // Grapple Lasso
    add_grapple_lasso_code_mp3(0x800DDE64, 0x80170CF0, 0x80171AD8);

    add_control_state_hook_mp3(0x80005880, Region::NTSC_U);
    add_grapple_slide_code_mp3(0x8017f2a0);

    mp3_static.cplayer_ptr_address = 0x805c6c6c;
    mp3_static.cursor_dlg_enabled_address = 0x805c8d77;
    mp3_static.cursor_ptr_address = 0x8066fd08;
    mp3_static.cursor_offset = 0xc54;
    mp3_static.boss_info_address = 0x8066e1ec;
    mp3_static.lockon_address = 0x805c6db7;
    mp3_static.gun_lag_toc_offset = 0x5ff0;
    mp3_static.beamvisor_menu_address = 0x8066fcfc;
    mp3_static.beamvisor_menu_offset = 0x300; // 0x16c + 0x194
    mp3_static.cplayer_pitch_offset = 0x784;
    mp3_static.cam_uid_ptr_address = 0x805c6c78;
    mp3_static.state_mgr_ptr_address = 0x805c6c40;
  } else if (region == Region::PAL) {
    add_code_change(0x80080ab8, 0xec010072);
    add_code_change(0x8014d9e0, 0x60000000);
    add_code_change(0x8014d9b8, 0x60000000);
    add_code_change(0x80133c74, 0x60000000);
    add_code_change(0x801332bc, 0x60000000);
    add_code_change(0x8000ab58, 0x4bffad29);
    add_code_change(0x80080d44, 0x60000000);
    add_code_change(0x8007fdc8, 0x480000e4);
    add_code_change(0x8017f1d8, 0x60000000);

    // Grapple Lasso
    add_grapple_lasso_code_mp3(0x800DDE44, 0x8017063C, 0x80171424);

    add_control_state_hook_mp3(0x80005880, Region::PAL);
    add_grapple_slide_code_mp3(0x8017ebec);

    mp3_static.cplayer_ptr_address = 0x805ca0ec;
    mp3_static.cursor_dlg_enabled_address = 0x805cc1d7;
    mp3_static.cursor_ptr_address = 0x80673588;
    mp3_static.cursor_offset = 0xd04;
    mp3_static.boss_info_address = 0x80671a6c;
    mp3_static.lockon_address = 0x805ca237;
    mp3_static.gun_lag_toc_offset = 0x6000;
    mp3_static.beamvisor_menu_address = 0x8067357c;
    mp3_static.beamvisor_menu_offset = 0x300; // 0x16c + 0x194
    mp3_static.cplayer_pitch_offset = 0x784;
    mp3_static.cam_uid_ptr_address = 0x805ca0f8;
    mp3_static.state_mgr_ptr_address = 0x805ca0c0;
  } else {}

  // Same for both.
  add_code_change(0x800614d0, 0x60000000, "visor_menu");

  active_visor_offset = 0x34;
  powerups_ptr_offset = 0x35a8;
  powerups_size = 12;
  powerups_offset = 0x58;
  has_beams = false;
}

void FpsControls::init_mod_mp3_standalone(Region region)
{
  if (region == Region::NTSC_U) {
    add_code_change(0x80080be8, 0xec010072);
    add_code_change(0x801521f0, 0x60000000);
    add_code_change(0x801521c8, 0x60000000);
    add_code_change(0x80139108, 0x60000000);
    add_code_change(0x80138750, 0x60000000);
    add_code_change(0x8000ae44, 0x4bffaa3d);
    add_code_change(0x80080e6c, 0x60000000);
    add_code_change(0x8007fef0, 0x480000e4);
    add_code_change(0x80183288, 0x60000000);

    add_code_change(0x800617e4, 0x60000000, "visor_menu");

    add_control_state_hook_mp3(0x80005880, Region::NTSC_U);
    add_grapple_slide_code_mp3(0x80182c9c);

    mp3_static.cplayer_ptr_address = 0x805c4f98;
    mp3_static.cursor_dlg_enabled_address = 0x805c70c7;
    mp3_static.cursor_ptr_address = 0x8067dc18;
    mp3_static.cursor_offset = 0xc54;
    mp3_static.boss_info_address = 0x8067c0e4;
    mp3_static.lockon_address = 0x805c50e4;
    mp3_static.gun_lag_toc_offset = 0x5ff0;
    mp3_static.beamvisor_menu_address = 0x8067dc0c;
    mp3_static.beamvisor_menu_offset = 0x1708;
    mp3_static.cplayer_pitch_offset = 0x77c;
    mp3_static.cam_uid_ptr_address = 0x805c4fa4;
    mp3_static.state_mgr_ptr_address = 0x805c4f6c;
    powerups_ptr_offset = 0x35a0;
  } else if (region == Region::NTSC_J) {
    add_code_change(0x80081018, 0xec010072);
    add_code_change(0x80153ed4, 0x60000000);
    add_code_change(0x80153eac, 0x60000000);
    add_code_change(0x8013a054, 0x60000000);
    add_code_change(0x8013969c, 0x60000000);
    add_code_change(0x8000ae44, 0x4bffaa3d);
    add_code_change(0x8008129c, 0x60000000);
    add_code_change(0x80080320, 0x480000e4);
    add_code_change(0x80184fd4, 0x60000000);

    add_code_change(0x80075f0c, 0x80061958, "visor_menu");

    add_control_state_hook_mp3(0x80005880, Region::NTSC_J);
    add_grapple_slide_code_mp3(0x801849e8);

    mp3_static.cplayer_ptr_address = 0x805caa5c;
    mp3_static.cursor_dlg_enabled_address = 0x805ccbd7;
    mp3_static.cursor_ptr_address = 0x80683a88;
    mp3_static.cursor_offset = 0xc54;
    mp3_static.boss_info_address = 0x80681f54;
    mp3_static.lockon_address = 0x805caba7;
    mp3_static.gun_lag_toc_offset = 0x5f68;
    mp3_static.beamvisor_menu_address = 0x80683a7C;
    mp3_static.beamvisor_menu_offset = 0x1708;
    mp3_static.cplayer_pitch_offset = 0x784;
    mp3_static.cam_uid_ptr_address = 0x805caa68;
    mp3_static.state_mgr_ptr_address = 0x805caa30;
    powerups_ptr_offset = 0x35a8;
  } else if (region == Region::PAL) {
    add_code_change(0x80080e84, 0xec010072);
    add_code_change(0x80152d50, 0x60000000);
    add_code_change(0x80152d28, 0x60000000);
    add_code_change(0x80139860, 0x60000000);
    add_code_change(0x80138ea8, 0x60000000);
    add_code_change(0x8000ae44, 0x4bffaa3d);
    add_code_change(0x80081108, 0x60000000);
    add_code_change(0x8008018c, 0x480000e4);
    add_code_change(0x80183dc8, 0x60000000);

    add_code_change(0x80061a88, 0x60000000, "visor_menu");

    add_control_state_hook_mp3(0x80005880, Region::PAL);
    add_grapple_slide_code_mp3(0x801837dc);

    mp3_static.cplayer_ptr_address = 0x805c759c;
    mp3_static.cursor_dlg_enabled_address = 0x805c96df;
    mp3_static.cursor_ptr_address = 0x80680240;
    mp3_static.cursor_offset = 0xc54;
    mp3_static.boss_info_address = 0x8067c87c;
    mp3_static.lockon_address = 0x805c76e7;
    mp3_static.gun_lag_toc_offset = 0x6000;
    mp3_static.beamvisor_menu_address = 0x80680234;
    mp3_static.beamvisor_menu_offset = 0x1708;
    mp3_static.cplayer_pitch_offset = 0x77c;
    mp3_static.cam_uid_ptr_address = 0x805c75a8;
    mp3_static.state_mgr_ptr_address = 0x805c7570;
    powerups_ptr_offset = 0x35a0;
  } else {}

  active_visor_offset = 0x34;
  powerups_size = 12;
  powerups_offset = 0x58;
  has_beams = false;
}
}
