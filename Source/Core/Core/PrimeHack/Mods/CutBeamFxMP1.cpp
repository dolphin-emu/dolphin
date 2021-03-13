#include "Core/PrimeHack/Mods/CutBeamFxMP1.h"

namespace prime {

bool CutBeamFxMP1::init_mod(Game game, Region region) {
  if (game != Game::PRIME_1) {
    return true;
  }
  
  u32 gunfx_offset;
  u32 transform_ctor_offset;
  u32 advance_particles_offset;
  if (region == Region::NTSC_U) {
    gunfx_offset = 0xfffffd68;              // 0x8018c410 - 0x8018c6a8;
    transform_ctor_offset = 0xac;           // 0x80348d2c - 0x80348c80;
    advance_particles_offset = 0xfffffeb0;  // 0x80139870 - 0x801399c0;
  } else if (region == Region::PAL) {
    gunfx_offset = 0;
    transform_ctor_offset = 0;
    advance_particles_offset = 0;
  } else if (region == Region::NTSC_J) {
    gunfx_offset = 0x8e8;                   // 0x8018cf90 - 0x8018c6a8;
    transform_ctor_offset = 0xfffffb70;     // 0x803487f0 - 0x80348c80;
    advance_particles_offset = 0x4b0;       // 0x80139e70 - 0x801399c0;
  } else {
    return true;
  }
  
  const u32 address1 = 0x80004a68;
  const u32 address2 = 0x80004968;
  add_code_change(address1, 0x9421ffa8);
  add_code_change(address1 + 0x4, 0x7c0802a6);
  add_code_change(address1 + 0x8, 0x9001001c);
  add_code_change(address1 + 0xc, 0x93a10010);
  add_code_change(address1 + 0x10, 0x93c10018);
  add_code_change(address1 + 0x14, 0x93e10014);
  add_code_change(address1 + 0x18, 0xd3e1000c);
  add_code_change(address1 + 0x1c, 0x7c7f1b78);
  add_code_change(address1 + 0x20, 0x7c9e2378);
  add_code_change(address1 + 0x24, 0x7cbd2b78);
  add_code_change(address1 + 0x28, 0xffe00890);
  add_code_change(address1 + 0x2c, 0x38610020);
  add_code_change(address1 + 0x30, 0x389f04b0);
  add_code_change(address1 + 0x34, 0x483441e5 + transform_ctor_offset);
  add_code_change(address1 + 0x38, 0x7fa3eb78);
  add_code_change(address1 + 0x3c, 0x38810020);
  add_code_change(address1 + 0x40, 0x7fe5fb78);
  add_code_change(address1 + 0x44, 0x48134f15 + advance_particles_offset);
  add_code_change(address1 + 0x48, 0xc03f0384);
  add_code_change(address1 + 0x4c, 0x38800000);
  add_code_change(address1 + 0x50, 0xc002cdf8);
  add_code_change(address1 + 0x54, 0x807f0734);
  add_code_change(address1 + 0x58, 0xfc010040);
  add_code_change(address1 + 0x5c, 0x40810018);
  add_code_change(address1 + 0x60, 0xc03f037c);
  add_code_change(address1 + 0x64, 0xc002d078);
  add_code_change(address1 + 0x68, 0xfc010040);
  add_code_change(address1 + 0x6c, 0x40810008);
  add_code_change(address1 + 0x70, 0x38800001);
  add_code_change(address1 + 0x74, 0x81830000);
  add_code_change(address1 + 0x78, 0x7fc5f378);
  add_code_change(address1 + 0x7c, 0x38df0510);
  add_code_change(address1 + 0x80, 0xfc20f890);
  add_code_change(address1 + 0x84, 0x818c001c);
  add_code_change(address1 + 0x88, 0x7d8903a6);
  add_code_change(address1 + 0x8c, 0x4e800421);
  add_code_change(address1 + 0x90, 0x83a10010);
  add_code_change(address1 + 0x94, 0x83c10018);
  add_code_change(address1 + 0x98, 0x83e10014);
  add_code_change(address1 + 0x9c, 0x8001001c);
  add_code_change(address1 + 0xa0, 0xc3e1000c);
  add_code_change(address1 + 0xa4, 0x7c0803a6);
  add_code_change(address1 + 0xa8, 0x38210058);
  add_code_change(address1 + 0xac, 0x4e800020);

  add_code_change(address2, 0x7f03c378);
  add_code_change(address2 + 0x4, 0x7f24cb78);
  add_code_change(address2 + 0x8, 0x7f65db78);
  add_code_change(address2 + 0xc, 0xfc20f890);
  add_code_change(address2 + 0x10, 0x480000f1);
  add_code_change(address2 + 0x14, 0x386100b0);
  add_code_change(address2 + 0x18, 0x48187d2c + gunfx_offset);

  add_code_change(0x8018c6a8 + gunfx_offset, 0x4be782c0 - gunfx_offset);
  return true;
}

}
