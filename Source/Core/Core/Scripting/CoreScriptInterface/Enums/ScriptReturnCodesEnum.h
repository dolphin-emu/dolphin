#pragma once

namespace Scripting
{

#ifdef __cplusplus
extern "C" {
#endif

//  ScriptReturnCodesEnum is used to represent the return value of executing a script.
//  This value for a script will be SuccessCode on success, and not SuccessCode if an error occured.
enum ScriptReturnCodesEnum
{
  // Indicates success (no errors occured)
  SuccessCode = 0,

  // Indicates that the DLL/SO file couldn't be found (or the user doesn't have permission to view
  // that file)
  DLLFileNotFoundError = 1,

  // Indicates that the DLL/SO file didn't define a function which it was required to define
  // (indicates an error for whoever wrote the DLL/SO).
  DLLComponentNotFoundError = 2,

  // Indicates that the script file the user tried to execute did not exist (or the user doesn't
  // have permission to view that file)
  ScriptFileNotFoundError = 3,

  // Indicates that some kind of runtime error happened while the script was executing (usually
  // indicates a bug in the script that the user wrote)
  RunTimeError = 4,

  UnknownError = 5
};

#ifdef __cplusplus
}
#endif

}  // namespace Scripting
