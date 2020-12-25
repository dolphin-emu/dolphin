#include "Core/PrimeHack/Mods/Noclip.h"
#include "Core/PrimeHack/PrimeUtils.h"

namespace prime
{
void Noclip::run_mod(Game game, Region region)
{
  switch (game)
  {
  case Game::PRIME_1:
    run_mod_mp1(read32(mp1_static.control_flag_address) == 1);
    break;
  case Game::PRIME_1_GCN:
    run_mod_mp1_gc(has_control_mp1_gc());
    break;
  case Game::PRIME_2:
    run_mod_mp2(has_control_mp2());
    break;
  case Game::PRIME_2_GCN:
    run_mod_mp2_gc(has_control_mp2_gc());
    break;
  case Game::PRIME_3:
    run_mod_mp3(has_control_mp3());
    break;
  case Game::PRIME_3_STANDALONE:
    run_mod_mp3(has_control_mp3());
    break;
  default:
    break;
  }
}

bool Noclip::has_control_mp1_gc()
{
  u8 version = read8(0x80000007);
  if (version != 0) {
    return false;
  }
  u32 world_address = read32(mp1_gc_static.state_mgr_address + 0x850);
  if (!mem_check(world_address))
  {
    return false;
  }
  int world_load_state = read32(world_address + 0x4);
  int camera_state = read32(mp1_gc_static.cplayer_address + 0x2f4);
  u8 statemgr_flags = read8(mp1_gc_static.state_mgr_address + 0xf94);
  return (world_load_state == 5) && (camera_state != 4) && (statemgr_flags & 0x80);
}

bool Noclip::has_control_mp2()
{
  u32 cplayer_address = read32(mp2_static.cplayer_ptr_address);
  if (!mem_check(cplayer_address))
  {
    return false;
  }
  return read32(mp2_static.control_flag_address) == 1 &&
         read32(mp2_static.load_state_address) == 1 && read32(cplayer_address + 0x374) != 5;
}

bool Noclip::has_control_mp2_gc()
{
  u32 world_address = read32(mp2_gc_static.state_mgr_address + 0x1604);
  if (!mem_check(world_address))
  {
    return false;
  }
  if (read32(world_address + 0x4) != 4)
  {
    return false;
  }
  u32 cplayer_address = read32(mp2_gc_static.state_mgr_address + 0x14fc);
  if (!mem_check(cplayer_address))
  {
    return false;
  }
  u32 camera_mgr_address = read32(mp2_gc_static.state_mgr_address + 0x151c);
  if (read16(camera_mgr_address + 0x16) != 0xffff)
  {
    return false;
  }
  return true;
}

bool Noclip::has_control_mp3()
{
  Game game = GetHackManager()->get_active_game();
  Region region = GetHackManager()->get_active_region();
  u32 state_mgr_ptr_offset = 0x28;
  if (game == Game::PRIME_3_STANDALONE && region == Region::NTSC_U)
    state_mgr_ptr_offset += 4;
  u32 state_mgr = read32(mp3_static.state_mgr_ptr_address + state_mgr_ptr_offset);
  if (!mem_check(state_mgr))
  {
    return false;
  }
  if (!mem_check(read32(state_mgr + 0x1010)))
  {
    return false;
  }
  return true;
}

vec3 Noclip::get_movement_vec(u32 camera_tf_addr)
{
  Transform camera_tf(camera_tf_addr);

  vec3 movement_vec;
  if (CheckForward())
  {
    movement_vec = movement_vec + camera_tf.fwd();
  }
  if (CheckBack())
  {
    movement_vec = movement_vec + -camera_tf.fwd();
  }
  if (CheckLeft())
  {
    movement_vec = movement_vec + -camera_tf.right();
  }
  if (CheckRight())
  {
    movement_vec = movement_vec + camera_tf.right();
  }
  if (CheckJump())
  {
    movement_vec = movement_vec + vec3(0, 0, 1);
  }

  DevInfoMatrix("Camera", camera_tf);

  return movement_vec;
}

void Noclip::run_mod_mp1(bool has_control)
{
  if (!has_control)
  {
    player_tf.read_from(mp1_static.cplayer_address + 0x2c);
    set_state(ModState::CODE_DISABLED);
    apply_instruction_changes();
    had_control = has_control;
    return;
  }
  if (has_control && !had_control)
  {
    set_state(ModState::ENABLED);
    had_control = has_control;
    return;
  }

  const u32 camera_ptr = read32(mp1_static.object_list_ptr_address);
  const u32 camera_offset = (((read32(mp1_static.camera_uid_address) + 10) >> 16) & 0x3ff) << 3;
  const u32 camera_tf_addr = read32(camera_ptr + camera_offset + 4) + 0x2c;
  vec3 movement_vec = (get_movement_vec(camera_tf_addr) * 0.5f) + player_tf.loc();

  DevInfo("Camera_Tf_Addr", "%08X", camera_tf_addr);

  player_tf.set_loc(movement_vec);
  writef32(movement_vec.x, mp1_static.cplayer_address + 0x2c + 0x0c);
  writef32(movement_vec.y, mp1_static.cplayer_address + 0x2c + 0x1c);
  writef32(movement_vec.z, mp1_static.cplayer_address + 0x2c + 0x2c);
}

void Noclip::run_mod_mp1_gc(bool has_control)
{
  u8 version = read8(0x80000007);

  if (version != 0) {
    return;
  }

  if (!has_control)
  {
    player_tf.read_from(mp1_gc_static.cplayer_address + 0x34);
    set_state(ModState::CODE_DISABLED);
    apply_instruction_changes();
    had_control = has_control;
    return;
  }
  if (has_control && !had_control)
  {
    set_state(ModState::ENABLED);
    had_control = has_control;
    return;
  }
  const u16 camera_uid = read16(mp1_gc_static.camera_uid_address);
  if (camera_uid == -1)
  {
    return;
  }
  u32 camera_offset = (camera_uid & 0x3ff) << 3;
  const u32 camera_address =
      read32(read32(mp1_gc_static.state_mgr_address + 0x810) + 4 + camera_offset);
  vec3 movement_vec = (get_movement_vec(camera_address + 0x34) * 0.5f) + player_tf.loc();

  DevInfo("Camera_Tf_Addr", "%08X", camera_address + 0x34);

  player_tf.set_loc(movement_vec);
  writef32(movement_vec.x, mp1_gc_static.cplayer_address + 0x34 + 0x0c);
  writef32(movement_vec.y, mp1_gc_static.cplayer_address + 0x34 + 0x1c);
  writef32(movement_vec.z, mp1_gc_static.cplayer_address + 0x34 + 0x2c);
  write32(0, mp1_gc_static.cplayer_address + 0x258 +
                 (GetHackManager()->get_active_region() == Region::PAL ? 0x10 : 0));
}

void Noclip::run_mod_mp2(bool has_control)
{
  const u32 cplayer_address = read32(mp2_static.cplayer_ptr_address);
  if (!has_control)
  {
    player_vec.read_from(cplayer_address + 0x50);
    set_state(ModState::CODE_DISABLED);
    apply_instruction_changes();
    had_control = has_control;
    return;
  }
  if (has_control && !had_control)
  {
    set_state(ModState::ENABLED);
    had_control = has_control;
    return;
  }
  u32 object_list_address = read32(mp2_static.object_list_ptr_address);
  if (!mem_check(object_list_address))
  {
    return;
  }
  const u16 camera_uid = read16(mp2_static.camera_uid_address);
  if (camera_uid == -1)
  {
    return;
  }
  const u32 camera_offset = (camera_uid & 0x3ff) << 3;
  u32 camera_address = read32(object_list_address + 4 + camera_offset);

  player_vec = (get_movement_vec(camera_address + 0x20) * 0.5f) + player_vec;
  player_vec.write_to(cplayer_address + 0x50);
}

void Noclip::run_mod_mp2_gc(bool has_control)
{
  const u32 cplayer_address = read32(mp2_gc_static.state_mgr_address + 0x14fc);
  if (!has_control)
  {
    player_vec.read_from(cplayer_address + 0x54);
    set_state(ModState::CODE_DISABLED);
    apply_instruction_changes();
    had_control = has_control;
    return;
  }
  if (has_control && !had_control)
  {
    set_state(ModState::ENABLED);
    had_control = has_control;
    return;
  }
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

  player_vec = (get_movement_vec(camera_address + 0x24) * 0.5f) + player_vec;
  player_vec.write_to(cplayer_address + 0x54);
}

void Noclip::run_mod_mp3(bool has_control)
{
  Game game = GetHackManager()->get_active_game();
  Region region = GetHackManager()->get_active_region();
  u32 offset = 0x28;
  if (game == Game::PRIME_3_STANDALONE && region == Region::NTSC_U)
    offset += 4;
  u32 cplayer_address = read32(read32(mp3_static.state_mgr_ptr_address + offset) + 0x2184);
  if (!mem_check(cplayer_address))
  {
    return;
  }

  if (!has_control)
  {
    player_vec.read_from(cplayer_address + 0x6c);
    set_state(ModState::CODE_DISABLED);
    apply_instruction_changes();
    had_control = has_control;
    return;
  }
  if (has_control && !had_control)
  {
    set_state(ModState::ENABLED);
    had_control = has_control;
    return;
  }

  u32 object_list_address = read32(read32(mp3_static.state_mgr_ptr_address + offset) + 0x1010);
  if (!mem_check(object_list_address))
  {
    return;
  }
  const u16 camera_id = read16(read32(read32(mp3_static.cam_uid_ptr_address) + 0xc) + 0x16);
  if (camera_id == 0xffff)
  {
    return;
  }
  u32 camera_address = read32(object_list_address + 4 + (8 * camera_id));

  player_vec = (get_movement_vec(camera_address + 0x3c) * 0.5f) + player_vec;
  player_vec.write_to(cplayer_address + 0x6c);
}

bool Noclip::init_mod(Game game, Region region)
{
  u8 version = read8(0x80000007);
  switch (game)
  {
  case Game::PRIME_1:
    if (region == Region::NTSC_U)
    {
      noclip_code_mp1(0x804d3c20, 0x800053a4, 0x8000bb80);
      add_code_change(0x80196ab4, 0x60000000);
      add_code_change(0x80196abc, 0x60000000);
      add_code_change(0x80196ac4, 0x60000000);
      add_code_change(0x80196ad8, 0xd0410084);
      add_code_change(0x80196adc, 0xd0210094);
      add_code_change(0x80196ae0, 0xd00100a4);
      add_code_change(0x80196ae4, 0x481b227d);

      mp1_static.control_flag_address = 0x8052e9b8;
      mp1_static.cplayer_address = 0x804d3c20;
      mp1_static.object_list_ptr_address = 0x804bfc30;
      mp1_static.camera_uid_address = 0x804c4a08;
    }
    else if (region == Region::NTSC_J)
    {
      noclip_code_mp1(0x804de278, 0x800053a4, 0x8000bb80);
      add_code_change(0x80197634, 0x60000000);
      add_code_change(0x8019763c, 0x60000000);
      add_code_change(0x80197644, 0x60000000);
      add_code_change(0x80197658, 0xd0410084);
      add_code_change(0x8019765c, 0xd0210094);
      add_code_change(0x80197660, 0xd00100a4);
      add_code_change(0x80197664, 0x481b11c1);

      mp1_static.control_flag_address = 0x805aecb8;
      mp1_static.cplayer_address = 0x804d3ea0;
      mp1_static.object_list_ptr_address = 0x804bfeb0;
      mp1_static.camera_uid_address = 0x804c4c88;
    }
    else if (region == Region::PAL)
    {
      noclip_code_mp1(0x804d7b60, 0x800053a4, 0x8000bb80);
      add_code_change(0x80196d4c, 0x60000000);
      add_code_change(0x80196d54, 0x60000000);
      add_code_change(0x80196d5c, 0x60000000);
      add_code_change(0x80196d70, 0xd0410084);
      add_code_change(0x80196d74, 0xd0210094);
      add_code_change(0x80196d78, 0xd00100a4);
      add_code_change(0x80196d7c, 0x481b1f39);

      mp1_static.control_flag_address = 0x80532b38;
      mp1_static.cplayer_address = 0x804d7b60;
      mp1_static.object_list_ptr_address = 0x804c3b70;
      mp1_static.camera_uid_address = 0x804c8948;
    }
    else
    {
    }
    break;
  case Game::PRIME_1_GCN:
    if (region == Region::NTSC_U)
    {
      if (version == 0) {
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

        mp1_gc_static.cplayer_address = 0x8046b97c;
        mp1_gc_static.state_mgr_address = 0x8045a1a8;
        mp1_gc_static.camera_uid_address = 0x8045c5b4;
      }
    }
    else if (region == Region::PAL)
    {
      noclip_code_mp1_gc(0x803f38a4, 0x80471d00, 0x80053fa4);

      add_code_change(0x802724ac, 0x60000000);
      add_code_change(0x802724bc, 0x60000000);
      add_code_change(0x802724c4, 0x60000000);
      add_code_change(0x802724ec, 0xd0010078);
      add_code_change(0x802724f0, 0xd0210088);
      add_code_change(0x802724f4, 0xd0410098);
      add_code_change(0x802724f8, 0x48089419);

      mp1_gc_static.cplayer_address = 0x803f38a4;
      mp1_gc_static.state_mgr_address = 0x803e2088;
      mp1_gc_static.camera_uid_address = 0x803e44dc;
    }
    else
    {
    }
    break;
  case Game::PRIME_2:
    if (region == Region::NTSC_U)
    {
      noclip_code_mp2(0x804e87dc, 0x800053a4, 0x8000d694);
      add_code_change(0x80160d68, 0x60000000);
      add_code_change(0x80160d70, 0x60000000);
      add_code_change(0x80160d78, 0x60000000);
      add_code_change(0x80160d80, 0xd0410084);
      add_code_change(0x80160d84, 0xd0210094);
      add_code_change(0x80160d88, 0xd00100a4);
      add_code_change(0x80160d8c, 0x4bead525);

      mp2_static.cplayer_ptr_address = 0x804e87dc;
      mp2_static.object_list_ptr_address = 0x804e7af8;
      mp2_static.camera_uid_address = 0x804eb9b0;
      mp2_static.control_flag_address = 0x805373f8;
      mp2_static.load_state_address = 0x804e8824;
    }
    else if (region == Region::NTSC_J)
    {
      noclip_code_mp2(0x804e8fcc, 0x800053a4, 0x8000d694);
      add_code_change(0x80160330, 0x60000000);
      add_code_change(0x80160338, 0x60000000);
      add_code_change(0x80160340, 0x60000000);
      add_code_change(0x80160348, 0xd0410084);
      add_code_change(0x8016034c, 0xd0210094);
      add_code_change(0x80160350, 0xd00100a4);
      add_code_change(0x80160354, 0x4bead525);

      mp2_static.cplayer_ptr_address = 0x804e8fcc;
      mp2_static.object_list_ptr_address = 0x804e82e8;
      mp2_static.camera_uid_address = 0x804ec158;
      mp2_static.control_flag_address = 0x80537bb8;
      mp2_static.load_state_address = 0x804e9014;
    }
    else if (region == Region::PAL)
    {
      noclip_code_mp2(0x804efc2c, 0x800053a4, 0x8000d694);
      add_code_change(0x801624e0, 0x60000000);
      add_code_change(0x801624e8, 0x60000000);
      add_code_change(0x801624f0, 0x60000000);
      add_code_change(0x801624f8, 0xd0410084);
      add_code_change(0x801624fc, 0xd0210094);
      add_code_change(0x80162500, 0xd00100a4);
      add_code_change(0x80162504, 0x4beabdad);

      mp2_static.cplayer_ptr_address = 0x804efc2c;
      mp2_static.object_list_ptr_address = 0x804eef48;
      mp2_static.camera_uid_address = 0x804f2f50;
      mp2_static.control_flag_address = 0x8053ebf8;
      mp2_static.load_state_address = 0x804efc74;
    }
    else
    {
    }
    break;
  case Game::PRIME_2_GCN:
    if (region == Region::NTSC_U)
    {
      noclip_code_mp2_gc(0x803dcbdc, 0x80420000, 0x8004abc8);
      add_code_change(0x801865d8, 0x60000000);
      add_code_change(0x801865e0, 0x60000000);
      add_code_change(0x801865e8, 0x60000000);
      add_code_change(0x801865f0, 0xd0410098);
      add_code_change(0x801865f4, 0xd0210088);
      add_code_change(0x801865f8, 0xd0010078);
      add_code_change(0x801865fc, 0x4bec395d);

      mp2_gc_static.state_mgr_address = 0x803db6e0;
    }
    else if (region == Region::PAL)
    {
      noclip_code_mp2_gc(0x803dddfc, 0x80420000, 0x8004ad44);
      add_code_change(0x801868bc, 0x60000000);
      add_code_change(0x801868c4, 0x60000000);
      add_code_change(0x801868cc, 0x60000000);
      add_code_change(0x801868d4, 0xd0410098);
      add_code_change(0x801868d8, 0xd0210088);
      add_code_change(0x801868dc, 0xd0010078);
      add_code_change(0x801868e0, 0x4bec37e9);

      mp2_gc_static.state_mgr_address = 0x803dc900;
    }
    else
    {
    }
    break;
  case Game::PRIME_3:
    if (region == Region::NTSC_U)
    {
      noclip_code_mp3(0x805c6c40, 0x80004380, 0x8000bbfc);
      add_code_change(0x801784ac, 0x60000000);
      add_code_change(0x801784b4, 0x60000000);
      add_code_change(0x801784bc, 0x60000000);
      add_code_change(0x801784c4, 0xd0410084);
      add_code_change(0x801784c8, 0xd0210094);
      add_code_change(0x801784cc, 0xd00100a4);
      add_code_change(0x801784d0, 0x4be938a1);

      mp3_static.state_mgr_ptr_address = 0x805c6c40;
      mp3_static.cam_uid_ptr_address = 0x805c6c78;
    }
    else if (region == Region::PAL)
    {
      noclip_code_mp3(0x805ca0c0, 0x80004380, 0x8000bbfc);
      add_code_change(0x80177df8, 0x60000000);
      add_code_change(0x80177e00, 0x60000000);
      add_code_change(0x80177e08, 0x60000000);
      add_code_change(0x80177e10, 0xd0410084);
      add_code_change(0x80177e14, 0xd0210094);
      add_code_change(0x80177e18, 0xd00100a4);
      add_code_change(0x80177e1c, 0x4be93f55);

      mp3_static.state_mgr_ptr_address = 0x805ca0c0;
      mp3_static.cam_uid_ptr_address = 0x805ca0f8;
    }
    else
    {
    }
    break;
  case Game::PRIME_3_STANDALONE:
    if (region == Region::NTSC_U)
    {
      noclip_code_mp3(0x805c4f6c, 0x80004380, 0x8000bee8);
      add_code_change(0x8017c054, 0x60000000);
      add_code_change(0x8017c05c, 0x60000000);
      add_code_change(0x8017c064, 0x60000000);
      add_code_change(0x8017c06c, 0xd0410084);
      add_code_change(0x8017c070, 0xd0210094);
      add_code_change(0x8017c074, 0xd00100a4);
      add_code_change(0x8017c078, 0x4be938a1);

      mp3_static.state_mgr_ptr_address = 0x805c4f6c;
      mp3_static.cam_uid_ptr_address = 0x805c4fa4;
    }
    else if (region == Region::NTSC_J)
    {
      noclip_code_mp3(0x805caa30, 0x80004380, 0x8000bee8);
      add_code_change(0x8017dd54, 0x60000000);
      add_code_change(0x8017dd5c, 0x60000000);
      add_code_change(0x8017dd64, 0x60000000);
      add_code_change(0x8017dd6c, 0xd0410084);
      add_code_change(0x8017dd70, 0xd0210094);
      add_code_change(0x8017dd74, 0xd00100a4);
      add_code_change(0x8017dd78, 0x4be938a1);

      mp3_static.state_mgr_ptr_address = 0x805caa30;
      mp3_static.cam_uid_ptr_address = 0x805caa68;
    }
    else if (region == Region::PAL)
    {
      noclip_code_mp3(0x805c7570, 0x80004380, 0x8000bee8);
      add_code_change(0x8017cb48, 0x60000000);
      add_code_change(0x8017cb50, 0x60000000);
      add_code_change(0x8017cb58, 0x60000000);
      add_code_change(0x8017cb60, 0xd0410084);
      add_code_change(0x8017cb74, 0xd0210094);
      add_code_change(0x8017cb78, 0xd00100a4);
      add_code_change(0x8017cb7c, 0x4be938a1);

      mp3_static.state_mgr_ptr_address = 0x805c7570;
      mp3_static.cam_uid_ptr_address = 0x805c75a8;
    }
    else
    {
    }
    break;
  default:
    break;
  }
  return true;
}

void Noclip::noclip_code_mp1(u32 cplayer_address, u32 start_point, u32 return_location)
{
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

void Noclip::noclip_code_mp1_gc(u32 cplayer_address, u32 start_point, u32 return_location)
{
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

void Noclip::noclip_code_mp2(u32 cplayer_address, u32 start_point, u32 return_location)
{
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

void Noclip::noclip_code_mp2_gc(u32 cplayer_address, u32 start_point, u32 return_location)
{
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

void Noclip::noclip_code_mp3(u32 cplayer_address, u32 start_point, u32 return_location)
{
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

void Noclip::on_state_change(ModState old_state)
{
  if (mod_state() == ModState::ENABLED && old_state != ModState::ENABLED)
  {
    switch (GetHackManager()->get_active_game())
    {
    case Game::PRIME_1:
      player_tf.read_from(mp1_static.cplayer_address + 0x2c);
      old_matexclude_list = read64(mp1_gc_static.cplayer_address + 0x70);
      write64(0xffffffffffffffff, mp1_gc_static.cplayer_address + 0x70);
      break;
    case Game::PRIME_1_GCN:
      player_tf.read_from(mp1_gc_static.cplayer_address + 0x34);
      old_matexclude_list = read64(mp1_gc_static.cplayer_address + 0x78);
      write64(0xffffffffffffffff, mp1_gc_static.cplayer_address + 0x78);
      break;
    case Game::PRIME_2:
    {
      const u32 cplayer_address = read32(mp2_static.cplayer_ptr_address);
      if (mem_check(cplayer_address))
      {
        player_vec.read_from(cplayer_address + 0x50);
        old_matexclude_list = read64(cplayer_address + 0x70);
        write64(0xffffffffffffffff, cplayer_address + 0x70);
      }
    }
    break;
    case Game::PRIME_2_GCN:
    {
      const u32 cplayer_address = read32(mp2_gc_static.state_mgr_address + 0x14fc);
      if (mem_check(cplayer_address))
      {
        player_vec.read_from(cplayer_address + 0x54);
        old_matexclude_list = read64(cplayer_address + 0x74);
        write64(0xffffffffffffffff, cplayer_address + 0x74);
      }
    }
    break;
    case Game::PRIME_3:
    {
      const u32 cplayer_address = read32(read32(mp3_static.state_mgr_ptr_address + 0x28) + 0x2184);
      if (mem_check(cplayer_address))
      {
        player_vec.read_from(cplayer_address + 0x6c);
        old_matexclude_list = read64(cplayer_address + 0x88);
        write64(0xffffffffffffffff, cplayer_address + 0x88);
      }
    }
    break;
    case Game::PRIME_3_STANDALONE:
    {
      switch (GetHackManager()->get_active_region())
      {
      case Region::NTSC_U:
      {
        const u32 cplayer_address = read32(read32(mp3_static.state_mgr_ptr_address + 0x2c) + 0x2184);
        if (mem_check(cplayer_address))
        {
          player_vec.read_from(cplayer_address + 0x6c);
          old_matexclude_list = read64(cplayer_address + 0x88);
          write64(0xffffffffffffffff, cplayer_address + 0x88);
        }
      }
      break;
      case Region::NTSC_J:
      case Region::PAL:
      {
        const u32 cplayer_address = read32(read32(mp3_static.state_mgr_ptr_address + 0x28) + 0x2184);
        if (mem_check(cplayer_address))
        {
          player_vec.read_from(cplayer_address + 0x6c);
          old_matexclude_list = read64(cplayer_address + 0x88);
          write64(0xffffffffffffffff, cplayer_address + 0x88);
        }
      }
      break;
      }
    }
    break;
    }
  }
  else if ((mod_state() == ModState::DISABLED || mod_state() == ModState::CODE_DISABLED) &&
           old_state == ModState::ENABLED)
  {
    switch (GetHackManager()->get_active_game())
    {
    case Game::PRIME_1:
      write64(old_matexclude_list, mp1_gc_static.cplayer_address + 0x70);
      break;
    case Game::PRIME_1_GCN:
      write64(old_matexclude_list, mp1_gc_static.cplayer_address + 0x78);
      break;
    case Game::PRIME_2:
    {
      const u32 cplayer_address = read32(mp2_static.cplayer_ptr_address);
      if (mem_check(cplayer_address))
      {
        write64(old_matexclude_list, cplayer_address + 0x70);
      }
      break;
    }
    case Game::PRIME_2_GCN:
    {
      const u32 cplayer_address = read32(mp2_gc_static.state_mgr_address + 0x14fc);
      if (mem_check(cplayer_address))
      {
        write64(old_matexclude_list, cplayer_address + 0x74);
      }
      break;
    }
    case Game::PRIME_3:
    {
      const u32 cplayer_address = read32(read32(mp3_static.state_mgr_ptr_address + 0x28) + 0x2184);
      if (mem_check(cplayer_address))
      {
        write64(old_matexclude_list, cplayer_address + 0x88);
      }
      break;
    }
    case Game::PRIME_3_STANDALONE:
    {
      switch (GetHackManager()->get_active_region())
      {
      case Region::NTSC_U:
      {
        const u32 cplayer_address = read32(read32(mp3_static.state_mgr_ptr_address + 0x2c) + 0x2184);
        if (mem_check(cplayer_address))
        {
          write64(old_matexclude_list, cplayer_address + 0x88);
        }
      }
      break;
      case Region::NTSC_J:
      case Region::PAL:
      {
        const u32 cplayer_address = read32(read32(mp3_static.state_mgr_ptr_address + 0x28) + 0x2184);
        if (mem_check(cplayer_address))
        {
          write64(old_matexclude_list, cplayer_address + 0x88);
        }
      }
      break;
      }
    }
    break;
    }
  }
}
}  // namespace prime
