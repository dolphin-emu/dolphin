#pragma once

#include "Core/PrimeHack/PrimeMod.h"
#include <VideoCommon/OnScreenDisplay.h>

namespace prime {
  class RestoreDashing : public PrimeMod {
  public:
    void run_mod(Game game, Region region) override { }
    bool init_mod(Game game, Region region) override {
      // d0210014 d0010010 4b -> pattern to search for CPlayer::FinishSidewaysDash() for Prime 2 GC
      // d0210024 d0010020 4b -> pattern to search for CPlayer::FinishSidewaysDash() for Prime 2 and 3 Wii
      switch (game) {
      case Game::PRIME_1:
        if (region == Region::NTSC_U) {
          // remove scan visor check
          add_code_change(0x80193334, 0x48000018);
          // restore dashing speed
          add_code_change(0x80194b60, 0x4800001c);
          // stop dash when done dashing
          add_code_change(0x80192cc0, 0x881f037c); // CPlayer + 0x37c
        } else if (region == Region::PAL) {
          // remove scan visor check
          add_code_change(0x801935cc, 0x48000018);
          // restore dashing speed
          add_code_change(0x80194df8, 0x4800001c);
          // stop dash when done dashing
          add_code_change(0x80192f58, 0x881f037c); // CPlayer + 0x37c
        } else { // region == Region::NTSC-J
          // remove scan visor check
          add_code_change(0x80193eb4, 0x48000018);
          // restore dashing speed
          add_code_change(0x801956e0, 0x4800001c);
          // stop dash when done dashing
          add_code_change(0x80193840, 0x881f037c); // CPlayer + 0x37c
        }
        break;
      case Game::PRIME_1_GCN:
        if (region == Region::PAL) {
          // remove scan visor check
          add_code_change(0x80275328, 0x48000018);
        } else if (region == Region::NTSC_J) {
          // remove scan visor check
          add_code_change(0x802770e4, 0x48000018);
        }
        break;
      case Game::PRIME_1_GCN_R2:
        add_code_change(0x802888d0, 0x48000018);
        break;
      case Game::PRIME_2:
        if (region == Region::NTSC_U) {
          // don't slow down when finishing the dash
          add_code_change(0x8015d690, 0x60000000);
          // stop dash when done dashing
          add_code_change(0x8015cd1c, 0x881e0574); // CPlayer + 0x574 as u8
        } else if (region == Region::NTSC_J) {
          // don't slow down when finishing the dash
          add_code_change(0x8015cc58, 0x60000000);
          // stop dash when done dashing
          add_code_change(0x8015c2e4, 0x881e0574); // CPlayer + 0x588 as u8
        } else if (region == Region::PAL) {
          // don't slow down when finishing the dash
          add_code_change(0x8015ee08, 0x60000000);
          // stop dash when done dashing
          add_code_change(0x8015e494, 0x881e0574); // CPlayer + 0x588 as u8
        }
        break;
      case Game::PRIME_2_GCN:
        if (region == Region::NTSC_U) {
          // don't slow down when finishing the dash
          add_code_change(0x8018961c, 0x60000000);
          // stop dash when done dashing
          add_code_change(0x80189d6c, 0x881e0588); // CPlayer + 0x588 as u8
        } else if (region == Region::NTSC_J) {
          // don't slow down when finishing the dash
          add_code_change(0x8018b130, 0x60000000);
          // stop dash when done dashing
          add_code_change(0x8018b884, 0x881e0588); // CPlayer + 0x588 as u8
        } else if (region == Region::PAL) {
          // don't slow down when finishing the dash
          add_code_change(0x80189914, 0x60000000);
          // stop dash when done dashing
          add_code_change(0x8018a068, 0x881e0588); // CPlayer + 0x588 as u8
        }
        break;
      case Game::PRIME_3:
        if (region == Region::NTSC_U) {
          // don't slow down when finishing the dash
          add_code_change(0x80174c60, 0x60000000);
          // stop dash when done dashing
          add_code_change(0x8017432c, 0x881e06d0); // CPlayer + 0x6d0 as u8
        } else if (region == Region::PAL) {
          // don't slow down when finishing the dash
          add_code_change(0x801745ac, 0x60000000);
          // stop dash when done dashing
          add_code_change(0x80173c78, 0x881e06d0); // CPlayer + 0x6d0 as u8
        }
        break;
      case Game::PRIME_3_STANDALONE:
        if (region == Region::NTSC_U) {
          // don't slow down when finishing the dash
          add_code_change(0x80178d90, 0x60000000);
          // stop dash when done dashing
          add_code_change(0x8017845c, 0x881e06cc); // CPlayer + 0x6cc as u8
        } else if (region == Region::NTSC_J) {
          // don't slow down when finishing the dash
          add_code_change(0x8017aa90, 0x60000000);
          // stop dash when done dashing
          add_code_change(0x8017a15c, 0x881e06d0); // CPlayer + 0x6d0 as u8
        } else if (region == Region::PAL) {
          // don't slow down when finishing the dash
          add_code_change(0x80179884, 0x60000000);
          // stop dash when done dashing
          add_code_change(0x80178F50, 0x881e06d0); // CPlayer + 0x6d0 as u8
        }
        break;
      }

      return true;
    }
    void on_state_change(ModState old_state) override {}
  };
}
