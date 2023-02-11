#include "Core/Lua/LuaFunctions/LuaGameCubeController.h"
#include "Core/Lua/LuaVersionResolver.h"

namespace Lua::LuaGameCubeController
{

std::array<bool, 4> overwrite_controller_at_specified_port = {};
std::array<bool, 4> add_to_controller_at_specified_port = {};
std::array<bool, 4> do_random_input_events_at_specified_port = {};

std::array<Movie::ControllerState, 4> new_overwrite_controller_inputs = {};
std::array<Movie::ControllerState, 4> add_to_controller_inputs = {};
std::array<std::vector<GcButtonName>, 4> button_lists_for_add_to_controller_inputs = {};
std::array<std::vector<std::unique_ptr<LuaGameCubeButtonProbabilityEvent>>, 4>
    random_button_events = {};

class GcControllerLua
{
public:
  inline GcControllerLua(){};
};

static GcControllerLua* gc_controller_pointer = nullptr;

GcControllerLua* getControllerInstance()
{
  if (gc_controller_pointer == nullptr)
    gc_controller_pointer = new GcControllerLua();
  return gc_controller_pointer;
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
      luaL_Reg_With_Version({"setInputs", "1.0", SetInputs}),
      luaL_Reg_With_Version({"addInputs", "1.0", AddInputs}),
      luaL_Reg_With_Version({"addButtonFlipChance", "1.0", AddButtonFlipChance}),
      luaL_Reg_With_Version({"addButtonPressChance", "1.0", AddButtonPressChance}),
      luaL_Reg_With_Version({"addButtonReleaseChance", "1.0", AddButtonReleaseChance}),
      luaL_Reg_With_Version({"addOrSubtractFromCurrentAnalogValueChance", "1.0",
                             AddOrSubtractFromCurrentAnalogValueChance}),
      luaL_Reg_With_Version({"addOrSubtractFromSpecificAnalogValueChance", "1.0",
                             AddOrSubtractFromSpecificAnalogValueChance}),
      luaL_Reg_With_Version({"addButtonComboChance", "1.0", AddButtonComboChance}),
      luaL_Reg_With_Version({"addControllerClearChance", "1.0", AddControllerClearChance}),
      luaL_Reg_With_Version({"getControllerInputs", "1.0", GetControllerInputs})};

  std::unordered_map<std::string, std::string> deprecated_functions_map;
  AddLatestFunctionsForVersion(lua_gc_controller_functions_with_versions_attached, lua_api_version,
                               deprecated_functions_map, lua_state);
  lua_setglobal(lua_state, "gc_controller");

  for (int i = 0; i < 4; ++i)
  {
    overwrite_controller_at_specified_port[i] = false;
    add_to_controller_at_specified_port[i] = false;
    do_random_input_events_at_specified_port[i] = false;

    random_button_events[i] = std::vector<std::unique_ptr<LuaGameCubeButtonProbabilityEvent>>();
    button_lists_for_add_to_controller_inputs[i] = std::vector<GcButtonName>();
  }
}

int GetPortNumberHelperFunction(lua_State* lua_state, const char* func_name)
{
  s64 controller_port_number = luaL_checkinteger(lua_state, 2);
  if (controller_port_number < 1 || controller_port_number > 4)
  {
    luaL_error(lua_state, (std::string("Error: In function gc_controller:") + func_name +
                           ", controller port number wasn't between 1 and 4!")
                              .c_str());
  }

  if (controller_port_number > Pad::GetConfig()->GetControllerCount())
  {
    luaL_error(lua_state, (std::string("Error: in ") + func_name +
                           "(), attempt was made to access a port which did not have a GameCube "
                           "controller plugged into it!")
                              .c_str());
  }

  return controller_port_number;
}

double GetProbabilityHelperFunction(lua_State* lua_state, const char* func_name)
{
  double probability = 0.0;
  if (lua_isinteger(lua_state, 3))
    probability = static_cast<double>(lua_tointeger(lua_state, 3));
  else
    probability = luaL_checknumber(lua_state, 3);

  if (probability < 0.0 || probability > 100.0)
    luaL_error(lua_state, (std::string("Error: In function gc_controller:") + func_name +
                           ", probability was outside the acceptable range of 0 to 100%")
                              .c_str());
  return probability;
}

