#include "Core/PrimeHack/Mods/ElfModLoader.h"

#include "Common/SymbolDB.h"
#include "Core/Boot/ElfReader.h"
#include "Core/PowerPC/PPCSymbolDB.h"
#include "Core/PrimeHack/PrimeUtils.h"
#include "Core/PowerPC/MMU.h"

#include <filesystem>
#include <fstream>
#include <regex>
#include <vector>

namespace prime {
namespace {
using Symbol = Common::Symbol;

size_t get_type_size(CVarType t) {
  switch (t) {
  case CVarType::BOOLEAN:
  case CVarType::INT8:
    return 1;
  case CVarType::INT16:
    return 2;
  case CVarType::FLOAT32:
  case CVarType::INT32:
    return 4;
  case CVarType::INT64:
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
  case CVarType::BOOLEAN:
    write8(static_cast<bool>(val), addr);
    break;
  default:
    break;
  }
}
u64 read_type(u32 addr, CVarType type) {
  switch (type) {
  case CVarType::INT8:
    return static_cast<u64>(read8(addr));
  case CVarType::INT16:
    return static_cast<u64>(read16(addr));
  case CVarType::INT32:
    return static_cast<u64>(read32(addr));
  case CVarType::INT64:
    return read64(addr);
  case CVarType::FLOAT32:
    return static_cast<u64>(read32(addr));
  case CVarType::FLOAT64:
    return read64(addr);
  case CVarType::BOOLEAN:
    return static_cast<u64>(read8(addr));
  default:
    return 0;
  }
}


// Might be overkill, hopefully more useful as more tags are added
template <typename... Args> struct all_same : std::false_type {};
template <> struct all_same<> : std::true_type {};
template <typename T> struct all_same<T> : std::true_type {};
template <typename T, typename... Ts> struct all_same<T, T, Ts...> : all_same<T, Ts...> {};

template <size_t idx, typename... Ts>
auto fold_all_non_null(std::tuple<Ts...> const& tuple)
    -> std::enable_if_t<all_same<Ts...>::value, bool> {
  if constexpr (idx < sizeof...(Ts)) {
    return std::get<idx>(tuple) != nullptr && fold_all_non_null<idx + 1>(tuple);
  } else {
    return true;
  }
}

template <typename T, typename... Args>
auto resolve_symbols(T&& cb, std::string const& name, Args const&... tnames)
    -> std::enable_if_t<all_same<std::string, Args...>::value, bool> {
  auto symbols = std::make_tuple(g_symbolDB.GetSymbolFromName(name), g_symbolDB.GetSymbolFromName(tnames)...);
  if (fold_all_non_null<0>(symbols)) {
    std::apply(std::forward<T&&>(cb), std::move(symbols));
    return true;
  }
  return false;
}
}

void ElfModLoader::run_mod(Game game, Region region) {
  const auto load_mod = [this] {
    std::string pending_elf = GetPendingModfile();
    ClearPendingModfile();
    parse_and_load_modfile(pending_elf);
    load_state = LoadState::ACTIVE;
  };

  // ELF is mapped into an extended memory region
  // We would do this with instruction patches but
  // dolphin can't patch fast enough to catch the BATs
  // being assigned!
  switch (game) {
  case Game::PRIME_1_GCN:
  case Game::PRIME_2_GCN:
    update_bat_regs();
    
    if (region != Region::NTSC_U) {
      return;
    }

    if (cleanup.shutting_down) {
      // Shutdown has been processed
      if (read32(cleanup.shutdown_signal) == 2) {
        // This should hopefully undo the remaining changes
        set_state(ModState::DISABLED);
        apply_instruction_changes();
        set_state(ModState::ENABLED);
        // To pass the mod load to the next iteration, clear out the
        // mod info from RAM and reset the mod loader's state
        reset_mod();
        cleanup.shutting_down = false;
      }
    } else {
      if (load_state == LoadState::NOT_LOADED) {
        if (ModPending()) {
          load_mod();
        }
      } else if (load_state == LoadState::ACTIVE) {
        if (ModPending()) {
          write32(1, cleanup.shutdown_signal);
          cleanup.shutting_down = true;
        } else if (ModSuspended()) {
          write32(3, cleanup.shutdown_signal);
          load_state = LoadState::SUSPENDED;
        }
      } else if (load_state == LoadState::SUSPENDED) {
        if (ModPending()) {
          write32(1, cleanup.shutdown_signal);
          cleanup.shutting_down = true;
        } else if (!ModSuspended()) {
          write32(0, cleanup.shutdown_signal);
          load_state = LoadState::ACTIVE;
        }
      }
    }

    break;
  }
  if (debug_output_addr != 0) {
    std::string debug_str = PowerPC::HostGetString(debug_output_addr);
    DevInfo("Mod Output", "%s", debug_str.c_str());
  }
}

bool ElfModLoader::init_mod(Game game, Region region) {
  if ((game == Game::PRIME_1_GCN || game == Game::PRIME_2_GCN) && region == Region::NTSC_U) {
    update_bat_regs();
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

void ElfModLoader::get_cvarlist(std::vector<CVar>& vars_out) {
  for (auto& entry : cvar_map) {
    vars_out.push_back(entry.second);
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

void ElfModLoader::load_presets(std::string const& path) {
  std::ifstream preset_file(path);

  if (!preset_file.is_open()) {
    // TODO: log error to user (log & OSD)
    return;
  }

  while (!preset_file.eof()) {
    std::string line;
    std::getline(preset_file, line);

    std::regex rx("^\\s*(\\w+)\\s*=\\s*(.*)$");
    std::smatch matches;
    if (std::regex_match(line, matches, rx)) {
      if (matches.size() < 3) {
        continue;
      }

      std::string var_name = matches[1];
      std::string var_val = matches[2];
      auto var_it = cvar_map.find(var_name);
      if (var_it == cvar_map.end()) {
        continue;
      }

      const u32 var_addr = var_it->second.addr;
      switch (var_it->second.type) {
      case CVarType::BOOLEAN:
        if (var_val == "true") {
          write8(true, var_addr);
        } else if (var_val == "false") {
          write8(false, var_addr);
        }
        break;
      case CVarType::INT8: {
          u8 val = static_cast<u8>(strtoul(var_val.c_str(), nullptr, 10));
          write8(val, var_addr);
        }
        break;
      case CVarType::INT16: {
          u16 val = static_cast<u16>(strtoul(var_val.c_str(), nullptr, 10));
          write16(val, var_addr);
        }
        break;
      case CVarType::INT32: {
          u32 val = strtoul(var_val.c_str(), nullptr, 10);
          write32(val, var_addr);
        }
        break;
      case CVarType::INT64: {
          u64 val = strtoull(var_val.c_str(), nullptr, 10);
          write64(val, var_addr);
        }
        break;
      case CVarType::FLOAT32: {
          float val = strtof(var_val.c_str(), nullptr);
          writef32(val, var_addr);
        }
        break;
      case CVarType::FLOAT64: {
          double val = strtod(var_val.c_str(), nullptr);
          write64(val, *reinterpret_cast<u64*>(&val));
        }
        break;
      }
    }
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
  std::regex rx("^([a-zA-Z_]\\w*)\\s+(i8|i16|i32|i64|f32|f64|bool)\\s*$");
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
    else if (type == "f64") { parsed_type = CVarType::FLOAT64; }
    else { parsed_type = CVarType::BOOLEAN; } // type == "bool"
    
    return std::make_optional<CVar>(matches[1], 0, parsed_type);
  }
  return std::nullopt;
}

std::optional<std::pair<std::string, u32>> ElfModLoader::parse_hook(std::string const& str) {
  std::regex rx("^([a-zA-Z_]\\w*)\\s+([0-9A-Fa-f]{8})\\s*(#.*)?$");
  std::smatch matches;
  if (std::regex_match(str, matches, rx)) {
    if (matches.size() < 3) {
      return std::nullopt;
    }
    std::string replace_fn_name = matches[1], vti_address_string = matches[2];
    u32 vti_address = static_cast<u32>(strtoul(vti_address_string.c_str(), nullptr, 16));
    return std::make_optional<std::pair<std::string, u32>>(replace_fn_name, vti_address);
  }
  return std::nullopt;
}

auto ElfModLoader::parse_callgate(std::string const& str) -> std::optional<callgate_unresolved> {
  // Careful!! MSVC++R has stack-overflowed large regexes before!!
  std::regex rx("^([a-zA-Z_]\\w*)\\s+([a-zA-Z_]\\w*)\\s+([a-zA-Z_]\\w*)\\s+([a-zA-Z_]\\w*)\\s*(#.*)?$");
  std::smatch matches;
  if (std::regex_match(str, matches, rx)) {
    if (matches.size() < 5) {
      return std::nullopt;
    }
    return std::make_optional<callgate_unresolved>(matches[1], matches[2], matches[3], matches[4]);
  }
  return std::nullopt;
}

auto ElfModLoader::parse_cleanup(std::string const& str) -> std::optional<cleanup_unresolved> {
  std::regex rx("^([a-zA-Z_]\\w*)\\s+([a-zA-Z_]\\w*)\\s+([0-9A-Fa-f]{8})\\s*(#.*)?$");
  std::smatch matches;
  if (std::regex_match(str, matches, rx)) {
    if (matches.size() < 4) {
      return std::nullopt;
    }
    std::string cleanup_blhook_address_string = matches[3];
    u32 cleanup_blhook_address = static_cast<u32>(strtoul(cleanup_blhook_address_string.c_str(), nullptr, 16));
    return std::make_optional<cleanup_unresolved>(matches[1], matches[2], cleanup_blhook_address);
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
    auto tmp = search_name.native();
    return std::make_optional<std::string>(tmp.begin(), tmp.end());
  }
  return std::nullopt;
}

void ElfModLoader::parse_and_load_modfile(std::string const& path) {
  std::ifstream modfile(path);

  if (!modfile.is_open()) {
    // TODO: log error to user (log & OSD)
    return;
  }
  std::vector<CVar> parsed_cvars;
  std::vector<CodeChange> parsed_changes;
  std::vector<std::pair<std::string, u32>> parsed_vthooks;
  std::vector<std::pair<std::string, u32>> parsed_blhooks;
  std::vector<std::pair<std::string, u32>> parsed_trampolines;
  std::optional<callgate_unresolved> parsed_callgate = std::nullopt;
  std::optional<cleanup_unresolved> parsed_cleanup = std::nullopt;
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
      } else if (type == "vthook") {
        auto opt = parse_hook(matches[2]);
        if (opt) {
          parsed_vthooks.emplace_back(*opt);
        }
      } else if (type == "blhook") {
        auto opt = parse_hook(matches[2]);
        if (opt) {
          parsed_blhooks.emplace_back(*opt);
        }
      } else if (type == "trampoline") {
        auto opt = parse_hook(matches[2]);
        if (opt) {
          parsed_trampolines.emplace_back(*opt);
        }
      } else if (type == "callgate") {
        parsed_callgate = std::move(parse_callgate(matches[2]));
      } else if (type == "cleanup") {
        parsed_cleanup = std::move(parse_cleanup(matches[2]));
      } else if (type == "elf") {
        parsed_elf = std::move(parse_elfpath(path, matches[2]));
      }
    }
  }

  if (!parsed_elf || !parsed_callgate || !parsed_cleanup) {
    // TODO: log error to user (log & OSD)
    return;
  }

  // ---------------------------- Load & resolve mod required info ---------------------------- //
  if (!load_elf(*parsed_elf)) {
    // TODO: log error to user (log & OSD)
    return;
  }

  bool resolved = resolve_symbols(
                  [this] (Symbol* callgate_fn_sym,
                          Symbol* dispatch_table_sym,
                          Symbol* callgate_dispatch_fn_sym,
                          Symbol* trampoline_restore_table_sym) {
      callgate.callgate_fn_table = callgate_fn_sym->address;
      callgate.dispatch_table = dispatch_table_sym->address;
      callgate.callgate_dispatch_fn = callgate_dispatch_fn_sym->address;
      callgate.trampoline_restore_table = trampoline_restore_table_sym->address;
    },
    std::get<0>(*parsed_callgate),
    std::get<1>(*parsed_callgate),
    std::get<2>(*parsed_callgate),
    std::get<3>(*parsed_callgate));
  if (!resolved) {
    // TODO: log error to user
    return;
  }

  resolved = resolve_symbols([this] (Symbol* release_fn_sym, Symbol* shutdown_signal_sym) {
      cleanup.release_fn = release_fn_sym->address;
      cleanup.shutdown_signal = shutdown_signal_sym->address;
    }, std::get<0>(*parsed_cleanup), std::get<1>(*parsed_cleanup));
  if (!resolved) {
    // TODO: log error to user
    return;
  }
  cleanup.cleanup_blhook_point = std::get<2>(*parsed_cleanup);

  // Inject the cleanup checker into a frequently hit point
  // Eg. somehwere in RsMain
  add_code_change(cleanup.cleanup_blhook_point,
    gen_branch_link(cleanup.cleanup_blhook_point, cleanup.release_fn));


  // ---------------------------- Load & resolve mod optional info ---------------------------- //
  for (CodeChange const& cc : parsed_changes) {
    add_code_change(cc.address, cc.var);
  }

  // TODO: possibly note unfound cvars & hooks (log or OSD)
  for (CVar const& cvar : parsed_cvars) {
    resolve_symbols([this, &cvar] (Symbol* cvar_sym) {
        cvar_map[cvar.name] = cvar;
        cvar_map[cvar.name].addr = cvar_sym->address;
      }, cvar.name);
  }

  // TODO: check if too many callgate table entries
  // should never go over really, but good for clarity
  for (auto&& [hook_fn, hook_addr] : parsed_vthooks) {
    resolve_symbols([this, hook_addr = hook_addr] (Symbol* hook_sym) {
        create_vthook_callgated(hook_sym->address, hook_addr);
      }, hook_fn);
  }

  for (auto&& [hook_fn, hook_addr] : parsed_blhooks) {
    resolve_symbols([this, hook_addr = hook_addr] (Symbol* hook_sym) {
        create_blhook_callgated(hook_sym->address, hook_addr);
      }, hook_fn);
  }

  for (auto&& [hook_fn, hook_addr] : parsed_trampolines) {
    resolve_symbols([this, hook_addr = hook_addr] (Symbol* hook_sym) {
        create_trampoline_callgated(hook_sym->address, hook_addr);
      }, hook_fn);
  }

  debug_output_addr = 0;
  resolve_symbols([this] (Symbol* debug_out_sym) {
      debug_output_addr = debug_out_sym->address;
    }, std::string("debug_output"));
}

bool ElfModLoader::load_elf(std::string const& path) {
  ElfReader elf_file(path);
  
  if (elf_file.IsValid()) {
    elf_file.LoadIntoMemory(false);
    g_symbolDB.Clear();
    elf_file.LoadSymbols();
  }
  return elf_file.IsValid();
}


u32 ElfModLoader::add_callgate_entry(u32 hook_target, u32 original_target) {
  // Fill in the dispatch table with original target and hook target
  const u32 dispatch_loc = 8 * callgate.callgate_count + callgate.dispatch_table;
  const u32 callgate_fn_table_loc = 12 * callgate.callgate_count + callgate.callgate_fn_table;
  add_code_change(dispatch_loc + 0, original_target);
  add_code_change(dispatch_loc + 4, hook_target);

  // Set up the callgate (put the (original,hook) pair into r11, jump to dispatcher)
  const u32 r11_dispatch_lis = gen_lis(11, static_cast<u16>(dispatch_loc >> 16));
  const u32 r11_dispatch_ori = gen_ori(11, 11, static_cast<u16>(dispatch_loc));
  const u32 branch_to_cg_dispatch = gen_branch(callgate_fn_table_loc + 8, callgate.callgate_dispatch_fn);
  add_code_change(callgate_fn_table_loc + 0, r11_dispatch_lis);
  add_code_change(callgate_fn_table_loc + 4, r11_dispatch_ori);
  add_code_change(callgate_fn_table_loc + 8, branch_to_cg_dispatch);
  callgate.callgate_count++;

  return callgate_fn_table_loc;
}

u32 ElfModLoader::add_trampoline_restore_entry(u32 func_start) {
  const u32 trampoline_restore_loc = 8 * callgate.trampoline_count + callgate.trampoline_restore_table;
  const u32 original_instruction = PowerPC::HostRead_Instruction(func_start);
  const u32 branch_to_after_trampoline = gen_branch(trampoline_restore_loc + 4, func_start + 4);
  add_code_change(trampoline_restore_loc + 0, original_instruction);
  add_code_change(trampoline_restore_loc + 4, branch_to_after_trampoline);
  callgate.trampoline_count++;

  return trampoline_restore_loc;
}

void ElfModLoader::create_vthook_callgated(u32 hook_target, u32 vfte_addr) {
  const u32 callgate_fn_table_loc = add_callgate_entry(hook_target, read32(vfte_addr));
  // VTable now redirects to our callgate func, leading to dispatcher
  add_code_change(vfte_addr, callgate_fn_table_loc);
}

void ElfModLoader::create_blhook_callgated(u32 hook_target, u32 bl_addr) {
  const u32 bl_target = bl_addr + get_branch_offset(PowerPC::HostRead_Instruction(bl_addr));
  const u32 callgate_fn_table_loc = add_callgate_entry(hook_target, bl_target);
  // BL will now redirect to the callgate func, leading to the dispatcher
  add_code_change(bl_addr, gen_branch_link(bl_addr, callgate_fn_table_loc));
}

void ElfModLoader::create_trampoline_callgated(u32 hook_target, u32 func_start) {
  const u32 trampoline_restore_loc = add_trampoline_restore_entry(func_start);
  const u32 callgate_fn_table_loc = add_callgate_entry(hook_target, trampoline_restore_loc);
  add_code_change(func_start, gen_branch(func_start, callgate_fn_table_loc));
}

void ElfModLoader::clear_active_mod() {
  callgate = CallgateDefinition{};
  cleanup = CleanupDefinition{};
  cvar_map.clear();
  debug_output_addr = 0;
  load_state = LoadState::NOT_LOADED;
}
}
