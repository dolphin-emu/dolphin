#pragma once

#include <map>
#include <string>
#include <string_view>
#include <vector>

#include "Core/PrimeHack/PrimeMod.h"
#include "Common/CommonTypes.h"

namespace prime {

// Defined in AddressDBInit.cpp
void init_db(AddressDB& addr_db);

class AddressDB {
public:
  using region_triple = std::tuple<u32, u32, u32>;

  AddressDB();
  void register_address(Game game, std::string_view name, u32 addr_ntsc_u = 0, u32 addr_pal = 0, u32 addr_ntsc_j = 0);
  // Address is computed as:
  //   [...[[source_var + offset0] + offset1] + ... + offsetN-2] + offsetN-1
  // singleton lists will only offset source_var, not dereference
  void register_dynamic_address(Game game, std::string_view name, std::string_view source_var,
                                std::vector<region_triple>&& offsets);
  u32 lookup_address(Game game, Region region, std::string_view name) const;
  u32 lookup_dynamic_address(Game game, Region region, std::string_view name) const;

private:
  struct DynamicVariable {
    DynamicVariable(std::string&& source, std::vector<region_triple>&& offsets)
      : source_var(std::move(source)), source_var_dynamic(false), offset_list(std::move(offsets)) {}

    std::string source_var;
    mutable bool source_var_dynamic;
    std::vector<region_triple> offset_list;
  };

  using var_map = std::map<std::string, region_triple, std::less<>>;
  using dyn_var_map = std::map<std::string, DynamicVariable, std::less<>>;
  using game_map = std::map<Game, var_map>;

  std::map<Game, var_map> addr_mapping;
  std::map<Game, dyn_var_map> dyn_addr_mapping;
};

}
