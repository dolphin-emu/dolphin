#pragma once

#include "Core/PrimeHack/PrimeMod.h"
#include "Core/PrimeHack/HackConfig.h"

namespace prime {

class DisableBloom : public PrimeMod {
public:
  void run_mod(Game game, Region region) override {
    if (game == Game::PRIME_3 || game == Game::PRIME_3_STANDALONE) {
      LOOKUP(bloom_offset);
      if (bloom_offset != 0) {
        constexpr std::array<float, 6> reduced_offsets = {-1.5f, -1.f, -0.5f, 0.5f, 1.f, 1.5f};
        constexpr std::array<float, 6> normal_offsets = {-5.5f, -3.5f, -1.5f, 1.5f, 3.5f, 5.5f};
        for (u32 i = 0; i < 6; i++) {
          if (GetReduceBloom()) {
            writef32(reduced_offsets[i], bloom_offset + (i * 4));
          } else {
            writef32(normal_offsets[i], bloom_offset + (i * 4));
          }
        }
      }
    }
    set_code_group_state("bloom_disable", GetBloom() ? ModState::ENABLED : ModState::DISABLED);
  }
  // BLR writing machine
  bool init_mod(Game game, Region region) override {
    switch (game) {
    case Game::PRIME_1:
      if (region == Region::NTSC_U) {
        add_code_change(0x80290edc, 0x4e800020, "bloom_disable");
      } else if (region == Region::PAL) {
        add_code_change(0x80291258, 0x4e800020, "bloom_disable");
      } else { // region == Region::NTSC_J
        add_code_change(0x802919bc, 0x4e800020, "bloom_disable");
      }
      break;
    case Game::PRIME_2:
      if (region == Region::NTSC_U) {
        add_code_change(0x80292204, 0x4e800020, "bloom_disable");
      } else if (region == Region::PAL) {
        add_code_change(0x80294a40, 0x4e800020, "bloom_disable");
      } else { // region == Region::NTSC_J
        add_code_change(0x8029137c, 0x4e800020, "bloom_disable");
      }
      break;
    default:
      break;
    }
    return true;
  }
  void on_state_change(ModState old_state) override {}
};
}
