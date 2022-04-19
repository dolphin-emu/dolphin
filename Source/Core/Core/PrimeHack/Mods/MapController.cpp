#include "Core/PrimeHack/Mods/MapController.h"

#include "Core/PrimeHack/PrimeUtils.h"
#include "Core/PrimeHack/Quaternion.h"

namespace prime {

constexpr float kPi = 3.141592654f;
namespace {
void rotate_map_mp1_gc(u32 job) {
  MapController* const map_controller = static_cast<MapController*>(GetHackManager()->get_mod("map_controller"));
  if (job == 0) {  // Executed during Automapper transitions
    // Only reset our rotations if transitioning to normal map from minimap
    if (GPR(30) == 1 && read32(GPR(28) + 0x1bc) == 0) {
      map_controller->reset_rotation(map_controller->get_player_yaw(), readf32(GPR(28) + 0xc4) * -(kPi / 180.f));
    }
    GPR(0) = read32(GPR(28) + 0x1c0);
  } else if (job == 1) {  // Hooks ProcessMapRotateInput
    quat r = map_controller->compute_orientation();
    r.write_to(GPR(29) + 0xb0);
  } else if (job == 2) {  // Because PAL just had to be different? Sure whatever
    if (GPR(27) == 1 && read32(GPR(28) + 0x1d4) == 0) {
      map_controller->reset_rotation(map_controller->get_player_yaw(), readf32(GPR(28) + 0xdc) * -(kPi / 180.f));
    }
    GPR(0) = read32(GPR(28) + 0x1d8);
  } else if (job == 3) {
    quat r = map_controller->compute_orientation();
    r.write_to(GPR(29) + 0xc8);
  }
}
void rotate_map_mp1(u32 job) {
  MapController* const map_controller = static_cast<MapController*>(GetHackManager()->get_mod("map_controller"));
  if (job == 0) {
    if (GPR(31) == 1 && read32(GPR(29) + 0x1d0) == 0) {
      map_controller->reset_rotation(map_controller->get_player_yaw(), readf32(GPR(29) + 0xd8) * -(kPi / 180.f));
    }
    GPR(24) = read32(GPR(29) + 0x1d4);
  } else if (job == 1) {
    quat r = map_controller->compute_orientation();
    r.write_to(GPR(29) + 0xc4);
  }
}
void rotate_map_mp2_gc(u32 job) {
  MapController* const map_controller = static_cast<MapController*>(GetHackManager()->get_mod("map_controller"));
  if (job == 0) {
    if (GPR(27) == 1 && read32(GPR(28) + 0x200) == 0) {
      map_controller->reset_rotation(map_controller->get_player_yaw(), readf32(GPR(28) + 0x108) * -(kPi / 180.f));
    }
    GPR(0) = read32(GPR(28) + 0x204);
  } else if (job == 1) {  // Hooks ProcessMapRotateInput
    quat r = map_controller->compute_orientation();
    r.write_to(GPR(29) + 0xf4);
  }
}
void rotate_map_mp2(u32 job) {
  MapController* const map_controller = static_cast<MapController*>(GetHackManager()->get_mod("map_controller"));
  if (job == 0) {
    if (GPR(31) == 1 && read32(GPR(29) + 0x1f8) == 0) {
      map_controller->reset_rotation(map_controller->get_player_yaw(), readf32(GPR(29) + 0x100) * -(kPi / 180.f));
    }
    GPR(24) = read32(GPR(29) + 0x1fc);
  } else if (job == 1) {  // Hooks ProcessMapRotateInput
    quat r = map_controller->compute_orientation();
    r.write_to(GPR(29) + 0xec);
  }
}
}

float MapController::get_player_yaw() const {
  LOOKUP_DYN(object_list);
  if (object_list == 0) {
    return 0.f;
  }
  
  LOOKUP_DYN(camera_manager);
  if (camera_manager == 0) {
    return 0.f;
  }

  u16 camera_id = read16(camera_manager);
  if (camera_id == 0xffff) {
    return 0.f;
  }

  const u32 camera = read32(object_list + ((camera_id & 0x3ff) << 3) + 4);
  if (!mem_check(camera)) {
    return 0.f;
  }
  Transform xf;
  LOOKUP(transform_offset);
  xf.read_from(camera + transform_offset);
  vec3 planar_fwd = vec3(0, 0, 1).cross(xf.right());
  return kPi + atan2(planar_fwd.x, planar_fwd.y);
}

void MapController::reset_rotation(float horiztontal, float vertical) {
  x_rot = horiztontal;
  y_rot = vertical;
}

quat MapController::compute_orientation() {
  const float compensated_sens = GetSensitivity() * kTurnrateRatio / 60.f;
  y_rot -= static_cast<float>(frame_dy) * compensated_sens *
    (InvertedY() ? 1.f : -1.f);
  x_rot += static_cast<float>(frame_dx) * compensated_sens *
    (InvertedX() ? 1.f : -1.f);
  y_rot = std::clamp(y_rot, -kPi / 2.f + 0.01f, 0.f);
  quat r = quat();
  r.rotate_z(y_rot);
  r.rotate_x(x_rot);
  return r;
}

bool MapController::init_mod(Game game, Region region) {
  switch (game) {
  case Game::PRIME_1:
    init_mod_mp1(region);
    break;
  case Game::PRIME_1_GCN:
  case Game::PRIME_1_GCN_R1:
  case Game::PRIME_1_GCN_R2:
    init_mod_mp1_gc(game, region);
    break;
  case Game::PRIME_2:
    init_mod_mp2(region);
    break;
  case Game::PRIME_2_GCN:
    init_mod_mp2_gc(region);
    break;
  default:
    break;
  }
  return true;
}

void MapController::init_mod_mp1_gc(Game game, Region region) {
  const int map_controller_rotate = PowerPC::RegisterVmcall(rotate_map_mp1_gc);
  if (map_controller_rotate == -1) {
    return;
  }
  const u32 vmc_update_rotation = gen_vmcall(static_cast<u32>(map_controller_rotate), region == Region::NTSC_U ? 0 : 2);
  const u32 vmc_rotate_map = gen_vmcall(static_cast<u32>(map_controller_rotate), region == Region::NTSC_U ? 1 : 3);

  if (game == Game::PRIME_1_GCN) {
    if (region == Region::NTSC_U) {
      add_code_change(0x80097384, vmc_update_rotation);
      add_code_change(0x8009ae7c, vmc_rotate_map);
      add_code_change(0x80099fdc, 0x38600027);
      add_code_change(0x80099fec, 0x38600026);
      add_code_change(0x80099ffc, 0x38600028);
      add_code_change(0x8009a00c, 0x38600029);
    } else if (region == Region::PAL) {
      add_code_change(0x800970ec, vmc_update_rotation);
      add_code_change(0x8009a838, vmc_rotate_map);
      add_code_change(0x80099998, 0x38600027);
      add_code_change(0x800999a8, 0x38600026);
      add_code_change(0x800999b8, 0x38600028);
      add_code_change(0x800999c8, 0x38600029);
    }
  } else if (game == Game::PRIME_1_GCN_R1) {
    add_code_change(0x80097400, vmc_update_rotation);
    add_code_change(0x8009aef8, vmc_rotate_map);
    add_code_change(0x8009a058, 0x38600027);
    add_code_change(0x8009a068, 0x38600026);
    add_code_change(0x8009a078, 0x38600028);
    add_code_change(0x8009a088, 0x38600029);
  } else if (game == Game::PRIME_1_GCN_R2) {
    add_code_change(0x80097908, vmc_update_rotation);
    add_code_change(0x8009b400, vmc_rotate_map);
    add_code_change(0x8009a560, 0x38600027);
    add_code_change(0x8009a570, 0x38600026);
    add_code_change(0x8009a580, 0x38600028);
    add_code_change(0x8009a590, 0x38600029);
  }
}

void MapController::init_mod_mp1(Region region) {
  const int map_controller_rotate = PowerPC::RegisterVmcall(rotate_map_mp1);
  if (map_controller_rotate == -1) {
    return;
  }
  const u32 vmc_update_rotation = gen_vmcall(static_cast<u32>(map_controller_rotate), 0);
  const u32 vmc_rotate_map = gen_vmcall(static_cast<u32>(map_controller_rotate), 1);

  if (region == Region::NTSC_U || region == Region::PAL) {
    add_code_change(0x80029070, vmc_update_rotation);
    add_code_change(0x80025ab8, vmc_rotate_map);
    add_code_change(0x80026350, 0x3880002b);
    add_code_change(0x80026388, 0x3880002c);
    add_code_change(0x800263a4, 0x3880002d);
    add_code_change(0x800263c0, 0x3880002e);
  }
}

void MapController::init_mod_mp2_gc(Region region) {
  const int map_controller_rotate = PowerPC::RegisterVmcall(rotate_map_mp2_gc);
  if (map_controller_rotate == -1) {
    return;
  }
  const u32 vmc_update_rotation = gen_vmcall(static_cast<u32>(map_controller_rotate), 0);
  const u32 vmc_rotate_map = gen_vmcall(static_cast<u32>(map_controller_rotate), 1);
  if (region == Region::NTSC_U) {
    add_code_change(0x80088f48, vmc_update_rotation);
    add_code_change(0x8008e648, vmc_rotate_map);
    add_code_change(0x8008d63c, 0x3880002c);
    add_code_change(0x8008d654, 0x3880002b);
    add_code_change(0x8008d670, 0x3880002e);
    add_code_change(0x8008d68c, 0x3880002d);
  } else if (region == Region::PAL) {
    add_code_change(0x80089084, vmc_update_rotation);
    add_code_change(0x8008e784, vmc_rotate_map);
    add_code_change(0x8008d778, 0x3880002c);
    add_code_change(0x8008d790, 0x3880002b);
    add_code_change(0x8008d7ac, 0x3880002e);
    add_code_change(0x8008d7c8, 0x3880002d);
  }
}

void MapController::init_mod_mp2(Region region) {
  const int map_controller_rotate = PowerPC::RegisterVmcall(rotate_map_mp2);
  if (map_controller_rotate == -1) {
    return;
  }
  const u32 vmc_update_rotation = gen_vmcall(static_cast<u32>(map_controller_rotate), 0);
  const u32 vmc_rotate_map = gen_vmcall(static_cast<u32>(map_controller_rotate), 1);
  if (region == Region::NTSC_U) {
    add_code_change(0x8002eadc, vmc_update_rotation);
    add_code_change(0x800293d0, vmc_rotate_map);
    add_code_change(0x80029d48, 0x38a0002b);
    add_code_change(0x80029d68, 0x38a0002c);
    add_code_change(0x80029d88, 0x38a0002d);
    add_code_change(0x80029da8, 0x38a0002e);
  } else if (region == Region::PAL) {
    add_code_change(0x80030030, vmc_update_rotation);
    add_code_change(0x80029440, vmc_rotate_map);
    add_code_change(0x80029db8, 0x38a0002b);
    add_code_change(0x80029dd8, 0x38a0002c);
    add_code_change(0x80029df8, 0x38a0002d);
    add_code_change(0x80029e18, 0x38a0002e);
  }
}

void MapController::run_mod(Game game, Region region) {
  frame_dx = GetHorizontalAxis();
  frame_dy = GetVerticalAxis();
}

}
