#pragma once

#include "Core/PrimeHack/Games/MP3.h"

namespace prime
{

  static std::array<std::tuple<int, int>, 4> prime_three_visors = {
    std::make_tuple<int, int>(0, 0x0b), std::make_tuple<int, int>(1, 0x0c),
    std::make_tuple<int, int>(2, 0x0d), std::make_tuple<int, int>(3, 0x0e)};

  // Overwrites the normal beam change check by looking at 0x804a79f0 and checking if it is flagged at
  // 1 if 1, reset it to 0 and read out the desired beam from 0x804a79f4. These two addresses are set
  // by the mod.

  // IIRC, this hook will be executed upon a control scheme (of some sort) being changed. The new ctl
  // scheme (pointer, offset, forgot?) is in r6 If the entity having this scheme being updated is the
  // player, it's happening because we're going into locked-camera-puzzle-mode (or in ship) so if we
  // cause the action of updating the scheme to not happen for the player, we can stick with
  // free-camera mode, and the game won't jack the controls from us (meaning we can use the same
  // addresses to write yaw velocity and pitch values).
  void MP3::control_state_hook(u32 base_offset, Region region)
  {
    if (region == Region::NTSC)
    {
      code_changes.emplace_back(base_offset + 0x00, 0x3c60805c);  // lis  r3, 0x805c      ;
      code_changes.emplace_back(base_offset + 0x04, 0x38636c40);  // addi r3, r3, 0x6c40  ; load 0x805c6c40 into r3
                                                                  // (indirect pointer to player camera control)
    }
    else if (region == Region::PAL)
    {
      code_changes.emplace_back(base_offset + 0x00, 0x3c60805d);  // lis  r3, 0x805d      ;
      code_changes.emplace_back(base_offset + 0x04, 0x3863a0c0);  // subi r3, r3, 0x5f40  ; load 0x805ca0c0 into r3, same reason as NTSC
    }
    code_changes.emplace_back(base_offset + 0x08, 0x8063002c);  // lwz  r3, 0x2c(r3)      ; deref player camera control base address into r3
    code_changes.emplace_back(base_offset + 0x0c, 0x80630004);  // lwz  r3, 0x04(r3)      ; the point at which the function which was hooked
    code_changes.emplace_back(base_offset + 0x10, 0x80632184);  // lwz  r3, 0x2184(r3)    ; should have r31 equal to the
                                                                // object which is being modified
    code_changes.emplace_back(base_offset + 0x14, 0x7c03f800);  // cmpw r3, r31           ; if r31 is player camera control (in r3)
    code_changes.emplace_back(base_offset + 0x18, 0x4d820020);  // beqlr                  ; then DON'T store the value of
                                                                // r6 into 0x78+(player camera control)
    code_changes.emplace_back(base_offset + 0x1c, 0x7fe3fb78);  // mr   r3, r31           ; otherwise do it
    code_changes.emplace_back(base_offset + 0x20, 0x90c30078);  // stw  r6, 0x78(r3)      ; this is the normal action
                                                                // which was overwritten by a bl to this mini-function
    code_changes.emplace_back(base_offset + 0x24, 0x4e800020);  // blr                    ; LR wasn't changed, so we're
                                                                // safe here (same case as beqlr)
  }

