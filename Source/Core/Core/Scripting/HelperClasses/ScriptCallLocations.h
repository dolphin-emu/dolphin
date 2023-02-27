#ifndef LUA_SCRIPT_CALL_LOCS
#define LUA_SCRIPT_CALL_LOCS

namespace Scripting
{
enum class ScriptCallLocations
{
  FromScriptStartup,
  FromFrameStartGlobalScope,
  FromFrameStartCallback,
  FromInstructionBreakpointCallback,
  FromMemoryAddressReadFromCallback,
  FromMemoryAddressWrittenToCallback,
  FromGCControllerInputPolled
};
}

#endif
