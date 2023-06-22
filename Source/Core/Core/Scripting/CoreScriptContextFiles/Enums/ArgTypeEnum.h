#pragma once
#ifndef ARG_TYPE_ENUM
#define ARG_TYPE_ENUM
namespace Scripting
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
  AddressToUnsignedByteMap = 14,
  AddressToSignedByteMap = 15,
  AddressToByteMap = 16,
  ControllerStateObject = 17,
  ListOfPoints = 18,
  ErrorStringType = 19,
  YieldType = 20,
  RegistrationInputType = 21,
  RegistrationWithAutoDeregistrationInputType = 22,
  RegistrationForButtonCallbackInputType = 23,
  UnregistrationInputType = 24,
  RegistrationReturnType = 25,
  RegistrationWithAutoDeregistrationReturnType = 26,
  UnregistrationReturnType = 27,
  ShutdownType = 28,
  UnknownArgTypeEnum = 29
};
#ifdef __cplusplus
}
#endif
}  // namespace Scripting
#endif