  // Thanks to grapple slides on Elysia, this needs to be done. The slide forces your look each
  // tick along it, and forces this with a call to lookAt. This call will be clobbered, and will
  // opt for setting player's transform origin to the lookAt origin param.
  void MP3::grapple_slide_no_lookat(u32 base_offset)
  {
    code_changes.emplace_back(base_offset + 0x00, 0x60000000);  // nop                  ; trashed because useless
    code_changes.emplace_back(base_offset + 0x04, 0x60000000);  // nop                  ; trashed because useless
    code_changes.emplace_back(base_offset + 0x08, 0x60000000);  // nop                  ; trashed because useless
    code_changes.emplace_back(base_offset + 0x0c, 0xc0010240);  // lfs  f0, 0x240(r1)   ; grab the x component of new origin
    code_changes.emplace_back(base_offset + 0x10, 0xd01f0048);  // stfs f0, 0x48(r31)   ; store it into player's xform x origin (CTransform + 0x0c)
    code_changes.emplace_back(base_offset + 0x14, 0xc0010244);  // lfs  f0, 0x244(r1)   ; grab the y component of new origin
    code_changes.emplace_back(base_offset + 0x18, 0xd01f0058);  // stfs f0, 0x58(r31)   ; store it into player's xform y origin (CTransform + 0x1c)
    code_changes.emplace_back(base_offset + 0x1c, 0xc0010248);  // lfs  f0, 0x248(r1)   ; grab the z component of new origin
    code_changes.emplace_back(base_offset + 0x20, 0xd01f0068);  // stfs f0, 0x68(r31)   ; store it into player's xform z origin (CTransform + 0xcc)
    code_changes.emplace_back(base_offset + 0x28, 0x389f003c);  // addi r4, r31, 0x3c   ; next sub call is SetTransform, so set player's transform to their own transform (safety no-op, does other updating too)
  }

  void MP3::run_mod()
  {
    u32 base_address = PowerPC::HostRead_U32(
      PowerPC::HostRead_U32(PowerPC::HostRead_U32(camera_ctl_address()) + 0x04) + 0x2184);
    if (!mem_check(base_address))
    {
      // Dupe code, oh well. Handle death screen - (shio says he's sorry)
      if (PowerPC::HostRead_U8(cursor_dlg_address()))
      {
        u32 cursor_base =
          PowerPC::HostRead_U32(PowerPC::HostRead_U32(cursor_address()) + cursor_offset());
        handle_cursor(cursor_base + 0x9c, cursor_base + 0x15c, 0.95f, 0.90f);
      }
      return;
    }

    if (PowerPC::HostRead_U8(cursor_enabled_address()) ||
      PowerPC::HostRead_U32(boss_id_address()) == 0x442f0000)
    {
      if (PowerPC::HostRead_U32(boss_id_address()) == 0x442f0000 && !fighting_ridley)
      {
        fighting_ridley = true;
        apply_normal_instructions();
      }
      u32 cursor_base =
        PowerPC::HostRead_U32(PowerPC::HostRead_U32(cursor_address()) + cursor_offset());
      handle_cursor(cursor_base + 0x9c, cursor_base + 0x15c, 0.95f, 0.90f);
      return;
    }
    else
    {
      if (fighting_ridley)
      {
        fighting_ridley = false;
        apply_mod_instructions();
      }
      u32 cursor_base =
        PowerPC::HostRead_U32(PowerPC::HostRead_U32(cursor_address()) + cursor_offset());
      set_cursor_pos(0, 0);
      PowerPC::HostWrite_U32(0, cursor_base + 0x9c);
      PowerPC::HostWrite_U32(0, cursor_base + 0x15c);
    }
    if (!PowerPC::HostRead_U8(base_address + 0x378) && PowerPC::HostRead_U8(lockon_address()))
    {
      PowerPC::HostWrite_U32(0, base_address + 0x174);
      return;
    }

    springball_check(base_address + 0x358, base_address + 0x29c);

    int dx = g_mouse_input->GetDeltaHorizontalAxis(), dy = g_mouse_input->GetDeltaVerticalAxis();
    const float compensated_sens = GetSensitivity() * TURNRATE_RATIO / 60.0f;

    pitch += static_cast<float>(dy) * -compensated_sens * (InvertedY() ? -1.f : 1.f);
    pitch = std::clamp(pitch, -1.5f, 1.5f);
    const float yaw_vel = dx * -GetSensitivity() * (InvertedX() ? -1.f : 1.f);

    PowerPC::HostWrite_U32(*reinterpret_cast<u32 const*>(&yaw_vel), base_address + 0x174);
    PowerPC::HostWrite_U32(0, base_address + 0x174 + 0x18);
    u32 rtoc_min_turn_rate = GPR(2) - cannon_lag_rtoc_offset();
    PowerPC::HostWrite_U32(0, rtoc_min_turn_rate);
    PowerPC::HostWrite_U32(*reinterpret_cast<u32*>(&pitch), base_address + 0x784);

    u32 visor_base = PowerPC::HostRead_U32(base_address + 0x35a8);
    int visor_id, visor_off;
    std::tie(visor_id, visor_off) = get_visor_switch(prime_three_visors, PowerPC::HostRead_U32(visor_base + 0x34) == 0);
    if (visor_id != -1)
    {
      if (PowerPC::HostRead_U32(visor_base + (visor_off * 0x0c) + 0x58) != 0)
      {
        PowerPC::HostWrite_U32(visor_id, visor_base + 0x34);
      }
    }

    if (UseMPAutoEFB())
    {
      bool useEFB = PowerPC::HostRead_U32(visor_base + 0x34) != 1;

      if (GetEFBTexture() != useEFB)
        SetEFBToTexture(useEFB);
    }

    u32 camera_fov = PowerPC::HostRead_U32(
      PowerPC::HostRead_U32(
        PowerPC::HostRead_U32(PowerPC::HostRead_U32(camera_pointer_address()) + 0x1010) + 0x1c) +
      0x178);
    u32 camera_fov_tp = PowerPC::HostRead_U32(
      PowerPC::HostRead_U32(
        PowerPC::HostRead_U32(PowerPC::HostRead_U32(camera_pointer_address()) + 0x1010) + 0x24) +
      0x178);
    const float fov = std::min(GetFov(), 94.f);
    PowerPC::HostWrite_U32(*reinterpret_cast<u32 const*>(&fov), camera_fov + 0x1c);
    PowerPC::HostWrite_U32(*reinterpret_cast<u32 const*>(&fov), camera_fov_tp + 0x1c);
    PowerPC::HostWrite_U32(*reinterpret_cast<u32 const*>(&fov), camera_fov + 0x18);
    PowerPC::HostWrite_U32(*reinterpret_cast<u32 const*>(&fov), camera_fov_tp + 0x18);

    set_cplayer_str(base_address);
  }