// NOTE: In SI.cpp, UpdateDevices() is called to update each device, which moves exactly 8 bytes
// forward for each controller. Also, it moves in order from controllers 1 to 4.
int SetInputs(lua_State* lua_state)
{
  LuaColonOperatorTypeCheck(lua_state, "setInputs",
                            "gc_controller:setInputs(1, controllerValuesTable)");
  s64 controller_port_number = GetPortNumberHelperFunction(lua_state, "setInputs");

  overwrite_controller_at_specified_port[controller_port_number - 1] = true;
  memset(&new_overwrite_controller_inputs[controller_port_number - 1], 0,
         sizeof(Movie::ControllerState));
  new_overwrite_controller_inputs[controller_port_number - 1].is_connected = true;
  new_overwrite_controller_inputs[controller_port_number - 1].CStickX = 128;
  new_overwrite_controller_inputs[controller_port_number - 1].CStickY = 128;
  new_overwrite_controller_inputs[controller_port_number - 1].AnalogStickX = 128;
  new_overwrite_controller_inputs[controller_port_number - 1].AnalogStickY = 128;
  s64 magnitude = 0;

  lua_pushnil(lua_state); /* first key */
  while (lua_next(lua_state, 3) != 0)
  {
    /* uses 'key' (at index -2) and 'value' (at index -1) */
    const char* button_name = luaL_checkstring(lua_state, -2);
    if (button_name == nullptr || button_name[0] == '\0')
    {
      luaL_error(lua_state,
                 "Error: in setInputs() function, an empty string was passed for a button name!");
    }

    switch (ParseGCButton(button_name))
    {
    case GcButtonName::A:
      new_overwrite_controller_inputs[controller_port_number - 1].A = lua_toboolean(lua_state, -1);
      break;

    case GcButtonName::B:
      new_overwrite_controller_inputs[controller_port_number - 1].B = lua_toboolean(lua_state, -1);
      break;

    case GcButtonName::X:
      new_overwrite_controller_inputs[controller_port_number - 1].X = lua_toboolean(lua_state, -1);
      break;

    case GcButtonName::Y:
      new_overwrite_controller_inputs[controller_port_number - 1].Y = lua_toboolean(lua_state, -1);
      break;

    case GcButtonName::Z:
      new_overwrite_controller_inputs[controller_port_number - 1].Z = lua_toboolean(lua_state, -1);
      break;

    case GcButtonName::L:
      new_overwrite_controller_inputs[controller_port_number - 1].L = lua_toboolean(lua_state, -1);
      break;

    case GcButtonName::R:
      new_overwrite_controller_inputs[controller_port_number - 1].R = lua_toboolean(lua_state, -1);
      break;

    case GcButtonName::DPadUp:
      new_overwrite_controller_inputs[controller_port_number - 1].DPadUp =
          lua_toboolean(lua_state, -1);
      break;

    case GcButtonName::DPadDown:
      new_overwrite_controller_inputs[controller_port_number - 1].DPadDown =
          lua_toboolean(lua_state, -1);
      break;

    case GcButtonName::DPadLeft:
      new_overwrite_controller_inputs[controller_port_number - 1].DPadLeft =
          lua_toboolean(lua_state, -1);
      break;

    case GcButtonName::DPadRight:
      new_overwrite_controller_inputs[controller_port_number - 1].DPadRight =
          lua_toboolean(lua_state, -1);
      break;

    case GcButtonName::Start:
      new_overwrite_controller_inputs[controller_port_number - 1].Start =
          lua_toboolean(lua_state, -1);
      break;

    case GcButtonName::Reset:
      new_overwrite_controller_inputs[controller_port_number - 1].reset =
          lua_toboolean(lua_state, -1);
      break;

    case GcButtonName::AnalogStickX:
      magnitude = luaL_checkinteger(lua_state, -1);
      if (magnitude < 0 || magnitude > 255)
        luaL_error(lua_state, "Error: analogStickX was outside bounds of 0 and 255!");
      new_overwrite_controller_inputs[controller_port_number - 1].AnalogStickX =
          static_cast<u8>(magnitude);
      break;

    case GcButtonName::AnalogStickY:
      magnitude = luaL_checkinteger(lua_state, -1);
      if (magnitude < 0 || magnitude > 255)
        luaL_error(lua_state, "Error: analogStickY was outside bounds of 0 and 255!");
      new_overwrite_controller_inputs[controller_port_number - 1].AnalogStickY =
          static_cast<u8>(magnitude);
      break;

    case GcButtonName::CStickX:
      magnitude = luaL_checkinteger(lua_state, -1);
      if (magnitude < 0 || magnitude > 255)
        luaL_error(lua_state, "Error: cStickX was outside bounds of 0 and 255!");
      new_overwrite_controller_inputs[controller_port_number - 1].CStickX =
          static_cast<u8>(magnitude);
      break;

    case GcButtonName::CStickY:
      magnitude = luaL_checkinteger(lua_state, -1);
      if (magnitude < 0 || magnitude > 255)
        luaL_error(lua_state, "Error: cStickY was outside bounds of 0 and 255!");
      new_overwrite_controller_inputs[controller_port_number - 1].CStickY =
          static_cast<u8>(magnitude);
      break;

    case GcButtonName::TriggerL:
      magnitude = luaL_checkinteger(lua_state, -1);
      if (magnitude < 0 || magnitude > 255)
        luaL_error(lua_state, "Error: triggerL was outside bounds of 0 and 255!");
      new_overwrite_controller_inputs[controller_port_number - 1].TriggerL =
          static_cast<u8>(magnitude);
      ;
      break;

    case GcButtonName::TriggerR:
      magnitude = luaL_checkinteger(lua_state, -1);
      if (magnitude < 0 || magnitude > 255)
        luaL_error(lua_state, "Error: triggerR was outside bounds of 0 and 255!");
      new_overwrite_controller_inputs[controller_port_number - 1].TriggerR =
          static_cast<u8>(magnitude);
      ;
      break;

    default:
      luaL_error(
          lua_state,
          "Error: Unknown button name passed in as input to setInputs(). Valid button "
          "names are: A, B, X, Y, Z, L, R, Z, dPadUp, dPadDown, dPadLeft, dPadRight, "
          "cStickX, cStickY, analogStickX, analogStickY, triggerL, triggerR, RESET, and START");
    }

    lua_pop(lua_state, 1);
  }

  return 0;
}

