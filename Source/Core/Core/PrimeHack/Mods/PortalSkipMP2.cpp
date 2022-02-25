#include "Core/PrimeHack/Mods/PortalSkipMP2.h"

#include "Core/PrimeHack/PrimeUtils.h"
#include "Core/PowerPC/Interpreter/Interpreter.h"

namespace prime {
namespace {
constexpr u32 kAgonWorldId = 0x42b935e4;
constexpr u32 kTorvusWorldId = 0x3dfd2249;
constexpr u32 kSanctuaryWorldId = 0x1baa96c2;

void fix_layer_bits(u32 param) {
  PortalSkipMP2* mod = static_cast<PortalSkipMP2*>(GetHackManager()->get_mod("portal_skip_mp2"));
  if (mod != nullptr) {
    mod->fix_portal_terminal_layer_bits();
  }

  // Original instruction: mr r3, r30
  GPR(3) = GPR(30);
}
}

// Stupid cutscene that we cut out sets some layer bits
void PortalSkipMP2::fix_portal_terminal_layer_bits() {
  LOOKUP_DYN(world_id);
  LOOKUP_DYN(area_id);
  LOOKUP_DYN(area_layers_vector);

  if (read32(world_id) == kAgonWorldId &&
      read32(area_id) == 0x12) {
    const u32 layer_bits_addr = 0x10 * 0x12 + area_layers_vector + 0x8;
    u64 layer_bits = read64(layer_bits_addr);

    // 2nd, 3rd, and 4th pass layers are OFF
    if ((layer_bits & 0x6008) == 0) {
      // mask ON 2nd pass, portal 2nd pass
      // mask OFF portal cinematics
      layer_bits |= 0x8008;
      layer_bits &= ~0x40;
      write64(layer_bits, layer_bits_addr);
    }
  }
}

void PortalSkipMP2::run_mod(Game game, Region region) {
  if (game != Game::PRIME_2 && game != Game::PRIME_2_GCN) {
    return;
  }
   
  LOOKUP_DYN(world_id);

  LOOKUP_DYN(object_list);
  if (object_list == 0) {
    return;
  }

  const u32 mlvl_id = read32(world_id);
  auto wpc_iter = portal_control_map.find(mlvl_id);
  if (wpc_iter == portal_control_map.end()) {
    return;
  }
  const auto& world_portal_controls = wpc_iter->second;

  LOOKUP_DYN(player);
  if (player == 0) {
    return;
  }
  const u32 area_id = read32(player + 4);

  const auto find_object = [object_list](u32 editor_id) -> u32 {
    for (int i = 0; i < 1024; i++) {
      const u32 object_ptr = read32(object_list + (i * 8) + 4);
      if (object_ptr != 0 && read32(object_ptr + 0xc) == editor_id) {
        return object_ptr;
      }
    }
    return 0;
  };

  LOOKUP(conn_vec_offset);
  LOOKUP(seq_timer_vec_offset);
  LOOKUP(seq_timer_fire_size);
  LOOKUP(seq_timer_time_offset);
  for (const PortalControl& pc : world_portal_controls) {
    if (((pc.scan_eid >> 16) & 0x3ff) == area_id) {
      const u32 scan_obj = find_object(pc.scan_eid);
      if (scan_obj == 0) { continue; }
      const u32 seq_timer_obj = find_object(pc.seq_timer_eid);
      if (seq_timer_obj == 0) { continue; }
      const u32 rip_fx_gen_obj = find_object(pc.rip_fx_gen_eid);
      if (rip_fx_gen_obj == 0) { continue; }
      // THREE STEP PROCESS:
      // 1. Remap scan connection to trigger the portal create
      // 2. Disable ripple FX connection
      // 3. Sequence timer needs timers readjusted

      const u32 scan_conn_vec_size = read32(scan_obj + conn_vec_offset);
      const u32 scan_conn_vec = read32(scan_obj + conn_vec_offset + 0x8);
      if (scan_conn_vec_size != 0 && scan_conn_vec != 0) {
        // remap all STRT
        for (u32 i = 0; i < scan_conn_vec_size; i++) {
          const u32 conn_msg_fcc = read32(scan_conn_vec + (i * 0xc) + 4);
          if (conn_msg_fcc == 0x53545254) {
            write32(pc.seq_timer_eid, scan_conn_vec + (i * 0xc) + 8);
            break;
          }
        }
      }
      
      const u32 timer_conn_vec_size = read32(seq_timer_obj + conn_vec_offset);
      const u32 timer_conn_vec = read32(seq_timer_obj + conn_vec_offset + 0x8);
      if (timer_conn_vec_size != 0 && timer_conn_vec != 0) {
        // Remove Ripple FX
        for (u32 i = 0; i < timer_conn_vec_size; i++) {
          const u32 conn_eid = read32(timer_conn_vec + (i * 0xc) + 8);
          if (conn_eid == pc.rip_fx_gen_eid) {
            write32(0, timer_conn_vec + (i * 0xc) + 4);
            break;
          }
        }
      }

      const u32 seq_timer_event_vec_size = read32(seq_timer_obj + seq_timer_vec_offset);
      const u32 seq_timer_event_vec = read32(seq_timer_obj + seq_timer_vec_offset + 0x8);
      if (seq_timer_event_vec_size != 0 && seq_timer_event_vec != 0) {
        for (u32 i = 0; i < seq_timer_event_vec_size; i++) {
          // Check if timer should be adjusted
          // These tables are all of size 1
          const u32 time_table_ptr = read32(seq_timer_event_vec + (i * seq_timer_fire_size) + seq_timer_time_offset);
          if (time_table_ptr != 0) {
            const float time = readf32(time_table_ptr);
            if (time < 8.f) {
              writef32(0.f, time_table_ptr);
            }
          }
        }
      }
    }
  }
}

bool PortalSkipMP2::init_mod(Game game, Region region) {
  if (game == Game::PRIME_2) {
    const int fix_portal_terminal_fn = PowerPC::RegisterVmcall(fix_layer_bits);
    const u32 fix_vmc = gen_vmcall(fix_portal_terminal_fn, 0);
    if (region == Region::NTSC_U) {
      add_code_change(0x801e6948, fix_vmc, "disable_portal_cutscene");
      add_code_change(0x801e695c, 0x48000114, "disable_portal_cutscene");
    } else if (region == Region::PAL) {
      add_code_change(0x801e8e60, fix_vmc, "disable_portal_cutscene");
      add_code_change(0x801e8e74, 0x48000114, "disable_portal_cutscene");
    } else { // region == Region::NTSC_J
      add_code_change(0x801e5948, fix_vmc, "disable_portal_cutscene");
      add_code_change(0x801e5950, 0x48000114, "disable_portal_cutscene");
    }
  } else if (game == Game::PRIME_2_GCN) {
    const int fix_portal_terminal_fn = PowerPC::RegisterVmcall(fix_layer_bits);
    const u32 fix_vmc = gen_vmcall(fix_portal_terminal_fn, 0);
    if (region == Region::NTSC_U) {
      add_code_change(0x800b7194, fix_vmc, "disable_portal_cutscene");
      add_code_change(0x800b71ac, 0x48000120, "disable_portal_cutscene");
    } else if (region == Region::PAL) {
      add_code_change(0x800b7228, fix_vmc, "disable_portal_cutscene");
      add_code_change(0x800b7240, 0x48000120, "disable_portal_cutscene");
    } else { // region == Region::NTSC_J
      add_code_change(0x800b7f24, fix_vmc, "disable_portal_cutscene");
      add_code_change(0x800b7f3c, 0x48000120, "disable_portal_cutscene");
    }
  } else {
    return true;
  }
  // agon
  portal_control_map[kAgonWorldId] = {{0x0012011e, 0x00120290, 0x0012023c},
                                      {0x001c0113, 0x001c0189, 0x001c0186}};
  // torvus
  portal_control_map[kTorvusWorldId] = {{0x00240087, 0x00240092, 0x0024007b},
                                        {0x001f0055, 0x001f00c5, 0x001f0061}};
  // sanctuary
  portal_control_map[kSanctuaryWorldId] = {{0x000a0185, 0x000a018b, 0x000a0194},
                                           {0x000f0052, 0x000f006d, 0x000f0062},
                                           {0x002c02e7, 0x002c02e6, 0x002c02e8},
                                           {0x00340015, 0x0034012e, 0x00340126}};
  return true;
}

}
