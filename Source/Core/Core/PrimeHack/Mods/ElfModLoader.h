#pragma once

#include <optional>
#include <map>
#include "Core/PrimeHack/PrimeMod.h"

namespace prime {
enum class CVarType {
  INT8, INT16, INT32, INT64, FLOAT32, FLOAT64, BOOLEAN
};
struct CVar {
  std::string name;
  u32 addr;
  CVarType type;

  CVar() = default;
  CVar(std::string&& name, u32 addr, CVarType type) : name(std::forward<std::string>(name)), addr(addr), type(type) {}
};

class ElfModLoader : public PrimeMod {
public:
  void run_mod(Game game, Region region) override;
  bool init_mod(Game game, Region region) override;
  void on_state_change(ModState old_state) override {}
  void on_reset() override { clear_active_mod(); }

  void get_cvarlist(std::vector<CVar>& vars_out);
  bool write_cvar(std::string const& name, void* data);
  bool get_cvar_val(std::string const& name, void* data_out, size_t out_sz);
  CVar* get_cvar(std::string const& name);
  void load_presets(std::string const& path);

private:
  enum class LoadState {
    NOT_LOADED, // Mod hasn't been loaded
    ACTIVE, // Mod was loaded from the UI
    SUSPENDED, // Mod is loaded but inactive
  };

  struct CallgateDefinition {
    u32 callgate_fn_table;
    u32 callgate_dispatch_fn;
    u32 dispatch_table;
    u32 trampoline_restore_table;

    u32 callgate_count = 0;
    u32 trampoline_count = 0;
  } callgate;
  using callgate_unresolved = std::tuple<std::string, std::string, std::string, std::string>;

  struct CleanupDefinition {
    u32 release_fn;
    u32 shutdown_signal;
    u32 cleanup_blhook_point;

    bool shutting_down = false;
  } cleanup;
  using cleanup_unresolved = std::tuple<std::string, std::string, u32>;

  std::map<std::string, CVar> cvar_map;
  u32 debug_output_addr = 0;
  LoadState load_state = LoadState::NOT_LOADED;
  
  void update_bat_regs();
  void parse_and_load_modfile(std::string const& path);

  // OPTIONAL -- can fail parse
  static std::optional<CodeChange> parse_code(std::string const& str);
  static std::optional<CVar> parse_cvar(std::string const& str);
  static std::optional<std::pair<std::string, u32>> parse_hook(std::string const& str);

  // REQUIRED -- must exist, can't fail parse
  static std::optional<callgate_unresolved> parse_callgate(std::string const& str);
  static std::optional<cleanup_unresolved> parse_cleanup(std::string const& str);
  static std::optional<std::string> parse_elfpath(std::string const& rel_file, std::string const& str);

  static bool load_elf(std::string const& path);

  // Sets up a callgate entry, returns the address in the callgate table
  // TODO: multiple hooks to same target --> same callgate location!
  // will cause issue with trampoline
  u32 add_callgate_entry(u32 hook_target, u32 vfte_addr);
  u32 add_trampoline_restore_entry(u32 original_addr);
  void create_vthook_callgated(u32 hook_target, u32 original_addr);
  void create_blhook_callgated(u32 hook_target, u32 bl_addr);
  void create_trampoline_callgated(u32 hook_target, u32 func_start);

  void clear_active_mod();

  // void clear_modinfo_from_ram();
  // void write_modinfo_to_ram();
  // bool read_modinfo_from_ram();

};
}
