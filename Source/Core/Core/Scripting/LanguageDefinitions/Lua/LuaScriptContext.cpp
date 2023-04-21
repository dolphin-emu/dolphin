#include "Core/Scripting/LanguageDefinitions/Lua/LuaScriptContext.h"

namespace Scripting::Lua
{
const char* THIS_VARIABLE_NAME = "THIS__Internal984Z234";  // Making this something unlikely to overlap with a user-defined global.
int x = 0;

class UserdataClass
{
public:
  inline UserdataClass(){};
};

 UserdataClass* GetNewUserdataInstance()
{
  return std::make_unique<UserdataClass>(UserdataClass()).get();
}

    static std::string GetMagnitudeOutOfBoundsErrorMessage(const std::string& class_name,
                                                       const std::string& func_name,
                                                       const std::string& button_name)
{
  return fmt::format("Error: in {}:{}() function, {} was outside bounds of 0 and 255!", class_name,
                     func_name, button_name);
}

void LuaScriptContext::ImportModule(const std::string& api_name, const std::string& api_version)
{
  lua_State* current_lua_state_thread = this->main_lua_thread;
  if (this->current_script_call_location == ScriptCallLocations::FromFrameStartCallback)
    current_lua_state_thread = this->frame_callback_lua_thread;

  else if (this->current_script_call_location == ScriptCallLocations::FromInstructionHitCallback)
    current_lua_state_thread = this->instruction_address_hit_callback_lua_thread;

  else if (this->current_script_call_location == ScriptCallLocations::FromMemoryAddressReadFromCallback)
    current_lua_state_thread = this->memory_address_read_from_callback_lua_thread;

  else if (this->current_script_call_location == ScriptCallLocations::FromMemoryAddressWrittenToCallback)
    current_lua_state_thread = this->memory_address_written_to_callback_lua_thread;

  else if (this->current_script_call_location == ScriptCallLocations::FromGCControllerInputPolled)
    current_lua_state_thread = this->gc_controller_input_polled_callback_lua_thread;

  else if (this->current_script_call_location == ScriptCallLocations::FromWiiInputPolled)
    current_lua_state_thread = this->wii_input_polled_callback_lua_thread;

  UserdataClass** userdata_ptr_ptr = (UserdataClass**)lua_newuserdata(current_lua_state_thread, sizeof(UserdataClass*));
  *userdata_ptr_ptr = GetNewUserdataInstance();

  luaL_newmetatable(
      current_lua_state_thread,
      (std::string("Lua") + char(std::toupper(api_name[0])) + api_name.substr(1) + "MetaTable")
          .c_str());
  lua_pushvalue(current_lua_state_thread, -1);
  lua_setfield(current_lua_state_thread, -2, "__index");
  ClassMetadata classMetadata = Scripting::GetClassMetadataForModule(api_name, api_version);
  if (classMetadata.class_name.empty())
    luaL_error(
        current_lua_state_thread,
        fmt::format("Error: Could not find the module the user imported with the name {}", api_name)
            .c_str());

  std::vector<luaL_Reg> final_lua_functions_list = std::vector<luaL_Reg>();

  for (auto& functionMetadata : classMetadata.functions_list)
  {
    lua_CFunction lambdaFunction = [](lua_State* lua_state) mutable {
      std::string class_name = lua_tostring(lua_state, lua_upvalueindex(1));
      FunctionMetadata* localFunctionMetadata =
          (FunctionMetadata*)lua_touserdata(lua_state, lua_upvalueindex(2));
      lua_getglobal(lua_state, THIS_VARIABLE_NAME);
      LuaScriptContext* corresponding_script_context =
          (LuaScriptContext*)lua_touserdata(lua_state, -1);
      lua_pop(lua_state, 1);
      std::vector<ArgHolder> arguments = std::vector<ArgHolder>();
      ArgHolder returnValue = {};
      std::map<long long, u8> address_to_unsigned_byte_map;
      std::map<long long, s8> address_to_signed_byte_map;
      std::map<long long, s16> address_to_byte_map;
      std::vector<ImVec2> list_of_points;
      float point1;
      float point2;
      int debug = 0;
      Movie::ControllerState controller_state_arg;
      u8 magnitude = 0;
      long long key = 0;
      long long value = 0;
      bool button_pressed = false;
      int function_reference = -1;

      if (lua_type(lua_state, 1) != LUA_TUSERDATA)
      {
        luaL_error(lua_state,
                   fmt::format("Error: User attempted to call the {}:{}() function using the dot "
                               "operator. Please use the colon operator instead like this: {}:{}",
                               class_name, localFunctionMetadata->function_name, class_name,
                               localFunctionMetadata->example_function_call)
                       .c_str());
      }

      if (localFunctionMetadata->return_type == ArgTypeEnum::U64)
      {
        luaL_error(
            lua_state,
            fmt::format("Error: User attempted to call the {}:{}() function, which returns a "
                        "u64. The largest size type that Dolphin supports for Lua is s64. As "
                        "such, please use an s64 version of this function instead.",
                        class_name, localFunctionMetadata->function_name)
                .c_str());
      }
      int next_index_in_args = 2;
      for (ArgTypeEnum arg_type : localFunctionMetadata->arguments_list)
      {
        switch (arg_type)
        {
        case ArgTypeEnum::Boolean:
          arguments.push_back(
              CreateBoolArgHolder(lua_toboolean(lua_state, next_index_in_args)));
          break;
        case ArgTypeEnum::U8:
          arguments.push_back(CreateU8ArgHolder(luaL_checkinteger(lua_state, next_index_in_args)));
          break;
        case ArgTypeEnum::U16:
          arguments.push_back(CreateU16ArgHolder(luaL_checkinteger(lua_state, next_index_in_args)));
          break;
        case ArgTypeEnum::U32:
          arguments.push_back(CreateU32ArgHolder(luaL_checkinteger(lua_state, next_index_in_args)));
          break;
        case ArgTypeEnum::U64:
          luaL_error(
              lua_state,
              fmt::format("Error: User attempted to call the {}:{}() function, which takes a "
                          "u64 as one of its parameters. The largest type supported in Dolphin "
                          "for Lua is s64. Please use an s64 version of the function instead.",
                          class_name, localFunctionMetadata->function_name)
                  .c_str());
          break;
        case ArgTypeEnum::S8:
          arguments.push_back(CreateS8ArgHolder(luaL_checkinteger(lua_state, next_index_in_args)));
          break;
        case ArgTypeEnum::S16:
          arguments.push_back(CreateS16ArgHolder(luaL_checkinteger(lua_state, next_index_in_args)));
          break;
        case ArgTypeEnum::Integer:
          arguments.push_back(CreateIntArgHolder(luaL_checkinteger(lua_state, next_index_in_args)));
          break;
        case ArgTypeEnum::LongLong:
          arguments.push_back(
              CreateLongLongArgHolder(luaL_checkinteger(lua_state, next_index_in_args)));
          break;
        case ArgTypeEnum::Float:
          arguments.push_back(
              CreateFloatArgHolder(luaL_checknumber(lua_state, next_index_in_args)));
          break;
        case ArgTypeEnum::Double:
          arguments.push_back(
              CreateDoubleArgHolder(luaL_checknumber(lua_state, next_index_in_args)));
          break;
        case ArgTypeEnum::String:
          arguments.push_back(
              CreateStringArgHolder(luaL_checkstring(lua_state, next_index_in_args)));
          break;
        case ArgTypeEnum::RegistrationInputType:
          lua_pushvalue(lua_state, next_index_in_args);
          function_reference = luaL_ref(lua_state, LUA_REGISTRYINDEX);
          arguments.push_back(CreateRegistrationInputTypeArgHolder(*((void**)(&function_reference))));
          break;
        case ArgTypeEnum::RegistrationWithAutoDeregistrationInputType:
          lua_pushvalue(lua_state, next_index_in_args);
          function_reference = luaL_ref(lua_state, LUA_REGISTRYINDEX);
          arguments.push_back(
              CreateRegistrationWithAutoDeregistrationInputTypeArgHolder(*((void**)(&function_reference))));
          break;
        case ArgTypeEnum::RegistrationForButtonCallbackInputType:
          if (!corresponding_script_context->IsButtonRegistered(
                  arguments[arguments.size() - 1].long_long_val)) // this is a terrible hack - but it works to prevent a button callback from being added to Lua's stack once every frame.
          {
            lua_pushvalue(lua_state, next_index_in_args);
            function_reference = luaL_ref(lua_state, LUA_REGISTRYINDEX);
          }
          else
          {
            function_reference = -1;
          }
            arguments.push_back(CreateRegistrationForButtonCallbackInputTypeArgHolder(*((void**)(&function_reference))));
          break;
        case ArgTypeEnum::UnregistrationInputType:
          function_reference = lua_tointeger(lua_state, next_index_in_args);
          luaL_unref(lua_state, LUA_REGISTRYINDEX, function_reference);
          arguments.push_back(CreateUnregistrationInputTypeArgHolder(*((void**)(&function_reference))));
          break;
        case ArgTypeEnum::AddressToByteMap:
          address_to_byte_map = std::map<long long, s16>();
          lua_pushnil(lua_state);
          while (lua_next(lua_state, next_index_in_args))
          {
            key = lua_tointeger(lua_state, -2);
            value = lua_tointeger(lua_state, -1);

            if (key < 0)
              luaL_error(lua_state,
                         fmt::format("Error: in {}:{}() function, address of byte was less than 0!",
                                     class_name, localFunctionMetadata->function_name)
                             .c_str());
            if (value < -128)
              luaL_error(lua_state,
                         fmt::format("Error: in {}:{}() function, value of byte was less than "
                                     "-128, which can't be represented by 1 byte!",
                                     class_name, localFunctionMetadata->function_name)
                             .c_str());
            if (value > 255)
              luaL_error(lua_state,
                         fmt::format("Error: in {}:{}() function, value of byte was more than "
                                     "255, which can't be represented by 1 byte!",
                                     class_name, localFunctionMetadata->function_name)
                             .c_str());
            address_to_byte_map[key] = static_cast<s16>(value);
            lua_pop(lua_state, 1);
          }
          arguments.push_back(CreateAddressToByteMapArgHolder(address_to_byte_map));
          break;

        case ArgTypeEnum::ControllerStateObject:
          controller_state_arg = {};
          memset(&controller_state_arg, 0, sizeof(Movie::ControllerState));
          controller_state_arg.is_connected = true;
          controller_state_arg.CStickX = 128;
          controller_state_arg.CStickY = 128;
          controller_state_arg.AnalogStickX = 128;
          controller_state_arg.AnalogStickY = 128;
          lua_pushnil(lua_state);
          while (lua_next(lua_state, next_index_in_args) != 0)
          {
            // uses 'key' (at index -2) and 'value' (at index -1)
            const char* button_name = luaL_checkstring(lua_state, -2);
            if (button_name == nullptr || button_name[0] == '\0')
            {
              luaL_error(lua_state,
                         fmt::format("Error: in {}:{} function, an empty string was passed "
                                     "for a button name!",
                                     class_name, localFunctionMetadata->function_name)
                             .c_str());
            }

            switch (ParseGCButton(button_name))
            {
            case GcButtonName::A:
              controller_state_arg.A = lua_toboolean(lua_state, -1);
              break;

            case GcButtonName::B:
              controller_state_arg.B = lua_toboolean(lua_state, -1);
              break;

            case GcButtonName::X:
              controller_state_arg.X = lua_toboolean(lua_state, -1);
              break;

            case GcButtonName::Y:
              controller_state_arg.Y = lua_toboolean(lua_state, -1);
              break;

            case GcButtonName::Z:
              controller_state_arg.Z = lua_toboolean(lua_state, -1);
              break;

            case GcButtonName::L:
              controller_state_arg.L = lua_toboolean(lua_state, -1);
              break;

            case GcButtonName::R:
              controller_state_arg.R = lua_toboolean(lua_state, -1);
              break;

            case GcButtonName::DPadUp:
              controller_state_arg.DPadUp = lua_toboolean(lua_state, -1);
              break;

            case GcButtonName::DPadDown:
              controller_state_arg.DPadDown = lua_toboolean(lua_state, -1);
              break;

            case GcButtonName::DPadLeft:
              controller_state_arg.DPadLeft = lua_toboolean(lua_state, -1);
              break;

            case GcButtonName::DPadRight:
              controller_state_arg.DPadRight = lua_toboolean(lua_state, -1);
              break;

            case GcButtonName::Start:
              controller_state_arg.Start = lua_toboolean(lua_state, -1);
              break;

            case GcButtonName::Reset:
              controller_state_arg.reset = lua_toboolean(lua_state, -1);
              break;

            case GcButtonName::AnalogStickX:
              magnitude = luaL_checkinteger(lua_state, -1);
              if (magnitude < 0 || magnitude > 255)
                luaL_error(lua_state,
                           GetMagnitudeOutOfBoundsErrorMessage(
                               class_name, localFunctionMetadata->function_name, "analogStickX")
                               .c_str());
              controller_state_arg.AnalogStickX = static_cast<u8>(magnitude);
              break;

            case GcButtonName::AnalogStickY:
              magnitude = luaL_checkinteger(lua_state, -1);
              if (magnitude < 0 || magnitude > 255)
                luaL_error(lua_state,
                           GetMagnitudeOutOfBoundsErrorMessage(
                               class_name, localFunctionMetadata->function_name, "analogStickY")
                               .c_str());
              controller_state_arg.AnalogStickY = static_cast<u8>(magnitude);
              break;

            case GcButtonName::CStickX:
              magnitude = luaL_checkinteger(lua_state, -1);
              if (magnitude < 0 || magnitude > 255)
                luaL_error(lua_state,
                           GetMagnitudeOutOfBoundsErrorMessage(
                               class_name, localFunctionMetadata->function_name, "cStickX")
                               .c_str());
              controller_state_arg.CStickX = static_cast<u8>(magnitude);
              break;

            case GcButtonName::CStickY:
              magnitude = luaL_checkinteger(lua_state, -1);
              if (magnitude < 0 || magnitude > 255)
                luaL_error(lua_state,
                           GetMagnitudeOutOfBoundsErrorMessage(
                               class_name, localFunctionMetadata->function_name, "cStickY")
                               .c_str());
              controller_state_arg.CStickY = static_cast<u8>(magnitude);
              break;

            case GcButtonName::TriggerL:
              magnitude = luaL_checkinteger(lua_state, -1);
              if (magnitude < 0 || magnitude > 255)
                luaL_error(lua_state,
                           GetMagnitudeOutOfBoundsErrorMessage(
                               class_name, localFunctionMetadata->function_name, "triggerL")
                               .c_str());
              controller_state_arg.TriggerL = static_cast<u8>(magnitude);
              break;

            case GcButtonName::TriggerR:
              magnitude = luaL_checkinteger(lua_state, -1);
              if (magnitude < 0 || magnitude > 255)
                luaL_error(lua_state,
                           GetMagnitudeOutOfBoundsErrorMessage(
                               class_name, localFunctionMetadata->function_name, "triggerR")
                               .c_str());
              controller_state_arg.TriggerR = static_cast<u8>(magnitude);
              break;

            default:
              luaL_error(
                  lua_state,
                  fmt::format("Error: Unknown button name passed in as input to {}:{}(). Valid "
                              "button "
                              "names are: A, B, X, Y, Z, L, R, Z, dPadUp, dPadDown, dPadLeft, "
                              "dPadRight, "
                              "cStickX, cStickY, analogStickX, analogStickY, triggerL, triggerR, "
                              "RESET, and START",
                              class_name, localFunctionMetadata->function_name)
                      .c_str());
            }

            lua_pop(lua_state, 1);
          }

          arguments.push_back(CreateControllerStateArgHolder(controller_state_arg));
          break;

          case ArgTypeEnum::ListOfPoints:
          list_of_points = std::vector<ImVec2>();
          lua_pushnil(lua_state);
          while (lua_next(lua_state, next_index_in_args) != 0)
          {
            debug = lua_tointeger(lua_state, -2);
            lua_pushnil(lua_state);
            lua_next(lua_state, -2);
            debug = lua_tointeger(lua_state, -2);
            point1 = lua_tonumber(lua_state, -1);
            lua_pop(lua_state, 1);
            lua_next(lua_state, -2);
            lua_tointeger(lua_state, -2);
            point2 = lua_tonumber(lua_state, -1);
            lua_pop(lua_state, 1);
            lua_next(lua_state, -2);
            lua_pop(lua_state, 1);
            list_of_points.push_back({point1, point2});
          }
          arguments.push_back(CreateListOfPointsArgHolder(list_of_points));
          break;

        default:
          luaL_error(lua_state, "Error: Unsupported type passed into Lua function...");
        }
        ++next_index_in_args;
      }

      returnValue =
          (localFunctionMetadata->function_pointer)(corresponding_script_context, arguments);

      if (!returnValue.contains_value)
      {
        lua_pushnil(lua_state);
        return 1;
      }

      switch (returnValue.argument_type)
      {
      case ArgTypeEnum::YieldType:
        lua_yield(lua_state, 0);
        return 0;
      case ArgTypeEnum::VoidType:
        return 0;
      case ArgTypeEnum::Boolean:
        lua_pushboolean(lua_state, returnValue.bool_val);
        return 1;
      case ArgTypeEnum::U8:
        lua_pushinteger(lua_state, returnValue.u8_val);
        return 1;
      case ArgTypeEnum::U16:
        lua_pushinteger(lua_state, returnValue.u16_val);
        return 1;
      case ArgTypeEnum::U32:
        lua_pushinteger(lua_state, returnValue.u32_val);
        return 1;
      case ArgTypeEnum::U64:
        luaL_error(lua_state,
                   fmt::format("Error: User called the {}:{}() function, which returned a value of "
                               "type u64. The largest value that Dolphin supports for Lua is s64. "
                               "Please call an s64 version of the function instead.",
                               class_name, localFunctionMetadata->function_name)
                       .c_str());
        break;
      case ArgTypeEnum::S8:
        lua_pushinteger(lua_state, returnValue.s8_val);
        return 1;
      case ArgTypeEnum::S16:
        lua_pushinteger(lua_state, returnValue.s16_val);
        return 1;
      case ArgTypeEnum::Integer:
        lua_pushinteger(lua_state, returnValue.int_val);
        return 1;
      case ArgTypeEnum::LongLong:
        lua_pushinteger(lua_state, returnValue.long_long_val);
        return 1;
      case ArgTypeEnum::Float:
        lua_pushnumber(lua_state, returnValue.float_val);
        return 1;
      case ArgTypeEnum::Double:
        lua_pushnumber(lua_state, returnValue.double_val);
        return 1;
      case ArgTypeEnum::String:
        lua_pushstring(lua_state, returnValue.string_val.c_str());
        return 1;
      case ArgTypeEnum::RegistrationReturnType:
        lua_pushinteger(lua_state, (*((int*)&(returnValue.void_pointer_val))));
        return 1;
      case ArgTypeEnum::RegistrationWithAutoDeregistrationReturnType:
      case ArgTypeEnum::UnregistrationReturnType:
        return 0;
      case ArgTypeEnum::ShutdownType:
        corresponding_script_context->ShutdownScript();
        return 0;
      case ArgTypeEnum::AddressToUnsignedByteMap:
        address_to_unsigned_byte_map = returnValue.address_to_unsigned_byte_map;
        lua_createtable(lua_state, static_cast<int>(address_to_unsigned_byte_map.size()), 0);
        for (auto& it : address_to_unsigned_byte_map)
        {
          long long address = it.first;
          u8 curr_byte = it.second;
          lua_pushinteger(lua_state, static_cast<lua_Integer>(curr_byte));
          lua_rawseti(lua_state, -2, address);
        }
        return 1;
      case ArgTypeEnum::AddressToSignedByteMap:
        address_to_signed_byte_map = returnValue.address_to_signed_byte_map;
        lua_createtable(lua_state, static_cast<int>(address_to_signed_byte_map.size()), 0);
        for (auto& it : address_to_signed_byte_map)
        {
          long long address = it.first;
          s8 curr_byte = it.second;
          lua_pushinteger(lua_state, static_cast<lua_Integer>(curr_byte));
          lua_rawseti(lua_state, -2, address);
        }
        return 1;

      case ArgTypeEnum::ControllerStateObject:
        controller_state_arg = returnValue.controller_state_val;

        lua_newtable(lua_state);
        for (GcButtonName button_code : GetListOfAllButtons())
        {
          const char* button_name_string = ConvertButtonEnumToString(button_code);
          lua_pushlstring(lua_state, button_name_string, std::strlen(button_name_string));

          switch (button_code)
          {
          case GcButtonName::A:
            button_pressed = controller_state_arg.A;
            lua_pushboolean(lua_state, button_pressed);
            break;
          case GcButtonName::B:
            button_pressed = controller_state_arg.B;
            lua_pushboolean(lua_state, button_pressed);
            break;
          case GcButtonName::X:
            button_pressed = controller_state_arg.X;
            lua_pushboolean(lua_state, button_pressed);
            break;
          case GcButtonName::Y:
            button_pressed = controller_state_arg.Y;
            lua_pushboolean(lua_state, button_pressed);
            break;
          case GcButtonName::Z:
            button_pressed = controller_state_arg.Z;
            lua_pushboolean(lua_state, button_pressed);
            break;
          case GcButtonName::L:
            button_pressed = controller_state_arg.L;
            lua_pushboolean(lua_state, button_pressed);
            break;
          case GcButtonName::R:
            button_pressed = controller_state_arg.R;
            lua_pushboolean(lua_state, button_pressed);
            break;
          case GcButtonName::Start:
            button_pressed = controller_state_arg.Start;
            lua_pushboolean(lua_state, button_pressed);
            break;
          case GcButtonName::Reset:
            button_pressed = controller_state_arg.reset;
            lua_pushboolean(lua_state, button_pressed);
            break;
          case GcButtonName::DPadUp:
            button_pressed = controller_state_arg.DPadUp;
            lua_pushboolean(lua_state, button_pressed);
            break;
          case GcButtonName::DPadDown:
            button_pressed = controller_state_arg.DPadDown;
            lua_pushboolean(lua_state, button_pressed);
            break;
          case GcButtonName::DPadLeft:
            button_pressed = controller_state_arg.DPadLeft;
            lua_pushboolean(lua_state, button_pressed);
            break;
          case GcButtonName::DPadRight:
            button_pressed = controller_state_arg.DPadRight;
            lua_pushboolean(lua_state, button_pressed);
            break;
          case GcButtonName::AnalogStickX:
            magnitude = controller_state_arg.AnalogStickX;
            lua_pushinteger(lua_state, magnitude);
            break;
          case GcButtonName::AnalogStickY:
            magnitude = controller_state_arg.AnalogStickY;
            lua_pushinteger(lua_state, magnitude);
            break;
          case GcButtonName::CStickX:
            magnitude = controller_state_arg.CStickX;
            lua_pushinteger(lua_state, magnitude);
            break;
          case GcButtonName::CStickY:
            magnitude = controller_state_arg.CStickY;
            lua_pushinteger(lua_state, magnitude);
            break;
          case GcButtonName::TriggerL:
            magnitude = controller_state_arg.TriggerL;
            lua_pushinteger(lua_state, magnitude);
            break;
          case GcButtonName::TriggerR:
            magnitude = controller_state_arg.TriggerR;
            lua_pushinteger(lua_state, magnitude);
            break;
          default:
            luaL_error(lua_state,
                       fmt::format("An unexpected implementation error occured in {}:{}(). "
                                   "Did you modify the order of the enums in LuaGCButtons.h?",
                                   class_name, localFunctionMetadata->function_name)
                           .c_str());
            break;
          }
          lua_settable(lua_state, -3);
        }
        return 1;
      case ArgTypeEnum::ErrorStringType:
        luaL_error(lua_state,
                   fmt::format("Error: in {}:{}() function, {}", class_name,
                               localFunctionMetadata->function_name, returnValue.error_string_val)
                       .c_str());
        return 0;
      default:
        luaL_error(lua_state, "Error: unsupported return type encountered in function!");
        return 0;
      }
      return 0;
    };

    FunctionMetadata* heap_function_metadata = new FunctionMetadata(functionMetadata);
    lua_pushstring(current_lua_state_thread, heap_function_metadata->function_name.c_str());
    lua_pushstring(current_lua_state_thread, classMetadata.class_name.c_str());
    lua_pushlightuserdata(current_lua_state_thread, heap_function_metadata);
    lua_pushcclosure(current_lua_state_thread, lambdaFunction, 2);
    lua_settable(current_lua_state_thread, -3);
  }

  lua_setmetatable(current_lua_state_thread, -2);
  lua_setglobal(current_lua_state_thread, api_name.c_str());
}
void LuaScriptContext::StartScript()
{
  int retVal = lua_resume(main_lua_thread, nullptr, 0, &Lua::x);
  if (retVal == LUA_YIELD)
    called_yielding_function_in_last_global_script_resume = true;
  else
  {
    called_yielding_function_in_last_global_script_resume = false;
    if (retVal == LUA_OK)
    {
      finished_with_global_code = true;
      if (ShouldCallEndScriptFunction())
        (*GetScriptEndCallback())(unique_script_identifier);
    }
    else
    {
      if (retVal == 2)
      {
        const char* error_msg = lua_tostring(main_lua_thread, -1);
        (*GetPrintCallback())(error_msg);
      }
      (*GetScriptEndCallback())(unique_script_identifier);
      is_script_active = false;
    }
  }

}
void LuaScriptContext::GenericRunCallbacksHelperFunction(lua_State*& current_lua_state, std::vector<int>& vector_of_callbacks, int& index_of_next_callback_to_run, bool& yielded_on_last_callback_call, bool yields_are_allowed)
{
  int ret_val = 0;
  if (!yields_are_allowed)
    index_of_next_callback_to_run = 0;
  else if (index_of_next_callback_to_run < 0 || index_of_next_callback_to_run >= vector_of_callbacks.size())
    index_of_next_callback_to_run = 0;

  for (index_of_next_callback_to_run; index_of_next_callback_to_run < vector_of_callbacks.size();
       ++index_of_next_callback_to_run)
  {
    if (yields_are_allowed && yielded_on_last_callback_call)
    {
      yielded_on_last_callback_call = false;
      ret_val = lua_resume(current_lua_state, nullptr, 0, &x);
    }
    else
    {
      yielded_on_last_callback_call = false;
      lua_rawgeti(current_lua_state, LUA_REGISTRYINDEX, vector_of_callbacks[index_of_next_callback_to_run]);
      ret_val = lua_resume(current_lua_state, nullptr, 0, &x);
    }

    if (ret_val == LUA_YIELD)
    {
      yielded_on_last_callback_call = true;
      break;
    }
    else
    {
      yielded_on_last_callback_call = false;
      if (ret_val != LUA_OK)
      {
        const char* error_msg = lua_tostring(current_lua_state, -1);
        (*GetPrintCallback())(error_msg);
        this->is_script_active = false;
        (*GetScriptEndCallback())(this->unique_script_identifier);
        return;
      }
    }
  }
}

void LuaScriptContext::RunGlobalScopeCode()
{
  if (ShouldCallEndScriptFunction())
  {
    ShutdownScript();
  }

  if (this->finished_with_global_code)
    return;
  int ret_val = lua_resume(this->main_lua_thread, nullptr, 0, &x);
  if (ret_val == LUA_YIELD)
    called_yielding_function_in_last_global_script_resume = true;
  else
  {
    called_yielding_function_in_last_global_script_resume = false;
    if (ret_val == LUA_OK) 
      this->finished_with_global_code = true;
    else
    {
      const char* error_msg = lua_tostring(this->main_lua_thread, -1);
      (*GetPrintCallback())(error_msg);
      this->is_script_active = false;
      (*GetScriptEndCallback())(this->unique_script_identifier);
    }
  }
}

void LuaScriptContext::RunOnFrameStartCallbacks()
{
  if (ShouldCallEndScriptFunction())
  {
    ShutdownScript();
  }

  this->GenericRunCallbacksHelperFunction(
      this->frame_callback_lua_thread, this->frame_callback_locations,
      this->index_of_next_frame_callback_to_execute,
      this->called_yielding_function_in_last_frame_callback_script_resume, true);
}

void LuaScriptContext::RunOnGCControllerPolledCallbacks()
{
  if (ShouldCallEndScriptFunction())
  {
    ShutdownScript();
  }

  int index_of_next_callback = 0;
  bool called_yield_previously = false;
  this->GenericRunCallbacksHelperFunction(this->gc_controller_input_polled_callback_lua_thread,
                                          this->gc_controller_input_polled_callback_locations,
                                          index_of_next_callback, called_yield_previously, false);
}


void LuaScriptContext::RunOnInstructionReachedCallbacks(u32 current_address)
{
  if (ShouldCallEndScriptFunction())
  {
    ShutdownScript();
  }

  int index_of_next_callback = 0;
  bool called_yield_previously = false;
  if (this->map_of_instruction_address_to_lua_callback_locations.size() == 0 || this->map_of_instruction_address_to_lua_callback_locations.count(current_address) == 0)
    return;
  this->GenericRunCallbacksHelperFunction(
      this->instruction_address_hit_callback_lua_thread,
      this->map_of_instruction_address_to_lua_callback_locations[current_address],
      index_of_next_callback, called_yield_previously, false);
}

void LuaScriptContext::RunOnMemoryAddressReadFromCallbacks(u32 current_memory_address)
{
  if (ShouldCallEndScriptFunction())
  {
    ShutdownScript();
  }

  int index_of_next_callback = 0;
  bool called_yield_previously = false;
  if (this->map_of_memory_address_read_from_to_lua_callback_locations.size() == 0 ||
      this->map_of_memory_address_read_from_to_lua_callback_locations.count(
          current_memory_address) == 0)
    return;
  this->GenericRunCallbacksHelperFunction(
      this->memory_address_read_from_callback_lua_thread,
      this->map_of_memory_address_read_from_to_lua_callback_locations[current_memory_address],
      index_of_next_callback, called_yield_previously, false);
}

void LuaScriptContext::RunOnMemoryAddressWrittenToCallbacks(u32 current_memory_address)
{
  if (ShouldCallEndScriptFunction())
  {
    ShutdownScript();
  }

  int index_of_next_callback = 0;
  bool called_yield_previously = false;
  if (this->map_of_memory_address_written_to_to_lua_callback_locations.size() == 0 ||
      this->map_of_memory_address_written_to_to_lua_callback_locations.count(
          current_memory_address) == 0)
    return;
  this->GenericRunCallbacksHelperFunction(
      this->memory_address_written_to_callback_lua_thread,
      this->map_of_memory_address_written_to_to_lua_callback_locations[current_memory_address],
      index_of_next_callback, called_yield_previously, false);
}

void LuaScriptContext::RunOnWiiInputPolledCallbacks()
{
  if (ShouldCallEndScriptFunction())
  {
    ShutdownScript();
  }

  int index_of_next_callback = 0;
  bool called_yield_previously = false;
  this->GenericRunCallbacksHelperFunction(this->wii_input_polled_callback_lua_thread,
                                          this->wii_controller_input_polled_callback_locations,
                                          index_of_next_callback, called_yield_previously, false);
}

void* LuaScriptContext::RegisterForVectorHelper(std::vector<int>& input_vector, void* callbacks)
{
  int function_reference = *((int*)(&callbacks));
  input_vector.push_back(function_reference);
  return callbacks;
}

void LuaScriptContext::RegisterForVectorWithAutoDeregistrationHelper(
  std::vector<int>& input_vector, void* callbacks,
  std::atomic<size_t>& number_of_auto_deregister_callbacks)
{
  int function_reference = *((int*)(&callbacks));
  input_vector.push_back(function_reference);
  number_of_auto_deregister_callbacks++;
}

bool LuaScriptContext::UnregisterForVectorHelper(std::vector<int>& input_vector, void* callbacks)
{
  int function_reference_to_delete = *((int*)(&callbacks));
  if (std::find(input_vector.begin(), input_vector.end(), function_reference_to_delete) == input_vector.end())
    return false;

  input_vector.erase((std::find(input_vector.begin(), input_vector.end(), function_reference_to_delete)));
  return true;
}

void* LuaScriptContext::RegisterForMapHelper(u32 address, std::unordered_map<u32, std::vector<int>>& input_map,
                                             void* callbacks)
{
  int function_reference = *((int*)(&callbacks));
  if (!input_map.count(address))
   input_map[address] = std::vector<int>();
  input_map[address].push_back(function_reference);
  return callbacks;
}

