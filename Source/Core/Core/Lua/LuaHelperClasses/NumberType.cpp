#include "NumberType.h"

constexpr unsigned long long hashFunc(const char* input_string)
{
  unsigned long long hash = 5381;
  for (size_t i = 0; input_string[i] != '\0'; ++i)
  {
    if (input_string[i] >= 'A' && input_string[i] <= 'Z')
      hash = 33 * hash + (unsigned char)(input_string[i] + 32);
    else if (input_string[i] == ' ')
      hash = 33 * hash + (unsigned char)('_');
    else
      hash = 33 * hash + (unsigned char)input_string[i];
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

constexpr unsigned long long UNSIGNED_8_ALT_HASH = hashFunc("unsigned8");
constexpr unsigned long long UNSIGNED_16_ALT_HASH = hashFunc("unsigned16");
constexpr unsigned long long UNSIGNED_32_ALT_HASH = hashFunc("unsigned32");
constexpr unsigned long long UNSIGNED_64_ALT_HASH = hashFunc("unsigned64");

constexpr unsigned long long SIGNED_8_HASH = hashFunc("SIGNED_8");
constexpr unsigned long long SIGNED_16_HASH = hashFunc("SIGNED_16");
constexpr unsigned long long SIGNED_32_HASH = hashFunc("SIGNED_32");
constexpr unsigned long long SIGNED_64_HASH = hashFunc("SIGNED_64");

constexpr unsigned long long SIGNED_8_ALT_HASH = hashFunc("signed8");
constexpr unsigned long long SIGNED_16_ALT_HASH = hashFunc("signed16");
constexpr unsigned long long SIGNED_32_ALT_HASH = hashFunc("signed32");
constexpr unsigned long long SIGNED_64_ALT_HASH = hashFunc("signed64");

constexpr unsigned long long UNSIGNED_BYTE_HASH = hashFunc("UNSIGNED BYTE");
constexpr unsigned long long SIGNED_BYTE_HASH = hashFunc("SIGNED BYTE");
constexpr unsigned long long UNSIGNED_INT_HASH = hashFunc("UNSIGNED INT");
constexpr unsigned long long SIGNED_INT_HASH = hashFunc("SIGNED INT");
constexpr unsigned long long UNSIGNED_LONG_LONG_HASH = hashFunc("UNSIGNED LONG LONG");
constexpr unsigned long long SIGNED_LONG_LONG_HASH = hashFunc("SIGNED LONG LONG");

constexpr unsigned long long UNSIGNED_BYTE_ALT_HASH = hashFunc("UnsignedByte");
constexpr unsigned long long SIGNED_BYTE_ALT_HASH = hashFunc("SignedByte");
constexpr unsigned long long UNSIGNED_INT_ALT_HASH = hashFunc("UnsignedInt");
constexpr unsigned long long SIGNED_INT_ALT_HASH = hashFunc("SignedInt");
constexpr unsigned long long UNSIGNED_LONG_LONG_ALT_HASH = hashFunc("UnsignedLongLong");
constexpr unsigned long long SIGNED_LONG_LONG_ALT_HASH = hashFunc("SignedLongLong");

// Checks if 2 strings are equal, but the function is case insensitive and treats '_' and ' ' as
// the same character.
// In other words, isEqualSpecial("ABe 0", "aBE_0") would return true.
bool IsEqualSpecial(const char* first_string, const char* second_string)
{
  for (size_t i = 0; first_string[i] != '\0'; ++i)
  {
    if (first_string[i] != second_string[i])
    {
      char first_string_char = first_string[i];
      char second_string_char = second_string[i];

      if (first_string_char >= 'A' && first_string_char <= 'Z')
        first_string_char += 32;
      else if (first_string_char == ' ')
        first_string_char = '_';
      if (second_string_char >= 'A' && second_string_char <= 'Z')
        second_string_char += 32;
      else if (second_string_char == ' ')
        second_string_char = '_';

      if (first_string_char != second_string_char)
        return false;
    }
  }
  return true;
}

NumberType ReturnTypeIfEqual(const char* first_string, const char* second_string,
                             NumberType return_type)
{
  if (IsEqualSpecial(first_string, second_string))
    return return_type;
  return NumberType::Undefined;
}

// Returns the type specified by the string the user passed in as an argument.
// inputString must be a non-NULL string. This function determines the type with 2 pass throughs
// of the string. Valid strings are any of the string literals in this function.
// Note that each of these strings is case insensitive, and any ' ' can be swapper with a '_' and
// vice versa
//
// Valid type strings that the user can pass in are listed below. Note that each string is case
// insensitive, and spaces can be replaced with underscores and vice versa. Also, any string which
// is 2 or more words can be written as one word, a series of words seperated by underscores, or a
// series of words seperated by spaces:
//
// "u8", "u16", "u32", "u64", "s8", "s16", "s32", "s64", "float", "double"
// "unsigned_8", "unsigned_16", "unsigned_32", "unsigned_64", "signed_8", "signed_16",
// "signed_32", "signed_64", "unsigned byte", "signed byte", "unsigned int", "signed int",
// "unsigned long long", "signed long long", "UnsignedByteArray", "SignedByteArray"

// Checks if 2 strings are equal, but the function is case insensitive and treats '_' and ' ' as
// the same character. In other words, isEqualSpecial("ABe 0", "aBE_0") would return true.

NumberType ParseType(const char* input_string)
{
  unsigned long long input_string_hash = hashFunc(input_string);
  // There are 42 cases below.
  switch (input_string_hash)
  {
  case U8_HASH:
    return ReturnTypeIfEqual(input_string, "u8", NumberType::Unsigned8);
  case U16_HASH:
    return ReturnTypeIfEqual(input_string, "u16", NumberType::Unsigned16);
  case U32_HASH:
    return ReturnTypeIfEqual(input_string, "u32", NumberType::Unsigned32);
  case U64_HASH:
    return ReturnTypeIfEqual(input_string, "u64", NumberType::Unsigned64);

  case S8_HASH:
    return ReturnTypeIfEqual(input_string, "s8", NumberType::Signed8);
  case S16_HASH:
    return ReturnTypeIfEqual(input_string, "s16", NumberType::Signed16);
  case S32_HASH:
    return ReturnTypeIfEqual(input_string, "s32", NumberType::Signed32);
  case S64_HASH:
    return ReturnTypeIfEqual(input_string, "s64", NumberType::Signed64);

  case FLOAT_HASH:
    return ReturnTypeIfEqual(input_string, "float", NumberType::Float);
  case DOUBLE_HASH:
    return ReturnTypeIfEqual(input_string, "double", NumberType::Double);

  case UNSIGNED_8_HASH:
    return ReturnTypeIfEqual(input_string, "unsigned_8", NumberType::Unsigned8);
  case UNSIGNED_16_HASH:
    return ReturnTypeIfEqual(input_string, "unsigned_16", NumberType::Unsigned16);
  case UNSIGNED_32_HASH:
    return ReturnTypeIfEqual(input_string, "unsigned_32", NumberType::Unsigned32);
  case UNSIGNED_64_HASH:
    return ReturnTypeIfEqual(input_string, "unsigned_64", NumberType::Unsigned64);

  case UNSIGNED_8_ALT_HASH:
    return ReturnTypeIfEqual(input_string, "unsigned8", NumberType::Unsigned8);
  case UNSIGNED_16_ALT_HASH:
    return ReturnTypeIfEqual(input_string, "unsigned16", NumberType::Unsigned16);
  case UNSIGNED_32_ALT_HASH:
    return ReturnTypeIfEqual(input_string, "unsigned32", NumberType::Unsigned32);
  case UNSIGNED_64_ALT_HASH:
    return ReturnTypeIfEqual(input_string, "unsigned64", NumberType::Unsigned64);

  case SIGNED_8_HASH:
    return ReturnTypeIfEqual(input_string, "signed_8", NumberType::Signed8);
  case SIGNED_16_HASH:
    return ReturnTypeIfEqual(input_string, "signed_16", NumberType::Signed16);
  case SIGNED_32_HASH:
    return ReturnTypeIfEqual(input_string, "signed_32", NumberType::Signed32);
  case SIGNED_64_HASH:
    return ReturnTypeIfEqual(input_string, "signed_64", NumberType::Signed64);

  case SIGNED_8_ALT_HASH:
    return ReturnTypeIfEqual(input_string, "signed8", NumberType::Signed8);
  case SIGNED_16_ALT_HASH:
    return ReturnTypeIfEqual(input_string, "signed16", NumberType::Signed16);
  case SIGNED_32_ALT_HASH:
    return ReturnTypeIfEqual(input_string, "signed32", NumberType::Signed32);
  case SIGNED_64_ALT_HASH:
    return ReturnTypeIfEqual(input_string, "signed64", NumberType::Signed64);

  case UNSIGNED_BYTE_HASH:
    return ReturnTypeIfEqual(input_string, "unsigned byte", NumberType::Unsigned8);
  case SIGNED_BYTE_HASH:
    return ReturnTypeIfEqual(input_string, "signed byte", NumberType::Signed8);
  case UNSIGNED_INT_HASH:
    return ReturnTypeIfEqual(input_string, "unsigned int", NumberType::Unsigned32);
  case SIGNED_INT_HASH:
    return ReturnTypeIfEqual(input_string, "signed int", NumberType::Signed32);
  case UNSIGNED_LONG_LONG_HASH:
    return ReturnTypeIfEqual(input_string, "unsigned long long", NumberType::Unsigned64);
  case SIGNED_LONG_LONG_HASH:
    return ReturnTypeIfEqual(input_string, "signed long long", NumberType::Signed64);

  case UNSIGNED_BYTE_ALT_HASH:
    return ReturnTypeIfEqual(input_string, "UnsignedByte", NumberType::Unsigned8);
  case SIGNED_BYTE_ALT_HASH:
    return ReturnTypeIfEqual(input_string, "SignedByte", NumberType::Signed8);
  case UNSIGNED_INT_ALT_HASH:
    return ReturnTypeIfEqual(input_string, "UnsignedInt", NumberType::Unsigned32);
  case SIGNED_INT_ALT_HASH:
    return ReturnTypeIfEqual(input_string, "SignedInt", NumberType::Signed32);
  case UNSIGNED_LONG_LONG_ALT_HASH:
    return ReturnTypeIfEqual(input_string, "UnsignedLongLong", NumberType::Unsigned64);
  case SIGNED_LONG_LONG_ALT_HASH:
    return ReturnTypeIfEqual(input_string, "SignedLongLong", NumberType::Signed64);
  default:
    return NumberType::Undefined;
  }
}

std::string GetNumberTypeAsString(NumberType input_type)
{
  switch (input_type)
  {
  case NumberType::Unsigned8:
    return "UNSIGNED_8";
  case NumberType::Unsigned16:
    return "UNSIGNED_16";
  case NumberType::Unsigned32:
    return "UNSIGNED_32";
  case NumberType::Unsigned64:
    return "UNSIGNED_64";
  case NumberType::Signed8:
    return "SIGNED_8";
  case NumberType::Signed16:
    return "SIGNED_16";
  case NumberType::Signed32:
    return "SIGNED_32";
  case NumberType::Signed64:
    return "SIGNED_64";
  case NumberType::Float:
    return "FLOAT";
  case NumberType::Double:
    return "DOUBLE";
  default:
    return "UNDEFINED";
  }
}

u8 GetMaxSize(NumberType input_type)
{
  switch (input_type)
  {
  case NumberType::Unsigned8:
  case NumberType::Signed8:
    return 1;
  case NumberType::Unsigned16:
  case NumberType::Signed16:
    return 2;
  case NumberType::Unsigned32:
  case NumberType::Signed32:
  case NumberType::Float:
    return 4;
  case NumberType::Unsigned64:
  case NumberType::Signed64:
  case NumberType::Double:
    return 8;
  default:
    return 0;
  }
}
