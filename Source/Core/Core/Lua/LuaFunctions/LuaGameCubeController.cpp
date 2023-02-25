#include "Core/Lua/LuaFunctions/LuaGameCubeController.h"

#include <fmt/format.h>
#include "Core/Lua/LuaVersionResolver.h"

namespace Lua::LuaGameCubeController
{
const char* class_name = "gcController";

std::array<bool, 4> overwrite_controller_at_specified_port = {};
std::array<Movie::ControllerState, 4> new_controller_inputs = {};
int current_controller_number_polled = -1;

std::array<Movie::ControllerState, 4> controller_inputs_on_last_frame = {};


class GcControllerLua
{
public:
  inline GcControllerLua(){};
};

static std::unique_ptr<GcControllerLua> gc_controller_pointer = nullptr;

GcControllerLua* getControllerInstance()
{
  if (gc_controller_pointer == nullptr)
    gc_controller_pointer = std::make_unique<GcControllerLua>(GcControllerLua());
  return gc_controller_pointer.get();
}

void InitLuaGameCubeControllerFunctions(lua_State* lua_state, const std::string& lua_api_version)
{
  GcControllerLua** gc_controller_lua_ptr_ptr =
      (GcControllerLua**)lua_newuserdata(lua_state, sizeof(GcControllerLua*));
  *gc_controller_lua_ptr_ptr = getControllerInstance();
  luaL_newmetatable(lua_state, "LuaGCControllerTable");
  lua_pushvalue(lua_state, -1);
  lua_setfield(lua_state, -2, "__index");

  std::array lua_gc_controller_functions_with_versions_attached = {
    luaL_Reg_With_Version({"getCurrentPortNumberOfPoll", "1.0", GetCurrentPortNumberOfPoll}),
    luaL_Reg_With_Version({"setInputsForPoll", "1.0", SetInputsForPoll}),
    luaL_Reg_With_Version({"getInputsForPoll", "1.0", GetInputsForPoll}),
    luaL_Reg_With_Version({"getInputsForPreviousFrame", "1.0", GetInputsForPreviousFrame})
  };

  std::unordered_map<std::string, std::string> deprecated_functions_map;
  AddLatestFunctionsForVersion(lua_gc_controller_functions_with_versions_attached, lua_api_version,
                               deprecated_functions_map, lua_state);
  lua_setglobal(lua_state, class_name);

  for (int i = 0; i < 4; ++i)
  {
    overwrite_controller_at_specified_port[i] = false;
    new_controller_inputs[i] = {};
    controller_inputs_on_last_frame[i] = {};
  }
}

std::string GetMagnitudeOutOfBoundsErrorMessage(const char* func_name, const char* button_name)
{
  return fmt::format("Error: in {}:{}() function, {} was outside bounds of 0 and 255!", class_name,
                     func_name, button_name);
}

int GetCurrentPortNumberOfPoll(lua_State* lua_state)
{
  LuaColonOperatorTypeCheck(lua_state, class_name, "GetCurrentPortNumberOfPoll", "()");
  lua_pushinteger(lua_state, current_controller_number_polled + 1);
  return 1;
}

// NOTE: In SI.cpp, UpdateDevices() is called to update each device, which moves exactly 8 bytes
// forward for each controller. Also, it moves in order from controllers 1 to 4.
int SetInputsForPoll(lua_State* lua_state)
{
  const char* func_name = "setInputs";
  LuaColonOperatorTypeCheck(lua_state, class_name, func_name, "(controllerValuesTable)");
  s64 controller_port_number = current_controller_number_polled;

  overwrite_controller_at_specified_port[controller_port_number] = true;
  memset(&new_controller_inputs[controller_port_number], 0,
         sizeof(Movie::ControllerState));
  new_controller_inputs[controller_port_number].is_connected = true;
  new_controller_inputs[controller_port_number].CStickX = 128;
  new_controller_inputs[controller_port_number].CStickY = 128;
  new_controller_inputs[controller_port_number].AnalogStickX = 128;
  new_controller_inputs[controller_port_number].AnalogStickY = 128;
  s64 magnitude = 0;

  lua_pushnil(lua_state); /* first key */
  while (lua_next(lua_state, 2) != 0)
  {
    /* uses 'key' (at index -2) and 'value' (at index -1) */
    const char* button_name = luaL_checkstring(lua_state, -2);
    if (button_name == nullptr || button_name[0] == '\0')
    {
      luaL_error(
          lua_state,
          fmt::format("Error: in {}:{} function, an empty string was passed for a button name!",
                      class_name, func_name)
              .c_str());
    }

    switch (ParseGCButton(button_name))
    {
    case GcButtonName::A:
      new_controller_inputs[controller_port_number].A = lua_toboolean(lua_state, -1);
      break;

    case GcButtonName::B:
      new_controller_inputs[controller_port_number].B = lua_toboolean(lua_state, -1);
      break;

    case GcButtonName::X:
      new_controller_inputs[controller_port_number].X = lua_toboolean(lua_state, -1);
      break;

    case GcButtonName::Y:
      new_controller_inputs[controller_port_number].Y = lua_toboolean(lua_state, -1);
      break;

    case GcButtonName::Z:
      new_controller_inputs[controller_port_number].Z = lua_toboolean(lua_state, -1);
      break;

    case GcButtonName::L:
      new_controller_inputs[controller_port_number].L = lua_toboolean(lua_state, -1);
      break;

    case GcButtonName::R:
      new_controller_inputs[controller_port_number].R = lua_toboolean(lua_state, -1);
      break;

    case GcButtonName::DPadUp:
      new_controller_inputs[controller_port_number].DPadUp =
          lua_toboolean(lua_state, -1);
      break;

    case GcButtonName::DPadDown:
      new_controller_inputs[controller_port_number].DPadDown =
          lua_toboolean(lua_state, -1);
      break;

    case GcButtonName::DPadLeft:
      new_controller_inputs[controller_port_number].DPadLeft =
          lua_toboolean(lua_state, -1);
      break;

    case GcButtonName::DPadRight:
      new_controller_inputs[controller_port_number].DPadRight =
          lua_toboolean(lua_state, -1);
      break;

    case GcButtonName::Start:
      new_controller_inputs[controller_port_number].Start =
          lua_toboolean(lua_state, -1);
      break;

    case GcButtonName::Reset:
      new_controller_inputs[controller_port_number].reset =
          lua_toboolean(lua_state, -1);
      break;

    case GcButtonName::AnalogStickX:
      magnitude = luaL_checkinteger(lua_state, -1);
      if (magnitude < 0 || magnitude > 255)
        luaL_error(lua_state,
                   GetMagnitudeOutOfBoundsErrorMessage(func_name, "analogStickX").c_str());
      new_controller_inputs[controller_port_number].AnalogStickX =
          static_cast<u8>(magnitude);
      break;

    case GcButtonName::AnalogStickY:
      magnitude = luaL_checkinteger(lua_state, -1);
      if (magnitude < 0 || magnitude > 255)
        luaL_error(lua_state,
                   GetMagnitudeOutOfBoundsErrorMessage(func_name, "analogStickY").c_str());
      new_controller_inputs[controller_port_number].AnalogStickY =
          static_cast<u8>(magnitude);
      break;

    case GcButtonName::CStickX:
      magnitude = luaL_checkinteger(lua_state, -1);
      if (magnitude < 0 || magnitude > 255)
        luaL_error(lua_state, GetMagnitudeOutOfBoundsErrorMessage(func_name, "cStickX").c_str());
      new_controller_inputs[controller_port_number].CStickX =
          static_cast<u8>(magnitude);
      break;

    case GcButtonName::CStickY:
      magnitude = luaL_checkinteger(lua_state, -1);
      if (magnitude < 0 || magnitude > 255)
        luaL_error(lua_state, GetMagnitudeOutOfBoundsErrorMessage(func_name, "cStickY").c_str());
      new_controller_inputs[controller_port_number].CStickY =
          static_cast<u8>(magnitude);
      break;

    case GcButtonName::TriggerL:
      magnitude = luaL_checkinteger(lua_state, -1);
      if (magnitude < 0 || magnitude > 255)
        luaL_error(lua_state, GetMagnitudeOutOfBoundsErrorMessage(func_name, "triggerL").c_str());
      new_controller_inputs[controller_port_number].TriggerL =
          static_cast<u8>(magnitude);
      ;
      break;

    case GcButtonName::TriggerR:
      magnitude = luaL_checkinteger(lua_state, -1);
      if (magnitude < 0 || magnitude > 255)
        luaL_error(lua_state, GetMagnitudeOutOfBoundsErrorMessage(func_name, "triggerR").c_str());
      new_controller_inputs[controller_port_number].TriggerR =
          static_cast<u8>(magnitude);
      ;
      break;

    default:
      luaL_error(
          lua_state,
          fmt::format(
              "Error: Unknown button name passed in as input to {}:{}(). Valid button "
              "names are: A, B, X, Y, Z, L, R, Z, dPadUp, dPadDown, dPadLeft, dPadRight, "
              "cStickX, cStickY, analogStickX, analogStickY, triggerL, triggerR, RESET, and START",
              class_name, func_name)
              .c_str());
    }

    lua_pop(lua_state, 1);
  }

  return 0;
}

int GetInputsForPoll(lua_State* lua_state)
{
  const char* func_name = "getInputsForPoll";
  LuaColonOperatorTypeCheck(lua_state, class_name, func_name, "()");
  s64 controller_port_number = current_controller_number_polled;
  Movie::ControllerState current_controller_state = new_controller_inputs[controller_port_number];
  bool button_pressed = false;
  s64 magnitude = 0;

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

  lua_newtable(lua_state);
  for (GcButtonName button_code : all_gc_buttons)
  {
    const char* button_name_string = ConvertButtonEnumToString(button_code);
    lua_pushlstring(lua_state, button_name_string, std::strlen(button_name_string));

    switch (button_code)
    {
    case GcButtonName::A:
      button_pressed = current_controller_state.A;
      lua_pushboolean(lua_state, button_pressed);
      break;
    case GcButtonName::B:
      button_pressed = current_controller_state.B;
      lua_pushboolean(lua_state, button_pressed);
      break;
    case GcButtonName::X:
      button_pressed = current_controller_state.X;
      lua_pushboolean(lua_state, button_pressed);
      break;
    case GcButtonName::Y:
      button_pressed = current_controller_state.Y;
      lua_pushboolean(lua_state, button_pressed);
      break;
    case GcButtonName::Z:
      button_pressed = current_controller_state.Z;
      lua_pushboolean(lua_state, button_pressed);
      break;
    case GcButtonName::L:
      button_pressed = current_controller_state.L;
      lua_pushboolean(lua_state, button_pressed);
      break;
    case GcButtonName::R:
      button_pressed = current_controller_state.R;
      lua_pushboolean(lua_state, button_pressed);
      break;
    case GcButtonName::Start:
      button_pressed = current_controller_state.Start;
      lua_pushboolean(lua_state, button_pressed);
      break;
    case GcButtonName::Reset:
      button_pressed = current_controller_state.reset;
      lua_pushboolean(lua_state, button_pressed);
      break;
    case GcButtonName::DPadUp:
      button_pressed = current_controller_state.DPadUp;
      lua_pushboolean(lua_state, button_pressed);
      break;
    case GcButtonName::DPadDown:
      button_pressed = current_controller_state.DPadDown;
      lua_pushboolean(lua_state, button_pressed);
      break;
    case GcButtonName::DPadLeft:
      button_pressed = current_controller_state.DPadLeft;
      lua_pushboolean(lua_state, button_pressed);
      break;
    case GcButtonName::DPadRight:
      button_pressed = current_controller_state.DPadRight;
      lua_pushboolean(lua_state, button_pressed);
      break;
    case GcButtonName::AnalogStickX:
      magnitude = current_controller_state.AnalogStickX;
      lua_pushinteger(lua_state, magnitude);
      break;
    case GcButtonName::AnalogStickY:
      magnitude = current_controller_state.AnalogStickY;
      lua_pushinteger(lua_state, magnitude);
      break;
    case GcButtonName::CStickX:
      magnitude = current_controller_state.CStickX;
      lua_pushinteger(lua_state, magnitude);
      break;
    case GcButtonName::CStickY:
      magnitude = current_controller_state.CStickY;
      lua_pushinteger(lua_state, magnitude);
      break;
    case GcButtonName::TriggerL:
      magnitude = current_controller_state.TriggerL;
      lua_pushinteger(lua_state, magnitude);
      break;
    case GcButtonName::TriggerR:
      magnitude = current_controller_state.TriggerR;
      lua_pushinteger(lua_state, magnitude);
      break;
    default:
      luaL_error(lua_state, fmt::format("An unexpected implementation error occured in {}:{}(). "
                                        "Did you modify the order of the enums in LuaGCButtons.h?",
                                        class_name, func_name)
                                .c_str());
      break;
    }
    lua_settable(lua_state, -3);
  }
  return 1;
}

int GetInputsForPreviousFrame(lua_State* lua_state)
{
  const char* func_name = "GetInputsForPreviousFrame";
  LuaColonOperatorTypeCheck(lua_state, class_name, func_name, "(1)");
  s64 controller_port_number = luaL_checkinteger(lua_state, 2);
  if (controller_port_number < 1 || controller_port_number > 4)
    luaL_error(
        lua_state,
        fmt::format("Error: in {}:{} function, controller port number was not between 1 and 4!",
                    class_name, func_name)
            .c_str());
  if (controller_port_number > Pad::GetConfig()->GetControllerCount())
    luaL_error(lua_state, fmt::format("Error: in {}:{} function, attempt was made to access a port "
                                      "which did not have a GameCube controller plugged into it!",
                                      class_name, func_name)
                              .c_str());

  Movie::ControllerState controller_inputs = controller_inputs_on_last_frame[controller_port_number - 1];
  bool button_pressed = false;
  s64 magnitude = 0;
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

  lua_newtable(lua_state);
  for (GcButtonName button_code : all_gc_buttons)
  {
    const char* button_name_string = ConvertButtonEnumToString(button_code);
    lua_pushlstring(lua_state, button_name_string, std::strlen(button_name_string));

    switch (button_code)
    {
    case GcButtonName::A:
      button_pressed = controller_inputs.A;
      lua_pushboolean(lua_state, button_pressed);
      break;
    case GcButtonName::B:
      button_pressed = controller_inputs.B;
      lua_pushboolean(lua_state, button_pressed);
      break;
    case GcButtonName::X:
      button_pressed = controller_inputs.X;
      lua_pushboolean(lua_state, button_pressed);
      break;
    case GcButtonName::Y:
      button_pressed = controller_inputs.Y;
      lua_pushboolean(lua_state, button_pressed);
      break;
    case GcButtonName::Z:
      button_pressed = controller_inputs.Z;
      lua_pushboolean(lua_state, button_pressed);
      break;
    case GcButtonName::L:
      button_pressed = controller_inputs.L;
      lua_pushboolean(lua_state, button_pressed);
      break;
    case GcButtonName::R:
      button_pressed = controller_inputs.R;
      lua_pushboolean(lua_state, button_pressed);
      break;
    case GcButtonName::Start:
      button_pressed = controller_inputs.Start;
      lua_pushboolean(lua_state, button_pressed);
      break;
    case GcButtonName::Reset:
      button_pressed = controller_inputs.reset;
      lua_pushboolean(lua_state, button_pressed);
      break;
    case GcButtonName::DPadUp:
      button_pressed = controller_inputs.DPadUp;
      lua_pushboolean(lua_state, button_pressed);
      break;
    case GcButtonName::DPadDown:
      button_pressed = controller_inputs.DPadDown;
      lua_pushboolean(lua_state, button_pressed);
      break;
    case GcButtonName::DPadLeft:
      button_pressed = controller_inputs.DPadLeft;
      lua_pushboolean(lua_state, button_pressed);
      break;
    case GcButtonName::DPadRight:
      button_pressed = controller_inputs.DPadRight;
      lua_pushboolean(lua_state, button_pressed);
      break;
    case GcButtonName::AnalogStickX:
      magnitude = controller_inputs.AnalogStickX;
      lua_pushinteger(lua_state, magnitude);
      break;
    case GcButtonName::AnalogStickY:
      magnitude = controller_inputs.AnalogStickY;
      lua_pushinteger(lua_state, magnitude);
      break;
    case GcButtonName::CStickX:
      magnitude = controller_inputs.CStickX;
      lua_pushinteger(lua_state, magnitude);
      break;
    case GcButtonName::CStickY:
      magnitude = controller_inputs.CStickY;
      lua_pushinteger(lua_state, magnitude);
      break;
    case GcButtonName::TriggerL:
      magnitude = controller_inputs.TriggerL;
      lua_pushinteger(lua_state, magnitude);
      break;
    case GcButtonName::TriggerR:
      magnitude = controller_inputs.TriggerR;
      lua_pushinteger(lua_state, magnitude);
      break;
    default:
      luaL_error(lua_state, fmt::format("An unexpected implementation error occured in {}:{}(). "
                                        "Did you modify the order of the enums in LuaGCButtons.h?",
                                        class_name, func_name)
                                .c_str());
      break;
    }
    lua_settable(lua_state, -3);
  }
  return 1;
}
}  // namespace Lua::LuaGameCubeController
