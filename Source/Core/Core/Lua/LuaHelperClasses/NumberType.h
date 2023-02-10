#pragma once
#include <Common/CommonTypes.h>
#include <string>

enum class NumberType
{
  Undefined,
  Unsigned8,
  Signed8,
  Unsigned16,
  Signed16,
  Unsigned32,
  Signed32,
  Unsigned64,
  Signed64,
  Float,
  Double
};

NumberType ParseType(const char* input_string);
std::string GetNumberTypeAsString(NumberType input_type);
u8 GetMaxSize(NumberType input_type);
