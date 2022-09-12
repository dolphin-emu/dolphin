#include "Core/PrimeHack/Mods/STRGPatch.h"
#include "Core/PrimeHack/PrimeUtils.h"

namespace prime {
namespace {
std::string readin_str(u32 str_ptr) {
  std::ostringstream key_readin;
  for (char c; (c = read8(str_ptr)); str_ptr++) {
    key_readin << c;
  }
  return key_readin.str();
}

u32 bsearch_strg_table(std::string const& key, u32 strg_header) {
  u32 bsearch_left = read32(strg_header + 0x14);
  int dist = read32(strg_header + 0x8);
  while (dist > 0) {
    int midpoint_offset = (dist * 4) & ~0x7;
    int half_dist = dist >> 1;
    std::string test_key = readin_str(read32(bsearch_left + midpoint_offset));
    if (test_key.compare(key) < 0) {
      dist -= (1 + half_dist);
      bsearch_left += midpoint_offset + 8;
    } else {
      dist = half_dist;
    }
  }
  return bsearch_left;
}

void patch_strg_entry_mp3(u32 rgn) {
  u32 strg_header = GPR(31), key_ptr = GPR(4);

  u32 patched_table_addr = rgn == 0 ? 0x80676c00 : 0x8067a400;
  std::string key = readin_str(key_ptr);
  if (key == "ShakeOffGandrayda") {
    u32 bsearch_result = bsearch_strg_table(key, strg_header);
    std::string found_key = readin_str(read32(bsearch_result));
    if (found_key == key) {
      u32 strg_val_index = read32(bsearch_result + 4);
      u32 strg_val_table = read32(strg_header + 0x1c);
      write32(patched_table_addr, strg_val_table + 4 * strg_val_index);
    }
  }
  GPR(3) = GPR(31);
}
}

void STRGPatch::run_mod(Game game, Region region) {
  switch (game) {
  case Game::PRIME_1: 
  case Game::PRIME_1_GCN:
  case Game::PRIME_1_GCN_R1:
  case Game::PRIME_1_GCN_R2:
  case Game::PRIME_2:
  case Game::PRIME_2_GCN:
  case Game::PRIME_3_STANDALONE:
    break;
  case Game::PRIME_3:
    run_mod_mp3(region);
    break;
  default:
    break;
  }
}

void STRGPatch::run_mod_mp3(Region region) {
  u32 addr = region == Region::NTSC_U ? 0x80676c00 : 0x8067a400;
  char str[] = "&just=center;Mash Jump [&image=0x5FC17B1F30BAA7AE;] to shake off Gandrayda!";
  for (size_t i = 0; i < sizeof(str); i++) {
    write8(str[i], addr + i);
  }
}

bool STRGPatch::init_mod(Game game, Region region) {
  switch (game) {
  case Game::PRIME_1: 
  case Game::PRIME_1_GCN:
  case Game::PRIME_1_GCN_R1:
  case Game::PRIME_1_GCN_R2:
  case Game::PRIME_2:
  case Game::PRIME_2_GCN:
  case Game::PRIME_3_STANDALONE:
    break;
  case Game::PRIME_3: {
    int vmc_id = PowerPC::RegisterVmcall(patch_strg_entry_mp3);
    if (region == Region::NTSC_U) {
      add_code_change(0x803cc3f4, gen_vmcall(vmc_id, 0));
    } else if (region == Region::PAL) {
      add_code_change(0x803cbb10, gen_vmcall(vmc_id, 1));
    }
    break;
  }
  default:
    break;
  }
  return true;
}

}
