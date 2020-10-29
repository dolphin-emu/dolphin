// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

// ScriptEngine
// Expands the emulator using LuaJIT scripts.

#include "Core/ScriptEngine.h"

#include <limits>
#include <set>
#include <vector>

#include <cassert>
#include <lua.hpp>

#include "Common/Assert.h"
#include "Common/IniFile.h"

#include "Core/ConfigManager.h"
#include "Core/PowerPC/MMU.h"
#include "Core/PowerPC/PowerPC.h"

namespace ScriptEngine
{
// Namespace Lua hosts static binding code.
namespace Lua
{
static const char* s_context_registry_key = "c";

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

static int add_hook(lua_State* L)
{
  std::optional<u32> addr_arg = luaL_checkaddress(L, 1);
  luaL_checkany(L, 2);  // hook function
  if (!addr_arg.has_value())
    return 0;
  if (!lua_isfunction(L, 2))
    return 0;
  u32 addr = *addr_arg;

  // Get reference to Lua function.
  Script::Context* ctx = get_context(L);
  lua_pushvalue(L, 2);
  int lua_ref = luaL_ref(L, LUA_REGISTRYINDEX);
  // Register breakpoint on PowerPC.
  LuaFuncHandle handle(ctx, lua_ref);
  PowerPC::script_hooks.m_hooks.emplace(addr, handle);

  return 0;
}

static int remove_hook(lua_State* L)
{
  // TODO This algorithm is kind of weird.
  // remove_hook takes the same arguments as add_hook, so a Lua function value.
  // However, the PowerPC only stores opaque Lua references.
  // So in order to know which function to remove, we have to check each stored value for equality.

  std::optional<u32> addr_arg = luaL_checkaddress(L, 1);
  luaL_checkany(L, 2);  // hook function
  if (!addr_arg.has_value())
    return 0;
  if (!lua_isfunction(L, 2))
    return 0;
  u32 addr = *addr_arg;

  for (;;)
  {
    auto range = PowerPC::script_hooks.m_hooks.equal_range(addr);
    for (auto it = range.first; it != range.second; it++)
    {
      lua_rawgeti(L, LUA_REGISTRYINDEX, it->second.lua_ref());
      int equal = lua_rawequal(L, 2, -1);
      lua_pop(L, 1);
      if (equal)
      {
        PowerPC::script_hooks.m_hooks.erase(it);
        goto removeNext;
      }
    }
    break;
  removeNext:
    continue;
  }

  // TODO Return flag whether something was removed maybe?
  return 0;
}

static int lua_memcpy(lua_State* L)
{
  std::optional<u32> addr_arg = luaL_checkaddress(L, 1);
  lua_Integer len_arg = luaL_checkinteger(L, 2);

  if (addr_arg)
  {
    size_t len = 0;
    if (len_arg > 0)
      len = static_cast<size_t>(len_arg);
    std::string cpp_str = PowerPC::HostGetString(*addr_arg, len);
    lua_pushlstring(L, cpp_str.c_str(), len);
  }
  else
    lua_pushlstring(L, "", 0);

  return 1;
}

static int lua_strncpy(lua_State* L)
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
    if (chars.empty() || chars.back() != '\0')
      chars.push_back('\0');
    lua_pushlstring(L, chars.data(), chars.size() - 1);
  }
  else
    lua_pushlstring(L, "", 0);

  return 1;
}

static int read_u8(lua_State* L)
{
  std::optional<u32> addr_arg = luaL_checkaddress(L, 1);
  lua_Integer result = 0;
  if (addr_arg)
    result = static_cast<lua_Integer>(PowerPC::HostRead_U8(*addr_arg));
  lua_pushinteger(L, result);
  return 1;
}

static int read_u16(lua_State* L)
{
  std::optional<u32> addr_arg = luaL_checkaddress(L, 1);
  lua_Integer result = 0;
  if (addr_arg)
    result = static_cast<lua_Integer>(PowerPC::HostRead_U16(*addr_arg));
  lua_pushinteger(L, result);
  return 1;
}

static int read_u32(lua_State* L)
{
  std::optional<u32> addr_arg = luaL_checkaddress(L, 1);
  lua_Integer result = 0;
  if (addr_arg)
    result = static_cast<lua_Integer>(PowerPC::HostRead_U32(*addr_arg));
  lua_pushinteger(L, result);
  return 1;
}

static int read_f32(lua_State* L)
{
  std::optional<u32> addr_arg = luaL_checkaddress(L, 1);
  lua_Number result = 0;
  if (addr_arg)
    result = static_cast<lua_Number>(PowerPC::HostRead_F32(*addr_arg));
  lua_pushnumber(L, result);
  return 1;
}

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
    {"add_hook", add_hook},
    {"remove_hook", remove_hook},
    {"memcpy", lua_memcpy},
    {"strncpy", lua_strncpy},
    {"read_u8", read_u8},
    {"read_u16", read_u16},
    {"read_u32", read_u32},
    {"read_f32", read_f32},
    {"read_f64", read_f64},
    {"get_gpr", get_gpr},
    {"set_gpr", set_gpr},
    // TODO Write methods
    {nullptr, nullptr}
};
// clang-format on

}  // namespace Lua

bool ScriptEngine::LuaFuncHandle::Equals(const LuaFuncHandle& other) const
{
  return m_ctx == other.m_ctx && m_lua_ref == other.m_lua_ref;
}

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

void Shutdown()
{
  s_scripts.clear();
}

}  // namespace ScriptEngine
