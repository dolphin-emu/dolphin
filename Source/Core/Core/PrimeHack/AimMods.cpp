#include "Core/PrimeHack/AimMods.h"

#include <cmath>

#include "Core/PowerPC/PowerPC.h"
#include "Core/PrimeHack/HackConfig.h"
#include "InputCommon/GenericMouse.h"
#include "VideoCommon/RenderBase.h"
#include "VideoCommon/VideoCommon.h"

namespace prime
{
  constexpr float TURNRATE_RATIO = 0.00498665500569808449206349206349f;

  /*
  * Prime one beam IDs: 0 = power, 1 = ice, 2 = wave, 3 = plasma
  * Prime one visor IDs: 0 = combat, 1 = xray, 2 = scan, 3 = thermal
  * Prime two beam IDs: 0 = power, 1 = dark, 2 = light, 3 = annihilator
  * Prime two visor IDs: 0 = combat, 1 = echo, 2 = scan, 3 = dark
  * ADDITIONAL INFO: Equipment have-status offsets:
  * Beams can be ignored (for now) as the existing code handles that for us
  * Prime one visor offsets: combat = 0x11, scan = 0x05, thermal = 0x09, xray = 0x0d
  * Prime two visor offsets: combat = 0x08, scan = 0x09, dark = 0x0a, echo = 0x0b
  */
  static std::array<int, 4> prime_one_beams = {0, 2, 1, 3};
  static std::array<int, 4> prime_two_beams = {0, 1, 2, 3};
  static std::array<bool, 4> beam_owned = {false, false, false, false};
  int current_beam = 0;

  // it can not be explained why combat->xray->scan->thermal is the ordering...
  static std::array<std::tuple<int, int>, 4> prime_one_visors = {
    std::make_tuple<int, int>(0, 0x11), std::make_tuple<int, int>(2, 0x05),
    std::make_tuple<int, int>(3, 0x09), std::make_tuple<int, int>(1, 0x0d)};
  static std::array<std::tuple<int, int>, 4> prime_two_visors = {
    std::make_tuple<int, int>(0, 0x08), std::make_tuple<int, int>(2, 0x09),
    std::make_tuple<int, int>(3, 0x0a), std::make_tuple<int, int>(1, 0x0b)};
  static std::array<std::tuple<int, int>, 4> prime_three_visors = {
    std::make_tuple<int, int>(0, 0x0b), std::make_tuple<int, int>(1, 0x0c),
    std::make_tuple<int, int>(2, 0x0d), std::make_tuple<int, int>(3, 0x0e)};

  static float cursor_x = 0, cursor_y = 0;

  static void write_invalidate(u32 address, u32 value)
  {
    PowerPC::HostWrite_U32(value, address);
    PowerPC::ScheduleInvalidateCacheThreadSafe(address);
  }

  static std::tuple<int, int> get_visor_switch(std::array<std::tuple<int, int>, 4> const& visors, bool combat_visor)
  {
    static bool pressing_button = false;
    if (CheckVisorCtl(0))
    {
      if (!combat_visor)
      {
        if (!pressing_button)
        {
          return visors[0];
        }
      }
    }
    else if (CheckVisorCtl(1))
    {
      if (!pressing_button)
      {
        pressing_button = true;
        return visors[1];
      }
    }
    else if (CheckVisorCtl(2))
    {
      if (!pressing_button)
      {
        pressing_button = true;
        return visors[2];
      }
    }
    else if (CheckVisorCtl(3))
    {
      if (!pressing_button)
      {
        pressing_button = true;
        return visors[3];
      }
    }
    else
    {
      pressing_button = false;
    }
    return std::make_tuple(-1, 0);
  }

  static int get_beam_switch(std::array<int, 4> const& beams)
  {
    static bool pressing_button = false;
    if (CheckBeamCtl(0))
    {
      if (!pressing_button)
      {
        pressing_button = true;
        return current_beam = beams[0];
      }
    }
    else if (CheckBeamCtl(1))
    {
      if (!pressing_button)
      {
        pressing_button = true;
        return current_beam = beams[1];
      }
    }
    else if (CheckBeamCtl(2))
    {
      if (!pressing_button)
      {
        pressing_button = true;
        return current_beam = beams[2];
      }
    }
    else if (CheckBeamCtl(3))
    {
      if (!pressing_button)
      {
        pressing_button = true;
        return current_beam = beams[3];
      }
    }
    else if (CheckBeamScrollCtl(true))
    {
      if (!pressing_button)
      {
        pressing_button = true;
        for (int i = 0; i < 4; i++) {
          if (beam_owned[current_beam = (current_beam + 1) % 4]) break;
        }
        return beams[current_beam];
      }
    }
    else if (CheckBeamScrollCtl(false))
    {
      if (!pressing_button)
      {
        pressing_button = true;
        for (int i = 0; i < 4; i++) {
          if (beam_owned[current_beam = (current_beam + 3) % 4]) break;
        }
        return beams[current_beam];
      }
    }
    else
    {
      pressing_button = false;
    }
    return -1;
  }