  uint32_t MP3NTSC::camera_ctl_address() const
  {
    return 0x805c6c6c;
  }
  uint32_t MP3NTSC::cursor_enabled_address() const
  {
    return 0x805c8d77;
  }
  uint32_t MP3NTSC::boss_id_address() const
  {
    return 0x805c6f44;
  }
  uint32_t MP3NTSC::cursor_dlg_address() const
  {
    return 0x805c8d77;
  }
  uint32_t MP3NTSC::cursor_address() const
  {
    return 0x8066fd08;
  }
  uint32_t MP3NTSC::cursor_offset() const
  {
    return 0xc54;
  }
  uint32_t MP3NTSC::cannon_lag_rtoc_offset() const
  {
    return 0x5ff0;
  }
  uint32_t MP3NTSC::lockon_address() const
  {
    return 0x805c6db7;
  }
  uint32_t MP3NTSC::camera_pointer_address() const
  {
    return 0x805c6c68;
  }

  MP3NTSC::MP3NTSC()
  {
    code_changes.emplace_back(0x80080ac0, 0xec010072);
    code_changes.emplace_back(0x8014e094, 0x60000000);
    code_changes.emplace_back(0x8014e06c, 0x60000000);
    code_changes.emplace_back(0x80134328, 0x60000000);
    code_changes.emplace_back(0x80133970, 0x60000000);
    code_changes.emplace_back(0x8000ab58, 0x4bffad29);
    code_changes.emplace_back(0x80080d44, 0x60000000);
    code_changes.emplace_back(0x8007fdc8, 0x480000e4);
    code_changes.emplace_back(0x8017f88c, 0x60000000);

    control_state_hook(0x80005880, Region::NTSC);
    grapple_slide_no_lookat(0x8017f2a0);
    springball_code(0x801077D4, &code_changes);
  }

