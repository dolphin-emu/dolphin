#pragma once
#include "Core/Lua/LuaHelperClasses/LuaGCButtons.h"
#include "ScriptContext.h"
#include "fmt/format.h"
#include "lua.hpp"

std::string GetMagnitudeOutOfBoundsErrorMessage(const char* class_name, const char* func_name,
                                                const char* button_name)
{
  return fmt::format("Error: in {}:{}() function, {} was outside bounds of 0 and 255!", class_name,
                     func_name, button_name);
}

class LuaScriptContext
    : public ScriptContext<luaL_Reg[], int, int, int, int, int, int, int, int, int, int, int, int,
                           int, lua_State*, const char*, const char*>
{
public:

  size_t GetFirstArgumentIndex() { return 1; }

  virtual 
  void RegisterFunctions(luaL_Reg function_registration_list[]) {}

  void Init() {}

  std::string GetStringArgumentAtIndex(int argument_index, lua_State* lua_state,
                                       const char* class_name, const char* function_name)
  {
    return luaL_checkstring(lua_state, argument_index);
  }

  u8 GetU8ArgumentAtIndex(int argument_index, lua_State* lua_state, const char* class_name,
                          const char* function_name)
  {
    return static_cast<u8>(luaL_checkinteger(lua_state, argument_index));
  }

  u16 GetU16ArgumentAtIndex(int argument_index, lua_State* lua_state, const char* class_name,
                            const char* function_name)
  {
    return static_cast<u16>(luaL_checkinteger(lua_state, argument_index));
  }

  u32 GetU32ArgumentAtIndex(int argument_index, lua_State* lua_state, const char* class_name,
                            const char* function_name)
  {
    return static_cast<u32>(luaL_checkinteger(lua_state, argument_index));
  }

  s8 GetS8ArgumentAtIndex(int argument_index, lua_State* lua_state, const char* class_name,
                          const char* function_name)
  {
    return static_cast<s8>(luaL_checkinteger(lua_state, argument_index));
  }

  s16 GetS16ArgumentAtIndex(int argument_index, lua_State* lua_state, const char* class_name,
                            const char* function_name)
  {
    return static_cast<s16>(luaL_checkinteger(lua_state, argument_index));
  }

  int GetIntArgumentAtIndex(int argument_index, lua_State* lua_state, const char* class_name,
                            const char* function_name)
  {
    return static_cast<int>(luaL_checkinteger(lua_state, argument_index));
  }

  long long GetLongLongArgumentAtIndex(int argument_index, lua_State* lua_state,
                                       const char* class_name, const char* function_name)
  {
    return static_cast<long long>(luaL_checkinteger(lua_state, argument_index));
  }

  float GetFloatArgumentAtIndex(int argument_index, lua_State* lua_state, const char* class_name,
                                const char* function_name)
  {
    return static_cast<float>(luaL_checknumber(lua_state, argument_index));
  }

  double GetDoubleArgumentAtIndex(int argument_index, lua_State* lua_state, const char* class_name,
                                  const char* function_name)
  {
    return static_cast<double>(luaL_checknumber(lua_state, argument_index));
  }

  Movie::ControllerState GetControllerStateAtIndex(int argument_index, lua_State* lua_state,
                                                   const char* class_name,
                                                   const char* function_name)
  {
    Movie::ControllerState new_controller_state = {};
    memset(&new_controller_state, 0, sizeof(Movie::ControllerState));
    new_controller_state.is_connected = true;
    new_controller_state.CStickX = 128;
    new_controller_state.CStickY = 128;
    new_controller_state.AnalogStickX = 128;
    new_controller_state.AnalogStickY = 128;
    s64 magnitude = 0;
    lua_pushnil(lua_state); /* first key */
    while (lua_next(lua_state, argument_index) != 0)
    {
      /* uses 'key' (at index -2) and 'value' (at index -1) */
      const char* button_name = luaL_checkstring(lua_state, -2);
      if (button_name == nullptr || button_name[0] == '\0')
      {
        luaL_error(
            lua_state,
            fmt::format("Error: in {}:{} function, an empty string was passed for a button name!",
                        class_name, function_name)
                .c_str());
      }

      switch (ParseGCButton(button_name))
      {
      case GcButtonName::A:
        new_controller_state.A = lua_toboolean(lua_state, -1);
        break;

      case GcButtonName::B:
        new_controller_state.B = lua_toboolean(lua_state, -1);
        break;

      case GcButtonName::X:
        new_controller_state.X = lua_toboolean(lua_state, -1);
        break;

      case GcButtonName::Y:
        new_controller_state.Y = lua_toboolean(lua_state, -1);
        break;

      case GcButtonName::Z:
        new_controller_state.Z = lua_toboolean(lua_state, -1);
        break;

      case GcButtonName::L:
        new_controller_state.L = lua_toboolean(lua_state, -1);
        break;

      case GcButtonName::R:
        new_controller_state.R = lua_toboolean(lua_state, -1);
        break;

      case GcButtonName::DPadUp:
        new_controller_state.DPadUp = lua_toboolean(lua_state, -1);
        break;

      case GcButtonName::DPadDown:
        new_controller_state.DPadDown = lua_toboolean(lua_state, -1);
        break;

      case GcButtonName::DPadLeft:
        new_controller_state.DPadLeft = lua_toboolean(lua_state, -1);
        break;

      case GcButtonName::DPadRight:
        new_controller_state.DPadRight = lua_toboolean(lua_state, -1);
        break;

      case GcButtonName::Start:
        new_controller_state.Start = lua_toboolean(lua_state, -1);
        break;

      case GcButtonName::Reset:
        new_controller_state.reset = lua_toboolean(lua_state, -1);
        break;

      case GcButtonName::AnalogStickX:
        magnitude = luaL_checkinteger(lua_state, -1);
        if (magnitude < 0 || magnitude > 255)
          luaL_error(lua_state,
                     GetMagnitudeOutOfBoundsErrorMessage(class_name, function_name, "analogStickX")
                         .c_str());
        new_controller_state.AnalogStickX = static_cast<u8>(magnitude);
        break;

      case GcButtonName::AnalogStickY:
        magnitude = luaL_checkinteger(lua_state, -1);
        if (magnitude < 0 || magnitude > 255)
          luaL_error(lua_state,
                     GetMagnitudeOutOfBoundsErrorMessage(class_name, function_name, "analogStickY")
                         .c_str());
        new_controller_state.AnalogStickY = static_cast<u8>(magnitude);
        break;

      case GcButtonName::CStickX:
        magnitude = luaL_checkinteger(lua_state, -1);
        if (magnitude < 0 || magnitude > 255)
          luaL_error(
              lua_state,
              GetMagnitudeOutOfBoundsErrorMessage(class_name, function_name, "cStickX").c_str());
        new_controller_state.CStickX = static_cast<u8>(magnitude);
        break;

      case GcButtonName::CStickY:
        magnitude = luaL_checkinteger(lua_state, -1);
        if (magnitude < 0 || magnitude > 255)
          luaL_error(
              lua_state,
              GetMagnitudeOutOfBoundsErrorMessage(class_name, function_name, "cStickY").c_str());
        new_controller_state.CStickY = static_cast<u8>(magnitude);
        break;

      case GcButtonName::TriggerL:
        magnitude = luaL_checkinteger(lua_state, -1);
        if (magnitude < 0 || magnitude > 255)
          luaL_error(
              lua_state,
              GetMagnitudeOutOfBoundsErrorMessage(class_name, function_name, "triggerL").c_str());
        new_controller_state.TriggerL = static_cast<u8>(magnitude);
        ;
        break;

      case GcButtonName::TriggerR:
        magnitude = luaL_checkinteger(lua_state, -1);
        if (magnitude < 0 || magnitude > 255)
          luaL_error(
              lua_state,
              GetMagnitudeOutOfBoundsErrorMessage(class_name, function_name, "triggerR").c_str());
        new_controller_state.TriggerR = static_cast<u8>(magnitude);
        ;
        break;

      default:
        luaL_error(
            lua_state,
            fmt::format("Error: Unknown button name passed in as input to {}:{}(). Valid button "
                        "names are: A, B, X, Y, Z, L, R, Z, dPadUp, dPadDown, dPadLeft, dPadRight, "
                        "cStickX, cStickY, analogStickX, analogStickY, triggerL, triggerR, RESET, "
                        "and START",
                        class_name, function_name)
                .c_str());
      }

      lua_pop(lua_state, 1);
    }
    return new_controller_state;
  }

  std::map<size_t, u8> GetUnsignedByteArrayAtIndex(int argument_index, lua_State* lua_state,
                                                   const char* class_name, const char* func_name)
  {
    return std::map<size_t, u8>();  // TODO: actually implement this later.
  }

  std::map<size_t, s8> GetSignedByteArrayAtIndex(int argument_index, lua_State* lua_state,
                                                 const char* class_name, const char* func_name)
  {
    return std::map<size_t, s8>();  // TODO: actually implement this later
  }

  int SetOrReturnString(std::string input_string, lua_State* lua_state, const char* class_name,
                        const char* func_name)
  {
    lua_pushfstring(lua_state, input_string.c_str());
    return 1;
  }

  int SetOrReturnU8(u8 input_number, lua_State* lua_state, const char* class_name,
                    const char* func_name)
  {
    lua_pushinteger(lua_state, input_number);
    return 1;
  }

  int SetOrReturnU16(u16 input_number, lua_State* lua_state, const char* class_name,
                     const char* func_name)
  {
    lua_pushinteger(lua_state, input_number);
    return 1;
  }

  int SetOrReturnU32(u32 input_number, lua_State* lua_state, const char* class_name,
                     const char* func_name)
  {
    lua_pushinteger(lua_state, input_number);
    return 1;
  }

  int SetOrReturnS8(s8 input_number, lua_State* lua_state, const char* class_name,
                    const char* func_name)
  {
    lua_pushinteger(lua_state, input_number);
    return 1;
  }

  int SetOrReturnS16(s16 input_number, lua_State* lua_state, const char* class_name,
                     const char* func_name)
  {
    lua_pushinteger(lua_state, input_number);
    return 1;
  }

  int SetOrReturnInt(int input_number, lua_State* lua_state, const char* class_name,
                     const char* func_name)
  {
    lua_pushinteger(lua_state, input_number);
    return 1;
  }

  int SetOrReturnLongLong(long long input_number, lua_State* lua_state, const char* class_name,
                          const char* func_name)
  {
    lua_pushinteger(lua_state, input_number);
    return 1;
  }

  int SetOrReturnFloat(float input_number, lua_State* lua_state, const char* class_name,
                       const char* func_name)
  {
    lua_pushnumber(lua_state, input_number);
    return 1;
  }

  int SetOrReturnDouble(double input_number, lua_State* lua_state, const char* class_name,
                        const char* func_name)
  {
    lua_pushinteger(lua_state, input_number);
    return 1;
  }

  int SetOrReturnControllerState(Movie::ControllerState controller_state, lua_State* lua_state,
                                 const char* class_name, const char* func_name)
  {
    // TODO: Implement this
    return 1;
  }

  int SetOrReturnUnsignedByteArray(std::map<size_t, u8> unsigned_byte_array, lua_State* lua_state,
                           const char* class_name, const char* func_name)
  {
    // TODO: Implement this
    return 1;
  }

  int SetOrReturnNothing(lua_State* lua_State, const char* class_name, const char* func_name)
  {
    return 0;
  }
};