int AddInputs(lua_State* lua_state)
{
  LuaColonOperatorTypeCheck(lua_state, "addInputs",
                            "gc_controller:addInputs(1, controllerValuesTable)");
  s64 controller_port_number = GetPortNumberHelperFunction(lua_state, "addInputs");

  add_to_controller_at_specified_port[controller_port_number - 1] = true;
  s64 magnitude;

  lua_pushnil(lua_state); /* first key */
  while (lua_next(lua_state, 3) != 0)
  {
    /* uses 'key' (at index -2) and 'value' (at index -1) */
    const char* button_name = luaL_checkstring(lua_state, -2);
    if (button_name == nullptr || button_name[0] == '\0')
    {
      luaL_error(lua_state,
                 "Error: in addInputs() function, an empty string was passed for a button name!");
    }

    switch (ParseGCButton(button_name))
    {
    case GcButtonName::A:
      add_to_controller_inputs[controller_port_number - 1].A = lua_toboolean(lua_state, -1);
      button_lists_for_add_to_controller_inputs[controller_port_number - 1].push_back(
          GcButtonName::A);
      break;

    case GcButtonName::B:
      add_to_controller_inputs[controller_port_number - 1].B = lua_toboolean(lua_state, -1);
      button_lists_for_add_to_controller_inputs[controller_port_number - 1].push_back(
          GcButtonName::B);
      break;

    case GcButtonName::X:
      add_to_controller_inputs[controller_port_number - 1].X = lua_toboolean(lua_state, -1);
      button_lists_for_add_to_controller_inputs[controller_port_number - 1].push_back(
          GcButtonName::X);
      break;

    case GcButtonName::Y:
      add_to_controller_inputs[controller_port_number - 1].Y = lua_toboolean(lua_state, -1);
      button_lists_for_add_to_controller_inputs[controller_port_number - 1].push_back(
          GcButtonName::Y);
      break;

    case GcButtonName::Z:
      add_to_controller_inputs[controller_port_number - 1].Z = lua_toboolean(lua_state, -1);
      button_lists_for_add_to_controller_inputs[controller_port_number - 1].push_back(
          GcButtonName::Z);
      break;

    case GcButtonName::L:
      add_to_controller_inputs[controller_port_number - 1].L = lua_toboolean(lua_state, -1);
      button_lists_for_add_to_controller_inputs[controller_port_number - 1].push_back(
          GcButtonName::L);
      break;

    case GcButtonName::R:
      add_to_controller_inputs[controller_port_number - 1].R = lua_toboolean(lua_state, -1);
      button_lists_for_add_to_controller_inputs[controller_port_number - 1].push_back(
          GcButtonName::R);
      break;

    case GcButtonName::DPadUp:
      add_to_controller_inputs[controller_port_number - 1].DPadUp = lua_toboolean(lua_state, -1);
      button_lists_for_add_to_controller_inputs[controller_port_number - 1].push_back(
          GcButtonName::DPadUp);
      break;

    case GcButtonName::DPadDown:
      add_to_controller_inputs[controller_port_number - 1].DPadDown = lua_toboolean(lua_state, -1);
      button_lists_for_add_to_controller_inputs[controller_port_number - 1].push_back(
          GcButtonName::DPadDown);
      break;

    case GcButtonName::DPadLeft:
      add_to_controller_inputs[controller_port_number - 1].DPadLeft = lua_toboolean(lua_state, -1);
      button_lists_for_add_to_controller_inputs[controller_port_number - 1].push_back(
          GcButtonName::DPadLeft);
      break;

    case GcButtonName::DPadRight:
      add_to_controller_inputs[controller_port_number - 1].DPadRight = lua_toboolean(lua_state, -1);
      button_lists_for_add_to_controller_inputs[controller_port_number - 1].push_back(
          GcButtonName::DPadRight);
      break;

    case GcButtonName::Start:
      add_to_controller_inputs[controller_port_number - 1].Start = lua_toboolean(lua_state, -1);
      button_lists_for_add_to_controller_inputs[controller_port_number - 1].push_back(
          GcButtonName::Start);
      break;

    case GcButtonName::Reset:
      add_to_controller_inputs[controller_port_number - 1].reset = lua_toboolean(lua_state, -1);
      button_lists_for_add_to_controller_inputs[controller_port_number - 1].push_back(
          GcButtonName::Reset);
      break;

    case GcButtonName::AnalogStickX:
      magnitude = luaL_checkinteger(lua_state, -1);
      if (magnitude < 0 || magnitude > 255)
        luaL_error(lua_state, "Error: analogStickX was outside bounds of 0 and 255!");
      add_to_controller_inputs[controller_port_number - 1].AnalogStickX =
          static_cast<u8>(magnitude);
      button_lists_for_add_to_controller_inputs[controller_port_number - 1].push_back(
          GcButtonName::AnalogStickX);
      break;

    case GcButtonName::AnalogStickY:
      magnitude = luaL_checkinteger(lua_state, -1);
      if (magnitude < 0 || magnitude > 255)
        luaL_error(lua_state, "Error: analogStickY was outside bounds of 0 and 255!");
      add_to_controller_inputs[controller_port_number - 1].AnalogStickY =
          static_cast<u8>(magnitude);
      button_lists_for_add_to_controller_inputs[controller_port_number - 1].push_back(
          GcButtonName::AnalogStickY);
      break;

    case GcButtonName::CStickX:
      magnitude = luaL_checkinteger(lua_state, -1);
      if (magnitude < 0 || magnitude > 255)
        luaL_error(lua_state, "Error: cStickX was outside bounds of 0 and 255!");
      add_to_controller_inputs[controller_port_number - 1].CStickX = static_cast<u8>(magnitude);
      button_lists_for_add_to_controller_inputs[controller_port_number - 1].push_back(
          GcButtonName::CStickX);
      break;

    case GcButtonName::CStickY:
      magnitude = luaL_checkinteger(lua_state, -1);
      if (magnitude < 0 || magnitude > 255)
        luaL_error(lua_state, "Error: cStickY was outside bounds of 0 and 255!");
      add_to_controller_inputs[controller_port_number - 1].CStickY = static_cast<u8>(magnitude);
      button_lists_for_add_to_controller_inputs[controller_port_number - 1].push_back(
          GcButtonName::CStickY);
      break;

    case GcButtonName::TriggerL:
      magnitude = luaL_checkinteger(lua_state, -1);
      if (magnitude < 0 || magnitude > 255)
        luaL_error(lua_state, "Error: triggerL was outside bounds of 0 and 255!");
      add_to_controller_inputs[controller_port_number - 1].TriggerL = static_cast<u8>(magnitude);
      button_lists_for_add_to_controller_inputs[controller_port_number - 1].push_back(
          GcButtonName::TriggerL);
      break;

    case GcButtonName::TriggerR:
      magnitude = luaL_checkinteger(lua_state, -1);
      if (magnitude < 0 || magnitude > 255)
        luaL_error(lua_state, "Error: triggerR was outside bounds of 0 and 255!");
      add_to_controller_inputs[controller_port_number - 1].TriggerR = static_cast<u8>(magnitude);
      button_lists_for_add_to_controller_inputs[controller_port_number - 1].push_back(
          GcButtonName::TriggerR);
      break;

    default:
      luaL_error(
          lua_state,
          "Error: Unknown button name passed in as input to addInputs(). Valid button "
          "names are: A, B, X, Y, Z, L, R, Z, dPadUp, dPadDown, dPadLeft, dPadRight, "
          "cStickX, cStickY, analogStickX, analogStickY, triggerL, triggerR, RESET, and START");
    }

    lua_pop(lua_state, 1);
  }

  return 0;
}

