#ifndef SCRIPT_CALL_LOCS
#define SCRIPT_CALL_LOCS
namespace ScriptingEnums
{
#ifdef __cplusplus
extern "C" {
#endif

enum ScriptCallLocations
{
  FromScriptStartup = 0,
  FromFrameStartGlobalScope = 1,
  FromFrameStartCallback = 2,
  FromInstructionHitCallback = 3,
  FromMemoryAddressReadFromCallback = 4,
  FromMemoryAddressWrittenToCallback = 5,
  FromGCControllerInputPolled = 6,
  FromWiiInputPolled = 7,
  FromGraphicsCallback = 8
};

#ifdef __cplusplus
}
#endif
}  // namespace ScriptingEnums
#endif
