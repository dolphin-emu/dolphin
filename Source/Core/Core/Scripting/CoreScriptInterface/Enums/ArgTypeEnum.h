#pragma once

namespace Scripting
{

#ifdef __cplusplus
extern "C" {
#endif

// This enum represents the types of parameters for Scripting API functions, and the return types
// of Scripting API functions (which are both specified in FunctionMetadata definitions).
enum ArgTypeEnum
{
  VoidType = 0,
  Boolean = 1,
  U8 = 2,
  U16 = 3,
  U32 = 4,
  U64 = 5,
  S8 = 6,
  S16 = 7,
  S32 = 8,
  S64 = 9,
  Float = 10,
  Double = 11,
  String = 12,
  VoidPointer = 13,
  ListOfBytes = 14,
  GameCubeControllerStateObject = 15,
  ListOfPoints = 16,
  RegistrationInputType = 17,
  RegistrationWithAutoDeregistrationInputType = 18,
  RegistrationForButtonCallbackInputType = 19,
  UnregistrationInputType = 20,
  RegistrationReturnType = 21,
  YieldType = 22,
  ShutdownType = 23,
  ErrorStringType = 24,
  UnknownArgTypeEnum = 25
};

#ifdef __cplusplus
}
#endif

}  // namespace Scripting