// Order of arguments for the following 3 functions is:
// 1. THIS
// 2. Port number.
// 3. Probability
// 4. Button Name (string)

int AddButtonFlipChance(lua_State* lua_state)
{
  LuaColonOperatorTypeCheck(lua_state, "addButtonFlipChance",
                            "gc_controller:addButtonFlipChance(1, 32, \"A\")");
  s64 controller_port_number = GetPortNumberHelperFunction(lua_state, "addButtonFlipChance");
  double probability = GetProbabilityHelperFunction(lua_state, "addButtonFlipChance");
  GcButtonName button_name = ParseGCButton(luaL_checkstring(lua_state, 4));

  if (button_name == GcButtonName::TriggerL)
    button_name = GcButtonName::L;
  if (button_name == GcButtonName::TriggerR)
    button_name = GcButtonName::R;

  if (!IsBinaryButton(button_name))
    luaL_error(
        lua_state,
        "Error: In function gc_controller:addButtonFlipChance(), button name string was not a "
        "valid button that could be pressed (analog input buttons are not allowed here)");

  do_random_input_events_at_specified_port[controller_port_number - 1] = true;
  random_button_events[controller_port_number - 1].push_back(
      std::unique_ptr<LuaGCButtonFlipProbabilityEvent>(
          new LuaGCButtonFlipProbabilityEvent(probability, button_name)));
  return 0;
}

