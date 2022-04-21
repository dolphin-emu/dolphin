#include "Core/PrimeHack/Mods/Motd.h"

#include "Core/PrimeHack/PrimeUtils.h"
#include "Core/PrimeHack/EmuVariableManager.h"

namespace prime {

void Motd::add_motd_hook(u32 inject_address, u32 original_jmp,
                         u32 old_message_len) {
  constexpr auto align_up = [](u32 addr) {
    return (addr + 4) & ~u32{0x3};
  };
  old_msg_start = inject_address + 0x8c;
  new_msg_start = align_up(inject_address + 0x8c + 0x4 + old_message_len);

  u32 orig_lis = gen_lis(8, old_msg_start >> 16);
  u32 orig_ori = gen_ori(8, 8, old_msg_start);
  u32 new_lis = gen_lis(8, new_msg_start >> 16);
  u32 new_ori = gen_ori(8, 8, new_msg_start);
  u32 bl_original = gen_branch(inject_address + 0x88, original_jmp);
  add_code_change(inject_address + 0x00, orig_lis);
  add_code_change(inject_address + 0x04, orig_ori);
  add_code_change(inject_address + 0x08, 0x80c80000);
  add_code_change(inject_address + 0x0c, 0x7c053000);
  add_code_change(inject_address + 0x10, 0x40820078);
  add_code_change(inject_address + 0x14, 0x38c00000);
  add_code_change(inject_address + 0x18, 0x80e30008);
  add_code_change(inject_address + 0x1c, 0x39080004);
  add_code_change(inject_address + 0x20, 0x48000018);
  add_code_change(inject_address + 0x24, 0x7c0638ae);
  add_code_change(inject_address + 0x28, 0x7d2640ae);
  add_code_change(inject_address + 0x2c, 0x7c004800);
  add_code_change(inject_address + 0x30, 0x40820058);
  add_code_change(inject_address + 0x34, 0x38c60001);
  add_code_change(inject_address + 0x38, 0x7c062800);
  add_code_change(inject_address + 0x3c, 0x4180ffe8);
  add_code_change(inject_address + 0x40, new_lis);
  add_code_change(inject_address + 0x44, new_ori);
  add_code_change(inject_address + 0x48, 0x80e80000);
  add_code_change(inject_address + 0x4c, 0x39080004);
  add_code_change(inject_address + 0x50, 0x38c00000);
  add_code_change(inject_address + 0x54, 0x48000010);
  add_code_change(inject_address + 0x58, 0x7c0640ae);
  add_code_change(inject_address + 0x5c, 0x7c0621ae);
  add_code_change(inject_address + 0x60, 0x38c60001);
  add_code_change(inject_address + 0x64, 0x7c063800);
  add_code_change(inject_address + 0x68, 0x4180fff0);
  add_code_change(inject_address + 0x6c, 0x80030008);
  add_code_change(inject_address + 0x70, 0x7c002a14);
  add_code_change(inject_address + 0x74, 0x90030008);
  add_code_change(inject_address + 0x78, 0x7cc53850);
  add_code_change(inject_address + 0x7c, 0x7f06c050);
  add_code_change(inject_address + 0x80, 0x7cfa3b78);
  add_code_change(inject_address + 0x84, 0x4e800020);
  add_code_change(inject_address + 0x88, bl_original);
}

bool Motd::init_mod(Game game, Region region) {
  new_message = GetMotd();
  if (game == Game::MENU && region == Region::NTSC_U) {
    old_message = "Nunchuk is required.";
    add_motd_hook(0x80626000, 0x8039e628, static_cast<u32>(old_message.length() + 1));
    add_code_change(0x8037e3f0, gen_branch_link(0x8037e3f0, 0x80626000));
  } else if (game == Game::MENU && region == Region::PAL) {
    old_message = "These games use the Nunchuk.";
    add_motd_hook(0x8062b800, 0x8039e274, static_cast<u32>(old_message.length() + 1));
    add_code_change(0x8037e03c, gen_branch_link(0x8037e03c, 0x8062b800));
  }
  return true;
}

void Motd::run_mod(Game game, Region region) {
  constexpr auto write_string = [](std::string const& str, u32 addr) {
    write32(static_cast<u32>(str.length() + 1), addr);
    for (size_t i = 0; i < str.length(); i++) {
      write8(str[i], addr + 4 + static_cast<u32>(i));
    }
    write8(0, addr + 4 + static_cast<u32>(str.length()));
  };
  if (game == Game::MENU && (region == Region::NTSC_U || region == Region::PAL)) {
    write_string(old_message, old_msg_start);
    write_string(new_message, new_msg_start);
  }
}

}
