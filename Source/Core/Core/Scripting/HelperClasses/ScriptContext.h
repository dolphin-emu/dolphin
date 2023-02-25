#pragma once
#include <map>
#include <string>
#include "Common/CommonTypes.h"
#include "Core/Movie.h"

template <typename FunctionRegistrationListType, typename ReturnTypeForString, typename ReturnTypeForU8, typename ReturnTypeForU16, typename ReturnTypeForU32,
typename ReturnTypeForS8, typename ReturnTypeForS16, typename ReturnTypeForInt, typename ReturnTypeForLongLong, typename ReturnTypeForFloat, typename ReturnTypeForDouble,
typename ReturnTypeForControllerState, typename ReturnTypeForByteArray, typename ReturnTypeForVoid, typename FirstArgType, typename... RemainingArgTypes>
class ScriptContext
{
public:
  virtual size_t GetFirstArgumentIndex() = 0;
  virtual void RegisterFunctions(FunctionRegistrationListType function_registration_list) = 0;
  virtual void Init() = 0;

  virtual std::string GetStringArgumentAtIndex(int argument_index, FirstArgType first_arg, RemainingArgTypes... remaining_args) = 0;
  virtual u8 GetU8ArgumentAtIndex(int argument_index, FirstArgType first_arg, RemainingArgTypes... remaining_args) = 0;
  virtual u16 GetU16ArgumentAtIndex(int argument_index, FirstArgType first_arg, RemainingArgTypes... remaining_args) = 0;
  virtual u32 GetU32ArgumentAtIndex(int argument_index, FirstArgType first_arg, RemainingArgTypes... remaining_args) = 0;
  virtual s8 GetS8ArgumentAtIndex(int argument_index, FirstArgType first_arg, RemainingArgTypes... remaining_args) = 0;
  virtual s16 GetS16ArgumentAtIndex(int argument_index, FirstArgType first_arg, RemainingArgTypes... remaining_args) = 0;
  virtual int GetIntArgumentAtIndex(int argument_index, FirstArgType first_arg, RemainingArgTypes... remaining_args) = 0;
  virtual long long GetLongLongArgumentAtIndex(int argument_index, FirstArgType first_arg, RemainingArgTypes... remaining_args) = 0;
  virtual float GetFloatArgumentAtIndex(int argument_index, FirstArgType first_arg, RemainingArgTypes... remaining_args) = 0;
  virtual double GetDoubleArgumentAtIndex(int argument_index, FirstArgType first_arg, RemainingArgTypes... remaining_args) = 0;
  virtual Movie::ControllerState GetControllerStateAtIndex(int argument_index, FirstArgType first_arg, RemainingArgTypes... remaining_args) = 0;
  virtual std::map<size_t, u8> GetUnsignedByteArrayAtIndex(int argument_index, FirstArgType first_arg, RemainingArgTypes... remaining_args) = 0;
  virtual std::map<size_t, s8> GetSignedByteArrayAtIndex(int argument_index, FirstArgType first_arg, RemainingArgTypes... remaining_args) = 0;

  virtual ReturnTypeForString SetOrReturnString(std::string input_string, FirstArgType first_arg, RemainingArgTypes... remaining_args) = 0;
  virtual ReturnTypeForU8 SetOrReturnU8(u8 input_number, FirstArgType first_arg, RemainingArgTypes... remaining_args) = 0;
  virtual ReturnTypeForU16 SetOrReturnU16(u16 input_number, FirstArgType first_arg, RemainingArgTypes... remaining_args) = 0;
  virtual ReturnTypeForU32 SetOrReturnU32(u32 input_number, FirstArgType first_arg, RemainingArgTypes... remaining_args) = 0;
  virtual ReturnTypeForS8 SetOrReturnS8(s8 input_number, FirstArgType first_arg, RemainingArgTypes... remaining_args) = 0;
  virtual ReturnTypeForS16 SetOrReturnS16(s16 input_number, FirstArgType first_arg, RemainingArgTypes... remaining_args) = 0;
  virtual ReturnTypeForInt SetOrReturnInt(int input_number, FirstArgType first_arg, RemainingArgTypes... remaining_args) = 0;
  virtual ReturnTypeForLongLong SetOrReturnLongLong(long long input_number, FirstArgType first_arg, RemainingArgTypes... remaining_args) = 0;
  virtual ReturnTypeForFloat SetOrReturnFloat(float input_number, FirstArgType first_arg, RemainingArgTypes... remaining_args) = 0;
  virtual ReturnTypeForDouble SetOrReturnDouble(double input_number, FirstArgType first_arg, RemainingArgTypes... remaining_args) = 0;
  virtual ReturnTypeForControllerState SetOrReturnControllerState(Movie::ControllerState controller_state, FirstArgType first_arg, RemainingArgTypes... remaining_args) = 0;
  virtual ReturnTypeForByteArray SetOrReturnUnsignedByteArray(std::map<size_t, u8> unsigned_byte_array, FirstArgType first_arg, RemainingArgTypes... remaining_args) = 0;
  virtual ReturnTypeForVoid SetOrReturnNothing(FirstArgType first_arg, RemainingArgTypes... remaining_args) = 0;
};
