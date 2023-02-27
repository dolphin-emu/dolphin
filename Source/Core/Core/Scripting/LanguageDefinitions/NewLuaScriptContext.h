#pragma once
#include "fmt/format.h"
#include "lua.hpp"
#include <memory>
#include <utility>
#include <fstream>

#include "Core/Scripting/HelperClasses/ClassMetadata.h"
#include "Core/Lua/LuaFunctions/LuaBitFunctions.h"
#include "Core/Lua/LuaFunctions/LuaEmuFunctions.h"
#include "Core/Lua/LuaFunctions/LuaGameCubeController.h"
#include "Core/Lua/LuaFunctions/LuaStatistics.h"
#include "Core/Lua/LuaHelperClasses/LuaStateToScriptContextMap.h"

namespace Scripting
{
class NewLuaScriptContext
{
public:

  class Bit
  {
  public:
    inline Bit() {}
  };

  class Emu
  {
  public:
    inline Emu() {}
  };

  class GcController
  {
  public:
    inline GcController(){};
  };

  class Statistics
  {
  public:
    inline Statistics(){};
  };

  static Bit* GetBitInstance() { return std::make_unique<Bit>(Bit()).get(); }

  static Emu* GetEmuInstance() { return std::make_unique<Emu>(Emu()).get(); }

  static GcController* GetGcControllerInstance() { return std::make_unique<GcController>(GcController()).get(); }

  static Statistics* GetStatisticsInstance()
  {
    return std::make_unique<Statistics>(Statistics()).get();
  }

  static std::string GetMagnitudeOutOfBoundsErrorMessage(const std::string& class_name, const std::string& func_name, const std::string& button_name)
  {
    return fmt::format("Error: in {}:{}() function, {} was outside bounds of 0 and 255!",
                       class_name, func_name, button_name);
  }

