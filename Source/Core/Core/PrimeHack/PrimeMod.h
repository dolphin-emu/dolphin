#pragma once

#include <stdint.h>
#include <vector>

#include "Core/PowerPC/PowerPC.h"
#include "Core/PowerPC/MMU.h"

namespace prime {
struct CodeChange {
  uint32_t address, var;
  CodeChange() : CodeChange(0, 0) {}
  CodeChange(uint32_t address, uint32_t var) : address(address), var(var) {}
};

enum class Game : int {
  INVALID_GAME = -1,
  MENU = 0,
  PRIME_1 = 1,
  PRIME_2 = 2,
  PRIME_3 = 3,
  PRIME_1_GCN = 4,
  PRIME_2_GCN = 5,
  MAX_VAL = PRIME_2_GCN,
};

enum class Region : int {
  INVALID_REGION = -1,
  NTSC = 0,
  PAL = 1,
  MAX_VAL = PAL,
};

enum class ModState {
  // not running, no active instruction changes
  DISABLED,
  // running, no active instruction changes
  CODE_DISABLED,
  // running, active instruction changes
  ENABLED,
};

// Skeleton for a game mod
class PrimeMod {
public:
  // Run the mod, called each time ActionReplay is hit
  virtual void run_mod(Game game, Region region) = 0;
  // Init the mod, called when a new game / new region is loaded
  // Should NOT do any modifying of the game!!!
  virtual void init_mod(Game game, Region region) = 0;
  virtual void on_state_change(ModState old_state) {}

  virtual bool should_apply_changes() const {
    std::vector<CodeChange> const *cc_vec;
    if (state == ModState::CODE_DISABLED ||
        state == ModState::DISABLED) {
      cc_vec = &original_instructions;
    }
    else {
      cc_vec = &code_changes;
    }
      
    for (CodeChange const& change : *cc_vec) {
      if (PowerPC::HostRead_U32(change.address) != change.var) {
        return true;
      }
    }
    return false;
  }

  virtual ~PrimeMod() {};

  void apply_instruction_changes(bool invalidate = true) {
    auto active_changes = get_instruction_changes();
    for (CodeChange const& change : active_changes) {
      PowerPC::HostWrite_U32(change.var, change.address);
      if (invalidate) {
        PowerPC::ScheduleInvalidateCacheThreadSafe(change.address);
      }
    }
  }

  std::vector<CodeChange> const& get_instruction_changes() const {
    if (state == ModState::CODE_DISABLED ||
        state == ModState::DISABLED) {
      return original_instructions;
    }
    else {
      return code_changes;
    }
  }

  bool has_saved_instructions() const {
    return !original_instructions.empty();
  }
  bool is_initialized() const {
    return initialized;
  }
  ModState mod_state() const { return state; }
  void reset_mod() {
    code_changes.clear();
    original_instructions.clear();
    initialized = false;
  }
  void set_state(ModState st) {
    ModState original = this->state;
    this->state = st;
    on_state_change(original);
  }
  void save_original_instructions() {
    for (CodeChange const& change : code_changes) {
      original_instructions.emplace_back(change.address,
        PowerPC::HostRead_Instruction(change.address));
    }
  }

protected:
  std::vector<CodeChange> code_changes;
  bool initialized;

private:
  std::vector<CodeChange> original_instructions;
  ModState state = ModState::DISABLED;
};
}
