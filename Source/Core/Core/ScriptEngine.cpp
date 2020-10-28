// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

// ScriptEngine
// Expands the emulator using LuaJIT scripts.

#include "Core/ScriptEngine.h"

#include <limits>
#include <vector>

#include <lua.hpp>

#include "Common/Assert.h"

#include "Core/PowerPC/MMU.h"
#include "Core/PowerPC/PowerPC.h"

namespace ScriptEngine
{
namespace lua
{
static lua_State* s_L = nullptr;
static const char* s_hooks_table = "_dolphin_hooks";

// TODO Register panic error handler
// TODO Support multiple scripts

static std::optional<u32> luaL_checkaddress(lua_State* L, int numIdx)
{
  lua_Integer addr_arg = luaL_checkinteger(L, numIdx);
  if (addr_arg > 0 && addr_arg < std::numeric_limits<u32>::max())
    return static_cast<u32>(addr_arg);
  PanicAlert("Unable to resolve read address %lx (Lua)", addr_arg);
  return std::nullopt;
}

static int set_hook(lua_State* L)
{
  std::optional<u32> addr_arg = luaL_checkaddress(L, 1);
  luaL_checkany(L, 2);  // hook function
  if (!addr_arg.has_value())
    return 0;
  u32 addr = *addr_arg;

  // Remember to execute Lua function at address.
  // TODO Support multiple hooks at same address.
  // hooks[addr] = func
  lua_getglobal(L, s_hooks_table);  // map
  const int table = lua_gettop(L);
  lua_pushvalue(L, 1);  // map key (address)
  lua_pushvalue(L, 2);  // map value (function)
  lua_settable(L, table);

  // Instruct PowerPC to break
  PowerPC::script_breakpoints.Add(addr, 0);

  return 0;
}

static void invoke_hook(u32 addr)
{
  lua_State* L = s_L;
  int frame = lua_gettop(L);

  // Look up Lua function at address.
  // func = hooks[addr]
  lua_getglobal(L, s_hooks_table);  // map
  const int table = lua_gettop(L);
  lua_pushinteger(L, static_cast<lua_Integer>(addr));
  lua_gettable(L, table);

  // Call function. (Should always evaluate to true)
  if (lua_isfunction(L, -1))
  {
    int err = lua_pcall(L, 0, 0, 0);
    if (err != 0)
    {
      const char* error_str = lua_tostring(lua::s_L, -1);
      PanicAlert("Error running Lua hook at %x: %s", addr, error_str);
    }
  }

  lua_settop(L, frame);
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
    {"set_hook", set_hook},
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

}  // namespace lua

static std::vector<Script> s_scripts;

void LoadScripts()
{
  // TODO Load from ini file
  s_scripts.push_back({.file_path = "/home/richie/opt/dolphin/scripts/test.lua", .active = true});
  // Create fresh Lua state.
  if (lua::s_L != nullptr)
  {
    lua_close(lua::s_L);
    lua::s_L = nullptr;
  }
  lua::s_L = luaL_newstate();
  // New empty hooks table.
  lua_newtable(lua::s_L);
  lua_setglobal(lua::s_L, lua::s_hooks_table);
  // Open library and scripts.
  luaL_openlib(lua::s_L, "dolphin", lua::dolphinlib, 0);
  luaL_openlibs(lua::s_L);
  for (auto& script : s_scripts)
  {
    int err = luaL_dofile(lua::s_L, script.file_path.c_str());
    if (err != 0)
    {
      const char* error_str = lua_tostring(lua::s_L, -1);
      PanicAlert("Error running Lua script: %s", error_str);
    }
  }
}

void Shutdown()
{
  s_scripts.clear();

  if (lua::s_L != nullptr)
  {
    lua_close(lua::s_L);
    lua::s_L = nullptr;
  }
}

void Reload()
{
  Shutdown();
  LoadScripts();
}

void ExecuteScript(u32 address, u32 _script_id)
{
  lua::invoke_hook(address);
}

}  // namespace ScriptEngine
