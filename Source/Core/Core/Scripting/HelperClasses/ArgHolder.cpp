#include "Core/Scripting/HelperClasses/ArgHolder.h"

namespace Scripting
{

ArgHolder* CreateBoolArgHolder(bool new_bool_value)
{
  ArgHolder* return_val = new ArgHolder();
  return_val->argument_type = ArgTypeEnum::Boolean;
  return_val->bool_val = new_bool_value;
  return_val->contains_value = true;
  return return_val;
}

ArgHolder* CreateU8ArgHolder(u8 new_u8_val)
{
  ArgHolder* return_val = new ArgHolder();
  return_val->argument_type = ArgTypeEnum::U8;
  return_val->u8_val = new_u8_val;
  return_val->contains_value = true;
  return return_val;
}

ArgHolder* CreateU16ArgHolder(u16 new_u16_val)
{
  ArgHolder* return_val = new ArgHolder();
  return_val->argument_type = ArgTypeEnum::U16;
  return_val->u16_val = new_u16_val;
  return_val->contains_value = true;
  return return_val;
}

ArgHolder* CreateU32ArgHolder(u32 new_u32_val)
{
  ArgHolder* return_val = new ArgHolder();
  return_val->argument_type = ArgTypeEnum::U32;
  return_val->u32_val = new_u32_val;
  return_val->contains_value = true;
  return return_val;
}

ArgHolder* CreateU64ArgHolder(u64 new_u64_val)
{
  ArgHolder* return_val = new ArgHolder();
  return_val->argument_type = ArgTypeEnum::U64;
  return_val->u64_val = new_u64_val;
  return_val->contains_value = true;
  return return_val;
}

ArgHolder* CreateS8ArgHolder(s8 new_s8_val)
{
  ArgHolder* return_val = new ArgHolder();
  return_val->argument_type = ArgTypeEnum::S8;
  return_val->s8_val = new_s8_val;
  return_val->contains_value = true;
  return return_val;
}

ArgHolder* CreateS16ArgHolder(s16 new_s16_val)
{
  ArgHolder* return_val = new ArgHolder();
  return_val->argument_type = ArgTypeEnum::S16;
  return_val->s16_val = new_s16_val;
  return_val->contains_value = true;
  return return_val;
}

ArgHolder* CreateIntArgHolder(int new_int_val)
{
  ArgHolder* return_val = new ArgHolder();
  return_val->argument_type = ArgTypeEnum::Integer;
  return_val->int_val = new_int_val;
  return_val->contains_value = true;
  return return_val;
}

ArgHolder* CreateLongLongArgHolder(long long new_long_long_val)
{
  ArgHolder* return_val = new ArgHolder();
  return_val->argument_type = ArgTypeEnum::LongLong;
  return_val->long_long_val = new_long_long_val;
  return_val->contains_value = true;
  return return_val;
}

ArgHolder* CreateFloatArgHolder(float new_float_val)
{
  ArgHolder* return_val = new ArgHolder();
  return_val->argument_type = ArgTypeEnum::Float;
  return_val->float_val = new_float_val;
  return_val->contains_value = true;
  return return_val;
}

ArgHolder* CreateDoubleArgHolder(double new_double_val)
{
  ArgHolder* return_val = new ArgHolder();
  return_val->argument_type = ArgTypeEnum::Double;
  return_val->double_val = new_double_val;
  return_val->contains_value = true;
  return return_val;
}

ArgHolder* CreateStringArgHolder(const std::string& new_string_val)
{
  ArgHolder* return_val = new ArgHolder();
  return_val->argument_type = ArgTypeEnum::String;
  return_val->string_val = new_string_val;
  return_val->contains_value = true;
  return return_val;
}

ArgHolder* CreateVoidPointerArgHolder(void* new_void_pointer_val)
{
  ArgHolder* return_val = new ArgHolder();
  return_val->argument_type = ArgTypeEnum::VoidPointer;
  return_val->void_pointer_val = new_void_pointer_val;
  return_val->contains_value = true;
  return return_val;
}

ArgHolder* CreateAddressToByteMapArgHolder(const std::map<long long, s16>& new_address_to_byte_map)
{
  ArgHolder* return_val = new ArgHolder();
  return_val->argument_type = ArgTypeEnum::AddressToByteMap;
  return_val->address_to_byte_map = new_address_to_byte_map;
  return_val->contains_value = true;
  return return_val;
}

ArgHolder* CreateControllerStateArgHolder(const Movie::ControllerState& new_controller_state_val)
{
  ArgHolder* return_val = new ArgHolder();
  return_val->argument_type = ArgTypeEnum::ControllerStateObject;
  return_val->controller_state_val = new_controller_state_val;
  return_val->contains_value = true;
  return return_val;
}

ArgHolder* CreateListOfPointsArgHolder(const std::vector<ImVec2>& new_points_list)
{
  ArgHolder* return_val = new ArgHolder();
  return_val->argument_type = ArgTypeEnum::ListOfPoints;
  return_val->list_of_points = new_points_list;
  return_val->contains_value = true;
  return return_val;
}

ArgHolder* CreateErrorStringArgHolder(const std::string& new_error_string_val)
{
  ArgHolder* return_val = new ArgHolder();
  return_val->argument_type = ArgTypeEnum::ErrorStringType;
  return_val->error_string_val = new_error_string_val;
  return_val->contains_value = true;
  return return_val;
}

ArgHolder* CreateYieldTypeArgHolder()
{
  ArgHolder* return_val = new ArgHolder();
  return_val->argument_type = ArgTypeEnum::YieldType;
  return_val->contains_value = true;
  return return_val;
}

ArgHolder* CreateVoidTypeArgHolder()
{
  ArgHolder* return_val = new ArgHolder();
  return_val->argument_type = ArgTypeEnum::VoidType;
  return_val->contains_value = true;
  return return_val;
}

ArgHolder* CreateRegistrationInputTypeArgHolder(void* new_val)
{
  ArgHolder* return_val = new ArgHolder();
  return_val->argument_type = ArgTypeEnum::RegistrationInputType;
  return_val->void_pointer_val = new_val;
  return_val->contains_value = true;
  return return_val;
}

ArgHolder* CreateRegistrationWithAutoDeregistrationInputTypeArgHolder(void* new_val)
{
  ArgHolder* return_val = new ArgHolder();
  return_val->argument_type = ArgTypeEnum::RegistrationWithAutoDeregistrationInputType;
  return_val->void_pointer_val = new_val;
  return_val->contains_value = true;
  return return_val;
}

ArgHolder* CreateRegistrationForButtonCallbackInputTypeArgHolder(void* new_val)
{
  ArgHolder* return_val = new ArgHolder();
  return_val->argument_type = ArgTypeEnum::RegistrationForButtonCallbackInputType;
  return_val->void_pointer_val = new_val;
  return_val->contains_value = true;
  return return_val;
}

ArgHolder* CreateUnregistrationInputTypeArgHolder(void* new_val)
{
  ArgHolder* return_val = new ArgHolder();
  return_val->argument_type = ArgTypeEnum::UnregistrationInputType;
  return_val->void_pointer_val = new_val;
  return_val->contains_value = true;
  return return_val;
}

ArgHolder* CreateRegistrationReturnTypeArgHolder(void* new_val)
{
  ArgHolder* return_val = new ArgHolder();
  return_val->argument_type = ArgTypeEnum::RegistrationReturnType;
  return_val->void_pointer_val = new_val;
  return_val->contains_value = true;
  return return_val;
}

ArgHolder* CreateRegistrationWithAutoDeregistrationReturnTypeArgHolder()
{
  ArgHolder* return_val = new ArgHolder();
  return_val->argument_type = ArgTypeEnum::RegistrationWithAutoDeregistrationReturnType;
  return_val->contains_value = true;
  return return_val;
}

ArgHolder* CreateUnregistrationReturnTypeArgHolder(void* new_val)
{
  ArgHolder* return_val = new ArgHolder();
  return_val->argument_type = ArgTypeEnum::UnregistrationReturnType;
  return_val->void_pointer_val = new_val;
  return_val->contains_value = true;
  return return_val;
}

ArgHolder* CreateShutdownTypeArgHolder()
{
  ArgHolder* return_val = new ArgHolder();
  return_val->argument_type = ArgTypeEnum::ShutdownType;
  return_val->contains_value = true;
  return return_val;
}

ArgHolder* CreateEmptyOptionalArgument()
{
  ArgHolder* return_val = new ArgHolder();
  return_val->contains_value = false;  // We always check if the ArgHolder contains a value before looking at anything else.
  return return_val;
}

}  // namespace Scripting
