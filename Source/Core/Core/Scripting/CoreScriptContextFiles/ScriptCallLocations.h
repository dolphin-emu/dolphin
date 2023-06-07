#ifdef __cplusplus
extern "C" {
#endif

#ifndef SCRIPT_CALL_LOCS
#define SCRIPT_CALL_LOCS

enum ScriptCallLocations
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

#endif

#ifdef __cplusplus
}
#endif
