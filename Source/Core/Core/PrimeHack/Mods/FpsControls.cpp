#include "Core/PrimeHack/Mods/FpsControls.h"

#include "Core/PrimeHack/PrimeUtils.h"

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
}
void FpsControls::run_mod(Game game, Region region) {
  switch (game) {
  case Game::MENU:
    run_mod_menu(region);
    break;
  case Game::PRIME_1:
    run_mod_mp1();
    break;
  case Game::PRIME_2:
    run_mod_mp2(region);
    break;
  case Game::PRIME_3:
    run_mod_mp3();
    break;
  case Game::PRIME_1_GCN:
    run_mod_mp1_gc();
    break;
  default:
    break;
  }
}

void FpsControls::calculate_pitch_delta() {
  const float compensated_sens = GetSensitivity() * TURNRATE_RATIO / 60.f;

  pitch += static_cast<float>(GetVerticalAxis()) * compensated_sens *
    (InvertedY() ? 1.f : -1.f);
  pitch = std::clamp(pitch, -1.52f, 1.52f);
}

float FpsControls::calculate_yaw_vel() {
  return GetHorizontalAxis() * GetSensitivity() * (InvertedX() ? 1.f : -1.f);;
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
    read32(powerups_array_base + 0x1c) == 0);

  if (visor_id != -1) {
    if (read32(powerups_array_base + (visor_off * powerups_size) + powerups_offset)) {
      write32(visor_id, powerups_array_base + active_visor_offset);
    }
  }

  DevInfo("Powerups_Base", "%08X", powerups_array_base);
}

void FpsControls::run_mod_menu(Region region) {
  if (region == Region::NTSC) {
    handle_cursor(0x80913c9c, 0x80913d5c, 0.95f, 0.90f);
  }
  else if (region == Region::PAL) {
    u32 cursor_address = PowerPC::HostRead_U32(0x80621ffc);
    handle_cursor(cursor_address + 0xdc, cursor_address + 0x19c, 0.95f, 0.90f);
  }
}

void FpsControls::run_mod_mp1() {
  handle_beam_visor_switch(prime_one_beams, prime_one_visors);

  // Allows freelook in grapple, otherwise we are orbiting (locked on) to something
  if (read32(mp1_static.orbit_state_address) != ORBIT_STATE_GRAPPLE &&
      read8(mp1_static.lockon_address)) {
    write32(0, mp1_static.yaw_vel_address);
    return;
  }

  // I write to two locations here to control the pitch, the rate of turning
  // has been unlocked already
  calculate_pitch_delta();
  writef32(pitch, mp1_static.pitch_address);
  writef32(pitch, mp1_static.pitch_goal_address);
  // Max pitch angle, as abs val (any higher = gimbal lock)
  writef32(1.52f, mp1_static.tweak_player_address + 0x134);

  // Setting this to 0 allows yaw velocity (Z component of an angular momentum
  // vector) to affect the transform matrix freely
  write32(0, mp1_static.angvel_limiter_address);
  writef32(calculate_yaw_vel(), mp1_static.yaw_vel_address);
}

void FpsControls::run_mod_mp1_gc() {
  if (read32(mp1_gc_static.orbit_state_address) != ORBIT_STATE_GRAPPLE &&
      read8(mp1_gc_static.orbit_state_address)) {
    write32(0, mp1_gc_static.yaw_vel_address);
    return;
  }

  calculate_pitch_delta();
  writef32(pitch, mp1_gc_static.pitch_address);
  writef32(1.52f, mp1_gc_static.tweak_player_address + 0x134);

  // Actual angular velocity Z address amazing
  writef32(calculate_yaw_vel() / 200.f, mp1_gc_static.yaw_vel_address);
  for (int i = 0; i < 8; i++) {
    writef32(100000000.f, mp1_gc_static.angvel_max_address + i * 4);
    writef32(1.f, mp1_gc_static.angvel_max_address + i * 4 - 32);
  }
}

