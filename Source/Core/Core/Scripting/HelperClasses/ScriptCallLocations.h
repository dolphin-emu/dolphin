#ifndef SCRIPT_CALL_LOCS
#define SCRIPT_CALL_LOCS

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
  FromGCControllerInputPolled,
  FromWiiInputPolled
};
}

#endif