 void LuaScriptContext::RegisterForMapWithAutoDeregistrationHelper(u32 address, std::unordered_map<u32, std::vector<int>>& input_map, void* callbacks, std::atomic<size_t>& number_of_auto_deregistration_callbacks)
{
  int function_reference = *((int*)(&callbacks));
  if (!input_map.count(address))
   input_map[address] = std::vector<int>();
  input_map[address].push_back(function_reference);
  number_of_auto_deregistration_callbacks++;
 }


bool LuaScriptContext::UnregisterForMapHelper(u32 address, std::unordered_map<u32, std::vector<int>>& input_map, void* callbacks)
{
  int function_reference_to_delete = *((int*)(&callbacks));
  if (!input_map.count(address))
   return false;

  if (std::find(input_map[address].begin(), input_map[address].end(), function_reference_to_delete) ==
      input_map[address].end())
   return false;

  input_map[address].erase((std::find(input_map[address].begin(), input_map[address].end(),
                                      function_reference_to_delete)));
  return true;
}

 void* LuaScriptContext::RegisterOnFrameStartCallbacks(void* callbacks)
{
  return this->RegisterForVectorHelper(this->frame_callback_locations, callbacks);
 }

 void LuaScriptContext::RegisterOnFrameStartWithAutoDeregistrationCallbacks(void* callbacks)
 {
  this->RegisterForVectorWithAutoDeregistrationHelper(this->frame_callback_locations, callbacks, number_of_frame_callbacks_to_auto_deregister);
 }

bool LuaScriptContext::UnregisterOnFrameStartCallbacks(void* callbacks)
{
  return this->UnregisterForVectorHelper(this->frame_callback_locations, callbacks);
}

