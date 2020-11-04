// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

// ScriptEngine
// Expands the emulator using Lua scripts.

#include "Core/ScriptEngine.h"

#include <limits>
#include <set>
#include <unordered_map>
#include <vector>

#include <cassert>
#include <lua5.3/lua.hpp>

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
static constexpr char S_CONTEXT_REGISTRY_KEY[] = "c";
static std::unordered_map<const void*, LuaFuncHandle> s_frame_hooks;

// Casts a value on the Lua stack to an u32 and raises a Dolphin alert on dubious integer values.
static std::optional<u32> luaL_checkaddress(lua_State* L, int num_idx)
{
  const lua_Integer addr_arg = luaL_checkinteger(L, num_idx);
  if (addr_arg > 0 && addr_arg < std::numeric_limits<u32>::max())
    return static_cast<u32>(addr_arg);
  PanicAlert("Unable to resolve read address %lx (Lua)", addr_arg);
  return std::nullopt;
}

// Reads a pointer to the current context from the Lua state.
// The provided Lua state must originate from that context.
static Script::Context* get_context(lua_State* L)
{
  lua_pushstring(L, Lua::S_CONTEXT_REGISTRY_KEY);
  lua_gettable(L, LUA_REGISTRYINDEX);
  void* const context_ptr = lua_touserdata(L, -1);
  assert(context_ptr != nullptr);
  lua_pop(L, 1);
  return reinterpret_cast<Script::Context*>(context_ptr);
}

// Lua: hook_instruction(number address, function hook) -> ()
// Registers a PowerPC breakpoint that calls the provided function.
static int hook_instruction(lua_State* L)
{
  const std::optional<u32> addr_arg = luaL_checkaddress(L, 1);
  luaL_checkany(L, 2);  // hook function
  if (!addr_arg.has_value())
    return 0;
  if (!lua_isfunction(L, 2))
    return 0;
  const u32 addr = *addr_arg;

  // Get pointer to Lua function.
  const void* lua_ptr = lua_topointer(L, 2);
  // Skip if function is already registered.
  const auto range = PowerPC::script_hooks.m_hooks.equal_range(addr);
  for (auto it = range.first; it != range.second; it++)
  {
    if (it->second.lua_ptr() == lua_ptr)
      return 0;
  }
  // Get reference to Lua function.
  Script::Context* const ctx = get_context(L);
  lua_pushvalue(L, 2);
  int lua_ref = luaL_ref(L, LUA_REGISTRYINDEX);
  // Register breakpoint on PowerPC.
  const LuaFuncHandle handle(ctx, lua_ptr, lua_ref);
  PowerPC::script_hooks.m_hooks.emplace(addr, handle);

  return 0;
}

// Lua: unhook_instruction(number address, function hook) -> ()
// Removes a hook previously registered with hook_instruction, with the same arguments.
static int unhook_instruction(lua_State* L)
{
  const std::optional<u32> addr_arg = luaL_checkaddress(L, 1);
  luaL_checkany(L, 2);  // hook function
  if (!addr_arg.has_value())
    return 0;
  if (!lua_isfunction(L, 2))
    return 0;
  const u32 addr = *addr_arg;

  // Get pointer to Lua function.
  const void* const lua_ptr = lua_topointer(L, 2);
  // Search for pointer and remove reference.
  const auto range = PowerPC::script_hooks.m_hooks.equal_range(addr);
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
  const void* const lua_ptr = lua_topointer(L, 1);
  // Skip if function is already registered.
  if (s_frame_hooks.count(lua_ptr) > 0)
    return 0;
  // Get reference to Lua function.
  Script::Context* const ctx = get_context(L);
  lua_pushvalue(L, 1);
  const int lua_ref = luaL_ref(L, LUA_REGISTRYINDEX);
  // Register frame hook.
  const LuaFuncHandle handle(ctx, lua_ptr, lua_ref);
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
  const auto entry = s_frame_hooks.find(lua_ptr);
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
  const std::optional<u32> addr_arg = luaL_checkaddress(L, 1);
  const lua_Integer len_arg = luaL_checkinteger(L, 2);

  if (addr_arg)
  {
    size_t len = 0;
    if (len_arg > 0)
      len = static_cast<size_t>(len_arg);
    const std::string cpp_str = PowerPC::HostGetString(*addr_arg, len);
    lua_pushlstring(L, cpp_str.data(), len);
  }
  else
  {
    lua_pushlstring(L, "", 0);
  }

  return 1;
}

