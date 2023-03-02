#pragma once
#ifndef ARG_TYPE_ENUM
#define ARG_TYPE_ENUM
namespace Scripting
{
enum class ArgTypeEnum
{
  VoidType,
  Boolean,
  U8,
  U16,
  U32,
  U64,
  S8,
  S16,
  Integer,
  LongLong,
  Float,
  Double,
  String,
  VoidPointer,
  AddressToUnsignedByteMap,
  AddressToSignedByteMap,
  AddressToByteMap,
  ControllerStateObject,
  ErrorStringType,
  YieldType,
  RegistrationInputType,
  UnregistrationInputType,
  RegistrationReturnType,
  UnregistrationReturnType
};
}
#endif