 void* LuaScriptContext::RegisterOnGCCControllerPolledCallbacks(void* callbacks)
{
  return this->RegisterForVectorHelper(this->gc_controller_input_polled_callback_locations, callbacks);
 }

 void LuaScriptContext::RegisterOnGCControllerPolledWithAutoDeregistrationCallbacks(void* callbacks)
 {
  return this->RegisterForVectorWithAutoDeregistrationHelper(
      this->gc_controller_input_polled_callback_locations, callbacks,
      number_of_gc_controller_input_callbacks_to_auto_deregister);
 }
 bool LuaScriptContext::UnregisterOnGCControllerPolledCallbacks(void* callbacks)
 {
  return this->UnregisterForVectorHelper(this->gc_controller_input_polled_callback_locations, callbacks);
 }

 void* LuaScriptContext::RegisterOnInstructionReachedCallbacks(u32 address, void* callbacks)
 {
  this->instructionsBreakpointHolder.AddBreakpoint(address);
  return this->RegisterForMapHelper(address, this->map_of_instruction_address_to_lua_callback_locations, callbacks);
 }

 void LuaScriptContext::RegisterOnInstructionReachedWithAutoDeregistrationCallbacks(u32 address,
                                                                                    void* callbacks)
 {
  this->instructionsBreakpointHolder.AddBreakpoint(address);
  this->RegisterForMapWithAutoDeregistrationHelper(
      address, this->map_of_instruction_address_to_lua_callback_locations, callbacks,
      number_of_instruction_address_callbacks_to_auto_deregister);
 }