int AddButtonPressChance(lua_State* lua_state)
{
  LuaColonOperatorTypeCheck(lua_state, "addButtonPressChance",
                            "gc_controller:addButtonPressChance(1, 32, \"A\")");
  s64 controller_port_number = GetPortNumberHelperFunction(lua_state, "addButtonPressChance");
  double probability = GetProbabilityHelperFunction(lua_state, "addButtonPressChance");
  GcButtonName button_name = ParseGCButton(luaL_checkstring(lua_state, 4));
  if (button_name == GcButtonName::TriggerL)
    button_name = GcButtonName::L;
  if (button_name == GcButtonName::TriggerR)
    button_name = GcButtonName::R;
  if (!IsBinaryButton(button_name))
    luaL_error(
        lua_state,
        "Error: In function gc_controller:addButtonPressChance(), button name string was not a "
        "valid button that could be pressed (analog input buttons are not allowed here)");

  do_random_input_events_at_specified_port[controller_port_number - 1] = true;
  random_button_events[controller_port_number - 1].push_back(
      std::unique_ptr<LuaGameCubeButtonProbabilityEvent>(
          new LuaGCButtonPressEvent(probability, button_name)));
  return 0;
}

int AddButtonReleaseChance(lua_State* lua_state)
{
  LuaColonOperatorTypeCheck(lua_state, "addButtonReleaseChance",
                            "gc_controller:addButtonReleaseChance(1, 32, \"A\")");
  s64 controller_port_number = GetPortNumberHelperFunction(lua_state, "addButtonReleaseChance");
  double probability = GetProbabilityHelperFunction(lua_state, "addButtonReleaseChance");
  GcButtonName button_name = ParseGCButton(luaL_checkstring(lua_state, 4));
  if (button_name == GcButtonName::TriggerL)
    button_name = GcButtonName::L;
  if (button_name == GcButtonName::TriggerR)
    button_name = GcButtonName::R;
  if (!IsBinaryButton(button_name))
    luaL_error(
        lua_state,
        "Error: In function gc_controller:addButtonReleaseChance(), button name string was not a "
        "valid button that could be pressed (analog input buttons are not allowed here)");

  do_random_input_events_at_specified_port[controller_port_number - 1] = true;
  random_button_events[controller_port_number - 1].push_back(
      std::unique_ptr<LuaGameCubeButtonProbabilityEvent>(
          new LuaGCButtonReleaseEvent(probability, button_name)));
  return 0;
}

// Order of arguments for this function is:

// 1. THIS
// 2. Port Number
// 3. Probability
// 4. Button Name (String)

// There can be only 5 arguments or 6 arguments. If there are 5, then the following is the
// format of the remaining args:
// 5. Magnitude difference (applied in both directions).

