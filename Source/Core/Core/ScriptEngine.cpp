// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

// ScriptEngine
// Expands the emulator using LuaJIT scripts.

#include "Core/ScriptEngine.h"

#include <limits>
#include <set>
#include <unordered_map>
#include <vector>

#include <cassert>
#include <lua.hpp>

#include "Common/Assert.h"
#include "Common/IniFile.h"

#include "Core/ConfigManager.h"
#include "Core/PowerPC/MMU.h"
#include "Core/PowerPC/PowerPC.h"

// clang-format off
#define DOLPHIN_LUA_METHOD(x) {.name = #x, .func = (x)}
// clang-format on

namespace ScriptEngine
{
// Namespace Lua hosts static binding code.
namespace Lua
{
static const char* s_context_registry_key = "c";
static std::unordered_map<const void*, LuaFuncHandle> s_frame_hooks;

static std::optional<u32> luaL_checkaddress(lua_State* L, int numIdx)
{
  lua_Integer addr_arg = luaL_checkinteger(L, numIdx);
  if (addr_arg > 0 && addr_arg < std::numeric_limits<u32>::max())
    return static_cast<u32>(addr_arg);
  PanicAlert("Unable to resolve read address %lx (Lua)", addr_arg);
  return std::nullopt;
}

static Script::Context* get_context(lua_State* L)
{
  lua_pushstring(L, Lua::s_context_registry_key);
  lua_gettable(L, LUA_REGISTRYINDEX);
  void* context_ptr = lua_touserdata(L, -1);
  assert(context_ptr != nullptr);
  lua_pop(L, 1);
  return reinterpret_cast<Script::Context*>(context_ptr);
}

// Lua: hook_instruction(number address, function hook) -> ()
// Registers a PowerPC breakpoint that calls the provided function.
static int hook_instruction(lua_State* L)
{
  std::optional<u32> addr_arg = luaL_checkaddress(L, 1);
  luaL_checkany(L, 2);  // hook function
  if (!addr_arg.has_value())
    return 0;
  if (!lua_isfunction(L, 2))
    return 0;
  u32 addr = *addr_arg;

  // Get pointer to Lua function.
  const void* lua_ptr = lua_topointer(L, 2);
  // Skip if function is already registered.
  auto range = PowerPC::script_hooks.m_hooks.equal_range(addr);
  for (auto it = range.first; it != range.second; it++)
    if (it->second.lua_ptr() == lua_ptr)
      return 0;
  // Get reference to Lua function.
  Script::Context* ctx = get_context(L);
  lua_pushvalue(L, 2);
  int lua_ref = luaL_ref(L, LUA_REGISTRYINDEX);
  // Register breakpoint on PowerPC.
  LuaFuncHandle handle(ctx, lua_ptr, lua_ref);
  PowerPC::script_hooks.m_hooks.emplace(addr, handle);

  return 0;
}

// Lua: unhook_instruction(number address, function hook) -> ()
// Removes a hook previously registered with hook_instruction, with the same arguments.
static int unhook_instruction(lua_State* L)
{
  std::optional<u32> addr_arg = luaL_checkaddress(L, 1);
  luaL_checkany(L, 2);  // hook function
  if (!addr_arg.has_value())
    return 0;
  if (!lua_isfunction(L, 2))
    return 0;
  u32 addr = *addr_arg;

  // Get pointer to Lua function.
  const void* lua_ptr = lua_topointer(L, 2);
  // Search for pointer and remove reference.
  auto range = PowerPC::script_hooks.m_hooks.equal_range(addr);
  for (auto it = range.first; it != range.second; it++)
  {
    if (it->second.lua_ptr() == lua_ptr)
    {
      luaL_unref(L, LUA_REGISTRYINDEX, it->second.lua_ref());
      PowerPC::script_hooks.m_hooks.erase(it);
      lua_pushboolean(L, 1);
      return 1;
    }
  }
  // Nothing found.
  lua_pushboolean(L, 0);
  return 1;
}

// Lua: hook_frame(function hook) -> ()
// Registers a function to be called each frame.
static int hook_frame(lua_State* L)
{
  if (!lua_isfunction(L, 1))
    return 0;

  // Get pointer to Lua function.
  const void* lua_ptr = lua_topointer(L, 1);
  // Skip if function is already registered.
  if (s_frame_hooks.count(lua_ptr) > 0)
    return 0;
  // Get reference to Lua function.
  Script::Context* ctx = get_context(L);
  lua_pushvalue(L, 1);
  int lua_ref = luaL_ref(L, LUA_REGISTRYINDEX);
  // Register frame hook.
  LuaFuncHandle handle(ctx, lua_ptr, lua_ref);
  s_frame_hooks.emplace(lua_ptr, handle);

  return 0;
}

// Lua: unhook_frame(function hook) -> ()
// Removes a hook previously registered with unhook_frame, with the same arguments.
static int unhook_frame(lua_State* L)
{
  if (!lua_isfunction(L, 1))
    return 0;

  const void* lua_ptr = lua_topointer(L, 1);
  auto entry = s_frame_hooks.find(lua_ptr);
  if (entry == s_frame_hooks.end())
    return 0;
  // Dereference function.
  luaL_unref(L, LUA_REGISTRYINDEX, entry->second.lua_ref());
  s_frame_hooks.erase(entry);

  return 0;
}

// Lua: mem_read(number address, number len) -> (string)
// Reads bytes from PowerPC memory beginning at the provided offset.
static int mem_read(lua_State* L)
{
  std::optional<u32> addr_arg = luaL_checkaddress(L, 1);
  lua_Integer len_arg = luaL_checkinteger(L, 2);

  if (addr_arg)
  {
    size_t len = 0;
    if (len_arg > 0)
      len = static_cast<size_t>(len_arg);
    std::string cpp_str = PowerPC::HostGetString(*addr_arg, len);
    lua_pushlstring(L, cpp_str.data(), len);
  }
  else
    lua_pushlstring(L, "", 0);

  return 1;
}

// Lua: mem_write(number address, string data) -> ()
// Writes the provided bytes to PowerPC memory at the provided offset.
static int mem_write(lua_State* L)
{
  std::optional<u32> addr_arg = luaL_checkaddress(L, 1);
  size_t mem_size;
  auto* mem_ptr = reinterpret_cast<const u8*>(luaL_checklstring(L, 2, &mem_size));
  if (!addr_arg.has_value())
    return 0;
  if (mem_ptr == nullptr)
    return 0;

  for (size_t i = 0; i < mem_size; i++)
    PowerPC::HostWrite_U8(mem_ptr[i], (*addr_arg) + i);

  return 0;
}

// Lua: str_read(number address, number max_len) -> (string)
// Reads a C string from PowerPC memory with a maximum length.
static int str_read(lua_State* L)
{
  std::optional<u32> addr_arg = luaL_checkaddress(L, 1);
  lua_Integer len_arg = luaL_checkinteger(L, 2);

  if (addr_arg)
  {
    size_t len = 0;
    if (len_arg > 0)
      len = static_cast<size_t>(len_arg);
    std::vector<char> chars;
    for (size_t i = 0; i < len; i++)
    {
      auto c = static_cast<char>(PowerPC::HostRead_U8(*addr_arg + i));
      chars.push_back(c);
      if (c == '\0')
        break;
    }
    if (!chars.empty())
      lua_pushlstring(L, chars.data(), chars.size());
    else
      lua_pushlstring(L, "", 0);
  }
  else
    lua_pushlstring(L, "", 0);

  return 1;
}

// Lua: str_write(number address, string data, number max_len) -> (string)
// Writes the provided data as a C string to PowerPC memory with a maximum length.
// The resulting string will always be zero-terminated.
static int str_write(lua_State* L)
{
  std::optional<u32> addr_arg = luaL_checkaddress(L, 1);
  if (!addr_arg.has_value())
    return 0;
  size_t size;
  const char* ptr = luaL_checklstring(L, 2, &size);
  if (ptr == nullptr)
    return 0;
  lua_Integer max_len_arg = luaL_checkinteger(L, 3);
  if (max_len_arg < 0)
    return 0;
  auto max_len = static_cast<size_t>(max_len_arg);

  if (size <= max_len)
  {
    for (size_t i = 0; i < size; i++)
      PowerPC::HostWrite_U8(ptr[i], (*addr_arg) + i);
  }
  else
  {
    size_t i;
    for (i = 0; i < max_len - 1; i++)
      PowerPC::HostWrite_U8(ptr[i], (*addr_arg) + i);
    PowerPC::HostWrite_U8(0, (*addr_arg) + i);
  }

  return 0;
}

// Lua: read_u8(_, number address) -> (number)
static int read_u8(lua_State* L)
{
  std::optional<u32> addr_arg = luaL_checkaddress(L, 2);
  lua_Integer result = 0;
  if (addr_arg)
    result = static_cast<lua_Integer>(PowerPC::HostRead_U8(*addr_arg));
  lua_pushinteger(L, result);
  return 1;
}

// Lua: read_u16(_, number address) -> (number)
static int read_u16(lua_State* L)
{
  std::optional<u32> addr_arg = luaL_checkaddress(L, 2);
  lua_Integer result = 0;
  if (addr_arg)
    result = static_cast<lua_Integer>(PowerPC::HostRead_U16(*addr_arg));
  lua_pushinteger(L, result);
  return 1;
}

// Lua: read_u32(_, number address) -> (number)
static int read_u32(lua_State* L)
{
  std::optional<u32> addr_arg = luaL_checkaddress(L, 2);
  lua_Integer result = 0;
  if (addr_arg)
    result = static_cast<lua_Integer>(PowerPC::HostRead_U32(*addr_arg));
  lua_pushinteger(L, result);
  return 1;
}

// Lua: read_f32(_, number address) -> (number)
static int read_f32(lua_State* L)
{
  std::optional<u32> addr_arg = luaL_checkaddress(L, 2);
  lua_Number result = 0;
  if (addr_arg)
    result = static_cast<lua_Number>(PowerPC::HostRead_F32(*addr_arg));
  lua_pushnumber(L, result);
  return 1;
}

// Lua: read_f64(_, number address) -> (number)
static int read_f64(lua_State* L)
{
  std::optional<u32> addr_arg = luaL_checkaddress(L, 2);
  lua_Number result = 0;
  if (addr_arg)
    result = PowerPC::HostRead_F64(*addr_arg);
  lua_pushnumber(L, result);
  return 1;
}

// Lua: write_u8(_, number address, number value) -> ()
static int write_u8(lua_State* L)
{
  std::optional<u32> addr_arg = luaL_checkaddress(L, 2);
  lua_Integer value = luaL_checkinteger(L, 3);
  if (addr_arg)
    PowerPC::HostWrite_U8(static_cast<u8>(value), *addr_arg);
  return 0;
}

// Lua: write_u16(_, number address, number value) -> ()
static int write_u16(lua_State* L)
{
  std::optional<u32> addr_arg = luaL_checkaddress(L, 2);
  lua_Integer value = luaL_checkinteger(L, 3);
  if (addr_arg)
    PowerPC::HostWrite_U16(static_cast<u16>(value), *addr_arg);
  return 0;
}

// Lua: write_u32(_, number address, number value) -> ()
static int write_u32(lua_State* L)
{
  std::optional<u32> addr_arg = luaL_checkaddress(L, 2);
  lua_Integer value = luaL_checkinteger(L, 3);
  if (addr_arg)
    PowerPC::HostWrite_U32(static_cast<u32>(value), *addr_arg);
  return 0;
}

// Lua: write_f32(_, number address, number value) -> ()
static int write_f32(lua_State* L)
{
  std::optional<u32> addr_arg = luaL_checkaddress(L, 2);
  lua_Number value = luaL_checknumber(L, 3);
  if (addr_arg)
    PowerPC::HostWrite_F32(static_cast<float>(value), *addr_arg);
  return 0;
}

// Lua: write_f64(_, number address, number value) -> ()
static int write_f64(lua_State* L)
{
  std::optional<u32> addr_arg = luaL_checkaddress(L, 2);
  lua_Number value = luaL_checknumber(L, 3);
  if (addr_arg)
    PowerPC::HostWrite_F64(value, *addr_arg);
  return 0;
}

// Lua: get_gpr(_, number idx) -> (number)
// Returns a PowerPC GPR (general purpose register) with the specified index.
static int get_gpr(lua_State* L)
{
  lua_Integer idx_arg = luaL_checkinteger(L, 2);
  u32 val = 0;
  if (idx_arg >= 0 && idx_arg < 32)
    val = PowerPC::ppcState.gpr[idx_arg];
  lua_pushinteger(L, static_cast<lua_Integer>(val));
  return 1;
}

// Lua: set_gpr(_, number idx, number value) -> (number)
// Sets a PowerPC GPR (general purpose register) to the specified value.
static int set_gpr(lua_State* L)
{
  lua_Integer idx_arg = luaL_checkinteger(L, 2);
  lua_Integer val_arg = luaL_checkinteger(L, 3);
  if (idx_arg >= 0 && idx_arg < 32)
    PowerPC::ppcState.gpr[idx_arg] = static_cast<u32>(val_arg);
  return 0;
}

// Lua: get_sr(_, number idx) -> (number)
// Returns a PowerPC SR (segment register) with the specified index.
static int get_sr(lua_State* L)
{
  lua_Integer idx_arg = luaL_checkinteger(L, 2);
  u32 val = 0;
  if (idx_arg >= 0 && idx_arg < 16)
    val = PowerPC::ppcState.sr[idx_arg];
  lua_pushinteger(L, static_cast<lua_Integer>(val));
  return 1;
}

// Lua: set_sr(_, number idx, number value) -> (number)
// Sets a PowerPC SR (segment register) to the specified value.
static int set_sr(lua_State* L)
{
  lua_Integer idx_arg = luaL_checkinteger(L, 2);
  lua_Integer val_arg = luaL_checkinteger(L, 3);
  if (idx_arg >= 0 && idx_arg < 16)
    PowerPC::ppcState.sr[idx_arg] = static_cast<u32>(val_arg);
  return 0;
}

// Lua: get_spr(_, number idx) -> (number)
// Returns a PowerPC SPR (special purpose) with the specified index.
static int get_spr(lua_State* L)
{
  lua_Integer idx_arg = luaL_checkinteger(L, 2);
  u32 val = 0;
  if (idx_arg >= 0 && idx_arg < 1024)
    val = PowerPC::ppcState.spr[idx_arg];
  lua_pushinteger(L, static_cast<lua_Integer>(val));
  return 1;
}

// Lua: set_spr(_, number idx) -> (number)
// Returns a PowerPC SPR (special purpose) with the specified index.
static int set_spr(lua_State* L)
{
  lua_Integer idx_arg = luaL_checkinteger(L, 2);
  lua_Integer val_arg = luaL_checkinteger(L, 3);
  if (idx_arg >= 0 && idx_arg < 1024)
    PowerPC::ppcState.spr[idx_arg] = static_cast<u32>(val_arg);
  return 0;
}

// Lua: get_ps_u32(userdata ps_reg, number idx) -> (number)
// Accesses one of the two elements of a PS (paired single) register in u32 mode.
static int get_ps_u32(lua_State* L)
{
  const u8* idx = reinterpret_cast<const u8*>(lua_touserdata(L, 1));
  lua_Integer idx_arg = luaL_checkinteger(L, 2);
  PowerPC::PairedSingle& ps = PowerPC::ppcState.ps[*idx];
  u32 value = 0;
  if (idx_arg == 0)
    value = ps.PS0AsU32();
  else if (idx_arg == 1)
    value = ps.PS1AsU32();
  lua_pushinteger(L, static_cast<lua_Integer>(value));
  return 1;
}

// Lua: set_ps_u32(userdata ps_reg, number idx, number value) -> ()
// Sets one of the two elements of a PS (paired single) register in u32 mode.
static int set_ps_u32(lua_State* L)
{
  const u8* idx = reinterpret_cast<const u8*>(lua_touserdata(L, 1));
  lua_Integer idx_arg = luaL_checkinteger(L, 2);
  lua_Integer value_arg = luaL_checkinteger(L, 3);
  u32 value_32 = static_cast<u32>(value_arg);
  u64 value = static_cast<u64>(value_32);
  PowerPC::PairedSingle& ps = PowerPC::ppcState.ps[*idx];
  if (idx_arg == 0)
    ps.SetPS0(value);
  else if (idx_arg == 1)
    ps.SetPS1(value);
  return 0;
}

// Lua: get_ps_f64(userdata ps_reg, number idx) -> (number)
// Accesses one of the two elements of a PS (paired single) register in f64 mode.
static int get_ps_f64(lua_State* L)
{
  const u8* idx = reinterpret_cast<const u8*>(lua_touserdata(L, 1));
  lua_Integer idx_arg = luaL_checkinteger(L, 2);
  PowerPC::PairedSingle& ps = PowerPC::ppcState.ps[*idx];
  double value = 0;
  if (idx_arg == 0)
    value = ps.PS0AsDouble();
  else if (idx_arg == 1)
    value = ps.PS1AsDouble();
  lua_pushnumber(L, value);
  return 1;
}

// Lua: set_ps_f64(userdata ps_reg, number idx, number value) -> ()
// Sets one of the two elements of a PS (paired single) register in f64 mode.
static int set_ps_f64(lua_State* L)
{
  const u8* idx = reinterpret_cast<const u8*>(lua_touserdata(L, 1));
  lua_Integer idx_arg = luaL_checkinteger(L, 2);
  lua_Number value = luaL_checkinteger(L, 3);
  PowerPC::PairedSingle& ps = PowerPC::ppcState.ps[*idx];
  if (idx_arg == 0)
    ps.SetPS0(value);
  else if (idx_arg == 1)
    ps.SetPS1(value);
  return 0;
}

// Lua: ppc_get(table ppc, string field) -> (any)
// Gets a field or register on the special "ppc" table.
static inline int ppc_get(lua_State* L)
{
  const char* field = luaL_checkstring(L, 2);
  if (std::strcmp(field, "pc") == 0)
    lua_pushinteger(L, static_cast<lua_Integer>(PowerPC::ppcState.pc));
  else if (std::strcmp(field, "npc") == 0)
    lua_pushinteger(L, static_cast<lua_Integer>(PowerPC::ppcState.npc));
  else
    lua_rawget(L, 1);
  return 1;
}

// Lua: ppc_set(table ppc, string field, number value) -> ()
// Sets a register on the special "ppc" table.
static inline int ppc_set(lua_State* L)
{
  const char* field = luaL_checkstring(L, 2);
  lua_Integer value = luaL_checkinteger(L, 3);
  if (std::strcmp(field, "pc") == 0)
    PowerPC::ppcState.pc = static_cast<u32>(value);
  if (std::strcmp(field, "npc") == 0)
    PowerPC::ppcState.npc = static_cast<u32>(value);
  return 0;
}

// clang-format off
static const luaL_Reg dolphinlib[] = {
    // Hooks
    DOLPHIN_LUA_METHOD(hook_instruction),
    DOLPHIN_LUA_METHOD(unhook_instruction),
    DOLPHIN_LUA_METHOD(hook_frame),
    DOLPHIN_LUA_METHOD(unhook_frame),
    // Memory access
    DOLPHIN_LUA_METHOD(mem_read),
    DOLPHIN_LUA_METHOD(mem_write),
    DOLPHIN_LUA_METHOD(str_read),
    DOLPHIN_LUA_METHOD(str_write),
    {nullptr, nullptr}
};
// clang-format on

// Sets a custom getter and setter to the table or userdata on the top of the stack.
static inline void new_getset_metatable(lua_State* L, lua_CFunction get, lua_CFunction set)
{
  lua_newtable(L);  // metatable
  // metatable[__index] = get
  lua_pushcfunction(L, get);
  lua_setfield(L, -2, "__index");
  // metatable[__newindex] = set
  lua_pushcfunction(L, set);
  lua_setfield(L, -2, "__newindex");
}

// Registers a new array-like object with a custom getter and setter
// to the table on the top of the stack.
static inline void register_getset_object(lua_State* L, const char* name, lua_CFunction get,
                                          lua_CFunction set)
{
  lua_newtable(L);
  new_getset_metatable(L, get, set);
  lua_setmetatable(L, -2);
  lua_setfield(L, -2, name);
}

// Registers a table with 16 userdatas representing the PS (paired single) registers.
static inline void register_ps_registers(lua_State* L, const char* name, lua_CFunction get,
                                         lua_CFunction set)
{
  lua_newtable(L);                    // object
  new_getset_metatable(L, get, set);  // metatable for elements
  for (u8 i = 0; i < 16; i++)
  {
    lua_pushinteger(L, static_cast<lua_Integer>(i));
    u8* idx = reinterpret_cast<u8*>(lua_newuserdata(L, 1));
    *idx = i;
    lua_pushvalue(L, -3);
    lua_setmetatable(L, -2);  // set metatable on userdata
    lua_settable(L, -4);
  }
  lua_pop(L, 1);  // metatable of elements
  lua_setfield(L, -2, name);
}

static void init(lua_State* L)
{
  luaL_openlib(L, "dolphin", Lua::dolphinlib, 0);  // TODO Don't use openlib
  int frame = lua_gettop(L);
  lua_getglobal(L, "dolphin");

  // ppc object
  lua_newtable(L);
  register_getset_object(L, "gpr", get_gpr, set_gpr);
  register_getset_object(L, "sr", get_sr, set_sr);
  register_getset_object(L, "spr", get_spr, set_spr);
  register_ps_registers(L, "ps_u32", get_ps_u32, set_ps_u32);
  register_ps_registers(L, "ps_f64", get_ps_f64, set_ps_f64);
  new_getset_metatable(L, ppc_get, ppc_set);
  lua_setmetatable(L, -2);

  // dolphin["ppc"] = ppc
  lua_setfield(L, -2, "ppc");

  // memory access
  register_getset_object(L, "mem_u8", read_u8, write_u8);
  register_getset_object(L, "mem_u16", read_u16, write_u16);
  register_getset_object(L, "mem_u32", read_u32, write_u32);
  register_getset_object(L, "mem_f32", read_f32, write_f32);
  register_getset_object(L, "mem_f64", read_f64, write_f64);

  lua_pop(L, 1);
  assert(lua_gettop(L) == frame);
  luaL_openlibs(L);
}

}  // namespace Lua

Script::Script(const ScriptTarget& other) : m_file_path(other.file_path)
{
  Script::Load();
}

Script::~Script()
{
  Unload();
}

Script::Script(Script&& other) noexcept : m_ctx(other.m_ctx)
{
  other.m_ctx = nullptr;
}

void Script::Load()
{
  if (m_ctx != nullptr)
    return;
  // Create Lua state.
  lua_State* L = luaL_newstate();
  Lua::init(L);
  // TODO Register panic error handler
  m_ctx = new Context(L);
  fflush(stdout);
  m_ctx->self = m_ctx;  // TODO This looks weird.
  // Write the context reference to Lua.
  lua_pushstring(L, Lua::s_context_registry_key);
  lua_pushlightuserdata(L, reinterpret_cast<void*>(m_ctx));
  lua_settable(L, LUA_REGISTRYINDEX);
  // Load script.
  int err = luaL_dofile(L, m_file_path.c_str());
  if (err != 0)
  {
    const char* error_str = lua_tostring(L, -1);
    PanicAlert("Error running Lua script: %s", error_str);
    Unload();
    return;
  }
}

void Script::Unload()
{
  fflush(stdout);
  delete m_ctx;
  m_ctx = nullptr;
}

Script::Context::~Context()
{
  auto& hooks = PowerPC::script_hooks.m_hooks;
  for (auto it = hooks.cbegin(); it != hooks.cend();)
    if (it->second.ctx() == self)
      hooks.erase(it++);
    else
      ++it;
  lua_close(L);
}

void Script::Context::ExecuteHook(int lua_ref)
{
  int frame = lua_gettop(L);
  lua_rawgeti(L, LUA_REGISTRYINDEX, lua_ref);
  int err = lua_pcall(L, 0, 0, 0);
  if (err != 0)
  {
    const char* error_str = lua_tostring(L, -1);
    PanicAlert("Error running Lua hook: %s", error_str);
  }
  lua_settop(L, frame);
}

static std::vector<Script> s_scripts;

void LoadScriptSection(const std::string& section, std::vector<ScriptTarget>& scripts,
                       IniFile& localIni)
{
  // Load the name of all enabled patches
  std::vector<std::string> lines;
  std::set<std::string> pathsEnabled;
  std::set<std::string> pathsDisabled;
  localIni.GetLines(section, &lines);
  for (const std::string& line : lines)
  {
    if (line.length() < 2 || line[1] != ' ')
      continue;
    std::string file_path = line.substr(2);
    if (line[0] == 'y')
      pathsEnabled.insert(file_path);
    else if (line[0] == 'n')
      pathsDisabled.insert(file_path);
  }
  for (const auto& it : pathsEnabled)
    scripts.push_back(ScriptTarget{.file_path = it, .active = true});
  for (const auto& it : pathsDisabled)
    scripts.push_back(ScriptTarget{.file_path = it, .active = false});
}

void LoadScripts()
{
  IniFile localIni = SConfig::GetInstance().LoadLocalGameIni();
  std::vector<ScriptTarget> scriptTargets;
  LoadScriptSection("Scripts", scriptTargets, localIni);

  s_scripts.clear();
  for (auto& target : scriptTargets)
    s_scripts.emplace_back(target);
}

void ExecuteFrameHooks()
{
  for (auto& hook : Lua::s_frame_hooks)
    hook.second.Execute();
}

void Shutdown()
{
  s_scripts.clear();
}

}  // namespace ScriptEngine
