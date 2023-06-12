#ifndef SCRIPT_CALL_LOCS
#define SCRIPT_CALL_LOCS

#ifdef __cplusplus
extern "C" {
#endif

typedef enum ScriptCallLocations
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
} ScriptCallLocations;

#ifdef __cplusplus
}
#endif

#endif