  void MP3NTSC::apply_mod_instructions()
  {
    write_invalidate(0x80080ac0, 0xec010072);
    write_invalidate(0x8014e094, 0x60000000);
    write_invalidate(0x8014e06c, 0x60000000);
    write_invalidate(0x80134328, 0x60000000);
    write_invalidate(0x80133970, 0x60000000);
    write_invalidate(0x8000ab58, 0x4bffad29);
    write_invalidate(0x80080d44, 0x60000000);
    write_invalidate(0x8007fdc8, 0x480000e4);
    write_invalidate(0x8017f88c, 0x60000000);
  }

  void MP3NTSC::apply_normal_instructions()
  {
    write_invalidate(0x80080ac0, 0xec000072);
    write_invalidate(0x8014e094, 0xd01e0784);
    write_invalidate(0x8014e06c, 0xd01e0784);
    write_invalidate(0x80134328, 0x901f0174);
    write_invalidate(0x80133970, 0x901f0174);
    write_invalidate(0x8000ab58, 0x90c30078);
    write_invalidate(0x80080d44, 0x4bffe9a5);
    write_invalidate(0x8007fdc8, 0x418200e4);
    write_invalidate(0x8017f88c, 0x4be8c4e5);
  }

  uint32_t MP3PAL::camera_ctl_address() const
  {
    return 0x805ca0ec;
  }
  uint32_t MP3PAL::cursor_enabled_address() const
  {
    return 0x805cc1d7;
  }
  uint32_t MP3PAL::boss_id_address() const
  {
    return 0x805ca3c4;
  }
  uint32_t MP3PAL::cursor_dlg_address() const
  {
    return 0x805cc1d7;
  }
  uint32_t MP3PAL::cursor_address() const
  {
    return 0x80673588;
  }
  uint32_t MP3PAL::cursor_offset() const
  {
    return 0xd04;
  }
  uint32_t MP3PAL::cannon_lag_rtoc_offset() const
  {
    return 0x6000;
  }
  uint32_t MP3PAL::lockon_address() const
  {
    return 0x805ca237;
  }
  uint32_t MP3PAL::camera_pointer_address() const
  {
    return 0x805ca0e8;
  }

  MP3PAL::MP3PAL()
  {
    code_changes.emplace_back(0x80080ab8, 0xec010072);
    code_changes.emplace_back(0x8014d9e0, 0x60000000);
    code_changes.emplace_back(0x8014d9b8, 0x60000000);
    code_changes.emplace_back(0x80133c74, 0x60000000);
    code_changes.emplace_back(0x801332bc, 0x60000000);
    code_changes.emplace_back(0x8000ab58, 0x4bffad29);
    code_changes.emplace_back(0x80080d44, 0x60000000);
    code_changes.emplace_back(0x8007fdc8, 0x480000e4);
    code_changes.emplace_back(0x8017f1d8, 0x60000000);

    control_state_hook(0x80005880, Region::PAL);
    grapple_slide_no_lookat(0x8017ebec);
    springball_code(0x80107120, &code_changes);
  }

  void MP3PAL::apply_mod_instructions()
  {
    write_invalidate(0x80080ab8, 0xec010072);
    write_invalidate(0x8014d9e0, 0x60000000);
    write_invalidate(0x8014d9b8, 0x60000000);
    write_invalidate(0x80133c74, 0x60000000);
    write_invalidate(0x801332bc, 0x60000000);
    write_invalidate(0x8000ab58, 0x4bffad29);
    write_invalidate(0x80080d44, 0x60000000);
    write_invalidate(0x8007fdc8, 0x480000e4);
    write_invalidate(0x8017f1d8, 0x60000000);
  }

  void MP3PAL::apply_normal_instructions()
  {
    write_invalidate(0x80080ab8, 0xc0030000);
    write_invalidate(0x8014d9e0, 0xd01e0784);
    write_invalidate(0x8014d9b8, 0xd01e0784);
    write_invalidate(0x80133c74, 0x901f0174);
    write_invalidate(0x801332bc, 0x901f0174);
    write_invalidate(0x8000ab58, 0x90c30078);
    write_invalidate(0x80080d44, 0x4bffe9a5);
    write_invalidate(0x8007fdc8, 0x418200e4);
    write_invalidate(0x8017f1d8, 0x4be8cb99);
  }
}