 bool LuaScriptContext::UnregisterOnInstructionReachedCallbacks(u32 address, void* callbacks)
 {
  this->instructionsBreakpointHolder.RemoveBreakpoint(address);
  return this->UnregisterForMapHelper(address, this->map_of_instruction_address_to_lua_callback_locations, callbacks);
 }

 void* LuaScriptContext::RegisterOnMemoryAddressReadFromCallbacks(u32 memory_address, void* callbacks)
 {
  this->memoryAddressBreakpointsHolder.AddReadBreakpoint(memory_address);
  return this->RegisterForMapHelper(memory_address, this->map_of_memory_address_read_from_to_lua_callback_locations, callbacks);
 }

 void LuaScriptContext::RegisterOnMemoryAddressReadFromWithAutoDeregistrationCallbacks(
     u32 memory_address, void* callbacks)
 {
  this->memoryAddressBreakpointsHolder.AddReadBreakpoint(memory_address);
  this->RegisterForMapWithAutoDeregistrationHelper(
      memory_address, this->map_of_memory_address_read_from_to_lua_callback_locations, callbacks,
      number_of_memory_address_read_callbacks_to_auto_deregister);
 }

 bool LuaScriptContext::UnregisterOnMemoryAddressReadFromCallbacks(u32 memory_address, void* callbacks)
 {
  this->memoryAddressBreakpointsHolder.RemoveReadBreakpoint(memory_address);
  return this->UnregisterForMapHelper(memory_address, this->map_of_memory_address_read_from_to_lua_callback_locations, callbacks);
 }

