#include "Core/PrimeHack/PrimeMod.h"

#include "Core/PrimeHack/AddressDB.h"
#include "Core/PrimeHack/HackManager.h"

#include "Core/PowerPC/MMU.h"

namespace prime {

bool PrimeMod::should_apply_changes() const {
  std::vector<CodeChange> const& cc_vec = get_changes_to_apply();
      
  for (CodeChange const& change : cc_vec) {
    if (PowerPC::HostRead_U32(change.address) != change.var) {
      return true;
    }
  }
  return false;
}

void PrimeMod::apply_instruction_changes(bool invalidate)  {
  auto active_changes = get_changes_to_apply();
  for (CodeChange const& change : active_changes) {
    PowerPC::HostWrite_U32(change.var, change.address);
    if (invalidate) {
      PowerPC::ScheduleInvalidateCacheThreadSafe(change.address);
    }
  }
}

const std::vector<CodeChange>& PrimeMod::get_changes_to_apply() const {
  if (state == ModState::CODE_DISABLED ||
      state == ModState::DISABLED) {
    return original_instructions;
  }
  else {
    return current_active_changes;
  }
}

void PrimeMod::set_code_group_state(const std::string& group_name, ModState new_state) {
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

void PrimeMod::reset_mod() {
  code_changes.clear();
  original_instructions.clear();
  pending_change_backups.clear();
  current_active_changes.clear();
  initialized = false;
  on_reset();
}

void PrimeMod::set_state(ModState new_state) {
  ModState original = this->state;
  this->state = new_state;
  on_state_change(original);
}

void PrimeMod::add_code_change(u32 addr, u32 code, std::string_view group) {
  if (group != "") {
    group_change& cg = code_groups[std::string(group)];
    std::get<0>(cg).push_back(code_changes.size());
    std::get<1>(cg) = ModState::ENABLED;
  }
  pending_change_backups.emplace_back(addr);
  code_changes.emplace_back(addr, code);
  current_active_changes.emplace_back(addr, code);
}

void PrimeMod::set_code_change(u32 address, u32 var) {
  code_changes[address].var = var;
  current_active_changes[address].var = var;
}

void PrimeMod::update_original_instructions() {
  for (u32 addr : pending_change_backups) {
    original_instructions.emplace_back(addr, PowerPC::HostRead_Instruction(addr));
  }
  pending_change_backups.clear();
}

u32 PrimeMod::lookup_address(std::string_view name) {
  return addr_db->lookup_address(hack_mgr->get_active_game(), hack_mgr->get_active_region(), name);
}

u32 PrimeMod::lookup_dynamic_address(std::string_view name) {
  return addr_db->lookup_dynamic_address(hack_mgr->get_active_game(), hack_mgr->get_active_region(), name);
}

}