// If there are 6 arguments, then the following is the format of the remaining args:
// 5. Lower Magnitude difference (subtracted from current value)
// 6. Upper Magnitude difference (added to current value)
int AddOrSubtractFromCurrentAnalogValueChance(lua_State* lua_state)
{
  LuaColonOperatorTypeCheck(
      lua_state, "addOrSubtractFromCurrentAnalogValueChance",
      "gc_controller:addOrSubtractFromCurrentAnalogValueChance(1, 32, \"cStickX\", 14, 9)");
  s64 controller_port_number =
      GetPortNumberHelperFunction(lua_state, "addOrSubtractFromCurrentAnalogValueChance");
  double probability =
      GetProbabilityHelperFunction(lua_state, "addOrSubtractFromCurrentAnalogValueChance");
  GcButtonName button_name = ParseGCButton(luaL_checkstring(lua_state, 4));
  if (button_name == GcButtonName::L)
    button_name = GcButtonName::TriggerL;
  if (button_name == GcButtonName::R)
    button_name = GcButtonName::TriggerR;

  if (!IsAnalogButton(button_name))
    luaL_error(lua_state,
               "Error: In function gc_controller:addOrSubtractFromCurrentAnalogValueChance(), ",
               "button name string was not an analog button!");

  s64 max_to_subtract = 0;
  s64 max_to_add = 0;

  if (lua_gettop(lua_state) <= 5)
  {
    max_to_add = luaL_checkinteger(lua_state, 5);
    if (max_to_add < 1)
      max_to_add *= -1;
    max_to_subtract = max_to_add;
  }
  else
  {
    max_to_subtract = luaL_checkinteger(lua_state, 5);
    if (max_to_subtract < 1)
      max_to_subtract *= -1;
    max_to_add = luaL_checkinteger(lua_state, 6);
    if (max_to_add < 1)
      max_to_add *= -1;
  }

  if (max_to_subtract > 255 || max_to_add > 255)
    luaL_error(
        lua_state,
        "Error: In function gc_controller:addOrSubtractFromCurrentAnalogValueChance(), the max ",
        "amount to add or subtract from the current analog value was greater than ",
        "the maximum value of 255");
  do_random_input_events_at_specified_port[controller_port_number - 1] = true;
  random_button_events[controller_port_number - 1].push_back(
      std::unique_ptr<LuaGameCubeButtonProbabilityEvent>(new LuaGCAlterAnalogInputFromCurrentValue(
          probability, button_name, static_cast<u8>(max_to_subtract),
          static_cast<u8>(max_to_add))));
  return 0;
}

// Order of arguments is:

// 1. THIS
// 2. Port Number
// 3. Probability.
// 4. Button Name (string)
// 5. Analog base value (int)

// There can now be either 1 or 2 more arguments left. If only 1 left, then this is the format:
// 6. Max Difference from base value (int, +-)

// If there are 2 arguments left, then this is the format:
// 6. Lower magnitude difference (subtracted from base value)
// 7. Upper magnitude difference (added to base value)
int AddOrSubtractFromSpecificAnalogValueChance(lua_State* lua_state)
{
  LuaColonOperatorTypeCheck(
      lua_state, "addOrSubtractFromSpecificAnalogValueChance",
      "gc_controller:addOrSubtractFromSpecificAnalogValueChance(1, 32, \"cStickX\", 184, 5, 13)");
  s64 controller_port_number =
      GetPortNumberHelperFunction(lua_state, "addOrSubtractFromSpecificAnalogValueChance");
  double probability =
      GetProbabilityHelperFunction(lua_state, "addOrSubtractFromSpecificAnalogValueChance");
  GcButtonName button_name = ParseGCButton(luaL_checkstring(lua_state, 4));
  if (button_name == GcButtonName::L)
    button_name = GcButtonName::TriggerL;
  if (button_name == GcButtonName::R)
    button_name = GcButtonName::TriggerR;

  s64 base_analog_value = luaL_checkinteger(lua_state, 5);
  if (base_analog_value < 0 || base_analog_value > 255)
    luaL_error(lua_state,
               "Error: in function gc_controller:addOrSubtractFromSpecificAnalogValueChance(), ",
               "base analog value was outside bounds of 0-255");

  s64 max_to_subtract = 0;
  s64 max_to_add = 0;

  if (lua_gettop(lua_state) <= 6)
  {
    max_to_subtract = max_to_add = luaL_checkinteger(lua_state, 6);
  }
  else
  {
    max_to_subtract = luaL_checkinteger(lua_state, 6);
    max_to_add = luaL_checkinteger(lua_state, 7);
  }

  if (max_to_subtract < 0)
    max_to_subtract *= -1;
  if (max_to_add < 0)
    max_to_add *= -1;

  if (max_to_subtract > 255 || max_to_add > 255)
    luaL_error(
        lua_state,
        "Error: In function gc_controller:addOrSubtractFromSpecificAnalogValueChance(), the ",
        "amount to add or subtract exceeded the maximum value of 255");

  do_random_input_events_at_specified_port[controller_port_number - 1] = true;
  random_button_events[controller_port_number - 1].push_back(
      std::unique_ptr<LuaGameCubeButtonProbabilityEvent>(new LuaGCAlterAnalogInputFromFixedValue(
          probability, button_name, static_cast<u8>(base_analog_value),
          static_cast<u8>(max_to_subtract), static_cast<u8>(max_to_add))));
  return 0;
}