void FpsControls::run_mod_mp2(Region region) {
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

  if (read32(cplayer_address + 0x390) != ORBIT_STATE_GRAPPLE &&
      read32(mp2_static.lockon_address)) {
    // Angular velocity (not really, but momentum) is being messed with like mp1
    // just being accessed relative to cplayer
    write32(0, cplayer_address + 0x178);
    return;
  }
    
  calculate_pitch_delta();
  // Grab the arm cannon address, go to its transform field (NOT the
  // Actor's xf @ 0x30!!)
  u32 arm_cannon_model_matrix = read32(cplayer_address + 0xea8) + 0x3b0;
  writef32(pitch, cplayer_address + 0x5f0);
  // For whatever god forsaken reason, writing pitch to the z component of the
  // right vector for this xf makes the gun not lag. Won't fix what ain't broken
  writef32(pitch, arm_cannon_model_matrix + 0x24);
  u32 tweak_player_address = 0;
  if (region == Region::NTSC) {
    tweak_player_address = read32(read32(GPR(13) - 0x6410));
  }
  else if (region == Region::PAL) {
    tweak_player_address = read32(read32(GPR(13) - 0x6368));
  }
  if (mem_check(tweak_player_address)) {
    // This one's stored as degrees instead of radians
    writef32(87.0896f, tweak_player_address + 0x180);
  }
  writef32(calculate_yaw_vel(), cplayer_address + 0x178);
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

// this game is        
void FpsControls::run_mod_mp3() {
  u32 cplayer_address = read32(read32(read32(mp3_static.cplayer_ptr_address) + 0x04) + 0x2184);

  if (!mem_check(cplayer_address)) {
    // Handles death screen cursor
    if (read8(mp3_static.cursor_dlg_enabled_address)) {
      mp3_handle_cursor(false);
    }
    return;
  }

  //u32 obj_list_iterator = read32(read32(mp3_static.cplayer_ptr_address - 4) + 0x1018) + 4;
  //const u32 base = obj_list_iterator;
  //while (true) {
  //  u32 obj = read32(obj_list_iterator);
  //  u32 flags = read32(obj + 0x38);

  //  bool should_process = false;
  //  if (flags & 0x20000000) {
  //    should_process = ((flags >> 8) & 0x2000) == 0;
  //  }
  //  should_process |= ((flags >> 8) & 0x1000) != 0;

  //  if (should_process) {
  //    u32 vt = read32(obj);
  //    u32 vtf = read32(vt + 0xc);
  //    if (vtf == 0x802e0dac) { // ensure Accept is this function
  //      writef32(1, obj + 0x154);
  //    }
  //  }
  //  u16 next_id = read16(obj_list_iterator + 6);
  //  if (next_id == 0xffff) {
  //    break;
  //  }
  //  obj_list_iterator = (base + next_id * 8);
  //}

  // HACK ooo
  powerups_ptr_address = cplayer_address + 0x35a8;
  handle_beam_visor_switch({}, prime_three_visors);

  // This is VERY LIKELY not to be "boss address" as that would be dynamic (LOL)
  // this is just something that always seems to match every ridley fight, but
  // nowhere else (except defense drone). Never looked into it. Never will. This
  // stupid game cannot be debugged, I just don't care about it anymore.
  if (read8(mp3_static.cursor_dlg_enabled_address) ||
    read64(mp3_static.boss_id_address) == mp3_static.boss_id) {
    if (read64(mp3_static.boss_id_address) == mp3_static.boss_id && !fighting_ridley) {
      fighting_ridley = true;
      set_state(ModState::CODE_DISABLED);
    }
    mp3_handle_cursor(false);
  }
  else {
    if (fighting_ridley) {
      fighting_ridley = false;
      set_state(ModState::ENABLED);
    }
    mp3_handle_cursor(true);
  }

  if (!read8(cplayer_address + 0x378) && read8(mp3_static.lockon_address)) {
    write32(0, cplayer_address + 0x174);
    return;
  }

  calculate_pitch_delta();
  // Gun damping uses its own TOC value, so screw it (I checked the binary)
  u32 rtoc_gun_damp = GPR(2) - mp3_static.gun_lag_toc_offset;
  write32(0, rtoc_gun_damp);
  writef32(pitch, cplayer_address + 0x784);

  // Nothing new here
  writef32(calculate_yaw_vel(), cplayer_address + 0x174);
  write32(0, cplayer_address + 0x174 + 0x18);
}

void FpsControls::init_mod(Game game, Region region) {
  switch (game) {
  case Game::PRIME_1_GCN:
    init_mod_mp1_gc(region);
    break;
  case Game::PRIME_1:
    init_mod_mp1(region);
    break;
  case Game::PRIME_2:
    init_mod_mp2(region);
    break;
  case Game::PRIME_3:
    init_mod_mp3(region);
    break;
  }
  initialized = true;
}

void FpsControls::add_beam_change_code_mp1(u32 start_point) {
  code_changes.emplace_back(start_point + 0x00, 0x3c80804a);  // lis    r4, 0x804a      ; set r4 to beam change base address
  code_changes.emplace_back(start_point + 0x04, 0x388479f0);  // addi   r4, r4, 0x79f0  ; (0x804a79f0)
  code_changes.emplace_back(start_point + 0x08, 0x80640000);  // lwz    r3, 0(r4)       ; grab flag
  code_changes.emplace_back(start_point + 0x0c, 0x2c030000);  // cmpwi  r3, 0           ; check if beam should change
  code_changes.emplace_back(start_point + 0x10, 0x41820058);  // beq    0x58            ; don't attempt beam change if 0
  code_changes.emplace_back(start_point + 0x14, 0x83440004);  // lwz    r26, 4(r4)      ; get expected beam (r25, r26 used to assign beam)
  code_changes.emplace_back(start_point + 0x18, 0x7f59d378);  // mr     r25, r26        ; copy expected beam to other reg
  code_changes.emplace_back(start_point + 0x1c, 0x38600000);  // li     r3, 0           ; reset flag
  code_changes.emplace_back(start_point + 0x20, 0x90640000);  // stw    r3, 0(r4)       ;
  code_changes.emplace_back(start_point + 0x24, 0x48000044);  // b      0x44            ; jump forward to beam assign
}

void FpsControls::add_beam_change_code_mp2(u32 start_point) {
  code_changes.emplace_back(start_point + 0x00, 0x3c80804d);  // lis    r4, 0x804d      ; set r4 to beam change base address
  code_changes.emplace_back(start_point + 0x04, 0x3884d250);  // subi   r4, r4, 0x2db0  ; (0x804cd250)
  code_changes.emplace_back(start_point + 0x08, 0x80640000);  // lwz    r3, 0(r4)       ; grab flag
  code_changes.emplace_back(start_point + 0x0c, 0x2c030000);  // cmpwi  r3, 0           ; check if beam should change
  code_changes.emplace_back(start_point + 0x10, 0x4182005c);  // beq    0x5c            ; don't attempt beam change if 0
  code_changes.emplace_back(start_point + 0x14, 0x83e40004);  // lwz    r31, 4(r4)      ; get expected beam (r31, r30 used to assign beam)
  code_changes.emplace_back(start_point + 0x18, 0x7ffefb78);  // mr     r30, r31        ; copy expected beam to other reg
  code_changes.emplace_back(start_point + 0x1c, 0x38600000);  // li     r3, 0           ; reset flag
  code_changes.emplace_back(start_point + 0x20, 0x90640000);  // stw    r3, 0(r4)       ;
  code_changes.emplace_back(start_point + 0x24, 0x48000048);  // b      0x48            ; jump forward to beam assign
}

void FpsControls::add_grapple_slide_code_mp3(u32 start_point) {
  code_changes.emplace_back(start_point + 0x00, 0x60000000);  // nop                  ; trashed because useless
  code_changes.emplace_back(start_point + 0x04, 0x60000000);  // nop                  ; trashed because useless
  code_changes.emplace_back(start_point + 0x08, 0x60000000);  // nop                  ; trashed because useless
  code_changes.emplace_back(start_point + 0x0c, 0xc0010240);  // lfs  f0, 0x240(r1)   ; grab the x component of new origin
  code_changes.emplace_back(start_point + 0x10, 0xd01f0048);  // stfs f0, 0x48(r31)   ; store it into player's xform x origin (CTransform + 0x0c)
  code_changes.emplace_back(start_point + 0x14, 0xc0010244);  // lfs  f0, 0x244(r1)   ; grab the y component of new origin
  code_changes.emplace_back(start_point + 0x18, 0xd01f0058);  // stfs f0, 0x58(r31)   ; store it into player's xform y origin (CTransform + 0x1c)
  code_changes.emplace_back(start_point + 0x1c, 0xc0010248);  // lfs  f0, 0x248(r1)   ; grab the z component of new origin
  code_changes.emplace_back(start_point + 0x20, 0xd01f0068);  // stfs f0, 0x68(r31)   ; store it into player's xform z origin (CTransform + 0xcc)
  code_changes.emplace_back(start_point + 0x28, 0x389f003c);  // addi r4, r31, 0x3c   ; next sub call is SetTransform, so set player's transform
                                                              // ; to their own transform (safety no-op, does other updating too)
}

void FpsControls::add_control_state_hook_mp3(u32 start_point, Region region) {
  if (region == Region::NTSC)
  {
    code_changes.emplace_back(start_point + 0x00, 0x3c60805c);  // lis  r3, 0x805c      ;
    code_changes.emplace_back(start_point + 0x04, 0x38636c40);  // addi r3, r3, 0x6c40  ; load 0x805c6c40 into r3
                                                                // ; (indirect pointer to player camera control)
  }
  else if (region == Region::PAL)
  {
    code_changes.emplace_back(start_point + 0x00, 0x3c60805d);  // lis  r3, 0x805d      ;
    code_changes.emplace_back(start_point + 0x04, 0x3863a0c0);  // subi r3, r3, 0x5f40  ; load 0x805ca0c0 into r3, same reason as NTSC
  }
  code_changes.emplace_back(start_point + 0x08, 0x8063002c);  // lwz  r3, 0x2c(r3)      ; deref player camera control base address into r3
  code_changes.emplace_back(start_point + 0x0c, 0x80630004);  // lwz  r3, 0x04(r3)      ; the point at which the function which was hooked
  code_changes.emplace_back(start_point + 0x10, 0x80632184);  // lwz  r3, 0x2184(r3)    ; should have r31 equal to the
                                                              // ; object which is being modified
  code_changes.emplace_back(start_point + 0x14, 0x7c03f800);  // cmpw r3, r31           ; if r31 is player camera control (in r3)
  code_changes.emplace_back(start_point + 0x18, 0x4d820020);  // beqlr                  ; then DON'T store the value of
                                                              // ; r6 into 0x78+(player camera control)
  code_changes.emplace_back(start_point + 0x1c, 0x7fe3fb78);  // mr   r3, r31           ; otherwise do it
  code_changes.emplace_back(start_point + 0x20, 0x90c30078);  // stw  r6, 0x78(r3)      ; this is the normal action
                                                              // ; which was overwritten by a bl to this mini-function
  code_changes.emplace_back(start_point + 0x24, 0x4e800020);  // blr                    ; LR wasn't changed, so we're
                                                              // ; safe here (same case as beqlr)
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
  code_changes.emplace_back(0x805afc00, 0x94210018);
  code_changes.emplace_back(0x805afc04, 0x7c0802a6);
  code_changes.emplace_back(0x805afc08, 0x9001001c);
  code_changes.emplace_back(0x805afc0c, 0x80ada118);
  code_changes.emplace_back(0x805afc10, 0x809d02b0);
  code_changes.emplace_back(0x805afc14, 0x2c040002);
  code_changes.emplace_back(0x805afc18, 0x38800004);
  code_changes.emplace_back(0x805afc1c, 0x40820008);
  code_changes.emplace_back(0x805afc20, 0x809d02ac);
  code_changes.emplace_back(0x805afc24, 0x5484103a);
  code_changes.emplace_back(0x805afc28, 0x7c642a14);
  code_changes.emplace_back(0x805afc2c, 0xc0230044);
  code_changes.emplace_back(0x805afc30, 0xc0430004);
  code_changes.emplace_back(0x805afc34, 0xec6206f2);
  code_changes.emplace_back(0x805afc38, 0xc01d00e8);
  code_changes.emplace_back(0x805afc3c, 0xec210032);
  code_changes.emplace_back(0x805afc40, 0xec211824);
  code_changes.emplace_back(0x805afc44, 0xc00300a4);
  code_changes.emplace_back(0x805afc48, 0xd0010010);
  code_changes.emplace_back(0x805afc4c, 0xec210032);
  code_changes.emplace_back(0x805afc50, 0xc002bda0);
  code_changes.emplace_back(0x805afc54, 0xfc1e0040);
  code_changes.emplace_back(0x805afc58, 0xc002bdc8);
  code_changes.emplace_back(0x805afc5c, 0x40810008);
  code_changes.emplace_back(0x805afc60, 0xc002bd80);
  code_changes.emplace_back(0x805afc64, 0xec000072);
  code_changes.emplace_back(0x805afc68, 0xc0610010);
  code_changes.emplace_back(0x805afc6c, 0xec630828);
  code_changes.emplace_back(0x805afc70, 0xec6307b2);
  code_changes.emplace_back(0x805afc74, 0xec00182a);
  code_changes.emplace_back(0x805afc78, 0xd0010018);
  code_changes.emplace_back(0x805afc7c, 0xd0410014);
  code_changes.emplace_back(0x805afc80, 0x38610004);
  code_changes.emplace_back(0x805afc84, 0x389d0034);
  code_changes.emplace_back(0x805afc88, 0x38bd0138);
  code_changes.emplace_back(0x805afc8c, 0x4bd62d99);
  code_changes.emplace_back(0x805afc90, 0xc0010018);
  code_changes.emplace_back(0x805afc94, 0xc0210004);
  code_changes.emplace_back(0x805afc98, 0xec000828);
  code_changes.emplace_back(0x805afc9c, 0xc0210010);
  code_changes.emplace_back(0x805afca0, 0xec000824);
  code_changes.emplace_back(0x805afca4, 0xc022bdc8);
  code_changes.emplace_back(0x805afca8, 0xfc000840);
  code_changes.emplace_back(0x805afcac, 0x4080000c);
  code_changes.emplace_back(0x805afcb0, 0xfc000890);
  code_changes.emplace_back(0x805afcb4, 0x48000014);
  code_changes.emplace_back(0x805afcb8, 0xc022bd80);
  code_changes.emplace_back(0x805afcbc, 0xfc000840);
  code_changes.emplace_back(0x805afcc0, 0x40810008);
  code_changes.emplace_back(0x805afcc4, 0xfc000890);
  code_changes.emplace_back(0x805afcc8, 0xc0210014);
  code_changes.emplace_back(0x805afccc, 0xec200072);
  code_changes.emplace_back(0x805afcd0, 0x8001001c);
  code_changes.emplace_back(0x805afcd4, 0x7c0803a6);
  code_changes.emplace_back(0x805afcd8, 0x3821ffe8);
  code_changes.emplace_back(0x805afcdc, 0x4e800020);

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
  code_changes.emplace_back(0x802875c4, 0xc022bda0);
  code_changes.emplace_back(0x802875c8, 0xc002be44);
  code_changes.emplace_back(0x802875cc, 0xec3e0828);
  code_changes.emplace_back(0x802875d0, 0xfc200a10);
  code_changes.emplace_back(0x802875d4, 0xfc010040);
  code_changes.emplace_back(0x802875d8, 0x4081002c);
  code_changes.emplace_back(0x802875dc, 0x48328625);
  code_changes.emplace_back(0x802875e0, 0x4bd93f55);
  code_changes.emplace_back(0x802875e4, 0x7c651b78);
  code_changes.emplace_back(0x802875e8, 0x7fa3eb78);
  code_changes.emplace_back(0x802875ec, 0xc002bda0);
  code_changes.emplace_back(0x802875f0, 0xd0210010);
  code_changes.emplace_back(0x802875f4, 0xd0010014);
  code_changes.emplace_back(0x802875f8, 0xd0010018);
  code_changes.emplace_back(0x802875fc, 0x38810010);

  // disable rotation on LR analog
  code_changes.emplace_back(0x80286fe0, 0x4bfffc71);
  code_changes.emplace_back(0x80286c88, 0x4800000c);
  code_changes.emplace_back(0x8028739c, 0x60000000);
  code_changes.emplace_back(0x802873e0, 0x60000000);
  code_changes.emplace_back(0x8028707c, 0x60000000);
  code_changes.emplace_back(0x802871bc, 0x60000000);
  code_changes.emplace_back(0x80287288, 0x60000000);

  // Clamp current xy velocity @ 802872A4
  // lfs f1, -0x7ec0(r2)
  // fmuls f0, f30, f30
  // fcmpo cr0, f0, f1
  // ble 0x134
  // fmuls f0, f31, f31
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
  code_changes.emplace_back(0x802872a4, 0xc0228140);
  code_changes.emplace_back(0x802872a8, 0xec1e07b2);
  code_changes.emplace_back(0x802872ac, 0xfc000840);
  code_changes.emplace_back(0x802872b0, 0x40810134);
  code_changes.emplace_back(0x802872b4, 0xec1f07f2);
  code_changes.emplace_back(0x802872b8, 0xfc000840);
  code_changes.emplace_back(0x802872bc, 0x40810128);
  code_changes.emplace_back(0x802872c0, 0xc01d0138);
  code_changes.emplace_back(0x802872c4, 0xc03d013c);
  code_changes.emplace_back(0x802872c8, 0xec000032);
  code_changes.emplace_back(0x802872cc, 0xec21007a);
  code_changes.emplace_back(0x802872d0, 0xfc200834);
  code_changes.emplace_back(0x802872d4, 0xec200830);
  code_changes.emplace_back(0x802872d8, 0x3862dfc0);
  code_changes.emplace_back(0x802872dc, 0x5400103a);
  code_changes.emplace_back(0x802872e0, 0x7c601a14);
  code_changes.emplace_back(0x802872e4, 0xc0030000);
  code_changes.emplace_back(0x802872e8, 0xfc010040);
  code_changes.emplace_back(0x802872ec, 0x408100f8);
  code_changes.emplace_back(0x802872f0, 0xc07d00e8);
  code_changes.emplace_back(0x802872f4, 0xc05d0138);
  code_changes.emplace_back(0x802872f8, 0xec420824);
  code_changes.emplace_back(0x802872fc, 0xec4000b2);
  code_changes.emplace_back(0x80287300, 0xd05d0138);
  code_changes.emplace_back(0x80287304, 0xec4300b2);
  code_changes.emplace_back(0x80287308, 0xd05d00fc);
  code_changes.emplace_back(0x8028730c, 0xc05d013c);
  code_changes.emplace_back(0x80287310, 0xec420824);
  code_changes.emplace_back(0x80287314, 0xec4000b2);
  code_changes.emplace_back(0x80287318, 0xd05d013c);
  code_changes.emplace_back(0x8028731c, 0xec4300b2);
  code_changes.emplace_back(0x80287320, 0xd05d0100);
  code_changes.emplace_back(0x80287324, 0x480000c0);

  // max speed values table @ 805afce0
  code_changes.emplace_back(0x805afce0, 0x41480000);
  code_changes.emplace_back(0x805afce4, 0x41480000);
  code_changes.emplace_back(0x805afce8, 0x41480000);
  code_changes.emplace_back(0x805afcec, 0x41480000);
  code_changes.emplace_back(0x805afcf0, 0x41480000);
  code_changes.emplace_back(0x805afcf4, 0x41480000);
  code_changes.emplace_back(0x805afcf8, 0x41480000);
  code_changes.emplace_back(0x805afcfc, 0x41480000);
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
  code_changes.emplace_back(0x80471c00, 0x94210018);
  code_changes.emplace_back(0x80471c04, 0x7c0802a6);
  code_changes.emplace_back(0x80471c08, 0x9001001c);
  code_changes.emplace_back(0x80471c0c, 0x80ada190);
  code_changes.emplace_back(0x80471c10, 0x809d02c0);
  code_changes.emplace_back(0x80471c14, 0x2c040002);
  code_changes.emplace_back(0x80471c18, 0x38800004);
  code_changes.emplace_back(0x80471c1c, 0x40820008);
  code_changes.emplace_back(0x80471c20, 0x809d02bc);
  code_changes.emplace_back(0x80471c24, 0x5484103a);
  code_changes.emplace_back(0x80471c28, 0x7c642a14);
  code_changes.emplace_back(0x80471c2c, 0xc0230044);
  code_changes.emplace_back(0x80471c30, 0xc0430004);
  code_changes.emplace_back(0x80471c34, 0xec6206f2);
  code_changes.emplace_back(0x80471c38, 0xc01d00f8);
  code_changes.emplace_back(0x80471c3c, 0xec210032);
  code_changes.emplace_back(0x80471c40, 0xec211824);
  code_changes.emplace_back(0x80471c44, 0xc00300a4);
  code_changes.emplace_back(0x80471c48, 0xd0010010);
  code_changes.emplace_back(0x80471c4c, 0xec210032);
  code_changes.emplace_back(0x80471c50, 0xc002be80);
  code_changes.emplace_back(0x80471c54, 0xfc1e0040);
  code_changes.emplace_back(0x80471c58, 0xc002bea8);
  code_changes.emplace_back(0x80471c5c, 0x40810008);
  code_changes.emplace_back(0x80471c60, 0xc002be60);
  code_changes.emplace_back(0x80471c64, 0xec000072);
  code_changes.emplace_back(0x80471c68, 0xc0610010);
  code_changes.emplace_back(0x80471c6c, 0xec630828);
  code_changes.emplace_back(0x80471c70, 0xec6307b2);
  code_changes.emplace_back(0x80471c74, 0xec00182a);
  code_changes.emplace_back(0x80471c78, 0xd0010018);
  code_changes.emplace_back(0x80471c7c, 0xd0410014);
  code_changes.emplace_back(0x80471c80, 0x38610004);
  code_changes.emplace_back(0x80471c84, 0x389d0034);
  code_changes.emplace_back(0x80471c88, 0x38bd0148);
  code_changes.emplace_back(0x80471c8c, 0x4be89b69);
  code_changes.emplace_back(0x80471c90, 0xc0010018);
  code_changes.emplace_back(0x80471c94, 0xc0210004);
  code_changes.emplace_back(0x80471c98, 0xec000828);
  code_changes.emplace_back(0x80471c9c, 0xc0210010);
  code_changes.emplace_back(0x80471ca0, 0xec000824);
  code_changes.emplace_back(0x80471ca4, 0xc022bea8);
  code_changes.emplace_back(0x80471ca8, 0xfc000840);
  code_changes.emplace_back(0x80471cac, 0x4080000c);
  code_changes.emplace_back(0x80471cb0, 0xfc000890);
  code_changes.emplace_back(0x80471cb4, 0x48000014);
  code_changes.emplace_back(0x80471cb8, 0xc022be60);
  code_changes.emplace_back(0x80471cbc, 0xfc000840);
  code_changes.emplace_back(0x80471cc0, 0x40810008);
  code_changes.emplace_back(0x80471cc4, 0xfc000890);
  code_changes.emplace_back(0x80471cc8, 0xc0210014);
  code_changes.emplace_back(0x80471ccc, 0xec200072);
  code_changes.emplace_back(0x80471cd0, 0x8001001c);
  code_changes.emplace_back(0x80471cd4, 0x7c0803a6);
  code_changes.emplace_back(0x80471cd8, 0x3821ffe8);
  code_changes.emplace_back(0x80471cdc, 0x4e800020);

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
  code_changes.emplace_back(0x802749a8, 0xc022be80);
  code_changes.emplace_back(0x802749ac, 0xc002bf24);
  code_changes.emplace_back(0x802749b0, 0xec3e0828);
  code_changes.emplace_back(0x802749b4, 0xfc200a10);
  code_changes.emplace_back(0x802749b8, 0xfc010040);
  code_changes.emplace_back(0x802749bc, 0x4081002c);
  code_changes.emplace_back(0x802749c0, 0x481fd241);
  code_changes.emplace_back(0x802749c4, 0x4bda74e1);
  code_changes.emplace_back(0x802749c8, 0x7c651b78);
  code_changes.emplace_back(0x802749cc, 0x7fa3eb78);
  code_changes.emplace_back(0x802749d0, 0xc002be80);
  code_changes.emplace_back(0x802749d4, 0xd0210010);
  code_changes.emplace_back(0x802749d8, 0xd0010014);
  code_changes.emplace_back(0x802749dc, 0xd0010018);
  code_changes.emplace_back(0x802749e0, 0x38810010);

  // disable rotation on LR analog
  code_changes.emplace_back(0x802743C4, 0x4bfffc71); // jump/address updated
  code_changes.emplace_back(0x8027406C, 0x4800000c); // updated following addresses
  code_changes.emplace_back(0x80274780, 0x60000000);
  code_changes.emplace_back(0x802747C4, 0x60000000);
  code_changes.emplace_back(0x80274460, 0x60000000);
  code_changes.emplace_back(0x802745A0, 0x60000000);
  code_changes.emplace_back(0x8027466C, 0x60000000);

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
  code_changes.emplace_back(0x80274688, 0xc0228140);
  code_changes.emplace_back(0x8027468c, 0xec1e07b2);
  code_changes.emplace_back(0x80274690, 0xfc000840);
  code_changes.emplace_back(0x80274694, 0x40810134);
  code_changes.emplace_back(0x80274698, 0xec1f07f2);
  code_changes.emplace_back(0x8027469c, 0xfc000840);
  code_changes.emplace_back(0x802746a0, 0x40810128);
  code_changes.emplace_back(0x802746a4, 0xc01d0148);
  code_changes.emplace_back(0x802746a8, 0xc03d014c);
  code_changes.emplace_back(0x802746ac, 0xec000032);
  code_changes.emplace_back(0x802746b0, 0xec21007a);
  code_changes.emplace_back(0x802746b4, 0xfc200834);
  code_changes.emplace_back(0x802746b8, 0xec200830);
  code_changes.emplace_back(0x802746bc, 0x3862de80);
  code_changes.emplace_back(0x802746c0, 0x5400103a);
  code_changes.emplace_back(0x802746c4, 0x7c601a14);
  code_changes.emplace_back(0x802746c8, 0xc0030000);
  code_changes.emplace_back(0x802746cc, 0xfc010040);
  code_changes.emplace_back(0x802746d0, 0x408100f8);
  code_changes.emplace_back(0x802746d4, 0xc07d00f8);
  code_changes.emplace_back(0x802746d8, 0xc05d0148);
  code_changes.emplace_back(0x802746dc, 0xec420824);
  code_changes.emplace_back(0x802746e0, 0xec4000b2);
  code_changes.emplace_back(0x802746e4, 0xd05d0148);
  code_changes.emplace_back(0x802746e8, 0xec4300b2);
  code_changes.emplace_back(0x802746ec, 0xd05d010c);
  code_changes.emplace_back(0x802746f0, 0xc05d014c);
  code_changes.emplace_back(0x802746f4, 0xec420824);
  code_changes.emplace_back(0x802746f8, 0xec4000b2);
  code_changes.emplace_back(0x802746fc, 0xd05d014c);
  code_changes.emplace_back(0x80274700, 0xec4300b2);
  code_changes.emplace_back(0x80274704, 0xd05d0110);
  code_changes.emplace_back(0x80274708, 0x480000c0);

  // No change needed
  // max speed values table @ 80471ce0
  code_changes.emplace_back(0x80471ce0, 0x41480000);
  code_changes.emplace_back(0x80471ce4, 0x41480000);
  code_changes.emplace_back(0x80471ce8, 0x41480000);
  code_changes.emplace_back(0x80471cec, 0x41480000);
  code_changes.emplace_back(0x80471cf0, 0x41480000);
  code_changes.emplace_back(0x80471cf4, 0x41480000);
  code_changes.emplace_back(0x80471cf8, 0x41480000);
  code_changes.emplace_back(0x80471cfc, 0x41480000);
}

void FpsControls::init_mod_mp1(Region region) {
  if (region == Region::NTSC) {
    // This instruction change is used in all 3 games, all 3 regions. It's an update to what I believe
    // to be interpolation for player camera pitch The change is from fmuls f0, f0, f1 (0xec000072) to
    // fmuls f0, f1, f1 (0xec010072), where f0 is the output. The higher f0 is, the faster the pitch
    // *can* change in-game, and squaring f1 seems to do the job.
    code_changes.emplace_back(0x80098ee4, 0xec010072);

    // NOPs for changing First Person Camera's pitch based on floor (normal? angle?)
    code_changes.emplace_back(0x80099138, 0x60000000);
    code_changes.emplace_back(0x80183a8c, 0x60000000);
    code_changes.emplace_back(0x80183a64, 0x60000000);
    code_changes.emplace_back(0x8017661c, 0x60000000);
    // Cursor location, sets to f17 (always 0 due to little use)
    code_changes.emplace_back(0x802fb5b4, 0xd23f009c);
    code_changes.emplace_back(0x8019fbcc, 0x60000000);

    // Disable Beams/Visor Menu, conditional -> unconditional branch
    code_changes.emplace_back(0x80075cd0, 0x48000044);
    add_beam_change_code_mp1(0x8018e544);

    mp1_static.yaw_vel_address = 0x804d3d38;
    mp1_static.pitch_address = 0x804d3ffc;
    mp1_static.pitch_goal_address = 0x804c10ec;
    mp1_static.angvel_limiter_address = 0x804d3d74;
    mp1_static.orbit_state_address = 0x804d3f20;
    mp1_static.lockon_address = 0x804c00b3;
    mp1_static.tweak_player_address = 0x804ddff8;
    powerups_ptr_address = 0x804bfcd4;
  }
  else if (region == Region::PAL) {
    // Same as NTSC but slightly offset
    code_changes.emplace_back(0x80099068, 0xec010072);
    code_changes.emplace_back(0x800992c4, 0x60000000);
    code_changes.emplace_back(0x80183cfc, 0x60000000);
    code_changes.emplace_back(0x80183d24, 0x60000000);
    code_changes.emplace_back(0x801768b4, 0x60000000);
    code_changes.emplace_back(0x802fb84c, 0xd23f009c);
    code_changes.emplace_back(0x8019fe64, 0x60000000);

    code_changes.emplace_back(0x80075d38, 0x48000044);
    add_beam_change_code_mp1(0x8018e7dc);

    mp1_static.yaw_vel_address = 0x804d7c78;
    mp1_static.pitch_address = 0x804d7f3c;
    mp1_static.pitch_goal_address = 0x804c502c;
    mp1_static.angvel_limiter_address = 0x804d7cb4;
    mp1_static.orbit_state_address = 0x804d7e60;
    mp1_static.lockon_address = 0x804c3ff3;
    mp1_static.tweak_player_address = 0x804e1f38;
    powerups_ptr_address = 0x804c3c14;
  }
  // If I add NTSC JP
  else {}

  active_visor_offset = 0x1c;
  powerups_size = 8;
  powerups_offset = 0x30;
  // I tried matching these two between ntsc & pal for whatever reason
  new_beam_address = 0x804a79f4;
  beamchange_flag_address = 0x804a79f0;
  has_beams = true;
}

void FpsControls::init_mod_mp1_gc(Region region) {
   if (region == Region::NTSC) {
    code_changes.emplace_back(0x8000f63c, 0x48000048);
    code_changes.emplace_back(0x8000e538, 0x60000000);
    code_changes.emplace_back(0x80016ee4, 0x4e800020);
    code_changes.emplace_back(0x80014820, 0x4e800020);
    code_changes.emplace_back(0x8000e73c, 0x60000000);
    code_changes.emplace_back(0x8000f810, 0x48000244);
    code_changes.emplace_back(0x8045c488, 0x4f800000);

    add_strafe_code_mp1_ntsc();

    mp1_gc_static.yaw_vel_address = 0x8046B97C + 0x14C;
    mp1_gc_static.pitch_address = 0x8046B97C + 0x3EC;
    mp1_gc_static.angvel_max_address = 0x8045c208 + 0x84;
    mp1_gc_static.orbit_state_address = 0x8046b97c + 0x304;
    mp1_gc_static.tweak_player_address = 0x8045c208;
  }
  else if (region == Region::PAL) {
    code_changes.emplace_back(0x8000FB4C, 0x48000048);  
    code_changes.emplace_back(0x8000EA60, 0x60000000);
    code_changes.emplace_back(0x80017878, 0x4e800020);
    code_changes.emplace_back(0x80015258, 0x4e800020);
    code_changes.emplace_back(0x8000EC64, 0x60000000);
    code_changes.emplace_back(0x8000FD20, 0x4800022C);
    code_changes.emplace_back(0x803e43b4, 0x4f800000);
    
    add_strafe_code_mp1_pal();
    
    mp1_gc_static.yaw_vel_address = 0x803F38A4 + 0x15C;
    mp1_gc_static.pitch_address = 0x803F38A4 + 0x3fC;
    mp1_gc_static.angvel_max_address = 0x803E4134 + 0x84;
    mp1_gc_static.orbit_state_address = 0x803F38A4 + 0x304;
    mp1_gc_static.tweak_player_address = 0x803E4134;
  }
  else {}
}

void FpsControls::init_mod_mp2(Region region) {
  if (region == Region::NTSC) {
    code_changes.emplace_back(0x8008ccc8, 0xc0430184);
    code_changes.emplace_back(0x8008cd1c, 0x60000000);
    code_changes.emplace_back(0x80147f70, 0x60000000);
    code_changes.emplace_back(0x80147f98, 0x60000000);
    code_changes.emplace_back(0x80135b20, 0x60000000);
    code_changes.emplace_back(0x8008bb48, 0x60000000);
    code_changes.emplace_back(0x8008bb18, 0x60000000);
    code_changes.emplace_back(0x803054a0, 0xd23f009c);
    code_changes.emplace_back(0x80169dbc, 0x60000000);
    code_changes.emplace_back(0x80143d00, 0x48000050);

    // Remove Beams/Visors Menu
    code_changes.emplace_back(0x8006fb58, 0x48000044);
    add_beam_change_code_mp2(0x8018cc88);

    mp2_static.cplayer_ptr_address = 0x804e87dc;
    mp2_static.load_state_address = 0x804e8824;
    mp2_static.lockon_address = 0x804e894f;
  }
  else if (region == Region::PAL) {
    code_changes.emplace_back(0x8008e30C, 0xc0430184);
    code_changes.emplace_back(0x8008e360, 0x60000000);
    code_changes.emplace_back(0x801496e4, 0x60000000);
    code_changes.emplace_back(0x8014970c, 0x60000000);
    code_changes.emplace_back(0x80137240, 0x60000000);
    code_changes.emplace_back(0x8008d18c, 0x60000000);
    code_changes.emplace_back(0x8008d15c, 0x60000000);
    code_changes.emplace_back(0x80307d2c, 0xd23f009c);
    code_changes.emplace_back(0x8016b534, 0x60000000);
    code_changes.emplace_back(0x80145474, 0x48000050);

    // Remove Beams/Visors Menu
    code_changes.emplace_back(0x800710D0, 0x48000044);
    add_beam_change_code_mp2(0x8018e41c);

    mp2_static.cplayer_ptr_address = 0x804efc2c;
    mp2_static.load_state_address = 0x804efc74;
    mp2_static.lockon_address = 0x804efd9f;
  }
  else {}

  active_visor_offset = 0x34;
  powerups_size = 12;
  powerups_offset = 0x5c;
  // They match again, serendipity
  new_beam_address = 0x804cd254;
  beamchange_flag_address = 0x804cd250;
  has_beams = true;
}

void FpsControls::init_mod_mp3(Region region) {
  if (region == Region::NTSC) {
    code_changes.emplace_back(0x80080ac0, 0xec010072);
    code_changes.emplace_back(0x8014e094, 0x60000000);
    code_changes.emplace_back(0x8014e06c, 0x60000000);
    code_changes.emplace_back(0x80134328, 0x60000000);
    code_changes.emplace_back(0x80133970, 0x60000000);
    code_changes.emplace_back(0x8000ab58, 0x4bffad29);
    code_changes.emplace_back(0x80080d44, 0x60000000);
    code_changes.emplace_back(0x8007fdc8, 0x480000e4);
    code_changes.emplace_back(0x8017f88c, 0x60000000);
    
    // Remove visors menu
    code_changes.emplace_back(0x800614ec, 0x48000018);
    add_control_state_hook_mp3(0x80005880, Region::NTSC);
    add_grapple_slide_code_mp3(0x8017f2a0);

    mp3_static.cplayer_ptr_address = 0x805c6c6c;
    mp3_static.cursor_dlg_enabled_address = 0x805c8d77;
    mp3_static.cursor_ptr_address = 0x8066fd08;
    mp3_static.cursor_offset = 0xc54;
    mp3_static.boss_id_address = 0x805c6f44;
    mp3_static.boss_id = 0x000201cd442f0000;
    mp3_static.lockon_address = 0x805c6db7;
    mp3_static.gun_lag_toc_offset = 0x5ff0;
  }
  else if (region == Region::PAL) {
    code_changes.emplace_back(0x80080ab8, 0xec010072);
    code_changes.emplace_back(0x8014d9e0, 0x60000000);
    code_changes.emplace_back(0x8014d9b8, 0x60000000);
    code_changes.emplace_back(0x80133c74, 0x60000000);
    code_changes.emplace_back(0x801332bc, 0x60000000);
    code_changes.emplace_back(0x8000ab58, 0x4bffad29);
    code_changes.emplace_back(0x80080d44, 0x60000000);
    code_changes.emplace_back(0x8007fdc8, 0x480000e4);
    code_changes.emplace_back(0x8017f1d8, 0x60000000);
    
    // Remove visors menu
    code_changes.emplace_back(0x800614ec, 0x48000018);
    add_control_state_hook_mp3(0x80005880, Region::PAL);
    add_grapple_slide_code_mp3(0x8017ebec);

    mp3_static.cplayer_ptr_address = 0x805ca0ec;
    mp3_static.cursor_dlg_enabled_address = 0x805cc1d7;
    mp3_static.cursor_ptr_address = 0x80673588;
    mp3_static.cursor_offset = 0xd04;
    mp3_static.boss_id_address = 0x805ca3c4;
    mp3_static.boss_id = 0x00020230442f0000;
    mp3_static.lockon_address = 0x805ca237;
    mp3_static.gun_lag_toc_offset = 0x6000;
  }
  else {}

  active_visor_offset = 0x34;
  powerups_size = 12;
  powerups_offset = 0x58;
  has_beams = false;
}
}
