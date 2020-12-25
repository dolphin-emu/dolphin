#pragma once

#include <stdint.h>
#include <map>
#include <string_view>
#include <tuple>
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
  /* New Play Controls only */
  MENU_PRIME_1 = 1,
  MENU_PRIME_2 = 2,
  /* */
  PRIME_1 = 3,
  PRIME_2 = 4,
  PRIME_3 = 5,
  PRIME_3_STANDALONE = 6,
  PRIME_1_GCN = 7,
  PRIME_2_GCN = 8,
  MAX_VAL = PRIME_2_GCN,
};

enum class Region : int {
  INVALID_REGION = -1,
  NTSC_U = 0,
  NTSC_J = 1,
  PAL = 2,
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
  virtual bool init_mod(Game game, Region region) = 0;
  virtual void on_state_change(ModState old_state) {}

  virtual bool should_apply_changes() const {
    std::vector<CodeChange> const& cc_vec = get_instruction_changes();
      
    for (CodeChange const& change : cc_vec) {
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
      return current_active_changes;
    }
  }

  void change_code_group_state(std::string const& group_name, ModState new_state) {
    if (code_groups.find(group_name) == code_groups.end()) {
      return;
    }

    group_change& cg = code_groups[std::string(group_name)];
    if (std::get<1>(cg) == new_state) {
      return; // Can't not do anything UwU! (Shio)
    }

    std::get<1>(cg) = new_state;
    std::vector<CodeChange> const& from_vec = (new_state == ModState::ENABLED ? code_changes : original_instructions);
    for (auto const& code_idx : std::get<0>(cg)) {
      current_active_changes[code_idx] = from_vec[code_idx];
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
    current_active_changes.clear();
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

  void add_code_change(u32 addr, u32 code, std::string_view group = "") {
    if (group != "") {
      group_change& cg = code_groups[std::string(group)];
      std::get<0>(cg).push_back(code_changes.size());
      std::get<1>(cg) = ModState::ENABLED;
    }
    code_changes.emplace_back(addr, code);
    current_active_changes.emplace_back(addr, code);
  }

  void mark_initialized() {
    initialized = true;
  }

private:
  using group_change = std::tuple<std::vector<size_t>, ModState>;

  bool initialized = false;

  std::vector<CodeChange> original_instructions;
  std::vector<CodeChange> code_changes;
  std::map<std::string, group_change> code_groups;
  ModState state = ModState::DISABLED;

  std::vector<CodeChange> current_active_changes;
};
}