int AddButtonComboChance(lua_State* lua_state)
{
  LuaColonOperatorTypeCheck(lua_state, "addButtonComboChance",
                            "gc_controller:AddButtonComboChance(1, 44, true, buttonTable)");
  s64 controller_port_number = GetPortNumberHelperFunction(lua_state, "addButtonComboChance");
  double probability = GetProbabilityHelperFunction(lua_state, "addButtonComboChance");
  bool set_other_buttons_to_blank = lua_toboolean(lua_state, 4);
  Movie::ControllerState controllerState;
  memset(&controllerState, 0, sizeof(controllerState));
  std::vector<GcButtonName> buttons_list = std::vector<GcButtonName>();
  lua_pushnil(lua_state);
  s64 magnitude = 0;

  while (lua_next(lua_state, 5) != 0)
  {
    /* uses 'key' (at index -2) and 'value' (at index -1) */
    const char* button_name = luaL_checkstring(lua_state, -2);
    if (button_name == nullptr || button_name[0] == '\0')
    {
      luaL_error(lua_state,
                 "Error: in addButtonComboChance() function, an empty string was passed ",
                 "for a button name!");
    }

    switch (ParseGCButton(button_name))
    {
    case GcButtonName::A:
      controllerState.A = lua_toboolean(lua_state, -1);
      buttons_list.push_back(GcButtonName::A);
      break;

    case GcButtonName::B:
      controllerState.B = lua_toboolean(lua_state, -1);
      buttons_list.push_back(GcButtonName::B);
      break;

    case GcButtonName::X:
      controllerState.X = lua_toboolean(lua_state, -1);
      buttons_list.push_back(GcButtonName::X);
      break;

    case GcButtonName::Y:
      controllerState.Y = lua_toboolean(lua_state, -1);
      buttons_list.push_back(GcButtonName::Y);
      break;

    case GcButtonName::Z:
      controllerState.Z = lua_toboolean(lua_state, -1);
      buttons_list.push_back(GcButtonName::Z);
      break;

    case GcButtonName::L:
      controllerState.L = lua_toboolean(lua_state, -1);
      buttons_list.push_back(GcButtonName::L);
      break;

    case GcButtonName::R:
      controllerState.R = lua_toboolean(lua_state, -1);
      buttons_list.push_back(GcButtonName::R);
      break;

    case GcButtonName::DPadUp:
      controllerState.DPadUp = lua_toboolean(lua_state, -1);
      buttons_list.push_back(GcButtonName::DPadUp);
      break;

    case GcButtonName::DPadDown:
      controllerState.DPadDown = lua_toboolean(lua_state, -1);
      buttons_list.push_back(GcButtonName::DPadDown);
      break;

    case GcButtonName::DPadLeft:
      controllerState.DPadLeft = lua_toboolean(lua_state, -1);
      buttons_list.push_back(GcButtonName::DPadLeft);
      break;

    case GcButtonName::DPadRight:
      controllerState.DPadRight = lua_toboolean(lua_state, -1);
      buttons_list.push_back(GcButtonName::DPadRight);
      break;

    case GcButtonName::Start:
      controllerState.Start = lua_toboolean(lua_state, -1);
      buttons_list.push_back(GcButtonName::Start);
      break;

    case GcButtonName::Reset:
      controllerState.reset = lua_toboolean(lua_state, -1);
      buttons_list.push_back(GcButtonName::Reset);
      break;

    case GcButtonName::AnalogStickX:
      magnitude = luaL_checkinteger(lua_state, -1);
      if (magnitude < 0 || magnitude > 255)
        luaL_error(lua_state, "Error: analogStickX was outside bounds of 0 and 255!");
      controllerState.AnalogStickX = static_cast<u8>(magnitude);
      buttons_list.push_back(GcButtonName::AnalogStickX);
      break;

    case GcButtonName::AnalogStickY:
      magnitude = luaL_checkinteger(lua_state, -1);
      if (magnitude < 0 || magnitude > 255)
        luaL_error(lua_state, "Error: analogStickY was outside bounds of 0 and 255!");
      controllerState.AnalogStickY = static_cast<u8>(magnitude);
      buttons_list.push_back(GcButtonName::AnalogStickY);
      break;

    case GcButtonName::CStickX:
      magnitude = luaL_checkinteger(lua_state, -1);
      if (magnitude < 0 || magnitude > 255)
        luaL_error(lua_state, "Error: cStickX was outside bounds of 0 and 255!");
      controllerState.CStickX = static_cast<u8>(magnitude);
      buttons_list.push_back(GcButtonName::CStickX);
      break;

    case GcButtonName::CStickY:
      magnitude = luaL_checkinteger(lua_state, -1);
      if (magnitude < 0 || magnitude > 255)
        luaL_error(lua_state, "Error: cStickY was outside bounds of 0 and 255!");
      controllerState.CStickY = static_cast<u8>(magnitude);
      buttons_list.push_back(GcButtonName::CStickY);
      break;

    case GcButtonName::TriggerL:
      magnitude = luaL_checkinteger(lua_state, -1);
      if (magnitude < 0 || magnitude > 255)
        luaL_error(lua_state, "Error: triggerL was outside bounds of 0 and 255!");
      controllerState.TriggerL = static_cast<u8>(magnitude);
      buttons_list.push_back(GcButtonName::TriggerL);
      break;

    case GcButtonName::TriggerR:
      magnitude = luaL_checkinteger(lua_state, -1);
      if (magnitude < 0 || magnitude > 255)
        luaL_error(lua_state, "Error: triggerR was outside bounds of 0 and 255!");
      controllerState.TriggerR = static_cast<u8>(magnitude);
      buttons_list.push_back(GcButtonName::TriggerR);
      break;

    default:
      luaL_error(
          lua_state,
          "Error: Unknown button name passed in as input to addButtonComboChance(). Valid button "
          "names are: A, B, X, Y, Z, L, R, Z, dPadUp, dPadDown, dPadLeft, dPadRight, "
          "cStickX, cStickY, analogStickX, analogStickY, triggerL, triggerR, RESET, and START");
    }

    lua_pop(lua_state, 1);
  }
  do_random_input_events_at_specified_port[controller_port_number - 1] = true;
  random_button_events[controller_port_number - 1].push_back(
      std::unique_ptr<LuaGameCubeButtonProbabilityEvent>(new LuaGCButtonComboEvent(
          probability, controllerState, buttons_list, set_other_buttons_to_blank)));
  return 0;
}

