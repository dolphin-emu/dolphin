#pragma once
#include <string>
#include <common/CommonTypes.h>

enum class NumberType
{
  UNDEFINED,
  UNSIGNED_8,
  SIGNED_8,
  UNSIGNED_16,
  SIGNED_16,
  UNSIGNED_32,
  SIGNED_32,
  UNSIGNED_64,
  SIGNED_64,
  FLOAT,
  DOUBLE,
  WRAPPER  // This type is just included in the enum to make it clear in the parseType() function
           // when the user wants a ByteWrapper type object - no actual ByteWrapper object should
           // have this as a type value.
};

NumberType parseType(const char* inputString);
std::string getNumberTypeAsString(NumberType inputType);
u8 getMaxSize(NumberType inputType);
