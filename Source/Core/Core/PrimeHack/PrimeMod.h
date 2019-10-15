#pragma once

#include <stdint.h>
#include <vector>

#include "Core/PowerPC/PowerPC.h"
#include "Core/PowerPC/MMU.h"

namespace prime {

  struct CodeChange {
    uint32_t address, var;
    CodeChange(uint32_t address, uint32_t var) : address(address), var(var) {}
  };

  enum class Game : int {
    INVALID_GAME = -1,
    MENU = 0,
    PRIME_1 = 1,
    PRIME_2 = 2,
    PRIME_3 = 3,
    MAX_VAL = PRIME_3,
  };

  enum class Region : int {
    INVALID_REGION = -1,
    NTSC = 0,
    PAL = 1,
    MAX_VAL = PAL,
  };

  constexpr std::size_t GAME_SZ = static_cast<std::size_t>(Game::MAX_VAL) + 1;
  constexpr std::size_t REGION_SZ = static_cast<std::size_t>(Region::MAX_VAL) + 1;

  // Skeleton for a game mod
  class PrimeMod {
  public:
    virtual Game game() const = 0;
    virtual Region region() const = 0;
    std::vector<CodeChange> const& get_instruction_changes() const {
      return code_changes;
    }
    virtual void run_mod() = 0;
    virtual bool should_apply_changes() const {
      for (CodeChange const& change : code_changes) {
        if (PowerPC::HostRead_U32(change.address) != change.var) {
          return true;
        }
      }
      return false;
    }

    virtual ~PrimeMod() {};

  protected:
    std::vector<CodeChange> code_changes;
  };

}
