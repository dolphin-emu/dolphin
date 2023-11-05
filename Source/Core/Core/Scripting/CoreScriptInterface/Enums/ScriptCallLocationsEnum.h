#pragma once

namespace Scripting
{

#ifdef __cplusplus
extern "C" {
#endif

// ScriptCallLocationsEnum represents the locations that a script can be run from.
// The ScriptUtilities file sets each script to have the appropriate enum value
// before it's run (ex. before a script's OnFrameStart callbacks are run, it has its
// script location variable set equal to FromFrameStartCallback)

// This way, a script which is running can check what context it was called from
// (which is used internally to determine if certain functions are allowed to be
// called, like OnMemoryAddressReadFrom.getMemoryAddressReadFromForCurrentCallback())
enum ScriptCallLocationsEnum
{
  // When a script is run for the very first time.
  FromScriptStartup = 0,

  // When a script executes global code (i.e. not a registered callback function) at the
  // start of a frame, and it isn't the very first time the script was run.
  FromFrameStartGlobalScope = 1,

  // When a registered OnFrameStart callback function is called at the start of a frame.
  FromFrameStartCallback = 2,

  FromInstructionHitCallback = 3,
  FromMemoryAddressReadFromCallback = 4,
  FromMemoryAddressWrittenToCallback = 5,
  FromGCControllerInputPolledCallback = 6,
  FromWiiInputPolledCallback = 7,
  FromGraphicsCallback = 8,
  UnknownLocation = 9
};

#ifdef __cplusplus
}
#endif

}  // namespace Scripting
