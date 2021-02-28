#include "Core/PrimeHack/Mods/PortalSkipMP2.h"

#include "Core/PrimeHack/PrimeUtils.h"

#pragma optimize("", off)

namespace prime {

void PortalSkipMP2::run_mod(Game game, Region region) {

  if (game != Game::PRIME_2 && region != Region::NTSC_U) {
    return;
  }

  const u32 object_list = read32(state_mgr_address + 0x810) + 4;
  if (!mem_check(object_list)) {
    return;
  }

  const u32 mlvl_id = read32(read32(world_id_address));
  auto wpc_iter = portal_control_map.find(mlvl_id);
  if (wpc_iter == portal_control_map.end()) {
    return;
  }
  const auto& world_portal_controls = wpc_iter->second;

  const u32 player = read32(state_mgr_address + 0x14f4);
  if (!mem_check(player)) {
    return;
  }
  const u32 area_id = read32(player + 4);

  auto find_object = [object_list](u32 editor_id) -> u32 {
    for (int i = 0; i < 1024; i++) {
      const u32 object_ptr = read32(object_list + (i * 8));
      if (object_ptr != 0 && read32(object_ptr + 0xc) == editor_id) {
        return object_ptr;
      }
    }
    return 0;
  };

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

      const u32 scan_conn_vec_size = read32(scan_obj + 0x10);
      const u32 scan_conn_vec = read32(scan_obj + 0x18);
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
      
      const u32 timer_conn_vec_size = read32(seq_timer_obj + 0x10);
      const u32 timer_conn_vec = read32(seq_timer_obj + 0x18);
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

      const u32 seq_timer_event_vec = read32(seq_timer_obj + 0x3c);
      const u32 seq_timer_event_vec_size = read32(seq_timer_obj + 0x34);
      if (seq_timer_event_vec_size != 0 && seq_timer_event_vec != 0) {
        for (u32 i = 0; i < seq_timer_event_vec_size; i++) {
          // Check if timer should be adjusted
          // These tables are all of size 1
          const u32 time_table_ptr = read32(seq_timer_event_vec + (i * 0x14) + 0xc);
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
  if (game != Game::PRIME_2) {
    return true;
  }

  if (region == Region::PAL) {
    state_mgr_address = 0x804ee738;
    world_id_address = 0x8050f76c;
    add_code_change(0x801e8e74, 0x48000114, "disable_portal_cutscene");
  } else if (region == Region::NTSC_U) {
    state_mgr_address = 0x804e72e8;
    world_id_address = 0x805081cc;
    add_code_change(0x801e695c, 0x48000114, "disable_portal_cutscene");
  }

  // agon
  portal_control_map[0x42b935e4] = {{0x0012011e, 0x00120290, 0x0012023c},
                                    {0x001c0113, 0x001c0189, 0x001c0186}};
  // torvus
  portal_control_map[0x3dfd2249] = {{0x00240087, 0x00240092, 0x0024007b},
                                    {0x001f0055, 0x001f00c5, 0x001f0061}};
  // sanctuary
  portal_control_map[0x1baa96c2] = {{0x000a0185, 0x000a018b, 0x000a0194},
                                    {0x000f0052, 0x000f006d, 0x000f0062},
                                    {0x002c02e7, 0x002c02e6, 0x002c02e8},
                                    {0x00340015, 0x0034012e, 0x00340126}};

  return true;
}

}
