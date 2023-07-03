#ifndef SCRIPT_RETURN_CODES
#define SCRIPT_RETURN_CODES
namespace ScriptingEnums
{
#ifdef __cplusplus
extern "C" {
#endif

enum ScriptReturnCodes
{
  SuccessCode = 0,
  DLLFileNotFoundError = 1,
  DLLComponentNotFoundError = 2,
  ScriptFileNotFoundError = 3,
  UnknownError = 4
};

#ifdef __cplusplus
}
#endif
}  // namespace ScriptingEnums
#endif
