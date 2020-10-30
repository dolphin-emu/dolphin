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

// Lua: read_u8(number address) -> (number)
static int read_u8(lua_State* L)
{
  std::optional<u32> addr_arg = luaL_checkaddress(L, 1);
  lua_Integer result = 0;
  if (addr_arg)
    result = static_cast<lua_Integer>(PowerPC::HostRead_U8(*addr_arg));
  lua_pushinteger(L, result);
  return 1;
}

// Lua: read_u16(number address) -> (number)
static int read_u16(lua_State* L)
{
  std::optional<u32> addr_arg = luaL_checkaddress(L, 1);
  lua_Integer result = 0;
  if (addr_arg)
    result = static_cast<lua_Integer>(PowerPC::HostRead_U16(*addr_arg));
  lua_pushinteger(L, result);
  return 1;
}

// Lua: read_u32(number address) -> (number)
static int read_u32(lua_State* L)
{
  std::optional<u32> addr_arg = luaL_checkaddress(L, 1);
  lua_Integer result = 0;
  if (addr_arg)
    result = static_cast<lua_Integer>(PowerPC::HostRead_U32(*addr_arg));
  lua_pushinteger(L, result);
  return 1;
}

// Lua: read_f32(number address) -> (number)
static int read_f32(lua_State* L)
{
  std::optional<u32> addr_arg = luaL_checkaddress(L, 1);
  lua_Number result = 0;
  if (addr_arg)
    result = static_cast<lua_Number>(PowerPC::HostRead_F32(*addr_arg));
  lua_pushnumber(L, result);
  return 1;
}

// Lua: read_f64(number address) -> (number)
static int read_f64(lua_State* L)
{
  std::optional<u32> addr_arg = luaL_checkaddress(L, 1);
  lua_Number result = 0;
  if (addr_arg)
    result = PowerPC::HostRead_F64(*addr_arg);
  lua_pushnumber(L, result);
  return 1;
}

// TODO Use metatable
// Lua: get_gpr(number idx) -> (number)
// Returns a PowerPC GPR (general purpose register) with the specified index.
static int get_gpr(lua_State* L)
{
  lua_Integer idx_arg = luaL_checkinteger(L, 1);
  u32 val = 0;
  if (idx_arg >= 0 && idx_arg < 32)
    val = PowerPC::ppcState.gpr[idx_arg];
  lua_pushinteger(L, static_cast<lua_Integer>(val));
  return 1;
}

// TODO Use metatable
// Lua: set_gpr(number idx, number value) -> (number)
// Sets a PowerPC GPR (general purpose register) to the specified value.
static int set_gpr(lua_State* L)
{
  lua_Integer idx_arg = luaL_checkinteger(L, 1);
  lua_Integer val_arg = luaL_checkinteger(L, 2);
  if (idx_arg >= 0 && idx_arg < 32)
    PowerPC::ppcState.gpr[idx_arg] = static_cast<u32>(val_arg);
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
    DOLPHIN_LUA_METHOD(read_u8),
    DOLPHIN_LUA_METHOD(read_u16),
    DOLPHIN_LUA_METHOD(read_u32),
    DOLPHIN_LUA_METHOD(read_f32),
    DOLPHIN_LUA_METHOD(read_f64),
    // Register access
    // TODO Write methods
    DOLPHIN_LUA_METHOD(get_gpr),
    DOLPHIN_LUA_METHOD(set_gpr),
    {nullptr, nullptr}
};
// clang-format on

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
  luaL_openlib(L, "dolphin", Lua::dolphinlib, 0);
  luaL_openlibs(L);
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