// Lua: mem_write(number address, string data) -> ()
// Writes the provided bytes to PowerPC memory at the provided offset.
static int mem_write(lua_State* L)
{
  const std::optional<u32> addr_arg = luaL_checkaddress(L, 1);
  size_t mem_size;
  auto* const mem_ptr = reinterpret_cast<const u8*>(luaL_checklstring(L, 2, &mem_size));
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
  const std::optional<u32> addr_arg = luaL_checkaddress(L, 1);
  const lua_Integer len_arg = luaL_checkinteger(L, 2);

  if (addr_arg)
  {
    size_t len = 0;
    if (len_arg > 0)
      len = static_cast<size_t>(len_arg);
    std::vector<char> chars;
    for (size_t i = 0; i < len; i++)
    {
      const auto c = static_cast<char>(PowerPC::HostRead_U8(*addr_arg + i));
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
  {
    lua_pushlstring(L, "", 0);
  }

  return 1;
}

// Lua: str_write(number address, string data, number max_len) -> ()
// Writes the provided data as a C string to PowerPC memory with a maximum length.
// The resulting string will always be zero-terminated.
static int str_write(lua_State* L)
{
  const std::optional<u32> addr_arg = luaL_checkaddress(L, 1);
  if (!addr_arg.has_value())
    return 0;
  size_t size;
  const char* ptr = luaL_checklstring(L, 2, &size);
  if (ptr == nullptr)
    return 0;
  const lua_Integer max_len_arg = luaL_checkinteger(L, 3);
  if (max_len_arg < 0)
    return 0;
  const auto max_len = static_cast<size_t>(max_len_arg);

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
  const std::optional<u32> addr_arg = luaL_checkaddress(L, 2);
  if (!addr_arg)
    return 0;
  const auto result = static_cast<lua_Integer>(PowerPC::HostRead_U8(*addr_arg));
  lua_pushinteger(L, result);
  return 1;
}

// Lua: read_u16(_, number address) -> (number)
static int read_u16(lua_State* L)
{
  const std::optional<u32> addr_arg = luaL_checkaddress(L, 2);
  if (!addr_arg)
    return 0;
  const auto result = static_cast<lua_Integer>(PowerPC::HostRead_U16(*addr_arg));
  lua_pushinteger(L, result);
  return 1;
}

// Lua: read_u32(_, number address) -> (number)
static int read_u32(lua_State* L)
{
  const std::optional<u32> addr_arg = luaL_checkaddress(L, 2);
  if (!addr_arg)
    return 0;
  const auto result = static_cast<lua_Integer>(PowerPC::HostRead_U32(*addr_arg));
  lua_pushinteger(L, result);
  return 1;
}

// Lua: read_f32(_, number address) -> (number)
static int read_f32(lua_State* L)
{
  const std::optional<u32> addr_arg = luaL_checkaddress(L, 2);
  if (!addr_arg)
    return 0;
  const auto result = static_cast<lua_Number>(PowerPC::HostRead_F32(*addr_arg));
  lua_pushnumber(L, result);
  return 1;
}

// Lua: read_f64(_, number address) -> (number)
static int read_f64(lua_State* L)
{
  const std::optional<u32> addr_arg = luaL_checkaddress(L, 2);
  if (!addr_arg)
    return 0;
  const lua_Number result = PowerPC::HostRead_F64(*addr_arg);
  lua_pushnumber(L, result);
  return 1;
}

// Lua: write_u8(_, number address, number value) -> ()
static int write_u8(lua_State* L)
{
  const std::optional<u32> addr_arg = luaL_checkaddress(L, 2);
  const lua_Integer value = luaL_checkinteger(L, 3);
  if (addr_arg)
    PowerPC::HostWrite_U8(static_cast<u8>(value), *addr_arg);
  return 0;
}

// Lua: write_u16(_, number address, number value) -> ()
static int write_u16(lua_State* L)
{
  const std::optional<u32> addr_arg = luaL_checkaddress(L, 2);
  const lua_Integer value = luaL_checkinteger(L, 3);
  if (addr_arg)
    PowerPC::HostWrite_U16(static_cast<u16>(value), *addr_arg);
  return 0;
}

// Lua: write_u32(_, number address, number value) -> ()
static int write_u32(lua_State* L)
{
  const std::optional<u32> addr_arg = luaL_checkaddress(L, 2);
  const lua_Integer value = luaL_checkinteger(L, 3);
  if (addr_arg)
    PowerPC::HostWrite_U32(static_cast<u32>(value), *addr_arg);
  return 0;
}

// Lua: write_f32(_, number address, number value) -> ()
static int write_f32(lua_State* L)
{
  const std::optional<u32> addr_arg = luaL_checkaddress(L, 2);
  const lua_Number value = luaL_checknumber(L, 3);
  if (addr_arg)
    PowerPC::HostWrite_F32(static_cast<float>(value), *addr_arg);
  return 0;
}

// Lua: write_f64(_, number address, number value) -> ()
static int write_f64(lua_State* L)
{
  const std::optional<u32> addr_arg = luaL_checkaddress(L, 2);
  const lua_Number value = luaL_checknumber(L, 3);
  if (addr_arg)
    PowerPC::HostWrite_F64(value, *addr_arg);
  return 0;
}

// Lua: gpr_get(_, number idx) -> (number)
// Returns a PowerPC GPR (general purpose register) with the specified index.
static int gpr_get(lua_State* L)
{
  const lua_Integer idx_arg = luaL_checkinteger(L, 2);
  assert(std::size(PowerPC::ppcState.gpr) == 32);
  if (idx_arg < 0 || idx_arg >= static_cast<lua_Integer>(std::size(PowerPC::ppcState.gpr)))
    return 0;
  const u32 val = PowerPC::ppcState.gpr[idx_arg];
  lua_pushinteger(L, static_cast<lua_Integer>(val));
  return 1;
}

// Lua: gpr_set(_, number idx, number value) -> (number)
// Sets a PowerPC GPR (general purpose register) to the specified value.
static int gpr_set(lua_State* L)
{
  const lua_Integer idx_arg = luaL_checkinteger(L, 2);
  const lua_Integer val_arg = luaL_checkinteger(L, 3);
  if (idx_arg >= 0 && idx_arg < static_cast<lua_Integer>(std::size(PowerPC::ppcState.gpr)))
    PowerPC::ppcState.gpr[idx_arg] = static_cast<u32>(val_arg);
  return 0;
}

// Lua: sr_get(_, number idx) -> (number)
// Returns a PowerPC SR (segment register) with the specified index.
static int sr_get(lua_State* L)
{
  const lua_Integer idx_arg = luaL_checkinteger(L, 2);
  if (idx_arg < 0 || idx_arg >= static_cast<lua_Integer>(std::size(PowerPC::ppcState.sr)))
    return 0;
  const u32 val = PowerPC::ppcState.sr[idx_arg];
  lua_pushinteger(L, static_cast<lua_Integer>(val));
  return 1;
}

// Lua: sr_set(_, number idx, number value) -> (number)
// Sets a PowerPC SR (segment register) to the specified value.
static int sr_set(lua_State* L)
{
  const lua_Integer idx_arg = luaL_checkinteger(L, 2);
  const lua_Integer val_arg = luaL_checkinteger(L, 3);
  if (idx_arg >= 0 && idx_arg < static_cast<lua_Integer>(std::size(PowerPC::ppcState.sr)))
    PowerPC::ppcState.sr[idx_arg] = static_cast<u32>(val_arg);
  return 0;
}

// Lua: spr_get(_, number idx) -> (number)
// Returns a PowerPC SPR (special purpose) with the specified index.
static int spr_get(lua_State* L)
{
  const lua_Integer idx_arg = luaL_checkinteger(L, 2);
  if (idx_arg < 0 || idx_arg >= static_cast<lua_Integer>(std::size(PowerPC::ppcState.spr)))
    return 0;
  const u32 val = PowerPC::ppcState.spr[idx_arg];
  lua_pushinteger(L, static_cast<lua_Integer>(val));
  return 1;
}

// Lua: spr_set(_, number idx) -> (number)
// Returns a PowerPC SPR (special purpose) with the specified index.
static int spr_set(lua_State* L)
{
  const lua_Integer idx_arg = luaL_checkinteger(L, 2);
  const lua_Integer val_arg = luaL_checkinteger(L, 3);
  if (idx_arg >= 0 && idx_arg < static_cast<lua_Integer>(std::size(PowerPC::ppcState.spr)))
    PowerPC::ppcState.spr[idx_arg] = static_cast<u32>(val_arg);
  return 0;
}

// Lua: get_ps_u32(userdata ps_reg, number idx) -> (number)
// Accesses one of the two elements of a PS (paired single) register in u32 mode.
static int get_ps_u32(lua_State* L)
{
  const u8* idx = reinterpret_cast<const u8*>(lua_touserdata(L, 1));
  const lua_Integer idx_arg = luaL_checkinteger(L, 2);
  PowerPC::PairedSingle& ps = PowerPC::ppcState.ps[*idx];
  u32 value;
  if (idx_arg == 0)
    value = ps.PS0AsU32();
  else if (idx_arg == 1)
    value = ps.PS1AsU32();
  else
    return 0;
  lua_pushinteger(L, static_cast<lua_Integer>(value));
  return 1;
}

// Lua: set_ps_u32(userdata ps_reg, number idx, number value) -> ()
// Sets one of the two elements of a PS (paired single) register in u32 mode.
static int set_ps_u32(lua_State* L)
{
  const u8* idx = reinterpret_cast<const u8*>(lua_touserdata(L, 1));
  const lua_Integer idx_arg = luaL_checkinteger(L, 2);
  const lua_Integer value_arg = luaL_checkinteger(L, 3);
  const u32 value_32 = static_cast<u32>(value_arg);
  const u64 value = static_cast<u64>(value_32);
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
  const lua_Integer idx_arg = luaL_checkinteger(L, 2);
  const PowerPC::PairedSingle& ps = PowerPC::ppcState.ps[*idx];
  double value;
  if (idx_arg == 0)
    value = ps.PS0AsDouble();
  else if (idx_arg == 1)
    value = ps.PS1AsDouble();
  else
    return 0;
  lua_pushnumber(L, value);
  return 1;
}

// Lua: set_ps_f64(userdata ps_reg, number idx, number value) -> ()
// Sets one of the two elements of a PS (paired single) register in f64 mode.
static int set_ps_f64(lua_State* L)
{
  const u8* idx = reinterpret_cast<const u8*>(lua_touserdata(L, 1));
  const lua_Integer idx_arg = luaL_checkinteger(L, 2);
  const lua_Number value = luaL_checkinteger(L, 3);
  PowerPC::PairedSingle& ps = PowerPC::ppcState.ps[*idx];
  if (idx_arg == 0)
    ps.SetPS0(value);
  else if (idx_arg == 1)
    ps.SetPS1(value);
  return 0;
}

// Converts a string with max 8 chars to u64.
static constexpr u64 string_to_u64(const std::string_view& e)
{
  u64 res = 0;
  if (e.length() >= 1)
    res |= (static_cast<u64>(e[0]) << 56u);
  if (e.length() >= 2)
    res |= (static_cast<u64>(e[1]) << 48u);
  if (e.length() >= 3)
    res |= (static_cast<u64>(e[2]) << 40u);
  if (e.length() >= 4)
    res |= (static_cast<u64>(e[3]) << 32u);
  if (e.length() >= 5)
    res |= (static_cast<u64>(e[4]) << 24u);
  if (e.length() >= 6)
    res |= (static_cast<u64>(e[5]) << 16u);
  if (e.length() >= 7)
    res |= (static_cast<u64>(e[6]) << 8u);
  if (e.length() >= 8)
    res |= static_cast<u64>(e[7]);
  if (e.length() >= 9)
    res = 0;
  return res;
}

// Lua: ppc_get(table ppc, string field) -> (any)
// Gets a field or register on the special "ppc" table.
static int ppc_get(lua_State* L)
{
  const char* const field = luaL_checkstring(L, 2);
  const std::string_view field_str(field);
  const u64 field_case = string_to_u64(field_str);
  u32 value = 0;

#define PPC_GET_REGISTER(name, v)                                                                  \
  case string_to_u64(name):                                                                        \
    value = PowerPC::ppcState.v;                                                                   \
    break;

  switch (field_case)
  {
    PPC_GET_REGISTER("pc", pc)
    PPC_GET_REGISTER("npc", npc)
    PPC_GET_REGISTER("xer", spr[SPR_XER])
    PPC_GET_REGISTER("lr", spr[SPR_LR])
    PPC_GET_REGISTER("ctr", spr[SPR_CTR])
    PPC_GET_REGISTER("dsisr", spr[SPR_DSISR])
    PPC_GET_REGISTER("dar", spr[SPR_DAR])
    PPC_GET_REGISTER("dec", spr[SPR_DEC])
    PPC_GET_REGISTER("sdr", spr[SPR_SDR])
    PPC_GET_REGISTER("srr0", spr[SPR_SRR0])
    PPC_GET_REGISTER("srr1", spr[SPR_SRR1])
    PPC_GET_REGISTER("tl", spr[SPR_TL])
    PPC_GET_REGISTER("tu", spr[SPR_TU])
    PPC_GET_REGISTER("tl_w", spr[SPR_TL_W])
    PPC_GET_REGISTER("tu_w", spr[SPR_TU_W])
    PPC_GET_REGISTER("pvr", spr[SPR_PVR])
    PPC_GET_REGISTER("sprg0", spr[SPR_SPRG0])
    PPC_GET_REGISTER("sprg1", spr[SPR_SPRG1])
    PPC_GET_REGISTER("sprg2", spr[SPR_SPRG2])
    PPC_GET_REGISTER("sprg3", spr[SPR_SPRG3])
    PPC_GET_REGISTER("ear", spr[SPR_EAR])
    PPC_GET_REGISTER("ibat0u", spr[SPR_IBAT0U])
    PPC_GET_REGISTER("ibat0l", spr[SPR_IBAT0L])
    PPC_GET_REGISTER("ibat1u", spr[SPR_IBAT1U])
    PPC_GET_REGISTER("ibat1l", spr[SPR_IBAT1L])
    PPC_GET_REGISTER("ibat2u", spr[SPR_IBAT2U])
    PPC_GET_REGISTER("ibat2l", spr[SPR_IBAT2L])
    PPC_GET_REGISTER("ibat3u", spr[SPR_IBAT3U])
    PPC_GET_REGISTER("ibat3l", spr[SPR_IBAT3L])
    PPC_GET_REGISTER("dbat0u", spr[SPR_DBAT0U])
    PPC_GET_REGISTER("dbat0l", spr[SPR_DBAT0L])
    PPC_GET_REGISTER("dbat1u", spr[SPR_DBAT1U])
    PPC_GET_REGISTER("dbat1l", spr[SPR_DBAT1L])
    PPC_GET_REGISTER("dbat2u", spr[SPR_DBAT2U])
    PPC_GET_REGISTER("dbat2l", spr[SPR_DBAT2L])
    PPC_GET_REGISTER("dbat3u", spr[SPR_DBAT3U])
    PPC_GET_REGISTER("dbat3l", spr[SPR_DBAT3L])
    PPC_GET_REGISTER("ibat4u", spr[SPR_IBAT4U])
    PPC_GET_REGISTER("ibat4l", spr[SPR_IBAT4L])
    PPC_GET_REGISTER("ibat5u", spr[SPR_IBAT5U])
    PPC_GET_REGISTER("ibat5l", spr[SPR_IBAT5L])
    PPC_GET_REGISTER("ibat6u", spr[SPR_IBAT6U])
    PPC_GET_REGISTER("ibat6l", spr[SPR_IBAT6L])
    PPC_GET_REGISTER("ibat7u", spr[SPR_IBAT7U])
    PPC_GET_REGISTER("ibat7l", spr[SPR_IBAT7L])
    PPC_GET_REGISTER("dbat4u", spr[SPR_DBAT4U])
    PPC_GET_REGISTER("dbat4l", spr[SPR_DBAT4L])
    PPC_GET_REGISTER("dbat5u", spr[SPR_DBAT5U])
    PPC_GET_REGISTER("dbat5l", spr[SPR_DBAT5L])
    PPC_GET_REGISTER("dbat6u", spr[SPR_DBAT6U])
    PPC_GET_REGISTER("dbat6l", spr[SPR_DBAT6L])
    PPC_GET_REGISTER("dbat7u", spr[SPR_DBAT7U])
    PPC_GET_REGISTER("dbat7l", spr[SPR_DBAT7L])
    PPC_GET_REGISTER("gqr0", spr[SPR_GQR0])
    PPC_GET_REGISTER("hid0", spr[SPR_HID0])
    PPC_GET_REGISTER("hid1", spr[SPR_HID1])
    PPC_GET_REGISTER("hid2", spr[SPR_HID2])
    PPC_GET_REGISTER("hid4", spr[SPR_HID4])
    PPC_GET_REGISTER("wpar", spr[SPR_WPAR])
    PPC_GET_REGISTER("dmau", spr[SPR_DMAU])
    PPC_GET_REGISTER("dmal", spr[SPR_DMAL])
    PPC_GET_REGISTER("ecid_u", spr[SPR_ECID_U])
    PPC_GET_REGISTER("ecid_m", spr[SPR_ECID_M])
    PPC_GET_REGISTER("ecid_l", spr[SPR_ECID_L])
    PPC_GET_REGISTER("l2cr", spr[SPR_L2CR])
    PPC_GET_REGISTER("ummcr0", spr[SPR_UMMCR0])
    PPC_GET_REGISTER("mmcr0", spr[SPR_MMCR0])
    PPC_GET_REGISTER("pmc1", spr[SPR_PMC1])
    PPC_GET_REGISTER("pmc2", spr[SPR_PMC2])
    PPC_GET_REGISTER("ummcr1", spr[SPR_UMMCR1])
    PPC_GET_REGISTER("mmcr1", spr[SPR_MMCR1])
    PPC_GET_REGISTER("pmc3", spr[SPR_PMC3])
    PPC_GET_REGISTER("pmc4", spr[SPR_PMC4])
    PPC_GET_REGISTER("thrm1", spr[SPR_THRM1])
    PPC_GET_REGISTER("thrm2", spr[SPR_THRM2])
    PPC_GET_REGISTER("thrm3", spr[SPR_THRM3])
  default:
    lua_rawget(L, 1);
    return 1 - lua_isnil(L, 1);
  }
#undef PPC_GET_REGISTER

  lua_pushinteger(L, static_cast<lua_Integer>(value));
  return 1;
}

// Lua: ppc_set(table ppc, string field, number value) -> ()
// Sets a register on the special "ppc" table.
static int ppc_set(lua_State* L)
{
  const char* const field = luaL_checkstring(L, 2);
  const lua_Integer value = luaL_checkinteger(L, 3);

#define PPC_SET_REGISTER(name, v)                                                                  \
  case string_to_u64(name):                                                                        \
    PowerPC::ppcState.v = static_cast<u32>(value);                                                 \
    break;

  const std::string_view field_str(field);
  const u64 field_case = string_to_u64(field_str);
  switch (field_case)
  {
    PPC_SET_REGISTER("pc", pc)
    PPC_SET_REGISTER("npc", npc)
    PPC_SET_REGISTER("xer", spr[SPR_XER])
    PPC_SET_REGISTER("lr", spr[SPR_LR])
    PPC_SET_REGISTER("ctr", spr[SPR_CTR])
    PPC_SET_REGISTER("dsisr", spr[SPR_DSISR])
    PPC_SET_REGISTER("dar", spr[SPR_DAR])
    PPC_SET_REGISTER("dec", spr[SPR_DEC])
    PPC_SET_REGISTER("sdr", spr[SPR_SDR])
    PPC_SET_REGISTER("srr0", spr[SPR_SRR0])
    PPC_SET_REGISTER("srr1", spr[SPR_SRR1])
    PPC_SET_REGISTER("tl", spr[SPR_TL])
    PPC_SET_REGISTER("tu", spr[SPR_TU])
    PPC_SET_REGISTER("tl_w", spr[SPR_TL_W])
    PPC_SET_REGISTER("tu_w", spr[SPR_TU_W])
    PPC_SET_REGISTER("pvr", spr[SPR_PVR])
    PPC_SET_REGISTER("sprg0", spr[SPR_SPRG0])
    PPC_SET_REGISTER("sprg1", spr[SPR_SPRG1])
    PPC_SET_REGISTER("sprg2", spr[SPR_SPRG2])
    PPC_SET_REGISTER("sprg3", spr[SPR_SPRG3])
    PPC_SET_REGISTER("ear", spr[SPR_EAR])
    PPC_SET_REGISTER("ibat0u", spr[SPR_IBAT0U])
    PPC_SET_REGISTER("ibat0l", spr[SPR_IBAT0L])
    PPC_SET_REGISTER("ibat1u", spr[SPR_IBAT1U])
    PPC_SET_REGISTER("ibat1l", spr[SPR_IBAT1L])
    PPC_SET_REGISTER("ibat2u", spr[SPR_IBAT2U])
    PPC_SET_REGISTER("ibat2l", spr[SPR_IBAT2L])
    PPC_SET_REGISTER("ibat3u", spr[SPR_IBAT3U])
    PPC_SET_REGISTER("ibat3l", spr[SPR_IBAT3L])
    PPC_SET_REGISTER("dbat0u", spr[SPR_DBAT0U])
    PPC_SET_REGISTER("dbat0l", spr[SPR_DBAT0L])
    PPC_SET_REGISTER("dbat1u", spr[SPR_DBAT1U])
    PPC_SET_REGISTER("dbat1l", spr[SPR_DBAT1L])
    PPC_SET_REGISTER("dbat2u", spr[SPR_DBAT2U])
    PPC_SET_REGISTER("dbat2l", spr[SPR_DBAT2L])
    PPC_SET_REGISTER("dbat3u", spr[SPR_DBAT3U])
    PPC_SET_REGISTER("dbat3l", spr[SPR_DBAT3L])
    PPC_SET_REGISTER("ibat4u", spr[SPR_IBAT4U])
    PPC_SET_REGISTER("ibat4l", spr[SPR_IBAT4L])
    PPC_SET_REGISTER("ibat5u", spr[SPR_IBAT5U])
    PPC_SET_REGISTER("ibat5l", spr[SPR_IBAT5L])
    PPC_SET_REGISTER("ibat6u", spr[SPR_IBAT6U])
    PPC_SET_REGISTER("ibat6l", spr[SPR_IBAT6L])
    PPC_SET_REGISTER("ibat7u", spr[SPR_IBAT7U])
    PPC_SET_REGISTER("ibat7l", spr[SPR_IBAT7L])
    PPC_SET_REGISTER("dbat4u", spr[SPR_DBAT4U])
    PPC_SET_REGISTER("dbat4l", spr[SPR_DBAT4L])
    PPC_SET_REGISTER("dbat5u", spr[SPR_DBAT5U])
    PPC_SET_REGISTER("dbat5l", spr[SPR_DBAT5L])
    PPC_SET_REGISTER("dbat6u", spr[SPR_DBAT6U])
    PPC_SET_REGISTER("dbat6l", spr[SPR_DBAT6L])
    PPC_SET_REGISTER("dbat7u", spr[SPR_DBAT7U])
    PPC_SET_REGISTER("dbat7l", spr[SPR_DBAT7L])
    PPC_SET_REGISTER("gqr0", spr[SPR_GQR0])
    PPC_SET_REGISTER("hid0", spr[SPR_HID0])
    PPC_SET_REGISTER("hid1", spr[SPR_HID1])
    PPC_SET_REGISTER("hid2", spr[SPR_HID2])
    PPC_SET_REGISTER("hid4", spr[SPR_HID4])
    PPC_SET_REGISTER("wpar", spr[SPR_WPAR])
    PPC_SET_REGISTER("dmau", spr[SPR_DMAU])
    PPC_SET_REGISTER("dmal", spr[SPR_DMAL])
    PPC_SET_REGISTER("ecid_u", spr[SPR_ECID_U])
    PPC_SET_REGISTER("ecid_m", spr[SPR_ECID_M])
    PPC_SET_REGISTER("ecid_l", spr[SPR_ECID_L])
    PPC_SET_REGISTER("l2cr", spr[SPR_L2CR])
    PPC_SET_REGISTER("ummcr0", spr[SPR_UMMCR0])
    PPC_SET_REGISTER("mmcr0", spr[SPR_MMCR0])
    PPC_SET_REGISTER("pmc1", spr[SPR_PMC1])
    PPC_SET_REGISTER("pmc2", spr[SPR_PMC2])
    PPC_SET_REGISTER("ummcr1", spr[SPR_UMMCR1])
    PPC_SET_REGISTER("mmcr1", spr[SPR_MMCR1])
    PPC_SET_REGISTER("pmc3", spr[SPR_PMC3])
    PPC_SET_REGISTER("pmc4", spr[SPR_PMC4])
    PPC_SET_REGISTER("thrm1", spr[SPR_THRM1])
    PPC_SET_REGISTER("thrm2", spr[SPR_THRM2])
    PPC_SET_REGISTER("thrm3", spr[SPR_THRM3])
  default:
    break;
  }
#undef PPC_SET_REGISTER

  return 0;
}

// Sets a custom getter and setter to the table or userdata on the top of the stack.
static void new_getset_metatable(lua_State* L, lua_CFunction get, lua_CFunction set)
{
  lua_newtable(L);  // metatable
  // metatable[__index] = get
  lua_pushcfunction(L, get);
  lua_setfield(L, -2, "__index");
  // metatable[__newindex] = set
  lua_pushcfunction(L, set);
  lua_setfield(L, -2, "__newindex");
}

// Registers a new array-like object with a custom get and set
// to the table on the top of the stack.
static void register_getset_object(lua_State* L, const char* name, lua_CFunction get,
                                   lua_CFunction set)
{
  lua_newtable(L);
  new_getset_metatable(L, get, set);
  lua_setmetatable(L, -2);
  lua_setfield(L, -2, name);
}

// Registers a table with 16 userdata representing the PS (paired single) registers.
// Each userdatum is exactly one byte carrying the index of the PS register.
static void register_ps_registers(lua_State* L, const char* name, lua_CFunction get,
                                  lua_CFunction set)
{
  lua_newtable(L);                    // object
  new_getset_metatable(L, get, set);  // metatable for elements
  for (u8 i = 0; i < static_cast<u8>(std::size(PowerPC::ppcState.ps)); i++)
  {
    lua_pushinteger(L, static_cast<lua_Integer>(i));
    u8* const idx = reinterpret_cast<u8*>(lua_newuserdata(L, 1));
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
  const int frame = lua_gettop(L);

  // Build "dolphin object".
  lua_newtable(L);
#define DOLPHIN_LUA_METHOD(m) lua_pushcfunction(L, (m)); lua_setfield(L, -2, (#m));
  // Hooks
  DOLPHIN_LUA_METHOD(hook_instruction);
  DOLPHIN_LUA_METHOD(unhook_instruction);
  DOLPHIN_LUA_METHOD(hook_frame);
  DOLPHIN_LUA_METHOD(unhook_frame);
  // Memory access
  DOLPHIN_LUA_METHOD(mem_read);
  DOLPHIN_LUA_METHOD(mem_write);
  DOLPHIN_LUA_METHOD(str_read);
  DOLPHIN_LUA_METHOD(str_write);
#undef DOLPHIN_LUA_METHOD

  // ppc object
  lua_newtable(L);
  register_getset_object(L, "gpr", gpr_get, gpr_set);
  register_getset_object(L, "sr", sr_get, sr_set);
  register_getset_object(L, "spr", spr_get, spr_set);
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

  lua_setglobal(L, "dolphin");
  assert(lua_gettop(L) == frame);
  luaL_openlibs(L);
}

}  // namespace Lua

// Constructs a script from a target.
Script::Script(std::string file_path) : m_file_path(std::move(file_path))
{
  Script::Load();
}

// Fully unloads a script destroying the context object, references to the script, and the Lua
// state.
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
  lua_State* const L = luaL_newstate();
  Lua::init(L);
  m_ctx = new Context(L);
  fflush(stdout);
  m_ctx->self = m_ctx;  // TODO This looks weird.
  // Write the context reference to Lua.
  lua_pushstring(L, Lua::S_CONTEXT_REGISTRY_KEY);
  lua_pushlightuserdata(L, reinterpret_cast<void*>(m_ctx));
  lua_settable(L, LUA_REGISTRYINDEX);
  // Load script.
  const int err = luaL_dofile(L, m_file_path.c_str());
  if (err != 0)
  {
    const char* const error_str = lua_tostring(L, -1);
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
  {
    if (it->second.ctx() == self)
      hooks.erase(it++);
    else
      ++it;
  }
  lua_close(L);
}

void Script::Context::ExecuteHook(int lua_ref)
{
  const int frame = lua_gettop(L);
  lua_rawgeti(L, LUA_REGISTRYINDEX, lua_ref);
  const int err = lua_pcall(L, 0, 0, 0);
  if (err != 0)
  {
    const char* const error_str = lua_tostring(L, -1);
    PanicAlert("Error running Lua hook: %s", error_str);
  }
  lua_settop(L, frame);
}

static std::vector<Script> s_scripts;

void LoadScriptSection(const std::string& section, std::vector<ScriptTarget>& scripts,
                       IniFile& local_ini)
{
  // Load the name of all enabled patches
  std::vector<std::string> lines;
  std::set<std::string> paths_enabled;
  std::set<std::string> paths_disabled;
  local_ini.GetLines(section, &lines);
  for (const std::string& line : lines)
  {
    if (line.length() < 2 || line[1] != ' ')
      continue;
    std::string file_path = line.substr(2);
    if (line[0] == 'y')
      paths_enabled.insert(std::move(file_path));
    else if (line[0] == 'n')
      paths_disabled.insert(std::move(file_path));
  }
  for (const auto& it : paths_enabled)
    scripts.push_back(ScriptTarget{.file_path = it, .active = true});
  for (const auto& it : paths_disabled)
    scripts.push_back(ScriptTarget{.file_path = it, .active = false});
}

void LoadScripts()
{
  IniFile local_ini = SConfig::GetInstance().LoadLocalGameIni();
  std::vector<ScriptTarget> script_targets;
  LoadScriptSection("Scripts", script_targets, local_ini);

  s_scripts.clear();
  for (const auto& target : script_targets)
  {
    if (target.active)
      s_scripts.emplace_back(Script(target.file_path));
  }
}

void ExecuteFrameHooks()
{
  for (auto& hook : Lua::s_frame_hooks)
    hook.second.Execute();
}

void Shutdown()
{
  Lua::s_frame_hooks.clear();
  s_scripts.clear();
}

}  // namespace ScriptEngine