 void* LuaScriptContext::RegisterOnMemoryAddressWrittenToCallbacks(u32 memory_address,
                                                                   void* callbacks)
 {
  this->memoryAddressBreakpointsHolder.AddWriteBreakpoint(memory_address);
  return this->RegisterForMapHelper(memory_address, this->map_of_memory_address_written_to_to_lua_callback_locations, callbacks);
 }

 void LuaScriptContext::RegisterOnMemoryAddressWrittenToWithAutoDeregistrationCallbacks(
     u32 memory_address, void* callbacks)
 {
  this->memoryAddressBreakpointsHolder.AddWriteBreakpoint(memory_address);
  this->RegisterForMapWithAutoDeregistrationHelper(
      memory_address, this->map_of_memory_address_written_to_to_lua_callback_locations, callbacks,
      number_of_memory_address_write_callbacks_to_auto_deregister);
 }

 bool LuaScriptContext::UnregisterOnMemoryAddressWrittenToCallbacks(u32 memory_address, void* callbacks)
 {
  this->memoryAddressBreakpointsHolder.RemoveWriteBreakpoint(memory_address);
  return this->UnregisterForMapHelper(memory_address, this->map_of_memory_address_written_to_to_lua_callback_locations, callbacks);
 }

 void* LuaScriptContext::RegisterOnWiiInputPolledCallbacks(void* callbacks)
 {
  return this->RegisterForVectorHelper(this->wii_controller_input_polled_callback_locations, callbacks);
 }

