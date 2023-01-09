#include "NumberType.h"

  constexpr unsigned long long hashFunc(const char* inputString)
{
  unsigned long long hash = 5381;
  for (size_t i = 0; inputString[i] != '\0'; ++i)
  {
    if (inputString[i] >= 'A' && inputString[i] <= 'Z')
      hash = 33 * hash + (unsigned char)(inputString[i] + 32);
    else if (inputString[i] == ' ')
      hash = 33 * hash + (unsigned char)('_');
    else
      hash = 33 * hash + (unsigned char)inputString[i];
  }
  return hash;
}

constexpr unsigned long long U8_HASH = hashFunc("U8");
constexpr unsigned long long U16_HASH = hashFunc("U16");
constexpr unsigned long long U32_HASH = hashFunc("U32");
constexpr unsigned long long U64_HASH = hashFunc("U64");

constexpr unsigned long long S8_HASH = hashFunc("S8");
constexpr unsigned long long S16_HASH = hashFunc("S16");
constexpr unsigned long long S32_HASH = hashFunc("S32");
constexpr unsigned long long S64_HASH = hashFunc("S64");

constexpr unsigned long long FLOAT_HASH = hashFunc("FLOAT");
constexpr unsigned long long DOUBLE_HASH = hashFunc("DOUBLE");

constexpr unsigned long long UNSIGNED_8_HASH = hashFunc("UNSIGNED_8");
constexpr unsigned long long UNSIGNED_16_HASH = hashFunc("UNSIGNED_16");
constexpr unsigned long long UNSIGNED_32_HASH = hashFunc("UNSIGNED_32");
constexpr unsigned long long UNSIGNED_64_HASH = hashFunc("UNSIGNED_64");

constexpr unsigned long long SIGNED_8_HASH = hashFunc("SIGNED_8");
constexpr unsigned long long SIGNED_16_HASH = hashFunc("SIGNED_16");
constexpr unsigned long long SIGNED_32_HASH = hashFunc("SIGNED_32");
constexpr unsigned long long SIGNED_64_HASH = hashFunc("SIGNED_64");

constexpr unsigned long long UNSIGNED_BYTE_HASH = hashFunc("UNSIGNED BYTE");
constexpr unsigned long long SIGNED_BYTE_HASH = hashFunc("SIGNED BYTE");
constexpr unsigned long long UNSIGNED_INT_HASH = hashFunc("UNSIGNED INT");
constexpr unsigned long long SIGNED_INT_HASH = hashFunc("SIGNED INT");
constexpr unsigned long long UNSIGNED_LONG_LONG_HASH = hashFunc("UNSIGNED LONG LONG");
constexpr unsigned long long SIGNED_LONG_LONG_HASH = hashFunc("SIGNED LONG LONG");

constexpr unsigned long long BYTE_WRAPPER_HASH = hashFunc("BYTE_WRAPPER");
constexpr unsigned long long WRAPPER_HASH = hashFunc("WRAPPER");

// Checks if 2 strings are equal, but the function is case insensitive and treats '_' and ' ' as
// the same character.
// In other words, isEqualSpecial("ABe 0", "aBE_0") would return true.
bool isEqualSpecial(const char* firstString, const char* secondString)
{
  for (size_t i = 0; firstString[i] != '\0'; ++i)
  {
    if (firstString[i] != secondString[i])
    {
      char firstStringChar = firstString[i];
      char secondStringChar = secondString[i];

      if (firstStringChar >= 'A' && firstStringChar <= 'Z')
        firstStringChar += 32;
      else if (firstStringChar == ' ')
        firstStringChar = '_';
      if (secondStringChar >= 'A' && secondStringChar <= 'Z')
        secondStringChar += 32;
      else if (secondStringChar == ' ')
        secondStringChar = '_';

      if (firstStringChar != secondStringChar)
        return false;
    }
  }
  return true;
}

NumberType returnTypeIfEqual(const char* firstString, const char* secondString, NumberType returnType)
{
  if (isEqualSpecial(firstString, secondString))
    return returnType;
  return NumberType::UNDEFINED;
}

// Returns the type specified by the string the user passed in as an argument.
// inputString must be a non-NULL string. This function determines the type with 2 pass throughs
// of the string. Valid strings are any of the string literals in this function.
// Note that each of these strings is case insensitive, and any ' ' can be swapper with a '_' and
// vice versa
//
// Valid type strings that the user can pass in are listed below. Note that each string is case
// insensitive, and spaces can be replaced with underscores and vice versa:
//
// "u8", "u16", "u32", "u64", "s8", "s16", "s32", "s64", "float", "double"
// "unsigned_8", "unsigned_16", "unsigned_32", "unsigned_64", "signed_8", "signed_16",
// "signed_32", "signed_64", "unsigned byte", "signed byte", "unsigned int", "signed int",
// "unsigned long long", "signed long long", "ByteWrapper" and "wrapper"

// Checks if 2 strings are equal, but the function is case insensitive and treats '_' and ' ' as
// the same character. In other words, isEqualSpecial("ABe 0", "aBE_0") would return true.

