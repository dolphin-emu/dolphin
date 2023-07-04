#pragma once
#ifndef ARG_TYPE_ENUM
#define ARG_TYPE_ENUM
namespace ScriptingEnums
{
#ifdef __cplusplus
extern "C" {
#endif
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
  AddressToByteMap = 14,
  ControllerStateObject = 15,
  ListOfPoints = 16,
  ErrorStringType = 17,
  YieldType = 18,
  RegistrationInputType = 19,
  RegistrationWithAutoDeregistrationInputType = 20,
  RegistrationForButtonCallbackInputType = 21,
  UnregistrationInputType = 22,
  RegistrationReturnType = 23,
  RegistrationWithAutoDeregistrationReturnType = 24,
  UnregistrationReturnType = 25,
  ShutdownType = 26,
  UnknownArgTypeEnum = 27
};
#ifdef __cplusplus
}
#endif
}  // namespace ScriptingEnums
#endif
