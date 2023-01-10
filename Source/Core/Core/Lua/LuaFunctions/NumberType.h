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
  UNSIGNED_BYTE_ARRAY,
  SIGNED_BYTE_ARRAY
};

NumberType parseType(const char* inputString);
std::string getNumberTypeAsString(NumberType inputType);
u8 getMaxSize(NumberType inputType);
