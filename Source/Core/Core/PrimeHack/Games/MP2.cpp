#pragma once

#include "Core/PrimeHack/Games/MP2.h"

namespace prime
{
  static std::array<int, 4> prime_two_beams = {0, 1, 2, 3};

  static std::array<std::tuple<int, int>, 4> prime_two_visors = {
    std::make_tuple<int, int>(0, 0x08), std::make_tuple<int, int>(2, 0x09),
    std::make_tuple<int, int>(3, 0x0a), std::make_tuple<int, int>(1, 0x0b)};

  void MP2::beam_change_code(u32 base_offset)
  {
    code_changes.emplace_back(base_offset + 0x00, 0x3c80804d);  // lis    r4, 0x804d      ; set r4 to beam change base address
    code_changes.emplace_back(base_offset + 0x04, 0x3884d250);  // subi   r4, r4, 0x2db0  ; (0x804cd250)
    code_changes.emplace_back(base_offset + 0x08, 0x80640000);  // lwz    r3, 0(r4)       ; grab flag
    code_changes.emplace_back(base_offset + 0x0c, 0x2c030000);  // cmpwi  r3, 0           ; check if beam should change
    code_changes.emplace_back(base_offset + 0x10, 0x4182005c);  // beq    0x5c            ; don't attempt beam change if 0
    code_changes.emplace_back(base_offset + 0x14, 0x83e40004);  // lwz    r31, 4(r4)      ; get expected beam (r31, r30 used to assign beam)
    code_changes.emplace_back(base_offset + 0x18, 0x7ffefb78);  // mr     r30, r31        ; copy expected beam to other reg
    code_changes.emplace_back(base_offset + 0x1c, 0x38600000);  // li     r3, 0           ; reset flag
    code_changes.emplace_back(base_offset + 0x20, 0x90640000);  // stw    r3, 0(r4)       ;
    code_changes.emplace_back(base_offset + 0x24, 0x48000048);  // b      0x48            ; jump forward to beam assign
  }

