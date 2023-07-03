#include "Core/Scripting/HelperClasses/ArgHolder_API_Implementations.h"

#include "Core/Scripting/CoreScriptContextFiles/Enums/ArgTypeEnum.h"
#include "Core/Scripting/CoreScriptContextFiles/Enums/GCButtonNameEnum.h"
#include "Core/Scripting/HelperClasses/ArgHolder.h"

namespace Scripting
{
ArgHolder* castToArgHolderPtr(void* input)
{
  return reinterpret_cast<ArgHolder*>(input);
}

std::map<long long, s16>::iterator* castToIteratorPtr(void* input)
{
  return reinterpret_cast<std::map<long long, s16>::iterator*>(input);
}

void* castToVoidPtr(ArgHolder* input)
{
  return reinterpret_cast<void*>(input);
}

int GetArgType_ArgHolder_impl(void* arg_holder_ptr)
{
  ArgHolder* castedArgHolderPtr = castToArgHolderPtr(arg_holder_ptr);
  return (int)castedArgHolderPtr->argument_type;
}

int GetIsEmpty_ArgHolder_impl(void* arg_holder_ptr)
{
  if (castToArgHolderPtr(arg_holder_ptr)->contains_value)
    return 0;
  else
    return 1;
}

void* CreateEmptyOptionalArgHolder_API_impl()
{
  return castToVoidPtr(CreateEmptyOptionalArgument());
}

void* CreateBoolArgHolder_API_impl(int new_int)
{
  bool new_bool = new_int != 0 ? true : false;
  return castToVoidPtr(CreateBoolArgHolder(new_bool));
}

void* CreateU8ArgHolder_API_impl(unsigned long long new_u64)
{
  u8 u8_val = static_cast<u8>(new_u64);
  return castToVoidPtr(CreateU8ArgHolder(u8_val));
}

void* CreateU16ArgHolder_API_impl(unsigned long long new_u64)
{
  u16 u16_val = static_cast<u16>(new_u64);
  return castToVoidPtr(CreateU16ArgHolder(u16_val));
}

void* CreateU32ArgHolder_API_impl(unsigned long long new_u64)
{
  u32 u32_val = static_cast<u32>(new_u64);
  return castToVoidPtr(CreateU32ArgHolder(u32_val));
}

void* CreateU64ArgHolder_API_impl(unsigned long long new_u64)
{
  return castToVoidPtr(CreateU64ArgHolder(new_u64));
}

void* CreateS8ArgHolder_API_impl(signed long long new_s64)
{
  s8 s8_val = static_cast<s8>(new_s64);
  return castToVoidPtr(CreateS8ArgHolder(s8_val));
}

void* CreateS16ArgHolder_API_impl(signed long long new_s64)
{
  s16 s16_val = static_cast<s16>(new_s64);
  return castToVoidPtr(CreateS16ArgHolder(s16_val));
}

void* CreateS32ArgHolder_API_impl(signed long long new_s64)
{
  s32 s32_val = static_cast<s32>(new_s64);
  return castToVoidPtr(CreateS32ArgHolder(s32_val));
}

void* CreateS64ArgHolder_API_impl(signed long long new_s64)
{
  return castToVoidPtr(CreateS64ArgHolder(new_s64));
}

void* CreateFloatArgHolder_API_impl(double new_double)
{
  float float_val = static_cast<float>(new_double);
  return castToVoidPtr(CreateFloatArgHolder(float_val));
}

void* CreateDoubleArgHolder_API_impl(double new_double)
{
  return castToVoidPtr(CreateDoubleArgHolder(new_double));
}

void* CreateStringArgHolder_API_impl(const char* new_str_val)
{
  return castToVoidPtr(CreateStringArgHolder(new_str_val));
}

void* CreateVoidPointerArgHolder_API_impl(void* new_void_ptr)
{
  return castToVoidPtr(CreateVoidPointerArgHolder(new_void_ptr));
}

void* CreateAddressToByteMapArgHolder_API_impl()
{
  std::map<long long, s16> address_to_byte_map = std::map<long long, s16>();
  return castToVoidPtr(CreateAddressToByteMapArgHolder(address_to_byte_map));
}

void AddPairToAddressToByteMapArgHolder_impl(void* address_to_byte_map_arg_holder_ptr,
                                             signed long long address, signed long long byte)
{
  castToArgHolderPtr(address_to_byte_map_arg_holder_ptr)->address_to_byte_map[address] =
      static_cast<s16>(byte);
}

unsigned long long GetSizeOfAddressToByteMapArgHolder_impl(void* address_to_byte_map_arg_holder_ptr)
{
  return castToArgHolderPtr(address_to_byte_map_arg_holder_ptr)->address_to_byte_map.size();
}

void* Create_IteratorForAddressToByteMapArgHolder_impl(void* address_to_byte_map_arg_holder_ptr)
{
  return reinterpret_cast<void*>(new std::map<long long, s16>::iterator(
      castToArgHolderPtr(address_to_byte_map_arg_holder_ptr)->address_to_byte_map.begin()));
}

void* IncrementIteratorForAddressToByteMapArgHolder_impl(void* iterator_ptr,
                                                         void* address_to_byte_map_arg_holder)
{
  castToIteratorPtr(iterator_ptr)->operator++();
  if ((*castToIteratorPtr(iterator_ptr)) ==
      castToArgHolderPtr(address_to_byte_map_arg_holder)->address_to_byte_map.end())
    return nullptr;
  else
    return iterator_ptr;
}

signed long long GetKeyForAddressToByteMapArgHolder_impl(void* iterator_ptr)
{
  return castToIteratorPtr(iterator_ptr)->operator*().first;
}

signed long long GetValueForAddressToUnsignedByteMapArgHolder_impl(void* iterator_ptr)
{
  return static_cast<signed int>(castToIteratorPtr(iterator_ptr)->operator*().second);
}

void Delete_IteratorForAddressToByteMapArgHolder_impl(void* iterator_ptr)
{
  delete (castToIteratorPtr(iterator_ptr));
}

void* CreateControllerStateArgHolder_API_impl()
{
  Movie::ControllerState controllerState = {};
  controllerState.A = false;
  controllerState.AnalogStickX = 128;
  controllerState.AnalogStickY = 128;
  controllerState.B = false;
  controllerState.CStickX = 128;
  controllerState.CStickY = 128;
  controllerState.DPadDown = false;
  controllerState.DPadLeft = false;
  controllerState.DPadRight = false;
  controllerState.DPadUp = false;
  controllerState.disc = false;
  controllerState.get_origin = false;
  controllerState.is_connected = true;
  controllerState.L = false;
  controllerState.R = false;
  controllerState.reset = false;
  controllerState.Start = false;
  controllerState.TriggerL = 0;
  controllerState.TriggerR = 0;
  controllerState.X = false;
  controllerState.Y = false;
  controllerState.Z = false;

  return reinterpret_cast<void*>(CreateControllerStateArgHolder(controllerState));
}

void SetControllerStateArgHolderValue_impl(void* input_ptr, int gc_button_name,
                                           unsigned char raw_char_button_value)
{
  u8 button_value = static_cast<u8>(raw_char_button_value);
  ArgHolder* controller_state_arg_holder = castToArgHolderPtr(input_ptr);
  ScriptingEnums::GcButtonNameEnum button_name_enum =
      (ScriptingEnums::GcButtonNameEnum)(gc_button_name);
  switch (button_name_enum)
  {
  case ScriptingEnums::GcButtonNameEnum::A:
    controller_state_arg_holder->controller_state_val.A = (button_value != 0);
    return;
  case ScriptingEnums::GcButtonNameEnum::AnalogStickX:
    controller_state_arg_holder->controller_state_val.AnalogStickX = button_value;
    return;
  case ScriptingEnums::GcButtonNameEnum::AnalogStickY:
    controller_state_arg_holder->controller_state_val.AnalogStickY = button_value;
    return;
  case ScriptingEnums::GcButtonNameEnum::B:
    controller_state_arg_holder->controller_state_val.B = (button_value != 0);
    return;
  case ScriptingEnums::GcButtonNameEnum::CStickX:
    controller_state_arg_holder->controller_state_val.CStickX = button_value;
    return;
  case ScriptingEnums::GcButtonNameEnum::CStickY:
    controller_state_arg_holder->controller_state_val.CStickY = button_value;
    return;
  case ScriptingEnums::GcButtonNameEnum::Disc:
    controller_state_arg_holder->controller_state_val.disc = (button_value != 0);
    return;
  case ScriptingEnums::GcButtonNameEnum::DPadDown:
    controller_state_arg_holder->controller_state_val.DPadDown = (button_value != 0);
    return;
  case ScriptingEnums::GcButtonNameEnum::DPadLeft:
    controller_state_arg_holder->controller_state_val.DPadLeft = (button_value != 0);
    return;
  case ScriptingEnums::GcButtonNameEnum::DPadRight:
    controller_state_arg_holder->controller_state_val.DPadRight = (button_value != 0);
    return;
  case ScriptingEnums::GcButtonNameEnum::DPadUp:
    controller_state_arg_holder->controller_state_val.DPadUp = (button_value != 0);
    return;
  case ScriptingEnums::GcButtonNameEnum::GetOrigin:
    controller_state_arg_holder->controller_state_val.get_origin = (button_value != 0);
    return;
  case ScriptingEnums::GcButtonNameEnum::IsConnected:
    controller_state_arg_holder->controller_state_val.is_connected = (button_value != 0);
    return;
  case ScriptingEnums::GcButtonNameEnum::L:
    controller_state_arg_holder->controller_state_val.L = (button_value != 0);
    return;
  case ScriptingEnums::GcButtonNameEnum::R:
    controller_state_arg_holder->controller_state_val.R = (button_value != 0);
    return;
  case ScriptingEnums::GcButtonNameEnum::Reset:
    controller_state_arg_holder->controller_state_val.reset = (button_value != 0);
    return;
  case ScriptingEnums::GcButtonNameEnum::Start:
    controller_state_arg_holder->controller_state_val.Start = (button_value != 0);
    return;
  case ScriptingEnums::GcButtonNameEnum::TriggerL:
    controller_state_arg_holder->controller_state_val.TriggerL = button_value;
    return;
  case ScriptingEnums::GcButtonNameEnum::TriggerR:
    controller_state_arg_holder->controller_state_val.TriggerR = button_value;
    return;
  case ScriptingEnums::GcButtonNameEnum::X:
    controller_state_arg_holder->controller_state_val.X = (button_value != 0);
    return;
  case ScriptingEnums::GcButtonNameEnum::Y:
    controller_state_arg_holder->controller_state_val.Y = (button_value != 0);
    return;
  case ScriptingEnums::GcButtonNameEnum::Z:
    controller_state_arg_holder->controller_state_val.Z = (button_value != 0);
    return;
  default:
    return;
  }
}

int GetControllerStateArgHolderValue_impl(void* input_ptr, int gc_button_name)
{
  ArgHolder* controller_state_arg_holder = castToArgHolderPtr(input_ptr);
  ScriptingEnums::GcButtonNameEnum button_name_enum =
      (ScriptingEnums::GcButtonNameEnum)(gc_button_name);

  switch (button_name_enum)
  {
  case ScriptingEnums::GcButtonNameEnum::A:
    return (int)controller_state_arg_holder->controller_state_val.A;

  case ScriptingEnums::GcButtonNameEnum::AnalogStickX:
    return (int)controller_state_arg_holder->controller_state_val.AnalogStickX;

  case ScriptingEnums::GcButtonNameEnum::AnalogStickY:
    return (int)controller_state_arg_holder->controller_state_val.AnalogStickY;

  case ScriptingEnums::GcButtonNameEnum::B:
    return (int)controller_state_arg_holder->controller_state_val.B;

  case ScriptingEnums::GcButtonNameEnum::CStickX:
    return (int)controller_state_arg_holder->controller_state_val.CStickX;

  case ScriptingEnums::GcButtonNameEnum::CStickY:
    return (int)controller_state_arg_holder->controller_state_val.CStickY;

  case ScriptingEnums::GcButtonNameEnum::Disc:
    return (int)controller_state_arg_holder->controller_state_val.disc;

  case ScriptingEnums::GcButtonNameEnum::DPadDown:
    return (int)controller_state_arg_holder->controller_state_val.DPadDown;

  case ScriptingEnums::GcButtonNameEnum::DPadLeft:
    return (int)controller_state_arg_holder->controller_state_val.DPadLeft;

  case ScriptingEnums::GcButtonNameEnum::DPadRight:
    return (int)controller_state_arg_holder->controller_state_val.DPadRight;

  case ScriptingEnums::GcButtonNameEnum::DPadUp:
    return (int)controller_state_arg_holder->controller_state_val.DPadUp;

  case ScriptingEnums::GcButtonNameEnum::GetOrigin:
    return (int)controller_state_arg_holder->controller_state_val.get_origin;

  case ScriptingEnums::GcButtonNameEnum::IsConnected:
    return (int)controller_state_arg_holder->controller_state_val.is_connected;

  case ScriptingEnums::GcButtonNameEnum::L:
    return (int)controller_state_arg_holder->controller_state_val.L;

  case ScriptingEnums::GcButtonNameEnum::R:
    return (int)controller_state_arg_holder->controller_state_val.R;

  case ScriptingEnums::GcButtonNameEnum::Reset:
    return (int)controller_state_arg_holder->controller_state_val.reset;

  case ScriptingEnums::GcButtonNameEnum::Start:
    return (int)controller_state_arg_holder->controller_state_val.Start;

  case ScriptingEnums::GcButtonNameEnum::TriggerL:
    return (int)controller_state_arg_holder->controller_state_val.TriggerL;

  case ScriptingEnums::GcButtonNameEnum::TriggerR:
    return (int)controller_state_arg_holder->controller_state_val.TriggerR;

  case ScriptingEnums::GcButtonNameEnum::X:
    return (int)controller_state_arg_holder->controller_state_val.X;

  case ScriptingEnums::GcButtonNameEnum::Y:
    return (int)controller_state_arg_holder->controller_state_val.Y;

  case ScriptingEnums::GcButtonNameEnum::Z:
    return (int)controller_state_arg_holder->controller_state_val.Z;

  default:
    return 0;
  }
}

void* CreateListOfPointsArgHolder_API_impl()
{
  std::vector<ImVec2> points_vector = std::vector<ImVec2>();
  return reinterpret_cast<void*>(CreateListOfPointsArgHolder(points_vector));
}

unsigned long long GetSizeOfListOfPointsArgHolder_impl(void* arg_holder_ptr)
{
  return castToArgHolderPtr(arg_holder_ptr)->list_of_points.size();
}

double GetListOfPointsXValueAtIndexForArgHolder_impl(void* arg_holder_ptr, unsigned int index)
{
  return static_cast<double>(castToArgHolderPtr(arg_holder_ptr)->list_of_points.at(index).x);
}

double GetListOfPointsYValueAtIndexForArgHolder_impl(void* arg_holder_ptr, unsigned int index)
{
  return static_cast<double>(castToArgHolderPtr(arg_holder_ptr)->list_of_points.at(index).y);
}

void ListOfPointsArgHolderPushBack_API_impl(void* arg_holder_ptr, double x, double y)
{
  ImVec2 new_point = {};
  new_point.x = static_cast<float>(x);
  new_point.y = static_cast<float>(y);
  castToArgHolderPtr(arg_holder_ptr)->list_of_points.push_back(new_point);
}

void* CreateRegistrationInputTypeArgHolder_API_impl(void* registration_input)
{
  return reinterpret_cast<void*>(CreateRegistrationInputTypeArgHolder(registration_input));
}

void* CreateRegistrationWithAutoDeregistrationInputTypeArgHolder_API_impl(
    void* auto_deregistration_input)
{
  return reinterpret_cast<void*>(
      CreateRegistrationWithAutoDeregistrationInputTypeArgHolder(auto_deregistration_input));
}

void* CreateRegistrationForButtonCallbackInputTypeArgHolder_API_impl(
    void* register_button_callback_input)
{
  return reinterpret_cast<void*>(
      CreateRegistrationForButtonCallbackInputTypeArgHolder(register_button_callback_input));
}

void* CreateUnregistrationInputTypeArgHolder_API_impl(void* unregistration_input)
{
  return reinterpret_cast<void*>(CreateUnregistrationInputTypeArgHolder(unregistration_input));
}

void* GetVoidPointerFromArgHolder_impl(void* arg_holder_ptr)
{
  return castToArgHolderPtr(arg_holder_ptr)->void_pointer_val;
}

int GetBoolFromArgHolder_impl(void* arg_holder_ptr)
{
  return (castToArgHolderPtr(arg_holder_ptr)->bool_val) ? 1 : 0;
}

unsigned long long GetU8FromArgHolder_impl(void* arg_holder_ptr)
{
  return static_cast<unsigned long long>(castToArgHolderPtr(arg_holder_ptr)->u8_val);
}

unsigned long long GetU16FromArgHolder_impl(void* arg_holder_ptr)
{
  return static_cast<unsigned long long>(castToArgHolderPtr(arg_holder_ptr)->u16_val);
}

unsigned long long GetU32FromArgHolder_impl(void* arg_holder_ptr)
{
  return static_cast<unsigned long long>(castToArgHolderPtr(arg_holder_ptr)->u32_val);
}

unsigned long long GetU64FromArgHolder_impl(void* arg_holder_ptr)
{
  return castToArgHolderPtr(arg_holder_ptr)->u64_val;
}

signed long long GetS8FromArgHolder_impl(void* arg_holder_ptr)
{
  return static_cast<signed long long>(castToArgHolderPtr(arg_holder_ptr)->s8_val);
}

signed long long GetS16FromArgHolder_impl(void* arg_holder_ptr)
{
  return static_cast<signed long long>(castToArgHolderPtr(arg_holder_ptr)->s16_val);
}

signed long long GetS32FromArgHolder_impl(void* arg_holder_ptr)
{
  return static_cast<signed long long>(castToArgHolderPtr(arg_holder_ptr)->s32_val);
}

signed long long GetS64FromArgHolder_impl(void* arg_holder_ptr)
{
  return castToArgHolderPtr(arg_holder_ptr)->s64_val;
}

double GetFloatFromArgHolder_impl(void* arg_holder_ptr)
{
  return static_cast<double>(castToArgHolderPtr(arg_holder_ptr)->float_val);
}

double GetDoubleFromArgHolder_impl(void* arg_holder_ptr)
{
  return castToArgHolderPtr(arg_holder_ptr)->double_val;
}

// WARNING: The const char* returned from this method is only valid as long as the ArgHolder* is
// valid (i.e. until its delete method is called).
const char* GetStringFromArgHolder_impl(void* arg_holder_ptr)
{
  return castToArgHolderPtr(arg_holder_ptr)->string_val.c_str();
}

const char* GetErrorStringFromArgHolder_impl(void* arg_holder_ptr)
{
  return castToArgHolderPtr(arg_holder_ptr)->error_string_val.c_str();
}

void Delete_ArgHolder_impl(void* arg_holder_ptr)
{
  delete (castToArgHolderPtr(arg_holder_ptr));
}

}  // namespace Scripting
