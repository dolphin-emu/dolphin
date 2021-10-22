#include "Core/PrimeHack/EmuVariableManager.h"
#include "Core/PrimeHack/PrimeUtils.h"

#include <cassert>

namespace prime {
  constexpr u32 base_address = 0x80004164; 
  constexpr int max_variables = 45;

  void EmuVariableManager::set_variable(const std::string& variable, u8 value) {
    auto result = variables_list.find(variable);
    if (result == variables_list.end()) {
      return;
    }

    write8(value, result->second);
  }

  void EmuVariableManager::set_variable(const std::string& variable, u32 value) {
    auto result = variables_list.find(variable);
    if (result == variables_list.end()) {
      return;
    }

    write32(value, result->second);
  }

  void EmuVariableManager::set_variable(const std::string& variable, float value) {
    auto result = variables_list.find(variable);
    if (result == variables_list.end()) {
      return;
    }

    writef32(value, result->second);
  }

  u32 EmuVariableManager::get_uint(const std::string& variable) const {
    auto result = variables_list.find(variable);
    if (result == variables_list.end()) {
      return 0;
    }

    return read32(result->second);
  }

  float EmuVariableManager::get_float(const std::string& variable) const {
    auto result = variables_list.find(variable);
    if (result == variables_list.end()) {
      return 0;
    }

    return readf32(result->second);
  }

  u32 EmuVariableManager::get_address(const std::string& variable) const {
    auto result = variables_list.find(variable);
    if (result == variables_list.end()) {
      return 0;
    }

    return result->second;
  }

  void EmuVariableManager::register_variable(const std::string& name) {
    if (variables_list.size() > max_variables) {
      assert("Too many variables registered");
      return;
    }

    if (variables_list.find(name) != variables_list.end())
      return;

    u32 address = base_address + (static_cast<u32>(variables_list.size()) * 0x4);
    variables_list[name] = address;
  }

  std::pair<u32, u32> EmuVariableManager::make_lis_ori(u32 gpr_num, const std::string& name) {
    u32 address = get_address(name);
    u32 lis = (0b001111 << 26) | (gpr_num << 21) | (0 << 16) | ((address >> 16) & 0xffff);
    u32 ori = (0b011000 << 26) | (gpr_num << 21) | (gpr_num << 16) | (address & 0xffff);

    return std::pair<u32, u32>(lis, ori);
  }

  void EmuVariableManager::reset_variables() {
    variables_list.clear();
  }
}; // namespace prime
