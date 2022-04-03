#include "Core/PrimeHack/Mods/Noclip.h"
#include "Core/PrimeHack/PrimeUtils.h"

namespace prime
{
void Noclip::run_mod(Game game, Region region)
{
  switch (game) {
  case Game::PRIME_1: {
    LOOKUP(control_flag);
    run_mod_mp1(read32(control_flag) == 1);
    break;
  }
  case Game::PRIME_1_GCN:
  case Game::PRIME_1_GCN_R1:
  case Game::PRIME_1_GCN_R2:
    run_mod_mp1_gc(has_control_mp1_gc());
    break;
  case Game::PRIME_2:
    run_mod_mp2(has_control_mp2());
    break;
  case Game::PRIME_2_GCN:
    run_mod_mp2_gc(has_control_mp2_gc());
    break;
  case Game::PRIME_3:
  case Game::PRIME_3_STANDALONE:
    run_mod_mp3(has_control_mp3());
    break;
  default:
    break;
  }
}

bool Noclip::has_control_mp1_gc() {
  LOOKUP(state_manager);
  LOOKUP_DYN(world);
  if (world == 0) {
    return false;
  }

  LOOKUP_DYN(camera_state);
  if (camera_state == 0) {
    return false;
  }

  return (read32(world + 0x4) == 5) && (read32(camera_state) != 4) && (read8(state_manager + 0xf94) & 0x80);
}

bool Noclip::has_control_mp2() {
  LOOKUP(state_manager);
  LOOKUP(control_flag);

  LOOKUP_DYN(player);
  if (player == 0) {
    return false;
  }

  return read32(control_flag) == 1 && read32(state_manager + 0x153c) == 1 && read32(player + 0x374) != 5;
}

bool Noclip::has_control_mp2_gc() {
  LOOKUP_DYN(world);
  if (world == 0) {
    return false;
  }
  if (read32(world + 0x4) != 4) {
    return false;
  }

  LOOKUP_DYN(player);
  if (player == 0) {
    return false;
  }

  LOOKUP_DYN(camera_manager);
  if (camera_manager == 0 || read16(camera_manager) == 0xffff) {
    return false;
  }
  return true;
}

bool Noclip::has_control_mp3() {
  LOOKUP_DYN(object_list);
  if (object_list == 0) {
    return false;
  }
  return true;
}

vec3 Noclip::get_movement_vec(u32 camera_tf_addr) {
  Transform camera_tf(camera_tf_addr);

  vec3 movement_vec;
  if (CheckForward()) {
    movement_vec = movement_vec + camera_tf.fwd();
  }
  if (CheckBack()) {
    movement_vec = movement_vec + -camera_tf.fwd();
  }
  if (CheckLeft()) {
    movement_vec = movement_vec + -camera_tf.right();
  }
  if (CheckRight()) {
    movement_vec = movement_vec + camera_tf.right();
  }
  if (CheckJump()) {
    movement_vec = movement_vec + vec3(0, 0, 1);
  }

  DevInfoMatrix("camera_tf", camera_tf);

  return movement_vec;
}

void Noclip::run_mod_mp1(bool has_control) {
  LOOKUP_DYN(player);
  if (player == 0) {
    return;
  }

  if (!has_control) {
    player_transform.read_from(player + 0x2c);
    set_state(ModState::CODE_DISABLED);
    apply_instruction_changes();
    had_control = has_control;
    return;
  }
  if (has_control && !had_control) {
    set_state(ModState::ENABLED);
    had_control = has_control;
    return;
  }

  LOOKUP_DYN(object_list);
  if (object_list == 0) {
    return;
  }
  LOOKUP_DYN(camera_manager);
  if (camera_manager == 0) {
    return;
  }
  const u16 camera_id = read16(camera_manager);

  const u32 camera_tf_addr = read32(object_list + ((camera_id & 0x3ff) << 3) + 4) + 0x2c;
  vec3 movement_vec = (get_movement_vec(camera_tf_addr) * 0.5f) + player_transform.loc();

  player_transform.set_loc(movement_vec);
  writef32(movement_vec.x, player + 0x2c + 0x0c);
  writef32(movement_vec.y, player + 0x2c + 0x1c);
  writef32(movement_vec.z, player + 0x2c + 0x2c);
}

void Noclip::run_mod_mp1_gc(bool has_control) {
  LOOKUP_DYN(player);
  if (player == 0) {
    return;
  }
  LOOKUP_DYN(player_xf);
  if (!has_control) {
    player_transform.read_from(player_xf);
    set_state(ModState::CODE_DISABLED);
    apply_instruction_changes();
    had_control = has_control;
    return;
  }

  if (has_control && !had_control) {
    set_state(ModState::ENABLED);
    had_control = has_control;
    return;
  }
  
  LOOKUP_DYN(object_list);
  if (object_list == 0) {
    return;
  }
  LOOKUP_DYN(camera_manager);
  if (camera_manager == 0) {
    return;
  }

  const u16 camera_uid = read16(camera_manager);
  if (camera_uid == 0xffff) {
    return;
  }

  u32 camera_offset = (camera_uid & 0x3ff) << 3;
  const u32 camera_address =
      read32(object_list + 4 + camera_offset);
  vec3 movement_vec = (get_movement_vec(camera_address + 0x34) * 0.5f) + player_transform.loc();

  LOOKUP_DYN(move_state);
  player_transform.set_loc(movement_vec);
  writef32(movement_vec.x, player_xf + 0x0c);
  writef32(movement_vec.y, player_xf + 0x1c);
  writef32(movement_vec.z, player_xf + 0x2c);
  write32(0, move_state);
}

void Noclip::run_mod_mp2(bool has_control) {
  LOOKUP_DYN(player);
  if (player == 0) {
    return;
  }

  if (!has_control) {
    player_vec.read_from(player + 0x50);
    set_state(ModState::CODE_DISABLED);
    apply_instruction_changes();
    had_control = has_control;
    return;
  }

  if (has_control && !had_control) {
    set_state(ModState::ENABLED);
    had_control = has_control;
    return;
  }
  LOOKUP_DYN(object_list);
  if (object_list == 0) {
    return;
  }
  LOOKUP_DYN(camera_manager);
  if (camera_manager == 0) {
    return;
  }

  const u16 camera_uid = read16(camera_manager);
  if (camera_uid == 0xffff) {
    return;
  }
  u32 camera_address = read32(object_list + 4 + ((camera_uid & 0x3ff) << 3));

  player_vec = (get_movement_vec(camera_address + 0x20) * 0.5f) + player_vec;
  player_vec.write_to(player + 0x50);
}

void Noclip::run_mod_mp2_gc(bool has_control) {
  LOOKUP_DYN(player);
  if (player == 0) {
    return;
  }
  
  if (!mem_check(player)) {
    return;
  }

  if (!has_control) {
    player_vec.read_from(player + 0x54);
    set_state(ModState::CODE_DISABLED);
    apply_instruction_changes();
    had_control = has_control;
    return;
  }
  if (has_control && !had_control) {
    set_state(ModState::ENABLED);
    had_control = has_control;
    return;
  }
  LOOKUP_DYN(object_list);
  if (object_list == 0) {
    return;
  }
  LOOKUP_DYN(camera_manager);
  if (camera_manager == 0) {
    return;
  }
  
  const u16 camera_uid = read16(camera_manager);
  if (camera_uid == 0xffff) {
    return;
  }
  u32 camera_address = read32(object_list + 4 + ((camera_uid & 0x3ff) << 3));

  player_vec = (get_movement_vec(camera_address + 0x24) * 0.5f) + player_vec;
  player_vec.write_to(player + 0x54);
}

void Noclip::run_mod_mp3(bool has_control) {
  LOOKUP_DYN(player);
  if (player == 0) {
    return;
  }

  if (!has_control) {
    player_vec.read_from(player + 0x6c);
    set_state(ModState::CODE_DISABLED);
    apply_instruction_changes();
    had_control = has_control;
    return;
  }

  if (has_control && !had_control) {
    set_state(ModState::ENABLED);
    had_control = has_control;
    return;
  }
  LOOKUP_DYN(object_list);
  if (object_list == 0) {
    return;
  }
  LOOKUP_DYN(camera_manager);
  if (camera_manager == 0) {
    return;
  }

  const u16 camera_id = read16(camera_manager);
  if (camera_id == 0xffff) {
    return;
  }
  const u32 camera_address = read32(object_list + 4 + ((camera_id & 0x7ff) << 3));

  player_vec = (get_movement_vec(camera_address + 0x3c) * 0.5f) + player_vec;
  player_vec.write_to(player + 0x6c);
}

bool Noclip::init_mod(Game game, Region region) {
  switch (game) {
  case Game::PRIME_1:
    if (region == Region::NTSC_U) {
      noclip_code_mp1(0x804d3c20, 0x800053a4, 0x8000bb80);
      add_code_change(0x80196ab4, 0x60000000);
      add_code_change(0x80196abc, 0x60000000);
      add_code_change(0x80196ac4, 0x60000000);
      add_code_change(0x80196ad8, 0xd0410084);
      add_code_change(0x80196adc, 0xd0210094);
      add_code_change(0x80196ae0, 0xd00100a4);
      add_code_change(0x80196ae4, 0x481b227d);
    } else if (region == Region::PAL) {
      noclip_code_mp1(0x804d7b60, 0x800053a4, 0x8000bb80);
      add_code_change(0x80196d4c, 0x60000000);
      add_code_change(0x80196d54, 0x60000000);
      add_code_change(0x80196d5c, 0x60000000);
      add_code_change(0x80196d70, 0xd0410084);
      add_code_change(0x80196d74, 0xd0210094);
      add_code_change(0x80196d78, 0xd00100a4);
      add_code_change(0x80196d7c, 0x481b1f39);
    } else if (region == Region::NTSC_J) {
      noclip_code_mp1(0x804de278, 0x800053a4, 0x8000bb80);
      add_code_change(0x80197634, 0x60000000);
      add_code_change(0x8019763c, 0x60000000);
      add_code_change(0x80197644, 0x60000000);
      add_code_change(0x80197658, 0xd0410084);
      add_code_change(0x8019765c, 0xd0210094);
      add_code_change(0x80197660, 0xd00100a4);
      add_code_change(0x80197664, 0x481b11c1);
    }
    break;
  case Game::PRIME_1_GCN:
    if (region == Region::NTSC_U) {
      noclip_code_mp1_gc(0x8046b97c, 0x805afd00, 0x80052e90);
      // For whatever reason CPlayer::Teleport calls SetTransform then SetTranslation
      // which the above code changes will mess up, so just force the position to be
      // used in SetTransform and remove the call to SetTranslation, now Teleport works
      // when SetTranslation is ignoring the player
      // This is handled above in mp1 trilogy as well
      add_code_change(0x80285128, 0x60000000);
      add_code_change(0x80285138, 0x60000000);
      add_code_change(0x80285140, 0x60000000);
      add_code_change(0x80285168, 0xd0010078);
      add_code_change(0x8028516c, 0xd0210088);
      add_code_change(0x80285170, 0xd0410098);
      add_code_change(0x80285174, 0x4808d9cd);
    } else if (region == Region::PAL) {
      noclip_code_mp1_gc(0x803f38a4, 0x80471d00, 0x80053fa4);

      add_code_change(0x802724ac, 0x60000000);
      add_code_change(0x802724bc, 0x60000000);
      add_code_change(0x802724c4, 0x60000000);
      add_code_change(0x802724ec, 0xd0010078);
      add_code_change(0x802724f0, 0xd0210088);
      add_code_change(0x802724f4, 0xd0410098);
      add_code_change(0x802724f8, 0x48089419);
    }
    break;
  case Game::PRIME_1_GCN_R1:
    // 0x8046bb5c, 0x805afd00 0x80052f0c
    noclip_code_mp1_gc(0x8046bb5c, 0x805afd00, 0x80052f0c);
    add_code_change(0x802851a4, 0x60000000);
    add_code_change(0x802851b4, 0x60000000);
    add_code_change(0x802851bc, 0x60000000);
    add_code_change(0x802851e4, 0xd0010078);
    add_code_change(0x802851e8, 0xd0210088);
    add_code_change(0x802851ec, 0xd0410098);
    add_code_change(0x802851f0, 0x4808da31);
    break;
  case Game::PRIME_1_GCN_R2:
    noclip_code_mp1_gc(0x8046c9e8, 0x805b0d00, 0x800531d8);

    add_code_change(0x80285a9c, 0x60000000);
    add_code_change(0x80285aac, 0x60000000);
    add_code_change(0x80285ab4, 0x60000000);
    add_code_change(0x80285adc, 0xd0010078);
    add_code_change(0x80285ae0, 0xd0210088);
    add_code_change(0x80285ae4, 0xd0410098);
    add_code_change(0x80285ae8, 0x4808daa9);
    break;
  case Game::PRIME_2:
    if (region == Region::NTSC_U) {
      noclip_code_mp2(0x804e87dc, 0x800053a4, 0x8000d694);
      add_code_change(0x80160d68, 0x60000000);
      add_code_change(0x80160d70, 0x60000000);
      add_code_change(0x80160d78, 0x60000000);
      add_code_change(0x80160d80, 0xd0410084);
      add_code_change(0x80160d84, 0xd0210094);
      add_code_change(0x80160d88, 0xd00100a4);
      add_code_change(0x80160d8c, 0x4bead525);
    } else if (region == Region::PAL) {
      noclip_code_mp2(0x804efc2c, 0x800053a4, 0x8000d694);
      add_code_change(0x801624e0, 0x60000000);
      add_code_change(0x801624e8, 0x60000000);
      add_code_change(0x801624f0, 0x60000000);
      add_code_change(0x801624f8, 0xd0410084);
      add_code_change(0x801624fc, 0xd0210094);
      add_code_change(0x80162500, 0xd00100a4);
      add_code_change(0x80162504, 0x4beabdad);
    } else if (region == Region::NTSC_J) {
      noclip_code_mp2(0x804e8fcc, 0x800053a4, 0x8000d694);
      add_code_change(0x80160330, 0x60000000);
      add_code_change(0x80160338, 0x60000000);
      add_code_change(0x80160340, 0x60000000);
      add_code_change(0x80160348, 0xd0410084);
      add_code_change(0x8016034c, 0xd0210094);
      add_code_change(0x80160350, 0xd00100a4);
      add_code_change(0x80160354, 0x4bead525);
    }
    break;
  case Game::PRIME_2_GCN:
    if (region == Region::NTSC_U) {
      noclip_code_mp2_gc(0x803dcbdc, 0x80420000, 0x8004abc8);
      add_code_change(0x801865d8, 0x60000000);
      add_code_change(0x801865e0, 0x60000000);
      add_code_change(0x801865e8, 0x60000000);
      add_code_change(0x801865f0, 0xd0410098);
      add_code_change(0x801865f4, 0xd0210088);
      add_code_change(0x801865f8, 0xd0010078);
      add_code_change(0x801865fc, 0x4bec395d);
    } else if (region == Region::NTSC_J) {
      noclip_code_mp2_gc(0x803dfb7c, 0x80420000, 0x8004b6ac);
      add_code_change(0x801880d8, 0x60000000);
      add_code_change(0x801880e0, 0x60000000);
      add_code_change(0x801880e8, 0x60000000);
      add_code_change(0x801880f0, 0xd0410098);
      add_code_change(0x801880f4, 0xd0210088);
      add_code_change(0x801880f8, 0xd0010078);
      add_code_change(0x801880fc, 0x4bec2939);
    } else if (region == Region::PAL) {
      noclip_code_mp2_gc(0x803dddfc, 0x80420000, 0x8004ad44);
      add_code_change(0x801868bc, 0x60000000);
      add_code_change(0x801868c4, 0x60000000);
      add_code_change(0x801868cc, 0x60000000);
      add_code_change(0x801868d4, 0xd0410098);
      add_code_change(0x801868d8, 0xd0210088);
      add_code_change(0x801868dc, 0xd0010078);
      add_code_change(0x801868e0, 0x4bec37e9);
    }
    break;
  case Game::PRIME_3:
    if (region == Region::NTSC_U) {
      noclip_code_mp3(0x805c6c40, 0x80004380, 0x8000bbfc);
      add_code_change(0x801784ac, 0x60000000);
      add_code_change(0x801784b4, 0x60000000);
      add_code_change(0x801784bc, 0x60000000);
      add_code_change(0x801784c4, 0xd0410084);
      add_code_change(0x801784c8, 0xd0210094);
      add_code_change(0x801784cc, 0xd00100a4);
      add_code_change(0x801784d0, 0x4be938a1);
    } else if (region == Region::PAL) {
      noclip_code_mp3(0x805ca0c0, 0x80004380, 0x8000bbfc);
      add_code_change(0x80177df8, 0x60000000);
      add_code_change(0x80177e00, 0x60000000);
      add_code_change(0x80177e08, 0x60000000);
      add_code_change(0x80177e10, 0xd0410084);
      add_code_change(0x80177e14, 0xd0210094);
      add_code_change(0x80177e18, 0xd00100a4);
      add_code_change(0x80177e1c, 0x4be93f55);
    }
    break;
  case Game::PRIME_3_STANDALONE:
    if (region == Region::NTSC_U) {
      noclip_code_mp3(0x805c4f70, 0x80004380, 0x8000bee8);
      add_code_change(0x8017c054, 0x60000000);
      add_code_change(0x8017c05c, 0x60000000);
      add_code_change(0x8017c064, 0x60000000);
      add_code_change(0x8017c06c, 0xd0410084);
      add_code_change(0x8017c070, 0xd0210094);
      add_code_change(0x8017c074, 0xd00100a4);
      add_code_change(0x8017c078, 0x4be938a1);
    } else if (region == Region::PAL) {
      noclip_code_mp3(0x805c7570, 0x80004380, 0x8000bee8);
      add_code_change(0x8017cb48, 0x60000000);
      add_code_change(0x8017cb50, 0x60000000);
      add_code_change(0x8017cb58, 0x60000000);
      add_code_change(0x8017cb60, 0xd0410084);
      add_code_change(0x8017cb74, 0xd0210094);
      add_code_change(0x8017cb78, 0xd00100a4);
      add_code_change(0x8017cb7c, 0x4be938a1);
    } else if (region == Region::NTSC_J) {
      noclip_code_mp3(0x805caa30, 0x80004380, 0x8000bee8);
      add_code_change(0x8017dd54, 0x60000000);
      add_code_change(0x8017dd5c, 0x60000000);
      add_code_change(0x8017dd64, 0x60000000);
      add_code_change(0x8017dd6c, 0xd0410084);
      add_code_change(0x8017dd70, 0xd0210094);
      add_code_change(0x8017dd74, 0xd00100a4);
      add_code_change(0x8017dd78, 0x4be938a1);
    }
    break;
  default:
    break;
  }
  return true;
}

void Noclip::noclip_code_mp1(u32 cplayer_address, u32 start_point, u32 return_location) {
  u32 return_delta = return_location - (start_point + 0x14);
  u32 start_delta = start_point - (return_location - 4);
  add_code_change(start_point + 0x00, 0x3ca00000 | ((cplayer_address >> 16) & 0xffff));
  add_code_change(start_point + 0x04, 0x60a50000 | (cplayer_address & 0xffff));
  add_code_change(start_point + 0x08, 0x7c032800);
  add_code_change(start_point + 0x0c, 0x4d820020);
  add_code_change(start_point + 0x10, 0x800300e8);
  add_code_change(start_point + 0x14, 0x48000000 | (return_delta & 0x3fffffc));
  add_code_change(return_location - 4, 0x48000000 | (start_delta & 0x3fffffc));
}

void Noclip::noclip_code_mp1_gc(u32 cplayer_address, u32 start_point, u32 return_location) {
  u32 return_delta = return_location - (start_point + 0x14);
  u32 start_delta = start_point - (return_location - 4);
  add_code_change(start_point + 0x00, 0x3ca00000 | ((cplayer_address >> 16) & 0xffff));
  add_code_change(start_point + 0x04, 0x60a50000 | (cplayer_address & 0xffff));
  add_code_change(start_point + 0x08, 0x7c032800);
  add_code_change(start_point + 0x0c, 0x4d820020);
  add_code_change(start_point + 0x10, 0xc0040000);
  add_code_change(start_point + 0x14, 0x48000000 | (return_delta & 0x3fffffc));
  add_code_change(return_location - 4, 0x48000000 | (start_delta & 0x3fffffc));
}

void Noclip::noclip_code_mp2(u32 cplayer_address, u32 start_point, u32 return_location) {
  u32 return_delta = return_location - (start_point + 0x18);
  u32 start_delta = start_point - (return_location - 4);
  add_code_change(start_point + 0x00, 0x3ca00000 | ((cplayer_address >> 16) & 0xffff));
  add_code_change(start_point + 0x04, 0x60a50000 | (cplayer_address & 0xffff));
  add_code_change(start_point + 0x08, 0x80a50000);
  add_code_change(start_point + 0x0c, 0x7c032800);
  add_code_change(start_point + 0x10, 0x4d820020);
  add_code_change(start_point + 0x14, 0xc0040000);
  add_code_change(start_point + 0x18, 0x48000000 | (return_delta & 0x3fffffc));
  add_code_change(return_location - 4, 0x48000000 | (start_delta & 0x3fffffc));
}

void Noclip::noclip_code_mp2_gc(u32 cplayer_address, u32 start_point, u32 return_location) {
  u32 return_delta = return_location - (start_point + 0x18);
  u32 start_delta = start_point - (return_location - 4);
  add_code_change(start_point + 0x00, 0x3ca00000 | ((cplayer_address >> 16) & 0xffff));
  add_code_change(start_point + 0x04, 0x60a50000 | (cplayer_address & 0xffff));
  add_code_change(start_point + 0x08, 0x80a50000);
  add_code_change(start_point + 0x0c, 0x7c032800);
  add_code_change(start_point + 0x10, 0x4d820020);
  add_code_change(start_point + 0x14, 0x9421fff0);
  add_code_change(start_point + 0x18, 0x48000000 | (return_delta & 0x3fffffc));
  add_code_change(return_location - 4, 0x48000000 | (start_delta & 0x3fffffc));
}

void Noclip::noclip_code_mp3(u32 cplayer_address, u32 start_point, u32 return_location) {
  u32 return_delta = return_location - (start_point + 0x1c);
  u32 start_delta = start_point - (return_location - 4);
  add_code_change(start_point + 0x00, 0x3ca00000 | ((cplayer_address >> 16) & 0xffff));
  add_code_change(start_point + 0x04, 0x60a50000 | (cplayer_address & 0xffff));
  add_code_change(start_point + 0x08, 0x80a50028);
  add_code_change(start_point + 0x0c, 0x80a52184);
  add_code_change(start_point + 0x10, 0x7c032800);
  add_code_change(start_point + 0x14, 0x4d820020);
  add_code_change(start_point + 0x18, 0xc0040000);
  add_code_change(start_point + 0x1c, 0x48000000 | (return_delta & 0x3fffffc));
  add_code_change(return_location - 4, 0x48000000 | (start_delta & 0x3fffffc));
}

void Noclip::on_state_change(ModState old_state) {
  LOOKUP_DYN(player);
  if (player == 0) {
    return;
  }

  if (mod_state() == ModState::ENABLED && old_state != ModState::ENABLED) {
    switch (hack_mgr->get_active_game()) {
    case Game::PRIME_1:
      player_transform.read_from(player + 0x2c);
      old_matexclude_list = read64(player + 0x70);
      write64(0xffffffffffffffff, player + 0x70);
      break;
    case Game::PRIME_1_GCN:
    case Game::PRIME_1_GCN_R1:
    case Game::PRIME_1_GCN_R2:
      player_transform.read_from(player + 0x34);
      old_matexclude_list = read64(player + 0x78);
      write64(0xffffffffffffffff, player + 0x78);
      break;
    case Game::PRIME_2:
      player_vec.read_from(player + 0x50);
      old_matexclude_list = read64(player + 0x70);
      write64(0xffffffffffffffff, player + 0x70);
      break;
    case Game::PRIME_2_GCN:
      player_vec.read_from(player + 0x54);
      old_matexclude_list = read64(player + 0x74);
      write64(0xffffffffffffffff, player + 0x74);
      break;
    case Game::PRIME_3:
      player_vec.read_from(player + 0x6c);
      old_matexclude_list = read64(player + 0x88);
      write64(0xffffffffffffffff, player + 0x88);
      break;
    case Game::PRIME_3_STANDALONE:
      player_vec.read_from(player + 0x6c);
      old_matexclude_list = read64(player + 0x88);
      write64(0xffffffffffffffff, player + 0x88);
      break;
    }
  } else if ((mod_state() == ModState::DISABLED || mod_state() == ModState::CODE_DISABLED) &&
           old_state == ModState::ENABLED) {
    switch (hack_mgr->get_active_game()) {
    case Game::PRIME_1:
      write64(old_matexclude_list, player + 0x70);
      break;
    case Game::PRIME_1_GCN:
    case Game::PRIME_1_GCN_R1:
    case Game::PRIME_1_GCN_R2:
      write64(old_matexclude_list, player + 0x78);
      break;
    case Game::PRIME_2:
      write64(old_matexclude_list, player + 0x70);
      break;
    case Game::PRIME_2_GCN:
      write64(old_matexclude_list, player + 0x74);
      break;
    case Game::PRIME_3:
      write64(old_matexclude_list, player + 0x88);
      break;
    case Game::PRIME_3_STANDALONE:
      write64(old_matexclude_list, player + 0x88);
      break;
    }
  }
}
}  // namespace prime
