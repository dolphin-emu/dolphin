#ifndef SCRIPT_CALL_LOCS
#define SCRIPT_CALL_LOCS

namespace Scripting
{
enum class ScriptCallLocations
{
  FromScriptStartup,
  FromFrameStartGlobalScope,
  FromFrameStartCallback,
  FromInstructionHitCallback,
  FromMemoryAddressReadFromCallback,
  FromMemoryAddressWrittenToCallback,
  FromGCControllerInputPolled,
  FromWiiInputPolled,
  FromGraphicsCallback
};
}

#endif