  void MP2::run_mod()
  {
    // Load state should be 1, otherwise addresses may be invalid
    if (PowerPC::HostRead_U32(load_state_address()) != 1)
    {
      return;
    }
    u32 base_address = PowerPC::HostRead_U32(camera_ctl_address());
    if (!mem_check(base_address))
    {
      return;
    }

    if (PowerPC::HostRead_U32(base_address + 0x390) != 5 && PowerPC::HostRead_U8(lockon_address()))
    {
      PowerPC::HostWrite_U32(0, base_address + 0x178);
      return;
    }

    int dx = g_mouse_input->GetDeltaHorizontalAxis(), dy = g_mouse_input->GetDeltaVerticalAxis();
    const float compensated_sens = GetSensitivity() * TURNRATE_RATIO / 60.0f;

    pitch += static_cast<float>(dy) * -compensated_sens * (InvertedY() ? -1.f : 1.f);
    pitch = std::clamp(pitch, -1.04f, 1.04f);
    const float yaw_vel = dx * -GetSensitivity() * (InvertedX() ? -1.f : 1.f);

    u32 arm_cannon_model_matrix = PowerPC::HostRead_U32(base_address + 0xea8) + 0x3b0;
    PowerPC::HostWrite_U32(*reinterpret_cast<u32*>(&pitch), base_address + 0x5f0);
    PowerPC::HostWrite_U32(*reinterpret_cast<u32*>(&pitch), arm_cannon_model_matrix + 0x24);
    PowerPC::HostWrite_U32(*reinterpret_cast<u32 const*>(&yaw_vel), base_address + 0x178);

    for (int i = 0; i < 4; i++) {
      u32 beam_base = PowerPC::HostRead_U32(base_address + 0x12ec);
      set_beam_owned(i, PowerPC::HostRead_U32(beam_base + (prime_two_beams[i] * 0x0c) + 0x5c) ? true : false);
    }

    int beam_id = prime::get_beam_switch(prime_two_beams);
    if (beam_id != -1)
    {
      PowerPC::HostWrite_U32(beam_id, new_beam_address());
      PowerPC::HostWrite_U32(1, beamchange_flag_address());
    }

    u32 visor_base = PowerPC::HostRead_U32(base_address + 0x12ec);
    int visor_id, visor_off;
    std::tie(visor_id, visor_off) = prime::get_visor_switch(prime_two_visors, PowerPC::HostRead_U32(visor_base + 0x34) == 0);
    if (visor_id != -1)
    {    
      if (PowerPC::HostRead_U32(visor_base + (visor_off * 0x0c) + 0x5c) != 0)
      {
        PowerPC::HostWrite_U32(visor_id, visor_base + 0x34);
      }
    }

    if (UseMPAutoEFB())
    {
      bool useEFB = PowerPC::HostRead_U32(visor_base + 0x34) != 2;

      if (GetEFBTexture() != useEFB)
        SetEFBToTexture(useEFB);
    }

    springball_check(base_address + 0x374, base_address + 0x2C4);

    u32 camera_ptr = PowerPC::HostRead_U32(camera_ptr_address());
    u32 camera_offset =
      ((PowerPC::HostRead_U32(PowerPC::HostRead_U32(camera_offset_address())) >> 16) & 0x3ff) << 3;
    u32 camera_offset_tp =
      ((PowerPC::HostRead_U32(PowerPC::HostRead_U32(camera_offset_address()) + 0x0a) >> 16) & 0x3ff)
      << 3;
    u32 camera_base = PowerPC::HostRead_U32(camera_ptr + camera_offset + 4);
    u32 camera_base_tp = PowerPC::HostRead_U32(camera_ptr + camera_offset_tp + 4);
    const float fov = std::min(GetFov(), 101.f);
    PowerPC::HostWrite_U32(*reinterpret_cast<u32 const*>(&fov), camera_base + 0x1e8);
    PowerPC::HostWrite_U32(*reinterpret_cast<u32 const*>(&fov), camera_base_tp + 0x1e8);

    set_cplayer_str(base_address);
  }

  MP2NTSC::MP2NTSC()
  {
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

    beam_change_code(0x8018cc88);
    springball_code(0x8010BD98, &code_changes);
  }

  uint32_t MP2NTSC::load_state_address() const
  {
    return 0x804e8824;
  }
  uint32_t MP2NTSC::camera_ctl_address() const
  {
    return 0x804e87dc;
  }
  uint32_t MP2NTSC::lockon_address() const
  {
    return 0x804e894f;
  }
  uint32_t MP2NTSC::new_beam_address() const
  {
    return 0x804cd254;
  }
  uint32_t MP2NTSC::beamchange_flag_address() const
  {
    return 0x804cd250;
  }
  uint32_t MP2NTSC::camera_ptr_address() const
  {
    return 0x804e7af8;
  }
  uint32_t MP2NTSC::camera_offset_address() const
  {
    return 0x804eb9ac;
  }

  MP2PAL::MP2PAL()
  {
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

    beam_change_code(0x8018e41c);
    springball_code(0x8010D440, &code_changes);
  }

  uint32_t MP2PAL::load_state_address() const
  {
    return 0x804efc74;
  }
  uint32_t MP2PAL::camera_ctl_address() const
  {
    return 0x804efc2c;
  }
  uint32_t MP2PAL::lockon_address() const
  {
    return 0x804efd9f;
  }
  uint32_t MP2PAL::new_beam_address() const
  {
    return 0x804cd254;
  }
  uint32_t MP2PAL::beamchange_flag_address() const
  {
    return 0x804cd250;
  }
  uint32_t MP2PAL::camera_ptr_address() const
  {
    return 0x804eef48;
  }
  uint32_t MP2PAL::camera_offset_address() const
  {
    return 0x804f2f4c;
  }

}
