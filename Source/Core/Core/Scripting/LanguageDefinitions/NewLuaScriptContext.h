#pragma once
#include "fmt/format.h"
#include "lua.hpp"
#include <memory>
#include <utility>
#include <fstream>

#include "Core/Scripting/HelperClasses/ClassMetadata.h"
#include "Core/Lua/LuaFunctions/LuaBitFunctions.h"

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

  static Bit* GetBitInstance() { return std::make_unique<Bit>(Bit()).get(); }

  static void ImportModule(lua_State* lua_state, const std::string& api_version,
                    const std::string& api_name)
  {
    if (api_name == "bit")
    {
      Bit** bit_ptr_ptr = (Bit**)lua_newuserdata(lua_state, sizeof(Bit*));
      *bit_ptr_ptr = GetBitInstance();
      luaL_newmetatable(lua_state, "LuaBitMetaTable");
      lua_pushvalue(lua_state, -1);
      lua_setfield(lua_state, -2, "__index");
      ClassMetadata bitClassMetadata = BitApi::GetBitApiClassData(api_version);
      std::vector<luaL_Reg> final_lua_functions_list = std::vector<luaL_Reg>();

      for (auto& functionMetadata : bitClassMetadata.functions_list)
      {
        lua_CFunction lambdaFunction = [](lua_State* lua_state) mutable {
          std::string class_name = lua_tostring(lua_state, lua_upvalueindex(1));
          FunctionMetadata* localFunctionMetadata = (FunctionMetadata*)lua_touserdata(lua_state, lua_upvalueindex(2));
          std::vector<ArgHolder> arguments = std::vector<ArgHolder>();
          ArgHolder returnValue = {};
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

            default:
              luaL_error(lua_state, "Error: Unsupported type passed into Lua function...");
            }
            ++next_index_in_args;
          }

          returnValue = (localFunctionMetadata->function_pointer)(arguments);

          switch (returnValue.argument_type)
          {
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
          case ArgTypeEnum::ErrorStringType:
            luaL_error(lua_state, fmt::format("Error: in {}:{}() function, {}", class_name,
                                              localFunctionMetadata->function_name,
                                              returnValue.error_string_val)
                                      .c_str());
          default:
            luaL_error(lua_state, "Error: unsupported return type encountered in function!");
            return 0;
          }
        };

        FunctionMetadata* heap_metadata = new FunctionMetadata(functionMetadata);
        lua_pushstring(lua_state, heap_metadata->function_name.c_str());
        lua_pushstring(lua_state, bitClassMetadata.class_name.c_str());
        lua_pushlightuserdata(lua_state, heap_metadata);
        lua_pushcclosure(lua_state, lambdaFunction, 2);
        lua_settable(lua_state, -3);
      }

      lua_setmetatable(lua_state, -2);
      lua_setglobal(lua_state, "bit");
    }
  }

};
}  // namespace Scripting
