#include "Core/PrimeHack/Mods/ElfModLoader.h"

#include "Core/Boot/ElfReader.h"
#include "Core/PowerPC/PPCSymbolDB.h"
#include "Core/PrimeHack/PrimeUtils.h"

#include <filesystem>
#include <fstream>
#include <regex>
#include <vector>

namespace prime {
namespace {
size_t get_type_size(CVarType t) {
  switch (t) {
  case CVarType::INT8:
    return 1;
  case CVarType::INT16:
    return 2;
  case CVarType::INT32:
    return 4;
  case CVarType::INT64:
    return 8;
  case CVarType::FLOAT32:
    return 4;
  case CVarType::FLOAT64:
    return 8;
  default:
    return 0;
  }
}
void write_type(u64 val, u32 addr, CVarType type) {
  switch (type) {
  case CVarType::INT8:
    write8(static_cast<u8>(val), addr);
    break;
  case CVarType::INT16:
    write16(static_cast<u16>(val), addr);
    break;
  case CVarType::INT32:
    write32(static_cast<u32>(val), addr);
    break;
  case CVarType::INT64:
    write64(val, addr);
    break;
  case CVarType::FLOAT32:
    write32(static_cast<u32>(val), addr);
    break;
  case CVarType::FLOAT64:
    write64(val, addr);
    break;
  default:
    break;
  }
}
u64 read_type(u32 addr, CVarType type) {
  switch (type) {
  case CVarType::INT8:
    return static_cast<u64>(read8(addr));
    break;
  case CVarType::INT16:
    return static_cast<u64>(read16(addr));
    break;
  case CVarType::INT32:
    return static_cast<u64>(read32(addr));
    break;
  case CVarType::INT64:
    return read64(addr);
    break;
  case CVarType::FLOAT32:
    return static_cast<u64>(read32(addr));
    break;
  case CVarType::FLOAT64:
    return read64(addr);
    break;
  default:
    return 0;
    break;
  }
}
}

void ElfModLoader::run_mod(Game game, Region region) {
  // ELF is mapped into an extended memory region
  // We would do this with instruction patches but
  // dolphin can't patch fast enough to catch the BATs
  // being assigned!

  switch (game) {
  case Game::PRIME_1_GCN:
    update_bat_regs();
    run_mod_mp1_gc(region);
    break;
  }
}

bool ElfModLoader::init_mod(Game game, Region region) {
  // Mod entry:
  //  add a bit of stack, save LR, forward to mod function, mark "in mod" state
  //  any parameters are implicitly forwarded (*NOT STACK PARAMS*)
  if (game == Game::PRIME_1_GCN && region == Region::NTSC_U) {
    update_bat_regs();
    add_code_change(0x81800000, 0x9421fffc);
    add_code_change(0x81800004, 0x7c0802a6);
    add_code_change(0x81800008, 0x90010008);
    add_code_change(0x8180000c, 0x3d608180);
    add_code_change(0x81800010, 0x39800001);
    add_code_change(0x81800014, 0x918b0038);
    entry_point_index = get_code_changes().size();
    add_code_change(0x81800018, 0x48000005);
    add_code_change(0x8180001c, 0x3d608180);
    add_code_change(0x81800020, 0x39800000);
    add_code_change(0x81800024, 0x918b0038);
    add_code_change(0x81800028, 0x80010008);
    add_code_change(0x8180002c, 0x7c0803a6);
    add_code_change(0x81800030, 0x38210004);
    add_code_change(0x81800034, 0x4e800020);
  }

  return true; 
}

void ElfModLoader::update_bat_regs() {
  bool should_update = !(PowerPC::ppcState.spr[SPR_DBAT2U] & 0x00000100 || PowerPC::ppcState.spr[SPR_IBAT2U] & 0x00000100);
  if (should_update) {
    PowerPC::ppcState.spr[SPR_DBAT2U] |= 0x00000100;
    PowerPC::ppcState.spr[SPR_IBAT2U] |= 0x00000100;

    PowerPC::DBATUpdated();
    PowerPC::IBATUpdated();
  }
}

void ElfModLoader::run_mod_mp1_gc(Region region) {
  if (region != Region::NTSC_U) {
    return;
  }
  
  if (ModPending()) {
    std::string pending_elf = GetPendingModfile();
    ClearPendingModfile();
    parse_and_load_modfile(pending_elf);
  }
}

std::optional<CodeChange> ElfModLoader::parse_code(std::string const& str) {
  std::regex rx("^([0-9A-Fa-f]{8})\\s+([0-9A-Fa-f]{8})\\s*(#.*)?$");
  std::smatch matches;
  if (std::regex_match(str, matches, rx)) {
    if (matches.size() < 3) {
      return std::nullopt;
    }
    std::string addr_string = matches[1], code_string = matches[2];
    u32 address = static_cast<u32>(strtoul(addr_string.c_str(), nullptr, 16));
    u32 code = static_cast<u32>(strtoul(code_string.c_str(), nullptr, 16));
    return std::make_optional<CodeChange>(address, code);
  }
  return std::nullopt;
}

std::optional<CVar> ElfModLoader::parse_cvar(std::string const& str) {
  std::regex rx("^([a-zA-Z_]\\w*)\\s+(i8|i16|i32|i64|f32|f64)\\s*$");
  std::smatch matches;
  if (std::regex_match(str, matches, rx)) {
    if (matches.size() < 3) {
      return std::nullopt;
    }
    std::string type = matches[2];
    CVarType parsed_type;
    if (type == "i8") { parsed_type = CVarType::INT8; }
    else if (type == "i16") { parsed_type = CVarType::INT16; }
    else if (type == "i32") { parsed_type = CVarType::INT32; }
    else if (type == "i64") { parsed_type = CVarType::INT64; }
    else if (type == "f32") { parsed_type = CVarType::FLOAT32; }
    else { parsed_type = CVarType::FLOAT64; } // type == "f64"
    
    return std::make_optional<CVar>(matches[1], 0, parsed_type);
  }
  return std::nullopt;
}

std::optional<std::string> ElfModLoader::parse_elfpath(std::string const& rel_file, std::string const& str) {
  std::filesystem::path search_name(str);
  
  if (search_name.is_absolute() && std::filesystem::exists(str)) {
    return std::make_optional<std::string>(str);
  }
  search_name = (std::filesystem::absolute(rel_file).parent_path()) / str;
  
  if (std::filesystem::exists(search_name)) {
    std::wstring tmp = search_name.native();
    return std::make_optional<std::string>(tmp.begin(), tmp.end());
  }
  return std::nullopt;
}

void ElfModLoader::parse_and_load_modfile(std::string const& path) {
  std::ifstream modfile(path);

  if (!modfile.is_open()) {
    return;
  }
  std::vector<CVar> parsed_cvars;
  std::vector<CodeChange> parsed_changes;
  std::optional<std::string> parsed_elf = std::nullopt;

  while (!modfile.eof()) {
    std::string line;
    std::getline(modfile, line);

    std::regex rx("^\\s*<(\\w+)>\\s*(.*)$");
    std::smatch matches;
    if (std::regex_match(line, matches, rx)) {
      if (matches.size() < 3) {
        continue;
      }

      std::string type = matches[1];
      if (type == "cvar") {
        auto opt = parse_cvar(matches[2]);
        if (opt) {
          parsed_cvars.emplace_back(*opt);
        }
      } else if (type == "change") {
        auto opt = parse_code(matches[2]);
        if (opt) {
          parsed_changes.emplace_back(*opt);
        }
      } else if (type == "elf") {
        parsed_elf = std::move(parse_elfpath(path, matches[2]));
      }
    }
  }
  if (parsed_elf) {
    load_elf(*parsed_elf);
    get_code_changes().insert(get_code_changes().end(), parsed_changes.begin(), parsed_changes.end());
    update_active_changes();

    for (CVar const& cvar : parsed_cvars) {
      Symbol* cvar_sym = g_symbolDB.GetSymbolFromName(cvar.name);
      if (cvar_sym == nullptr) {
        continue;
      }
      cvar_map[cvar.name] = cvar;
      cvar_map[cvar.name].addr = cvar_sym->address;
    }
  }
}

bool ElfModLoader::executing_mod() const {
  return (read32(0x81800038) || (PC >= 0x81800000));
}

void ElfModLoader::load_elf(std::string const& path) {
  ElfReader elf_file(path);
  if (elf_file.IsValid())
  elf_file.LoadIntoMemory(false);
  g_symbolDB.Clear();
  elf_file.LoadSymbols();
  const u32 entry_delta = elf_file.GetEntryPoint() - 0x81800018;
  set_code_change(entry_point_index, 0x48000001 | (entry_delta & 0x3ffffffc));
  write_invalidate(get_code_changes()[entry_point_index].address, get_code_changes()[entry_point_index].var);
}

void ElfModLoader::on_state_change(ModState old_state) {
  if (mod_state() != ModState::ENABLED) {
    get_code_changes().clear();
    init_mod(GetHackManager()->get_active_game(), GetHackManager()->get_active_region());
  }
}

void ElfModLoader::get_cvarlist(std::vector<CVar*>& vars_out) {
  for (auto& entry : cvar_map) {
    vars_out.push_back(&entry.second);
  }
}

bool ElfModLoader::write_cvar(std::string const& name, void* data) {
  auto result = cvar_map.find(name);
  if (result == cvar_map.end()) {
    return false;
  }
  u64 write_data = 0;
  memcpy(&write_data, data, get_type_size(result->second.type));
  write_type(write_data, result->second.addr, result->second.type);
  return true;
}

bool ElfModLoader::get_cvar_val(std::string const& name, void* data_out, size_t out_sz) {
  auto result = cvar_map.find(name);
  if (result == cvar_map.end()) {
    return false;
  }
  u64 data = read_type(result->second.addr, result->second.type);
  memcpy(data_out, &data, std::min(get_type_size(result->second.type), out_sz));
  return true;
}

CVar* ElfModLoader::get_cvar(std::string const& name) {
  auto result = cvar_map.find(name);
  if (result == cvar_map.end()) {
    return nullptr;
  }
  return &result->second;
}

}