  static inline bool mem_check(u32 address)
  {
    return (address >= 0x80000000) && (address < 0x81800000);
  }

  static float getAspectRatio()
  {
    const unsigned int scale = g_renderer->GetEFBScale();
    float sW = scale * EFB_WIDTH;
    float sH = scale * EFB_HEIGHT;
    return sW / sH;
  }

  static void handle_cursor(u32 x_address, u32 y_address, float right_bound, float bottom_bound)
  {
    int dx = prime::g_mouse_input->GetDeltaHorizontalAxis(),
      dy = prime::g_mouse_input->GetDeltaVerticalAxis();
    float aspect_ratio = getAspectRatio();
    if (std::isnan(aspect_ratio))
      return;
    const float cursor_sensitivity_conv = prime::GetCursorSensitivity() / 10000.f;
    cursor_x += static_cast<float>(dx) * cursor_sensitivity_conv;
    cursor_y += static_cast<float>(dy) * cursor_sensitivity_conv * aspect_ratio;

    cursor_x = std::clamp(cursor_x, -1.f, right_bound);
    cursor_y = std::clamp(cursor_y, -1.f, bottom_bound);

    PowerPC::HostWrite_U32(*reinterpret_cast<u32*>(&cursor_x), x_address);
    PowerPC::HostWrite_U32(*reinterpret_cast<u32*>(&cursor_y), y_address);
  }

  // Overwrites the normal beam change check by looking at 0x804a79f0 and checking if it is flagged at
  // 1 if 1, reset it to 0 and read out the desired beam from 0x804a79f4. These two addresses are set
  // by the mod.
  void MP1::beam_change_code(uint32_t base_offset)
  {
    code_changes.emplace_back(base_offset + 0x00, 0x3c80804a);  // lis    r4, 0x804a      ; set r4 to beam change base address
    code_changes.emplace_back(base_offset + 0x04, 0x388479f0);  // addi   r4, r4, 0x79f0  ; (0x804a79f0)
    code_changes.emplace_back(base_offset + 0x08, 0x80640000);  // lwz    r3, 0(r4)       ; grab flag
    code_changes.emplace_back(base_offset + 0x0c, 0x2c030000);  // cmpwi  r3, 0           ; check if beam should change
    code_changes.emplace_back(base_offset + 0x10, 0x41820058);  // beq    0x58            ; don't attempt beam change if 0
    code_changes.emplace_back(base_offset + 0x14, 0x83440004);  // lwz    r26, 4(r4)      ; get expected beam (r25, r26 used to assign beam)
    code_changes.emplace_back(base_offset + 0x18, 0x7f59d378);  // mr     r25, r26        ; copy expected beam to other reg
    code_changes.emplace_back(base_offset + 0x1c, 0x38600000);  // li     r3, 0           ; reset flag
    code_changes.emplace_back(base_offset + 0x20, 0x90640000);  // stw    r3, 0(r4)       ;
    code_changes.emplace_back(base_offset + 0x24, 0x48000044);  // b      0x44            ; jump forward to beam assign
  }

