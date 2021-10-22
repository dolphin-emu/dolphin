#include "Core/PrimeHack/Mods/BloomIntensityMP3.h"

#include "Core/PrimeHack/PrimeUtils.h"
#include "Core/PowerPC/Interpreter/Interpreter.h"

namespace prime {
  void BloomIntensityMP3::replace_start_bloom_func(u32 branch_pos, u32 bloom_func) {
    u32 lis, ori;
    std::tie<u32, u32>(lis, ori) = prime::GetVariableManager()->make_lis_ori(16, "bloom_intensity");
    prime::GetVariableManager()->set_variable("bloom_intensity", slider_val);
    add_code_change(bloom_func, 0x48000000 | ((branch_pos - bloom_func) & 0x3fffffc));
    add_code_change(branch_pos, lis);                // lis r16, bloom_intensity
    add_code_change(branch_pos + 0x04, ori);         // ori r16, r16, bloom_intensity
    add_code_change(branch_pos + 0x08, 0xc0300000);  // lfs f1, 0(r16)
    add_code_change(branch_pos + 0x0C, 0x9421fe00);  // stwu	sp, -0x0200 (sp)
    add_code_change(branch_pos + 0x10, 0x48000000 | ((bloom_func - branch_pos - 0xC) & 0x3fffffc));
  }

  bool BloomIntensityMP3::init_mod(Game game, Region region) {
    if (game != Game::PRIME_3 && game != Game::PRIME_3_STANDALONE) {
      return true;
    }
    prime::GetVariableManager()->register_variable("bloom_intensity");
    slider_val = (GetBloomIntensity() / 100.f);
    switch (game) {
    case Game::PRIME_3:
      if (region == Region::NTSC_U) {
        replace_start_bloom_func(0x80004290, 0x804852cc);
      }
      else if (region == Region::PAL) {
        replace_start_bloom_func(0x80004290, 0x804849e8);
      }
      break;
    case Game::PRIME_3_STANDALONE:
      if (region == Region::NTSC_U) {
        replace_start_bloom_func(0x80004290, 0x80486880);
      }
      else if (region == Region::PAL) {
        replace_start_bloom_func(0x80004290, 0x804885a4);
      }
      else {  // region == Region::NTSC_J
        replace_start_bloom_func(0x80004290, 0x8048b34c);
      }
      break;
    default:
      break;
    }
    return true;
  }

  void BloomIntensityMP3::run_mod(Game game, Region region) {
    switch (game) {
    case Game::PRIME_3:
    case Game::PRIME_3_STANDALONE:
      if (slider_val != (GetBloomIntensity() / 100.f)) {
        slider_val = (GetBloomIntensity() / 100.f);
        prime::GetVariableManager()->set_variable("bloom_intensity", slider_val);
      }
      break;
    default:
      break;
    }
  }
}  // namespace prime