NumberType parseType(const char* inputString)
{
  unsigned long long inputStringHash = hashFunc(inputString);

  switch (inputStringHash)
  {
  case U8_HASH:
    return returnTypeIfEqual(inputString, "u8", NumberType::UNSIGNED_8);

  case U16_HASH:
    return returnTypeIfEqual(inputString, "u16", NumberType::UNSIGNED_16);

  case U32_HASH:
    return returnTypeIfEqual(inputString, "u32", NumberType::UNSIGNED_32);

  case U64_HASH:
    return returnTypeIfEqual(inputString, "u64", NumberType::UNSIGNED_64);

  case S8_HASH:
    return returnTypeIfEqual(inputString, "s8", NumberType::SIGNED_8);

  case S16_HASH:
    return returnTypeIfEqual(inputString, "s16", NumberType::SIGNED_16);

  case S32_HASH:
    return returnTypeIfEqual(inputString, "s32", NumberType::SIGNED_32);

  case S64_HASH:
    return returnTypeIfEqual(inputString, "s64", NumberType::SIGNED_64);

  case FLOAT_HASH:
    return returnTypeIfEqual(inputString, "float", NumberType::FLOAT);

  case DOUBLE_HASH:
    return returnTypeIfEqual(inputString, "double", NumberType::DOUBLE);

  case UNSIGNED_8_HASH:
    return returnTypeIfEqual(inputString, "unsigned_8", NumberType::UNSIGNED_8);

  case UNSIGNED_16_HASH:
    return returnTypeIfEqual(inputString, "unsigned_16", NumberType::UNSIGNED_16);

  case UNSIGNED_32_HASH:
    return returnTypeIfEqual(inputString, "unsigned_32", NumberType::UNSIGNED_32);

  case UNSIGNED_64_HASH:
    return returnTypeIfEqual(inputString, "unsigned_64", NumberType::UNSIGNED_64);

  case SIGNED_8_HASH:
    return returnTypeIfEqual(inputString, "signed_8", NumberType::SIGNED_8);

  case SIGNED_16_HASH:
    return returnTypeIfEqual(inputString, "signed_16", NumberType::SIGNED_16);

  case SIGNED_32_HASH:
    return returnTypeIfEqual(inputString, "signed_32", NumberType::SIGNED_32);

  case SIGNED_64_HASH:
    return returnTypeIfEqual(inputString, "signed_64", NumberType::SIGNED_64);

  case UNSIGNED_BYTE_HASH:
    return returnTypeIfEqual(inputString, "unsigned byte", NumberType::UNSIGNED_8);

  case SIGNED_BYTE_HASH:
    return returnTypeIfEqual(inputString, "signed byte", NumberType::SIGNED_8);

  case UNSIGNED_INT_HASH:
    return returnTypeIfEqual(inputString, "unsigned int", NumberType::UNSIGNED_32);

  case SIGNED_INT_HASH:
    return returnTypeIfEqual(inputString, "signed int", NumberType::SIGNED_32);

  case UNSIGNED_LONG_LONG_HASH:
    return returnTypeIfEqual(inputString, "unsigned long long", NumberType::UNSIGNED_64);

  case SIGNED_LONG_LONG_HASH:
    return returnTypeIfEqual(inputString, "signed long long", NumberType::SIGNED_64);

  case BYTE_WRAPPER_HASH:
    return returnTypeIfEqual(inputString, "ByteWrapper", NumberType::WRAPPER);

  case WRAPPER_HASH:
    return returnTypeIfEqual(inputString, "wrapper", NumberType::WRAPPER);

  default:
    return NumberType::UNDEFINED;
  }
}

std::string getNumberTypeAsString(NumberType inputType)
{
    switch (inputType)
    {
    case NumberType::UNSIGNED_8:
      return "UNSIGNED_8";
    case NumberType::UNSIGNED_16:
      return "UNSIGNED_16";
    case NumberType::UNSIGNED_32:
      return "UNSIGNED_32";
    case NumberType::UNSIGNED_64:
      return "UNSIGNED_64";
    case NumberType::SIGNED_8:
      return "SIGNED_8";
    case NumberType::SIGNED_16:
      return "SIGNED_16";
    case NumberType::SIGNED_32:
      return "SIGNED_32";
    case NumberType::SIGNED_64:
      return "SIGNED_64";
    case NumberType::FLOAT:
      return "FLOAT";
    case NumberType::DOUBLE:
      return "DOUBLE";
    default:
      return "UNDEFINED";
    }
  }

u8 getMaxSize(NumberType inputType)
{
  switch (inputType)
  {
  case NumberType::UNSIGNED_8:
  case NumberType::SIGNED_8:
    return 1;
  case NumberType::UNSIGNED_16:
  case NumberType::SIGNED_16:
    return 2;
  case NumberType::UNSIGNED_32:
  case NumberType::SIGNED_32:
  case NumberType::FLOAT:
    return 4;
  case NumberType::UNSIGNED_64:
  case NumberType::SIGNED_64:
  case NumberType::DOUBLE:
    return 8;
  default:
    return 0;
  
  }
 }
