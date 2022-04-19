#pragma once

#include "Core/PrimeHack/PrimeMod.h"

namespace prime {

class Motd : public PrimeMod {
public:
  void run_mod(Game game, Region region) override;
  bool init_mod(Game game, Region region) override;
  void on_state_change(ModState old_state) override {}

private:
  void add_motd_hook(u32 inject_address, u32 original_jmp, u32 old_message_len);

  u32 old_msg_start;
  u32 new_msg_start;
  std::string old_message;
  std::string new_message;
};

}
