#ifndef ARG_HOLDER
#define ARG_HOLDER
#include <string>
#include <vector>
#include "Common/CommonTypes.h"
#include "Core/Movie.h"
#include "Core/Scripting/HelperClasses/ArgTypeEnum.h"

namespace Scripting
{

struct ArgHolder
{
  ArgTypeEnum argument_type;

  // The below lines should all ideally be in a union. However, C++ won't let vectors be included in
  // unions without complicated shenanigans, and the extra variables only add about 100 bytes, so it
  // is what it is...
  bool bool_val;
  u8 u8_val;
  u16 u16_val;
  u32 u32_val;
  s8 s8_val;
  s16 s16_val;
  int int_val;
  long long long_long_val;
  float float_val;
  double double_val;
  std::string string_val;
  void* void_pointer_val;
  std::vector<u8> unsigned_bytes_vector_val;
  std::vector<s8> signed_bytes_vector_val;
  Movie::ControllerState controller_state_val;
};

ArgHolder CreateBoolArgHolder(bool new_bool_value);
ArgHolder CreateU8ArgHolder(u8 new_u8_val);
ArgHolder CreateU16ArgHolder(u16 new_u16_val);
ArgHolder CreateU32ArgHolder(u32 new_u32_val);
ArgHolder CreateS8ArgHolder(s8 new_s8_val);
ArgHolder CreateS16ArgHolder(s16 new_s16_val);
ArgHolder CreateIntArgHolder(int new_int_val);
ArgHolder CreateLongLongArgHolder(long long new_long_long_val);
ArgHolder CreateFloatArgHolder(float new_float_val);
ArgHolder CreateDoubleArgHolder(double new_double_val);
ArgHolder CreateStringArgHolder(const std::string& new_string_val);
ArgHolder CreateVoidPointerArgHolder(void* new_void_pointer_val);
ArgHolder CreateUnsignedBytesVectorArgHolder(const std::vector<u8>& new_unsigned_bytes_vector_val);
ArgHolder CreateSignedBytesVectorArgHolder(const std::vector<s8>& new_signed_bytes_vector_val);
ArgHolder CreateControllerStateArgHolder(const Movie::ControllerState& new_controller_state_val);
}  // namespace Scripting
#endif