  void MP1::run_mod()
  {
    if (PowerPC::HostRead_U32(orbit_state_address()) != 5 && PowerPC::HostRead_U8(lockon_address()))
    {
      PowerPC::HostWrite_U32(0, yaw_vel_address());
      return;
    }

    int dx = g_mouse_input->GetDeltaHorizontalAxis(), dy = g_mouse_input->GetDeltaVerticalAxis();
    const float compensated_sens = GetSensitivity() * TURNRATE_RATIO / 60.0f;

    pitch += static_cast<float>(dy) * -compensated_sens * (InvertedY() ? -1.f : 1.f);
    pitch = std::clamp(pitch, -1.22f, 1.22f);
    const float yaw_vel = dx * -GetSensitivity() * (InvertedX() ? -1.f : 1.f);

    PowerPC::HostWrite_U32(*reinterpret_cast<u32*>(&pitch), pitch_address());
    PowerPC::HostWrite_U32(*reinterpret_cast<u32*>(&pitch), pitch_goal_address());

    PowerPC::HostWrite_U32(0, avel_limiter_address());
    PowerPC::HostWrite_U32(*reinterpret_cast<u32 const*>(&yaw_vel), yaw_vel_address());

    for (int i = 0; i < 4; i++) {
      u32 beam_base = PowerPC::HostRead_U32(powerups_base_address());
      beam_owned[i] = PowerPC::HostRead_U32(beam_base + (prime_one_beams[i] * 0x08) + 0x30) ? true : false;
    }

    int beam_id = get_beam_switch(prime_one_beams);
    if (beam_id != -1)
    {
      PowerPC::HostWrite_U32(beam_id, new_beam_address());
      PowerPC::HostWrite_U32(1, beamchange_flag_address());
    }

    springball_check(cplayer() + 0x2f4, cplayer() + 0x25C);

    u32 visor_base = PowerPC::HostRead_U32(powerups_base_address());
    int visor_id, visor_off;
    std::tie(visor_id, visor_off) = get_visor_switch(prime_one_visors, PowerPC::HostRead_U32(visor_base + 0x1c));
    if (visor_id != -1)
    {
      if (PowerPC::HostRead_U32(visor_base + (visor_off * 0x08) + 0x30))
      {
        PowerPC::HostWrite_U32(visor_id, visor_base + 0x1c);
      }
    }
    {
      u32 camera_ptr = PowerPC::HostRead_U32(camera_pointer_address());
      u32 camera_offset = ((PowerPC::HostRead_U32(active_camera_offset_address()) >> 16) & 0x3ff)
        << 3;
      u32 camera_base = PowerPC::HostRead_U32(camera_ptr + camera_offset + 0x04);
      const float fov = std::min(GetFov(), 101.f);
      PowerPC::HostWrite_U32(*reinterpret_cast<u32 const*>(&fov), camera_base + 0x164);
      PowerPC::HostWrite_U32(*reinterpret_cast<u32 const*>(&fov), global_fov1());
      PowerPC::HostWrite_U32(*reinterpret_cast<u32 const*>(&fov), global_fov2());
    }
  }

  MP1NTSC::MP1NTSC()
  {
    // This instruction change is used in all 3 games, all 3 regions. It's an update to what I believe
    // to be interpolation for player camera pitch The change is from fmuls f0, f0, f1 (0xec000072) to
    // fmuls f0, f1, f1 (0xec010072), where f0 is the output. The higher f0 is, the faster the pitch
    // *can* change in-game, and squaring f1 seems to do the job.
    code_changes.emplace_back(0x80098ee4, 0xec010072);
    code_changes.emplace_back(0x80099138, 0x60000000);
    code_changes.emplace_back(0x80183a8c, 0x60000000);
    code_changes.emplace_back(0x80183a64, 0x60000000);
    code_changes.emplace_back(0x8017661c, 0x60000000);
    code_changes.emplace_back(0x802fb5b4, 0xd23f009c);
    code_changes.emplace_back(0x8019fbcc, 0x60000000);

    beam_change_code(0x8018e544);
    prime::springball_code(0x801476D0, &code_changes);
  }

  uint32_t MP1NTSC::orbit_state_address() const
  {
    return 0x804d3f20;
  }
  uint32_t MP1NTSC::lockon_address() const
  {
    return 0x804c00b3;
  }
  uint32_t MP1NTSC::yaw_vel_address() const
  {
    return 0x804d3d38;
  }
  uint32_t MP1NTSC::pitch_address() const
  {
    return 0x804d3ffc;
  }
  uint32_t MP1NTSC::pitch_goal_address() const
  {
    return 0x804c10ec;
  }
  uint32_t MP1NTSC::avel_limiter_address() const
  {
    return 0x804d3d74;
  }
  uint32_t MP1NTSC::new_beam_address() const
  {
    return 0x804a79f4;
  }
  uint32_t MP1NTSC::beamchange_flag_address() const
  {
    return 0x804a79f0;
  }
  uint32_t MP1NTSC::powerups_base_address() const
  {
    return 0x804bfcd4;
  }
  uint32_t MP1NTSC::camera_pointer_address() const
  {
    return 0x804bfc30;
  }
  uint32_t MP1NTSC::cplayer() const
  {
    return 0x804d3c20;
  }
  uint32_t MP1NTSC::active_camera_offset_address() const
  {
    return 0x804c4a08;
  }
  uint32_t MP1NTSC::global_fov1() const
  {
    return 0x805c0e38;
  }
  uint32_t MP1NTSC::global_fov2() const
  {
    return 0x805c0e3c;
  }

  MP1PAL::MP1PAL()
  {
    code_changes.emplace_back(0x80099068, 0xec010072);
    code_changes.emplace_back(0x800992c4, 0x60000000);
    code_changes.emplace_back(0x80183cfc, 0x60000000);
    code_changes.emplace_back(0x80183d24, 0x60000000);
    code_changes.emplace_back(0x801768b4, 0x60000000);
    code_changes.emplace_back(0x802fb84c, 0xd23f009c);
    code_changes.emplace_back(0x8019fe64, 0x60000000);

    beam_change_code(0x8018e7dc);
    prime::springball_code(0x80147820, &code_changes);
  }