int AddControllerClearChance(lua_State* lua_state)
{
  LuaColonOperatorTypeCheck(lua_state, "addControllerClearChance",
                            "gc_controller:addControllerClearChance(1, 56)");
  s64 controller_port_number = GetPortNumberHelperFunction(lua_state, "addControllerClearChance");
  double probability = GetProbabilityHelperFunction(lua_state, "addControllerClearChance");

  do_random_input_events_at_specified_port[controller_port_number - 1] = true;
  random_button_events[controller_port_number - 1].push_back(
      std::unique_ptr<LuaGameCubeButtonProbabilityEvent>(
          new LuaGCClearControllerEvent(probability)));
  return 0;
}

int GetControllerInputs(lua_State* lua_state)
{
  LuaColonOperatorTypeCheck(lua_state, "getControllerInputs",
                            "gc_controller:getControllerInputs(1)");
  s64 controller_port_number = GetPortNumberHelperFunction(lua_state, "getControllerInputs");

  Movie::ControllerState controller_inputs =
      Movie::GetLuaGCInputsForPort(controller_port_number - 1);
  bool button_pressed = false;
  s64 magnitude = 0;
  lua_newtable(lua_state);
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
      luaL_error(
          lua_state,
          "An unexpected implementation error occured in gc_controller:getControllerInputs(). "
          "Did you modify the order of the enums in LuaGCButtons.h?");
      break;
    }
    lua_settable(lua_state, -3);
  }
  return 1;
}
}  // namespace Lua::LuaGameCubeController
