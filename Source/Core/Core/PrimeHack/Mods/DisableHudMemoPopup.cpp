#include "Core/PrimeHack/Mods/DisableHudMemoPopup.h"

#include "Core/PrimeHack/PrimeUtils.h"

namespace prime {
namespace {
void hudmemo_overlay_adjust_mp1(u32 job) {
  if (job == 0) {  // Fix time
    const u32 time_addr = GPR(28) + 0x30;
    writef32(std::max(readf32(time_addr), 3.f), time_addr);

    // Original instruction: li r4, 0
    GPR(4) = 0;
  } else if (job == 1) {  // Fix justification of hud text overlay
    const u32 justification_addr = GPR(3) + 0xd4 + 0x18;
    // 1 = Center justify
    write32(1, justification_addr);

    // Original instruction: addi r3, r3, 0xd4
    GPR(3) += 0xd4;
  } else {}
}

void hudmemo_overlay_adjust_mp1_gc(u32 job) {
  if (job == 0) {  // Fix time
    const u32 time_addr = GPR(28) + 0x34;
    writef32(std::max(readf32(time_addr), 3.f), time_addr);

    // Original instruction: li r4, 0
    GPR(4) = 0;
  } else if (job == 1) {  // Fix justification of hud text overlay
    const u32 justification_addr = GPR(3) + 0xd4 + 0x18;
    // 1 = Center justify
    write32(1, justification_addr);

    // Original instruction: addi r3, r3, 0xd4
    GPR(3) += 0xd4;
  } else {}
}

float restart_audio_timer = 0.f;
bool restart_audio_signal = false;
void restart_streamed_audio_mp3(u32 param) {
  // check if the hudmemo was going to be a popup
  if (GPR(0) == 1) {
    restart_audio_timer = 3.f;
    restart_audio_signal = true;
  }
}
}

void DisableHudMemoPopup::run_mod(Game game, Region region) {
  if (game != Game::PRIME_3 && game != Game::PRIME_3_STANDALONE) {
    return;
  }

  if (restart_audio_signal) {
    restart_audio_timer -= 1.f / 60.f;
    if (restart_audio_timer <= 0.f) {
      restart_audio_signal = false;
      LOOKUP_DYN(audio_fadein_time);
      LOOKUP_DYN(audio_fade_mode);
      if (audio_fadein_time != 0 && audio_fade_mode != 0) {
        writef32(2.f, audio_fadein_time);
        write32(2, audio_fade_mode);
      }
    }
  }
}

void DisableHudMemoPopup::init_mod_mp1(Region region) {
  const int hudmemo_fix_fn = PowerPC::RegisterVmcall(hudmemo_overlay_adjust_mp1);
  if (hudmemo_fix_fn == -1) {
    // HOW??? I SURE DO I USE THIS COOL FEATURE ENOUGH TO BE A PROBLEM :)
    return;
  }
  const u32 vmc_fix_time = gen_vmcall(static_cast<u32>(hudmemo_fix_fn), 0);
  const u32 vmc_fix_justification = gen_vmcall(static_cast<u32>(hudmemo_fix_fn), 1);

  if (region == Region::NTSC_U) {
    add_code_change(0x801ed3e0, 0x48000018);
    add_code_change(0x801ed408, vmc_fix_time);
    add_code_change(0x801cc668, vmc_fix_justification);
  } else if (region == Region::PAL) {
    add_code_change(0x801ed734, 0x48000018);
    add_code_change(0x801ed75c, vmc_fix_time);
    add_code_change(0x801cc964, vmc_fix_justification);
  } else { // region == Region::NTSC_J
    add_code_change(0x801ee014, 0x48000018);
    add_code_change(0x801ee03c, vmc_fix_time);
    add_code_change(0x801cd208, vmc_fix_justification);
  }
}

void DisableHudMemoPopup::init_mod_mp1gc(Region region) {
  const int hudmemo_fix_fn = PowerPC::RegisterVmcall(hudmemo_overlay_adjust_mp1_gc);
  if (hudmemo_fix_fn == -1) {
    return;
  }
  const u32 vmc_fix_time = gen_vmcall(static_cast<u32>(hudmemo_fix_fn), 0);
  const u32 vmc_fix_justification = gen_vmcall(static_cast<u32>(hudmemo_fix_fn), 1);
  if (region == Region::NTSC_U) {
    add_code_change(0x800e83bc, 0x48000018);
    add_code_change(0x800e83e4, vmc_fix_time);
    add_code_change(0x80064928, vmc_fix_justification);
  } else if (region == Region::PAL) {
    add_code_change(0x800e03c8, 0x48000018);
    add_code_change(0x800e03f0, vmc_fix_time);
    add_code_change(0x800657c4, vmc_fix_justification);
  } else {}
}

void DisableHudMemoPopup::init_mod_mp1gc_r1() {
  // Revision 1 matches revision 0 in behavior
  const int hudmemo_fix_fn = PowerPC::RegisterVmcall(hudmemo_overlay_adjust_mp1_gc);
  if (hudmemo_fix_fn == -1) {
    return;
  }
  const u32 vmc_fix_time = gen_vmcall(static_cast<u32>(hudmemo_fix_fn), 0);
  const u32 vmc_fix_justification = gen_vmcall(static_cast<u32>(hudmemo_fix_fn), 1);
  add_code_change(0x800e8438, 0x48000018);
  add_code_change(0x800e8460, vmc_fix_time);
  add_code_change(0x800649a4, vmc_fix_justification);
}

void DisableHudMemoPopup::init_mod_mp1gc_r2() {
  const int hudmemo_fix_fn = PowerPC::RegisterVmcall(hudmemo_overlay_adjust_mp1_gc);
  if (hudmemo_fix_fn == -1) {
    return;
  }
  const u32 vmc_fix_time = gen_vmcall(static_cast<u32>(hudmemo_fix_fn), 0);
  const u32 vmc_fix_justification = gen_vmcall(static_cast<u32>(hudmemo_fix_fn), 1);
  add_code_change(0x800e8940, 0x48000018);
  add_code_change(0x800e8968, vmc_fix_time);
  add_code_change(0x80064eac, vmc_fix_justification);
}

void DisableHudMemoPopup::init_mod_mp3(Game game, Region region) {
  const int audio_restart_fn = PowerPC::RegisterVmcall(restart_streamed_audio_mp3);
  const u32 vmc_restart_audio = gen_vmcall(static_cast<u32>(audio_restart_fn), 0);
  if (game == Game::PRIME_3) {
    if (region == Region::NTSC_U) {
      add_code_change(0x8021c9c4, vmc_restart_audio);
    } else if (region == Region::PAL) {
      add_code_change(0x8021c4a4, vmc_restart_audio);
    } else {}
  } else if (game == Game::PRIME_3_STANDALONE) {
    if (region == Region::NTSC_U) {
      add_code_change(0x8021fe80, vmc_restart_audio);
    } else if (region == Region::PAL) {
      add_code_change(0x80220f10, vmc_restart_audio);
    } else { // region == Region::NTSC_J
      add_code_change(0x8022251c, vmc_restart_audio);
    }
  }
}

bool DisableHudMemoPopup::init_mod(Game game, Region region) {
  switch (game) {
  case Game::PRIME_1:
    init_mod_mp1(region);
    break;
  case Game::PRIME_1_GCN:
    init_mod_mp1gc(region);
    break;
  case Game::PRIME_1_GCN_R1:
    init_mod_mp1gc_r1();
    break;
  case Game::PRIME_1_GCN_R2:
    init_mod_mp1gc_r2();
    break;
  case Game::PRIME_2:
    if (region == Region::NTSC_U) {
      add_code_change(0x801f3354, 0x48000018);
    } else if  (region == Region::PAL) {
      add_code_change(0x801f586c, 0x48000018);
    } else { // region == Region::NTSC_J
      add_code_change(0x801f2368, 0x48000018);
    }
    break;
  case Game::PRIME_2_GCN:
    if (region == Region::NTSC_U) {
      add_code_change(0x800bb10c, 0x48000018);
    } else if (region == Region::PAL) {
      add_code_change(0x800bb1a0, 0x48000018);
    } else { // region == Region::NTSC_J
      add_code_change(0x800bbe9c, 0x48000018);
    }
    break;
  case Game::PRIME_3:
  case Game::PRIME_3_STANDALONE:
    init_mod_mp3(game, region);
    break;
  }

  return true;
}
}