  uint32_t MP1PAL::orbit_state_address() const
  {
    return 0x804d7e60;
  }
  uint32_t MP1PAL::lockon_address() const
  {
    return 0x804c3ff3;
  }
  uint32_t MP1PAL::yaw_vel_address() const
  {
    return 0x804d7c78;
  }
  uint32_t MP1PAL::pitch_address() const
  {
    return 0x804d7f3c;
  }
  uint32_t MP1PAL::pitch_goal_address() const
  {
    return 0x804c502c;
  }
  uint32_t MP1PAL::avel_limiter_address() const
  {
    return 0x804d7cb4;
  }
  uint32_t MP1PAL::new_beam_address() const
  {
    return 0x804a79f4;
  }
  uint32_t MP1PAL::beamchange_flag_address() const
  {
    return 0x804a79f0;
  }
  uint32_t MP1PAL::powerups_base_address() const
  {
    return 0x804c3c14;
  }
  uint32_t MP1PAL::camera_pointer_address() const
  {
    return 0x804c3b70;
  }
  uint32_t MP1PAL::cplayer() const
  {
    return 0x804d7b60;
  }
  uint32_t MP1PAL::active_camera_offset_address() const
  {
    return 0x804c8948;
  }
  uint32_t MP1PAL::global_fov1() const
  {
    return 0x805c5178;
  }
  uint32_t MP1PAL::global_fov2() const
  {
    return 0x805c517c;
  }

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
      beam_owned[i] = PowerPC::HostRead_U32(beam_base + (prime_two_beams[i] * 0x0c) + 0x5c) ? true : false;
    }

    int beam_id = get_beam_switch(prime_two_beams);
    if (beam_id != -1)
    {
      PowerPC::HostWrite_U32(beam_id, new_beam_address());
      PowerPC::HostWrite_U32(1, beamchange_flag_address());
    }

    u32 visor_base = PowerPC::HostRead_U32(base_address + 0x12ec);
    int visor_id, visor_off;
    std::tie(visor_id, visor_off) = get_visor_switch(prime_two_visors, PowerPC::HostRead_U32(visor_base + 0x34) == 0);
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
    prime::springball_code(0x8010BD98, &code_changes);
  }

  uint32_t MP2NTSC::load_state_address() const
  {
    return 0x804e8824;
  }
  uint32_t MP2NTSC::camera_ctl_address() const
  {
    return 0x804e87dc;  // 0x1a8
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
    prime::springball_code(0x8010D440, &code_changes);
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
      cursor_x = cursor_y = 0;
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
  }

  uint32_t MP3NTSC::camera_ctl_address() const
  {
    return 0x805c6c6c;  // camera_ctl_address()) + 0x04) + 0x2184
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
    prime::springball_code(0x801077D4, &code_changes);
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
    prime::springball_code(0x80107120, &code_changes);
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

  void MenuNTSC::run_mod()
  {
    handle_cursor(0x80913c9c, 0x80913d5c, 0.95f, 0.90f);
  }

  void MenuPAL::run_mod()
  {
    u32 cursor_base = PowerPC::HostRead_U32(0x80621ffc);
    handle_cursor(cursor_base + 0xdc, cursor_base + 0x19c, 0.95f, 0.90f);
  }

  void springball_code(u32 base_offset, std::vector<CodeChange>* code_changes)
  {
    code_changes->emplace_back(base_offset + 0x00, 0x3C808000);  // lis r4, 0x8000
    code_changes->emplace_back(base_offset + 0x04, 0x60844164);  // ori r4, r4, 0x4164
    code_changes->emplace_back(base_offset + 0x08, 0x88640000);  // lbz r3, 0(r4)
    code_changes->emplace_back(base_offset + 0x0c, 0x38A00000);  // li r5, 0
    code_changes->emplace_back(base_offset + 0x10, 0x98A40000);  // stb r5, 0(r4)
    code_changes->emplace_back(base_offset + 0x14, 0x2C030000);  // cmpwi r3, 0
  }

  void springball_check(u32 ball_address, u32 movement_address)
  {
    if (CheckSpringBallCtl())
    {             
      u32 ball_state = PowerPC::HostRead_U32(ball_address);
      u32 movement_state = PowerPC::HostRead_U32(movement_address);

      if ((ball_state == 1 || ball_state == 2) && movement_state == 0)
        PowerPC::HostWrite_U8(1, 0x80004164);
    }
  }
}  // namespace prime
