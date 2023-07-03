#ifndef ARG_HOLDER
#define ARG_HOLDER
#include <imgui.h>
#include <map>
#include <string>
#include <vector>
#include "Common/CommonTypes.h"
#include "Core/Movie.h"
#include "Core/Scripting/CoreScriptContextFiles/Enums/ArgTypeEnum.h"

namespace Scripting
{

struct ArgHolder
{
  ScriptingEnums::ArgTypeEnum argument_type;
  bool contains_value;

  // The below lines should all ideally be in a union. However, C++ won't let vectors be included in
  // unions without complicated shenanigans, and the extra variables only add about 100 bytes, so it
  // is what it is...
  bool bool_val;
  u8 u8_val;
  u16 u16_val;
  u32 u32_val;
  u64 u64_val;
  s8 s8_val;
  s16 s16_val;
  s32 s32_val;
  s64 s64_val;
  float float_val;
  double double_val;
  std::string string_val;
  void* void_pointer_val;
  std::map<long long, s16> address_to_byte_map;
  Movie::ControllerState controller_state_val;
  std::vector<ImVec2> list_of_points;

  std::string error_string_val;
};

ArgHolder* CreateBoolArgHolder(bool new_bool_value);
ArgHolder* CreateU8ArgHolder(u8 new_u8_val);
ArgHolder* CreateU16ArgHolder(u16 new_u16_val);
ArgHolder* CreateU32ArgHolder(u32 new_u32_val);
ArgHolder* CreateU64ArgHolder(u64 new_u64_val);
ArgHolder* CreateS8ArgHolder(s8 new_s8_val);
ArgHolder* CreateS16ArgHolder(s16 new_s16_val);
ArgHolder* CreateS32ArgHolder(s32 new_s32_val);
ArgHolder* CreateS64ArgHolder(s64 new_s64_val);
ArgHolder* CreateFloatArgHolder(float new_float_val);
ArgHolder* CreateDoubleArgHolder(double new_double_val);
ArgHolder* CreateStringArgHolder(const std::string& new_string_val);
ArgHolder* CreateVoidPointerArgHolder(void* new_void_pointer_val);

ArgHolder* CreateAddressToByteMapArgHolder(const std::map<long long, s16>& new_address_to_byte_map);

ArgHolder* CreateControllerStateArgHolder(const Movie::ControllerState& new_controller_state_val);

ArgHolder* CreateListOfPointsArgHolder(const std::vector<ImVec2>& new_points_list);
ArgHolder* CreateErrorStringArgHolder(const std::string& new_error_string_val);
ArgHolder* CreateYieldTypeArgHolder();
ArgHolder* CreateVoidTypeArgHolder();
ArgHolder* CreateRegistrationInputTypeArgHolder(void* new_val);
ArgHolder* CreateRegistrationWithAutoDeregistrationInputTypeArgHolder(void* new_val);
ArgHolder* CreateRegistrationForButtonCallbackInputTypeArgHolder(void* new_val);
ArgHolder* CreateUnregistrationInputTypeArgHolder(void* new_val);
ArgHolder* CreateRegistrationReturnTypeArgHolder(void* new_val);
ArgHolder* CreateRegistrationWithAutoDeregistrationReturnTypeArgHolder();
ArgHolder* CreateUnregistrationReturnTypeArgHolder(void* new_val);
ArgHolder* CreateShutdownTypeArgHolder();

ArgHolder* CreateEmptyOptionalArgument();

}  // namespace Scripting
#endif
