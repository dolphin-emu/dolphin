#include "Core/PrimeHack/AddressDB.h"
#include "Core/PrimeHack/PrimeUtils.h"

namespace prime {

AddressDB::AddressDB() {
  addr_mapping.emplace(Game::PRIME_1, var_map{});
  addr_mapping.emplace(Game::PRIME_2, var_map{});
  addr_mapping.emplace(Game::PRIME_3, var_map{});
  addr_mapping.emplace(Game::PRIME_1_GCN, var_map{});
  addr_mapping.emplace(Game::PRIME_2_GCN, var_map{});
  addr_mapping.emplace(Game::PRIME_3_STANDALONE, var_map{});
  addr_mapping.emplace(Game::PRIME_1_GCN_R1, var_map{});
  addr_mapping.emplace(Game::PRIME_1_GCN_R2, var_map{});
  
  dyn_addr_mapping.emplace(Game::PRIME_1, dyn_var_map{});
  dyn_addr_mapping.emplace(Game::PRIME_2, dyn_var_map{});
  dyn_addr_mapping.emplace(Game::PRIME_3, dyn_var_map{});
  dyn_addr_mapping.emplace(Game::PRIME_1_GCN, dyn_var_map{});
  dyn_addr_mapping.emplace(Game::PRIME_2_GCN, dyn_var_map{});
  dyn_addr_mapping.emplace(Game::PRIME_3_STANDALONE, dyn_var_map{});
  dyn_addr_mapping.emplace(Game::PRIME_1_GCN_R1, dyn_var_map{});
  dyn_addr_mapping.emplace(Game::PRIME_1_GCN_R2, dyn_var_map{});
}

void AddressDB::register_address(Game game, std::string_view name, u32 addr_ntsc_u, u32 addr_pal, u32 addr_ntsc_j) {
  auto r1 = addr_mapping.find(game);
  if (r1 == addr_mapping.end()) {
    return;
  }

  var_map& vars = r1->second;
  auto r2 = vars.find(name);
  if (r2 == vars.end()) {
    vars[std::string(name)] = std::make_tuple(addr_ntsc_u, addr_pal, addr_ntsc_j);
  } else {
    r2->second = std::make_tuple(addr_ntsc_u, addr_pal, addr_ntsc_j);
  }
}

void AddressDB::register_dynamic_address(Game game, std::string_view name, std::string_view source_var,
                                         std::vector<region_triple>&& offsets) {
  auto r1 = dyn_addr_mapping.find(game);
  if (r1 == dyn_addr_mapping.end()) {
    return;
  }

  dyn_var_map& vars = r1->second;
  auto r2 = vars.find(name);
  if (r2 == vars.end()) {
    vars.emplace(std::string(name), DynamicVariable(std::string(source_var), std::move(offsets)));
  }
}

u32 AddressDB::lookup_address(Game game, Region region, std::string_view name) const {
  auto r1 = addr_mapping.find(game);
  if (r1 == addr_mapping.end()) {
    return 0;
  }

  const var_map& vars = r1->second;
  auto r2 = vars.find(name);
  if (r2 == vars.end()) {
    return 0;
  }
  switch (region) {
  case Region::NTSC_U:
    return std::get<0>(r2->second);
  case Region::PAL:
    return std::get<1>(r2->second);
  case Region::NTSC_J:
    return std::get<2>(r2->second);
  default:
    return 0;
  }
}

u32 AddressDB::lookup_dynamic_address(Game game, Region region, std::string_view name) const {
  auto r1 = dyn_addr_mapping.find(game);
  if (r1 == dyn_addr_mapping.end()) {
    return 0;
  }

  const dyn_var_map& vars = r1->second;
  auto r2 = vars.find(name);
  if (r2 == vars.end()) {
    return 0;
  }
  u32 result_addr;
  if (r2->second.source_var_dynamic) {
    result_addr = lookup_dynamic_address(game, region, r2->second.source_var);
    if (result_addr == 0) {
      return 0;
    }
  } else {
    result_addr = lookup_address(game, region, r2->second.source_var);
    if (result_addr == 0) {
      result_addr = lookup_dynamic_address(game, region, r2->second.source_var);
      if (result_addr == 0) {
        return 0;
      }
      r2->second.source_var_dynamic = true;
    }
  }
  int i;
  for (i = 0; i < (r2->second.offset_list.size() - 1); i++) {
    u32 offset;
    switch (region) {
    case Region::NTSC_U:
      offset = std::get<0>(r2->second.offset_list[i]);
      break;
    case Region::PAL:
      offset =  std::get<1>(r2->second.offset_list[i]);
      break;
    case Region::NTSC_J:
      offset = std::get<2>(r2->second.offset_list[i]);
      break;
    default:
      return 0;
    }
    result_addr = read32(result_addr + offset);
    if (!mem_check(result_addr)) {
      return 0;
    }
  }
  if (r2->second.offset_list.size() > 0) {
    switch (region) {
    case Region::NTSC_U:
      return result_addr + std::get<0>(r2->second.offset_list[i]);
      break;
    case Region::PAL:
      return result_addr + std::get<1>(r2->second.offset_list[i]);
      break;
    case Region::NTSC_J:
      return result_addr + std::get<2>(r2->second.offset_list[i]);
      break;
    default:
      return 0;
    }
  }
  return 0;
}

}
