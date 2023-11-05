#include "Core/Scripting/HelperClasses/ArgHolder_API_Implementations.h"

#include "Core/Scripting/CoreScriptInterface/Enums/ArgTypeEnum.h"
#include "Core/Scripting/CoreScriptInterface/Enums/GCButtonNameEnum.h"
#include "Core/Scripting/HelperClasses/ArgHolder.h"

namespace Scripting
{

ArgHolder* castToArgHolderPtr(void* input)
{
  return reinterpret_cast<ArgHolder*>(input);
}

void* castToVoidPtr(ArgHolder* input)
{
  return reinterpret_cast<void*>(input);
}

int GetArgType_API_impl(void* arg_holder_ptr)
{
  ArgHolder* castedArgHolderPtr = castToArgHolderPtr(arg_holder_ptr);
  return (int)castedArgHolderPtr->argument_type;
}

int GetIsEmpty_API_impl(void* arg_holder_ptr)
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

void* CreateStringArgHolder_API_impl(const char* new_string_value)
{
  return castToVoidPtr(CreateStringArgHolder(new_string_value));
}

void* CreateVoidPointerArgHolder_API_impl(void* new_void_ptr)
{
  return castToVoidPtr(CreateVoidPointerArgHolder(new_void_ptr));
}

void* CreateBytesArgHolder_API_impl()
{
  std::vector<s16> bytes_list = std::vector<s16>();
  return castToVoidPtr(CreateBytesListArgHolder(bytes_list));
}

unsigned long long GetSizeOfBytesArgHolder_API_impl(void* bytes_list_arg_holder_ptr)
{
  return castToArgHolderPtr(bytes_list_arg_holder_ptr)->bytes_list.size();
}

void AddByteToBytesArgHolder_API_impl(void* bytes_list_arg_holder_ptr, signed long long new_s64)
{
  s16 s16_val = static_cast<s16>(new_s64);
  castToArgHolderPtr(bytes_list_arg_holder_ptr)->bytes_list.push_back(s16_val);
}

signed long long GetByteFromBytesArgHolderAtIndex_API_impl(void* bytes_list_arg_holder_ptr,
                                                           unsigned long long index)
{
  return castToArgHolderPtr(bytes_list_arg_holder_ptr)->bytes_list.at(index);
}

void* CreateGameCubeControllerStateArgHolder_API_impl()
{
  Movie::ControllerState game_cube_controller_state = {};
  game_cube_controller_state.A = false;
  game_cube_controller_state.AnalogStickX = 128;
  game_cube_controller_state.AnalogStickY = 128;
  game_cube_controller_state.B = false;
  game_cube_controller_state.CStickX = 128;
  game_cube_controller_state.CStickY = 128;
  game_cube_controller_state.DPadDown = false;
  game_cube_controller_state.DPadLeft = false;
  game_cube_controller_state.DPadRight = false;
  game_cube_controller_state.DPadUp = false;
  game_cube_controller_state.disc = false;
  game_cube_controller_state.get_origin = false;
  game_cube_controller_state.is_connected = true;
  game_cube_controller_state.L = false;
  game_cube_controller_state.R = false;
  game_cube_controller_state.reset = false;
  game_cube_controller_state.Start = false;
  game_cube_controller_state.TriggerL = 0;
  game_cube_controller_state.TriggerR = 0;
  game_cube_controller_state.X = false;
  game_cube_controller_state.Y = false;
  game_cube_controller_state.Z = false;

  return reinterpret_cast<void*>(
      CreateGameCubeControllerStateArgHolder(game_cube_controller_state));
}

void SetGameCubeControllerStateArgHolderValue_API_impl(void* input_arg_holder_ptr,
                                                       int gc_button_name,
                                                       unsigned char raw_button_value)
{
  u8 button_value = static_cast<u8>(raw_button_value);
  ArgHolder* game_cube_controller_state_arg_holder = castToArgHolderPtr(input_arg_holder_ptr);
  GCButtonNameEnum button_name_enum = (GCButtonNameEnum)(gc_button_name);

  switch (button_name_enum)
  {
  case GCButtonNameEnum::A:
    game_cube_controller_state_arg_holder->game_cube_controller_state_val.A = (button_value != 0);
    return;
  case GCButtonNameEnum::AnalogStickX:
    game_cube_controller_state_arg_holder->game_cube_controller_state_val.AnalogStickX =
        button_value;
    return;
  case GCButtonNameEnum::AnalogStickY:
    game_cube_controller_state_arg_holder->game_cube_controller_state_val.AnalogStickY =
        button_value;
    return;
  case GCButtonNameEnum::B:
    game_cube_controller_state_arg_holder->game_cube_controller_state_val.B = (button_value != 0);
    return;
  case GCButtonNameEnum::CStickX:
    game_cube_controller_state_arg_holder->game_cube_controller_state_val.CStickX = button_value;
    return;
  case GCButtonNameEnum::CStickY:
    game_cube_controller_state_arg_holder->game_cube_controller_state_val.CStickY = button_value;
    return;
  case GCButtonNameEnum::Disc:
    game_cube_controller_state_arg_holder->game_cube_controller_state_val.disc =
        (button_value != 0);
    return;
  case GCButtonNameEnum::DPadDown:
    game_cube_controller_state_arg_holder->game_cube_controller_state_val.DPadDown =
        (button_value != 0);
    return;
  case GCButtonNameEnum::DPadLeft:
    game_cube_controller_state_arg_holder->game_cube_controller_state_val.DPadLeft =
        (button_value != 0);
    return;
  case GCButtonNameEnum::DPadRight:
    game_cube_controller_state_arg_holder->game_cube_controller_state_val.DPadRight =
        (button_value != 0);
    return;
  case GCButtonNameEnum::DPadUp:
    game_cube_controller_state_arg_holder->game_cube_controller_state_val.DPadUp =
        (button_value != 0);
    return;
  case GCButtonNameEnum::GetOrigin:
    game_cube_controller_state_arg_holder->game_cube_controller_state_val.get_origin =
        (button_value != 0);
    return;
  case GCButtonNameEnum::IsConnected:
    game_cube_controller_state_arg_holder->game_cube_controller_state_val.is_connected =
        (button_value != 0);
    return;
  case GCButtonNameEnum::L:
    game_cube_controller_state_arg_holder->game_cube_controller_state_val.L = (button_value != 0);
    return;
  case GCButtonNameEnum::R:
    game_cube_controller_state_arg_holder->game_cube_controller_state_val.R = (button_value != 0);
    return;
  case GCButtonNameEnum::Reset:
    game_cube_controller_state_arg_holder->game_cube_controller_state_val.reset =
        (button_value != 0);
    return;
  case GCButtonNameEnum::Start:
    game_cube_controller_state_arg_holder->game_cube_controller_state_val.Start =
        (button_value != 0);
    return;
  case GCButtonNameEnum::TriggerL:
    game_cube_controller_state_arg_holder->game_cube_controller_state_val.TriggerL = button_value;
    return;
  case GCButtonNameEnum::TriggerR:
    game_cube_controller_state_arg_holder->game_cube_controller_state_val.TriggerR = button_value;
    return;
  case GCButtonNameEnum::X:
    game_cube_controller_state_arg_holder->game_cube_controller_state_val.X = (button_value != 0);
    return;
  case GCButtonNameEnum::Y:
    game_cube_controller_state_arg_holder->game_cube_controller_state_val.Y = (button_value != 0);
    return;
  case GCButtonNameEnum::Z:
    game_cube_controller_state_arg_holder->game_cube_controller_state_val.Z = (button_value != 0);
    return;
  default:
    return;
  }
}
int GetGameCubeControllerStateArgHolderValue_API_impl(void* input_arg_holder_ptr,
                                                      int gc_button_name)
{
  ArgHolder* game_cube_controller_state_arg_holder = castToArgHolderPtr(input_arg_holder_ptr);
  GCButtonNameEnum button_name_enum = (GCButtonNameEnum)(gc_button_name);

  switch (button_name_enum)
  {
  case GCButtonNameEnum::A:
    return (int)game_cube_controller_state_arg_holder->game_cube_controller_state_val.A;

  case GCButtonNameEnum::AnalogStickX:
    return (int)game_cube_controller_state_arg_holder->game_cube_controller_state_val.AnalogStickX;

  case GCButtonNameEnum::AnalogStickY:
    return (int)game_cube_controller_state_arg_holder->game_cube_controller_state_val.AnalogStickY;

  case GCButtonNameEnum::B:
    return (int)game_cube_controller_state_arg_holder->game_cube_controller_state_val.B;

  case GCButtonNameEnum::CStickX:
    return (int)game_cube_controller_state_arg_holder->game_cube_controller_state_val.CStickX;

  case GCButtonNameEnum::CStickY:
    return (int)game_cube_controller_state_arg_holder->game_cube_controller_state_val.CStickY;

  case GCButtonNameEnum::Disc:
    return (int)game_cube_controller_state_arg_holder->game_cube_controller_state_val.disc;

  case GCButtonNameEnum::DPadDown:
    return (int)game_cube_controller_state_arg_holder->game_cube_controller_state_val.DPadDown;

  case GCButtonNameEnum::DPadLeft:
    return (int)game_cube_controller_state_arg_holder->game_cube_controller_state_val.DPadLeft;

  case GCButtonNameEnum::DPadRight:
    return (int)game_cube_controller_state_arg_holder->game_cube_controller_state_val.DPadRight;

  case GCButtonNameEnum::DPadUp:
    return (int)game_cube_controller_state_arg_holder->game_cube_controller_state_val.DPadUp;

  case GCButtonNameEnum::GetOrigin:
    return (int)game_cube_controller_state_arg_holder->game_cube_controller_state_val.get_origin;

  case GCButtonNameEnum::IsConnected:
    return (int)game_cube_controller_state_arg_holder->game_cube_controller_state_val.is_connected;

  case GCButtonNameEnum::L:
    return (int)game_cube_controller_state_arg_holder->game_cube_controller_state_val.L;

  case GCButtonNameEnum::R:
    return (int)game_cube_controller_state_arg_holder->game_cube_controller_state_val.R;

  case GCButtonNameEnum::Reset:
    return (int)game_cube_controller_state_arg_holder->game_cube_controller_state_val.reset;

  case GCButtonNameEnum::Start:
    return (int)game_cube_controller_state_arg_holder->game_cube_controller_state_val.Start;

  case GCButtonNameEnum::TriggerL:
    return (int)game_cube_controller_state_arg_holder->game_cube_controller_state_val.TriggerL;

  case GCButtonNameEnum::TriggerR:
    return (int)game_cube_controller_state_arg_holder->game_cube_controller_state_val.TriggerR;

  case GCButtonNameEnum::X:
    return (int)game_cube_controller_state_arg_holder->game_cube_controller_state_val.X;

  case GCButtonNameEnum::Y:
    return (int)game_cube_controller_state_arg_holder->game_cube_controller_state_val.Y;

  case GCButtonNameEnum::Z:
    return (int)game_cube_controller_state_arg_holder->game_cube_controller_state_val.Z;

  default:
    return 0;
  }
}

void* CreateListOfPointsArgHolder_API_impl()
{
  std::vector<ImVec2> list_of_points = std::vector<ImVec2>();
  return castToVoidPtr(CreateListOfPointsArgHolder(list_of_points));
}

unsigned long long GetSizeOfListOfPointsArgHolder_API_impl(void* list_of_points_arg_holder_ptr)
{
  return castToArgHolderPtr(list_of_points_arg_holder_ptr)->list_of_points.size();
}

double GetListOfPointsXValueAtIndexForArgHolder_API_impl(void* list_of_points_arg_holder_ptr,
                                                         unsigned int index)
{
  return static_cast<double>(
      castToArgHolderPtr(list_of_points_arg_holder_ptr)->list_of_points.at(index).x);
}

double GetListOfPointsYValueAtIndexForArgHolder_API_impl(void* list_of_points_arg_holder_ptr,
                                                         unsigned int index)
{
  return static_cast<double>(
      castToArgHolderPtr(list_of_points_arg_holder_ptr)->list_of_points.at(index).y);
}

void ListOfPointsArgHolderPushBack_API_impl(void* list_of_points_arg_holder_ptr, double x, double y)
{
  castToArgHolderPtr(list_of_points_arg_holder_ptr)
      ->list_of_points.push_back(ImVec2(static_cast<float>(x), static_cast<float>(y)));
}

void* CreateRegistrationInputTypeArgHolder_API_impl(void* registration_input)
{
  return castToVoidPtr(CreateRegistrationInputTypeArgHolder(registration_input));
}

void* CreateRegistrationWithAutoDeregistrationInputTypeArgHolder_API_impl(
    void* auto_deregistration_input)
{
  return castToVoidPtr(
      CreateRegistrationWithAutoDeregistrationInputTypeArgHolder(auto_deregistration_input));
}

void* CreateRegistrationForButtonCallbackInputTypeArgHolder_API_impl(
    void* registration_for_button_input)
{
  return castToVoidPtr(
      CreateRegistrationForButtonCallbackInputTypeArgHolder(registration_for_button_input));
}

void* CreateUnregistrationInputTypeArgHolder_API_impl(void* unregistration_input)
{
  return castToVoidPtr(CreateUnregistrationInputTypeArgHolder(unregistration_input));
}

void* GetVoidPointerFromArgHolder_API_impl(void* arg_holder_ptr)
{
  return castToArgHolderPtr(arg_holder_ptr)->void_pointer_val;
}

int GetBoolFromArgHolder_API_impl(void* arg_holder_ptr)
{
  return static_cast<int>(castToArgHolderPtr(arg_holder_ptr)->bool_val);
}

unsigned long long GetU8FromArgHolder_API_impl(void* arg_holder_ptr)
{
  return static_cast<unsigned long long>(castToArgHolderPtr(arg_holder_ptr)->u8_val);
}

unsigned long long GetU16FromArgHolder_API_impl(void* arg_holder_ptr)
{
  return static_cast<unsigned long long>(castToArgHolderPtr(arg_holder_ptr)->u16_val);
}

unsigned long long GetU32FromArgHolder_API_impl(void* arg_holder_ptr)
{
  return static_cast<unsigned long long>(castToArgHolderPtr(arg_holder_ptr)->u32_val);
}

unsigned long long GetU64FromArgHolder_API_impl(void* arg_holder_ptr)
{
  return castToArgHolderPtr(arg_holder_ptr)->u64_val;
}

signed long long GetS8FromArgHolder_API_impl(void* arg_holder_ptr)
{
  return static_cast<signed long long>(castToArgHolderPtr(arg_holder_ptr)->s8_val);
}

signed long long GetS16FromArgHolder_API_impl(void* arg_holder_ptr)
{
  return static_cast<signed long long>(castToArgHolderPtr(arg_holder_ptr)->s16_val);
}

signed long long GetS32FromArgHolder_API_impl(void* arg_holder_ptr)
{
  return static_cast<signed long long>(castToArgHolderPtr(arg_holder_ptr)->s32_val);
}

signed long long GetS64FromArgHolder_API_impl(void* arg_holder_ptr)
{
  return castToArgHolderPtr(arg_holder_ptr)->s64_val;
}

double GetFloatFromArgHolder_API_impl(void* arg_holder_ptr)
{
  return static_cast<double>(castToArgHolderPtr(arg_holder_ptr)->float_val);
}

double GetDoubleFromArgHolder_API_impl(void* arg_holder_ptr)
{
  return castToArgHolderPtr(arg_holder_ptr)->double_val;
}

const char* GetStringFromArgHolder_API_impl(void* arg_holder_ptr)
{
  return castToArgHolderPtr(arg_holder_ptr)->string_val.c_str();
}

const char* GetErrorStringFromArgHolder_API_impl(void* arg_holder_ptr)
{
  return castToArgHolderPtr(arg_holder_ptr)->error_string_val.c_str();
}

void Delete_ArgHolder_API_impl(void* arg_holder_ptr)
{
  delete (castToArgHolderPtr(arg_holder_ptr));
}

}  // namespace Scripting