 void LuaScriptContext::RegisterOnWiiInputPolledWithAutoDeregistrationCallbacks(void* callbacks)
 {
  this->RegisterForVectorWithAutoDeregistrationHelper(
      this->wii_controller_input_polled_callback_locations, callbacks,
      number_of_wii_input_callbacks_to_auto_deregister);
 }
 bool LuaScriptContext::UnregisterOnWiiInputPolledCallbacks(void* callbacks)
 {
  return this->UnregisterForVectorHelper(this->wii_controller_input_polled_callback_locations, callbacks);
 }


void LuaScriptContext::RegisterButtonCallback(long long button_id, void* callbacks)
 {
  int function_reference = *((int*)(&callbacks));
  if (function_reference >= 0)
    map_of_button_id_to_callback[button_id] = function_reference;
}

bool LuaScriptContext::IsButtonRegistered(long long button_id)
{
  return map_of_button_id_to_callback.count(button_id) > 0;
}

 void LuaScriptContext::GetButtonCallbackAndAddToQueue(long long button_id)
{
  int callback = map_of_button_id_to_callback[button_id];
  button_callbacks_to_run.push(callback);
}


 void LuaScriptContext::RunButtonCallbacksInQueue()
 {
  bool yielded_on_last_call = false;
  int next_index = 0;
  std::vector<int> vector_of_functions_to_run;
  while (!button_callbacks_to_run.IsEmpty())
  {
   int function_reference = button_callbacks_to_run.pop();
   vector_of_functions_to_run.push_back(function_reference);

  }
  this->GenericRunCallbacksHelperFunction(button_callback_thread, vector_of_functions_to_run, next_index,
                                          yielded_on_last_call, true);
 }

 }  // namespace Scripting
