#ifdef __cplusplus
extern "C" {
#endif

#ifndef SCRIPT_CALL_LOCS
#define SCRIPT_CALL_LOCS

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

#endif

#ifdef __cplusplus
}
#endif