  static void ImportModule(lua_State* lua_state, const std::string& api_version,
                    const std::string& api_name)
  {

    if (api_name == "bit")
    {
      Bit** bit_ptr_ptr = (Bit**)lua_newuserdata(lua_state, sizeof(Bit*));
      *bit_ptr_ptr = GetBitInstance();
    }
    else if (api_name == "emu")
    {
      Emu** emu_ptr_ptr = (Emu**)lua_newuserdata(lua_state, sizeof(Emu*));
      *emu_ptr_ptr = GetEmuInstance();
    }
    else if (api_name == "gcController")
    {
      GcController** gc_controller_ptr_ptr =
          (GcController**)lua_newuserdata(lua_state, sizeof(GcController*));
      *gc_controller_ptr_ptr = GetGcControllerInstance();
    }
    else if (api_name == "statistics")
    {
      Statistics** statistics_ptr_ptr =
          (Statistics**)lua_newuserdata(lua_state, sizeof(Statistics*));
      *statistics_ptr_ptr = GetStatisticsInstance();
    }

      luaL_newmetatable(lua_state, (std::string("Lua") + char(std::toupper(api_name[0])) + api_name.substr(1) + "MetaTable").c_str());
      lua_pushvalue(lua_state, -1);
      lua_setfield(lua_state, -2, "__index");
      ClassMetadata classMetadata = {};

      if (api_name == "bit")
        classMetadata = BitApi::GetBitApiClassData(api_version);
      else if (api_name == "emu")
        classMetadata = EmuApi::GetEmuApiClassData(api_version);
      else if (api_name == "gcController")
        classMetadata = GameCubeControllerApi::GetGameCubeControllerApiClassData(api_version);
      else if (api_name == "statistics")
        classMetadata = StatisticsApi::GetStatisticsApiClassData(api_version);

      std::vector<luaL_Reg> final_lua_functions_list = std::vector<luaL_Reg>();

      for (auto& functionMetadata : classMetadata.functions_list)
      {
        lua_CFunction lambdaFunction = [](lua_State* lua_state) mutable {
           std::string class_name = lua_tostring(lua_state, lua_upvalueindex(1));
          FunctionMetadata* localFunctionMetadata =
              (FunctionMetadata*)lua_touserdata(lua_state, lua_upvalueindex(2));
          lua_getglobal(lua_state, "StateToScriptContextMap");
          Lua::LuaStateToScriptContextMap* state_to_script_context_map =
              (Lua::LuaStateToScriptContextMap*)lua_touserdata(lua_state, -1);
          lua_pop(lua_state, 1);
          Lua::LuaScriptContext* corresponding_script_context =
              state_to_script_context_map->lua_state_to_script_context_pointer_map[lua_state];
          std::vector<ArgHolder> arguments = std::vector<ArgHolder>();
          ArgHolder returnValue = {};
          std::map<long long, u8> address_to_unsigned_byte_map;
          std::map<long long, s8> address_to_signed_byte_map;
          std::map<long long, s16> address_to_byte_map;
          Movie::ControllerState controller_state_arg;
          GcButtonName all_gc_buttons[] = {GcButtonName::A,
                                           GcButtonName::B,
                                           GcButtonName::X,
                                           GcButtonName::Y,
                                           GcButtonName::Z,
                                           GcButtonName::L,
                                           GcButtonName::R,
                                           GcButtonName::DPadUp,
                                           GcButtonName::DPadDown,
                                           GcButtonName::DPadLeft,
                                           GcButtonName::DPadRight,
                                           GcButtonName::AnalogStickX,
                                           GcButtonName::AnalogStickY,
                                           GcButtonName::CStickX,
                                           GcButtonName::CStickY,
                                           GcButtonName::TriggerL,
                                           GcButtonName::TriggerR,
                                           GcButtonName::Start,
                                           GcButtonName::Reset};
          u8 magnitude = 0;
          long long key = 0;
          long long value = 0;
          bool button_pressed = false;
          
          if (lua_type(lua_state, 1) != LUA_TUSERDATA)
          {
            luaL_error(
                lua_state,
                fmt::format("Error: User attempted to call the {}:{}() function using the dot "
                            "operator. Please use the colon operator instead like this: {}:{}",
                            class_name, localFunctionMetadata->function_name, class_name,
                            localFunctionMetadata->example_function_call)
                    .c_str());
          }

          int next_index_in_args = 2;
          for (ArgTypeEnum arg_type : localFunctionMetadata->arguments_list)
          {
            switch (arg_type)
            {
            case ArgTypeEnum::Boolean:
              arguments.push_back(
                  CreateBoolArgHolder(luaL_checkinteger(lua_state, next_index_in_args)));
              break;
            case ArgTypeEnum::U8:
              arguments.push_back(
                  CreateU8ArgHolder(luaL_checkinteger(lua_state, next_index_in_args)));
              break;
            case ArgTypeEnum::U16:
              arguments.push_back(
                  CreateU16ArgHolder(luaL_checkinteger(lua_state, next_index_in_args)));
              break;
            case ArgTypeEnum::U32:
              arguments.push_back(
                  CreateU32ArgHolder(luaL_checkinteger(lua_state, next_index_in_args)));
              break;
            case ArgTypeEnum::S8:
              arguments.push_back(
                  CreateS8ArgHolder(luaL_checkinteger(lua_state, next_index_in_args)));
              break;
            case ArgTypeEnum::S16:
              arguments.push_back(
                  CreateS16ArgHolder(luaL_checkinteger(lua_state, next_index_in_args)));
              break;
            case ArgTypeEnum::Integer:
              arguments.push_back(
                  CreateIntArgHolder(luaL_checkinteger(lua_state, next_index_in_args)));
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
            case ArgTypeEnum::AddressToByteMap:
              address_to_byte_map = std::map<long long, s16>();
              lua_pushnil(lua_state);
              while (lua_next(lua_state, next_index_in_args))
              {
                key = lua_tointeger(lua_state, -2);
                value = lua_tointeger(lua_state, -1);

                if (key < 0)
                  luaL_error(
                      lua_state,
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
                      fmt::format(
                          "Error: Unknown button name passed in as input to {}:{}(). Valid "
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

            default:
              luaL_error(lua_state, "Error: Unsupported type passed into Lua function...");
            }
            ++next_index_in_args;
          }

          returnValue =
              (localFunctionMetadata->function_pointer)(corresponding_script_context->current_call_location, arguments);

          
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
            for (GcButtonName button_code : all_gc_buttons)
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
            luaL_error(lua_state, fmt::format("Error: in {}:{}() function, {}", class_name,
                                              localFunctionMetadata->function_name,
                                              returnValue.error_string_val)
                                      .c_str());
            return 0;
          default:
            luaL_error(lua_state, "Error: unsupported return type encountered in function!");
            return 0;
          }
            return 0;
        };

        FunctionMetadata* heap_function_metadata = new FunctionMetadata(functionMetadata);
        lua_pushstring(lua_state, heap_function_metadata->function_name.c_str());
        lua_pushstring(lua_state, classMetadata.class_name.c_str());
        lua_pushlightuserdata(lua_state, heap_function_metadata);
        lua_pushcclosure(lua_state, lambdaFunction, 2);
        lua_settable(lua_state, -3);
      }

      lua_setmetatable(lua_state, -2);
      lua_setglobal(lua_state, api_name.c_str());
  }

};
}  // namespace Scripting
