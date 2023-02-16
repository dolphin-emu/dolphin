#ifndef LUA_SCRIPT_CALL_LOCS
#define LUA_SCRIPT_CALL_LOCS

namespace Lua
{
enum class LuaScriptCallLocations
{
  FromScriptStartup,
  FromFrameStartGlobalScope,
  FromFrameStartCallback,
  FromInstructionBreakpointCallback,
  FromMemoryAddressReadFromCallback,
  FromMemoryAddressWrittenToCallback
};
}

#endif
