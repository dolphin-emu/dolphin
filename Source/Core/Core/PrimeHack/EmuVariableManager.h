#pragma once

#include <string>
#include <map>

#include "Common/CommonTypes.h"

namespace prime {

class EmuVariableManager {
public:
  void set_variable(const std::string& variable, u8 value);
  void set_variable(const std::string& variable, u32 value);
  void set_variable(const std::string& variable, float value);

  u32 get_uint(const std::string& variable) const;
  float get_float(const std::string& variable) const;
  u32 get_address(const std::string& variable) const;

  void register_variable(const std::string& name);
  std::pair<u32, u32> make_lis_ori(u32 gpr_num, const std::string& name);

  void reset_variables();

private:
  std::map<std::string, u32> variables_list;
};
}
