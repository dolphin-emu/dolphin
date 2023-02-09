#include "LuaGameCubeController.h"
#include "Core/Lua/LuaVersionResolver.h"

namespace Lua:: LuaGameCubeController
{
class gc_controller_lua
{
public:
  inline gc_controller_lua(){};
};

  gc_controller_lua* gc_controller_pointer = nullptr;
  std::array<bool, 4> overwriteControllerAtSpecifiedPort = {};
  std::array<bool, 4> addToControllerAtSpecifiedPort = {};
  std::array<bool, 4> doRandomInputEventsAtSpecifiedPort = {};

  std::array<Movie::ControllerState, 4> newOverwriteControllerInputs = {};
  std::array<Movie::ControllerState, 4> addToControllerInputs = {};
  std::array<std::vector<GcButtonName>, 4> buttonListsForAddToControllerInputs = {};
  std::array<std::vector<std::unique_ptr<LuaGameCubeButtonProbabilityEvent>>, 4> randomButtonEvents = {};

gc_controller_lua* getControllerInstance()
{
  if (gc_controller_pointer == nullptr)
    gc_controller_pointer = new gc_controller_lua();
  return gc_controller_pointer;
}

  void InitLuaGameCubeControllerFunctions(lua_State* luaState, const std::string& luaApiVersion)
{
    gc_controller_lua** gcControllerLuaPtrPtr = (gc_controller_lua**)lua_newuserdata(luaState, sizeof(gc_controller_lua*));
    *gcControllerLuaPtrPtr = getControllerInstance();
    luaL_newmetatable(luaState, "LuaGCControllerTable");
    lua_pushvalue(luaState, -1);
    lua_setfield(luaState, -2, "__index");

    std::array luaGCControllerFunctionsWithVersionsAttached = {
      luaL_Reg_With_Version({"setInputs", "1.0", setInputs}),
      luaL_Reg_With_Version({"addInputs", "1.0", addInputs}),
      luaL_Reg_With_Version({"addButtonFlipChance", "1.0", addButtonFlipChance}),
      luaL_Reg_With_Version({"addButtonPressChance", "1.0", addButtonPressChance}),
      luaL_Reg_With_Version({"addButtonReleaseChance", "1.0", addButtonReleaseChance}),
      luaL_Reg_With_Version({"addOrSubtractFromCurrentAnalogValueChance", "1.0", addOrSubtractFromCurrentAnalogValueChance}),
      luaL_Reg_With_Version({"addOrSubtractFromSpecificAnalogValueChance", "1.0", addOrSubtractFromSpecificAnalogValueChance}),
      luaL_Reg_With_Version({"addButtonComboChance", "1.0", addButtonComboChance}),
      luaL_Reg_With_Version({"addControllerClearChance", "1.0", addControllerClearChance}),
      luaL_Reg_With_Version({"getControllerInputs", "1.0", getControllerInputs})
    };

    std::unordered_map<std::string, std::string> deprecatedFunctionsMap;
    addLatestFunctionsForVersion(luaGCControllerFunctionsWithVersionsAttached, luaApiVersion, deprecatedFunctionsMap, luaState);
    lua_setglobal(luaState, "gc_controller");

    for (int i = 0; i < 4; ++i)
    {
      overwriteControllerAtSpecifiedPort[i] = false;
      addToControllerAtSpecifiedPort[i] = false;
      doRandomInputEventsAtSpecifiedPort[i] = false;

      randomButtonEvents[i] = std::vector<std::unique_ptr<LuaGameCubeButtonProbabilityEvent>>();
      buttonListsForAddToControllerInputs[i] = std::vector<GcButtonName>();
    }
  }

  int getPortNumberHelperFunction(lua_State* luaState, const char* funcName)
  {
    s64 controllerPortNumber = luaL_checkinteger(luaState, 2);
    if (controllerPortNumber < 1 || controllerPortNumber > 4)
    {
      luaL_error(luaState, (std::string("Error: In function gc_controller:") + funcName + ", controller port number wasn't between 1 and 4!").c_str());
    }

    if (controllerPortNumber > Pad::GetConfig()->GetControllerCount())
    {
      luaL_error(luaState, (std::string("Error: in ") + funcName + "(), attempt was made to access a port which did not have a GameCube controller plugged into it!").c_str());
    }


    return controllerPortNumber;
  }

  double getProbabilityHelperFunction(lua_State* luaState, const char* funcName)
  {
    double probability = 0.0;
    if (lua_isinteger(luaState, 3))
      probability = static_cast<double>(lua_tointeger(luaState, 3));
    else
      probability = luaL_checknumber(luaState, 3);

    if (probability < 0.0 || probability > 100.0)
      luaL_error(luaState, (std::string("Error: In function gc_controller:") + funcName + ", probability was outside the acceptable range of 0 to 100%").c_str());
    return probability;
  }

  //NOTE: In SI.cpp, UpdateDevices() is called to update each device, which moves exactly 8 bytes forward for each controller. Also, it moves in order from controllers 1 to 4.
  int setInputs(lua_State* luaState)
  {
    luaColonOperatorTypeCheck(luaState, "setInputs", "gc_controller:setInputs(1, controllerValuesTable)");
    s64 controllerPortNumber = getPortNumberHelperFunction(luaState, "setInputs");

    overwriteControllerAtSpecifiedPort[controllerPortNumber - 1] = true;
    memset(&newOverwriteControllerInputs[controllerPortNumber - 1], 0, sizeof(Movie::ControllerState));
    newOverwriteControllerInputs[controllerPortNumber - 1].is_connected = true;
    newOverwriteControllerInputs[controllerPortNumber - 1].CStickX = 128;
    newOverwriteControllerInputs[controllerPortNumber - 1].CStickY = 128;
    newOverwriteControllerInputs[controllerPortNumber - 1].AnalogStickX = 128;
    newOverwriteControllerInputs[controllerPortNumber - 1].AnalogStickY = 128;
    s64 magnitude = 0;


    lua_pushnil(luaState); /* first key */
    while (lua_next(luaState, 3) != 0)
    {
      /* uses 'key' (at index -2) and 'value' (at index -1) */
      const char* buttonName = luaL_checkstring(luaState, -2);
      if (buttonName == nullptr || buttonName[0] == '\0')
      {
        luaL_error(luaState, "Error: in setInputs() function, an empty string was passed for a button name!");
      }

      switch (parseGCButton(buttonName))
      {
      case GcButtonName::A:
        newOverwriteControllerInputs[controllerPortNumber - 1].A = lua_toboolean(luaState, -1);
          break;

      case GcButtonName::B:
        newOverwriteControllerInputs[controllerPortNumber - 1].B = lua_toboolean(luaState, -1);
        break;

      case GcButtonName::X:
        newOverwriteControllerInputs[controllerPortNumber - 1].X = lua_toboolean(luaState, -1);
        break;

      case GcButtonName::Y:
        newOverwriteControllerInputs[controllerPortNumber - 1].Y = lua_toboolean(luaState, -1);
        break;

      case GcButtonName::Z:
        newOverwriteControllerInputs[controllerPortNumber - 1].Z = lua_toboolean(luaState, -1);
        break;

      case GcButtonName::L:
        newOverwriteControllerInputs[controllerPortNumber - 1].L = lua_toboolean(luaState, -1);
        break;

      case GcButtonName::R:
        newOverwriteControllerInputs[controllerPortNumber - 1].R = lua_toboolean(luaState, -1);
        break;

      case GcButtonName::DPadUp:
        newOverwriteControllerInputs[controllerPortNumber - 1].DPadUp = lua_toboolean(luaState, -1);
        break;

      case GcButtonName::DPadDown:
        newOverwriteControllerInputs[controllerPortNumber - 1].DPadDown = lua_toboolean(luaState, -1);
        break;

      case GcButtonName::DPadLeft:
        newOverwriteControllerInputs[controllerPortNumber - 1].DPadLeft = lua_toboolean(luaState, -1);
        break;

      case GcButtonName::DPadRight:
        newOverwriteControllerInputs[controllerPortNumber - 1].DPadRight = lua_toboolean(luaState, -1);
        break;

      case GcButtonName::Start:
        newOverwriteControllerInputs[controllerPortNumber - 1].Start = lua_toboolean(luaState, -1);
        break;

      case GcButtonName::Reset:
        newOverwriteControllerInputs[controllerPortNumber - 1].reset = lua_toboolean(luaState, -1);
        break;

      case GcButtonName::AnalogStickX:
        magnitude = luaL_checkinteger(luaState, -1);
        if (magnitude < 0 || magnitude > 255)
          luaL_error(luaState, "Error: analogStickX was outside bounds of 0 and 255!");
        newOverwriteControllerInputs[controllerPortNumber - 1].AnalogStickX = static_cast<u8>(magnitude);
        break;

      case GcButtonName::AnalogStickY:
        magnitude = luaL_checkinteger(luaState, -1);
        if (magnitude < 0 || magnitude > 255)
          luaL_error(luaState, "Error: analogStickY was outside bounds of 0 and 255!");
        newOverwriteControllerInputs[controllerPortNumber - 1].AnalogStickY = static_cast<u8>(magnitude);
        break;

      case GcButtonName::CStickX:
        magnitude = luaL_checkinteger(luaState, -1);
        if (magnitude < 0 || magnitude > 255)
          luaL_error(luaState, "Error: cStickX was outside bounds of 0 and 255!");
        newOverwriteControllerInputs[controllerPortNumber - 1].CStickX = static_cast<u8>(magnitude);
        break;

      case GcButtonName::CStickY:
        magnitude = luaL_checkinteger(luaState, -1);
        if (magnitude < 0 || magnitude > 255)
          luaL_error(luaState, "Error: cStickY was outside bounds of 0 and 255!");
        newOverwriteControllerInputs[controllerPortNumber - 1].CStickY = magnitude;
        break;

      case GcButtonName::TriggerL:
        magnitude = luaL_checkinteger(luaState, -1);
        if (magnitude < 0 || magnitude > 255)
          luaL_error(luaState, "Error: triggerL was outside bounds of 0 and 255!");
        newOverwriteControllerInputs[controllerPortNumber - 1].TriggerL = magnitude;
        break;

      case GcButtonName::TriggerR:
        magnitude = luaL_checkinteger(luaState, -1);
        if (magnitude < 0 || magnitude > 255)
          luaL_error(luaState, "Error: triggerR was outside bounds of 0 and 255!");
        newOverwriteControllerInputs[controllerPortNumber - 1].TriggerR = magnitude;
        break;

      default:
        luaL_error(luaState,
                   "Error: Unknown button name passed in as input to setInputs(). Valid button "
                   "names are: A, B, X, Y, Z, L, R, Z, dPadUp, dPadDown, dPadLeft, dPadRight, "
                   "cStickX, cStickY, analogStickX, analogStickY, triggerL, triggerR, RESET, and START");
      }

      lua_pop(luaState, 1);
    }

    return 0;
  }

  int addInputs(lua_State* luaState)
  {
    luaColonOperatorTypeCheck(luaState, "addInputs", "gc_controller:addInputs(1, controllerValuesTable)");
    s64 controllerPortNumber = getPortNumberHelperFunction(luaState, "addInputs");

    addToControllerAtSpecifiedPort[controllerPortNumber - 1] = true;
    s64 magnitude;

    lua_pushnil(luaState); /* first key */
    while (lua_next(luaState, 3) != 0)
    {
      /* uses 'key' (at index -2) and 'value' (at index -1) */
      const char* buttonName = luaL_checkstring(luaState, -2);
      if (buttonName == nullptr || buttonName[0] == '\0')
      {
        luaL_error(luaState,
                   "Error: in addInputs() function, an empty string was passed for a button name!");
      }

      switch (parseGCButton(buttonName))
      {
      case GcButtonName::A:
        addToControllerInputs[controllerPortNumber - 1].A = lua_toboolean(luaState, -1);
        buttonListsForAddToControllerInputs[controllerPortNumber - 1].push_back(GcButtonName::A);
        break;

      case GcButtonName::B:
        addToControllerInputs[controllerPortNumber - 1].B = lua_toboolean(luaState, -1);
        buttonListsForAddToControllerInputs[controllerPortNumber - 1].push_back(GcButtonName::B);
        break;

      case GcButtonName::X:
        addToControllerInputs[controllerPortNumber - 1].X = lua_toboolean(luaState, -1);
        buttonListsForAddToControllerInputs[controllerPortNumber - 1].push_back(GcButtonName::X);
        break;

      case GcButtonName::Y:
        addToControllerInputs[controllerPortNumber - 1].Y = lua_toboolean(luaState, -1);
        buttonListsForAddToControllerInputs[controllerPortNumber - 1].push_back(GcButtonName::Y);
        break;

      case GcButtonName::Z:
        addToControllerInputs[controllerPortNumber - 1].Z = lua_toboolean(luaState, -1);
        buttonListsForAddToControllerInputs[controllerPortNumber - 1].push_back(GcButtonName::Z);
        break;

      case GcButtonName::L:
        addToControllerInputs[controllerPortNumber - 1].L = lua_toboolean(luaState, -1);
        buttonListsForAddToControllerInputs[controllerPortNumber - 1].push_back(GcButtonName::L);
        break;

      case GcButtonName::R:
        addToControllerInputs[controllerPortNumber - 1].R = lua_toboolean(luaState, -1);
        buttonListsForAddToControllerInputs[controllerPortNumber - 1].push_back(GcButtonName::R);
        break;

      case GcButtonName::DPadUp:
        addToControllerInputs[controllerPortNumber - 1].DPadUp = lua_toboolean(luaState, -1);
        buttonListsForAddToControllerInputs[controllerPortNumber - 1].push_back(
            GcButtonName::DPadUp);
        break;

      case GcButtonName::DPadDown:
        addToControllerInputs[controllerPortNumber - 1].DPadDown = lua_toboolean(luaState, -1);
        buttonListsForAddToControllerInputs[controllerPortNumber - 1].push_back(
            GcButtonName::DPadDown);
        break;

      case GcButtonName::DPadLeft:
        addToControllerInputs[controllerPortNumber - 1].DPadLeft = lua_toboolean(luaState, -1);
        buttonListsForAddToControllerInputs[controllerPortNumber - 1].push_back(
            GcButtonName::DPadLeft);
        break;

      case GcButtonName::DPadRight:
        addToControllerInputs[controllerPortNumber - 1].DPadRight = lua_toboolean(luaState, -1);
        buttonListsForAddToControllerInputs[controllerPortNumber - 1].push_back(
            GcButtonName::DPadRight);
        break;

      case GcButtonName::Start:
        addToControllerInputs[controllerPortNumber - 1].Start = lua_toboolean(luaState, -1);
        buttonListsForAddToControllerInputs[controllerPortNumber - 1].push_back(
            GcButtonName::Start);
        break;

      case GcButtonName::Reset:
        addToControllerInputs[controllerPortNumber - 1].reset = lua_toboolean(luaState, -1);
        buttonListsForAddToControllerInputs[controllerPortNumber - 1].push_back(
            GcButtonName::Reset);
        break;

      case GcButtonName::AnalogStickX:
        magnitude = luaL_checkinteger(luaState, -1);
        if (magnitude < 0 || magnitude > 255)
          luaL_error(luaState, "Error: analogStickX was outside bounds of 0 and 255!");
        addToControllerInputs[controllerPortNumber - 1].AnalogStickX = static_cast<u8>(magnitude);
        buttonListsForAddToControllerInputs[controllerPortNumber - 1].push_back(
            GcButtonName::AnalogStickX);
        break;

      case GcButtonName::AnalogStickY:
        magnitude = luaL_checkinteger(luaState, -1);
        if (magnitude < 0 || magnitude > 255)
          luaL_error(luaState, "Error: analogStickY was outside bounds of 0 and 255!");
        addToControllerInputs[controllerPortNumber - 1].AnalogStickY = static_cast<u8>(magnitude);
        buttonListsForAddToControllerInputs[controllerPortNumber - 1].push_back(
            GcButtonName::AnalogStickY);
        break;

      case GcButtonName::CStickX:
        magnitude = luaL_checkinteger(luaState, -1);
        if (magnitude < 0 || magnitude > 255)
          luaL_error(luaState, "Error: cStickX was outside bounds of 0 and 255!");
        addToControllerInputs[controllerPortNumber - 1].CStickX = static_cast<u8>(magnitude);
        buttonListsForAddToControllerInputs[controllerPortNumber - 1].push_back(
            GcButtonName::CStickX);
        break;

      case GcButtonName::CStickY:
        magnitude = luaL_checkinteger(luaState, -1);
        if (magnitude < 0 || magnitude > 255)
          luaL_error(luaState, "Error: cStickY was outside bounds of 0 and 255!");
        addToControllerInputs[controllerPortNumber - 1].CStickY = magnitude;
        buttonListsForAddToControllerInputs[controllerPortNumber - 1].push_back(
            GcButtonName::CStickY);
        break;

      case GcButtonName::TriggerL:
        magnitude = luaL_checkinteger(luaState, -1);
        if (magnitude < 0 || magnitude > 255)
          luaL_error(luaState, "Error: triggerL was outside bounds of 0 and 255!");
        addToControllerInputs[controllerPortNumber - 1].TriggerL = magnitude;
        buttonListsForAddToControllerInputs[controllerPortNumber - 1].push_back(
            GcButtonName::TriggerL);
        break;

      case GcButtonName::TriggerR:
        magnitude = luaL_checkinteger(luaState, -1);
        if (magnitude < 0 || magnitude > 255)
          luaL_error(luaState, "Error: triggerR was outside bounds of 0 and 255!");
        addToControllerInputs[controllerPortNumber - 1].TriggerR = magnitude;
        buttonListsForAddToControllerInputs[controllerPortNumber - 1].push_back(
            GcButtonName::TriggerR);
        break;

      default:
        luaL_error(luaState,
                   "Error: Unknown button name passed in as input to addInputs(). Valid button "
                   "names are: A, B, X, Y, Z, L, R, Z, dPadUp, dPadDown, dPadLeft, dPadRight, "
                   "cStickX, cStickY, analogStickX, analogStickY, triggerL, triggerR, RESET, and START");
      }

      lua_pop(luaState, 1);
    }

    return 0;
  }

  // Order of arguments is:
  // 1. THIS
  // 2. Port number.
  // 3. Probability
  // 4. Button Name (string)

  int addButtonFlipChance(lua_State* luaState)
  {
    luaColonOperatorTypeCheck(luaState, "addButtonFlipChance", "gc_controller:addButtonFlipChance(1, 32, \"A\")");
    s64 controllerPortNumber = getPortNumberHelperFunction(luaState, "addButtonFlipChance");
    double probability = getProbabilityHelperFunction(luaState, "addButtonFlipChance");
    GcButtonName buttonName = parseGCButton(luaL_checkstring(luaState, 4));

    if (buttonName == GcButtonName::TriggerL)
      buttonName = GcButtonName::L;
    if (buttonName == GcButtonName::TriggerR)
      buttonName = GcButtonName::R;

    if (!isBinaryButton(buttonName))
      luaL_error(
          luaState,
          "Error: In function gc_controller:addButtonFlipChance(), button name string was not a "
          "valid button that could be pressed (analog input buttons are not allowed here)");

    doRandomInputEventsAtSpecifiedPort[controllerPortNumber - 1] = true;
    randomButtonEvents[controllerPortNumber - 1].push_back(std::unique_ptr<LuaGCButtonFlipProbabilityEvent>(new LuaGCButtonFlipProbabilityEvent(probability, buttonName)));
    return 0;
  }

  int addButtonPressChance(lua_State* luaState)
  {
    luaColonOperatorTypeCheck(luaState, "addButtonPressChance", "gc_controller:addButtonPressChance(1, 32, \"A\")");
    s64 controllerPortNumber = getPortNumberHelperFunction(luaState, "addButtonPressChance");
    double probability = getProbabilityHelperFunction(luaState, "addButtonPressChance");
    GcButtonName buttonName = parseGCButton(luaL_checkstring(luaState, 4));
    if (buttonName == GcButtonName::TriggerL)
      buttonName = GcButtonName::L;
    if (buttonName == GcButtonName::TriggerR)
      buttonName = GcButtonName::R;
    if (!isBinaryButton(buttonName))
      luaL_error(
          luaState,
          "Error: In function gc_controller:addButtonPressChance(), button name string was not a "
          "valid button that could be pressed (analog input buttons are not allowed here)");

    doRandomInputEventsAtSpecifiedPort[controllerPortNumber - 1] = true;
    randomButtonEvents[controllerPortNumber - 1].push_back(std::unique_ptr<LuaGameCubeButtonProbabilityEvent>(new LuaGCButtonPressEvent(probability, buttonName)));
    return 0;
  }

  int addButtonReleaseChance(lua_State* luaState)
  {
    luaColonOperatorTypeCheck(luaState, "addButtonReleaseChance", "gc_controller:addButtonReleaseChance(1, 32, \"A\")");
    s64 controllerPortNumber = getPortNumberHelperFunction(luaState, "addButtonReleaseChance");
    double probability = getProbabilityHelperFunction(luaState, "addButtonReleaseChance");
    GcButtonName buttonName = parseGCButton(luaL_checkstring(luaState, 4));
    if (buttonName == GcButtonName::TriggerL)
      buttonName = GcButtonName::L;
    if (buttonName == GcButtonName::TriggerR)
      buttonName = GcButtonName::R;
    if (!isBinaryButton(buttonName))
      luaL_error(
          luaState,
          "Error: In function gc_controller:addButtonReleaseChance(), button name string was not a "
          "valid button that could be pressed (analog input buttons are not allowed here)");

    doRandomInputEventsAtSpecifiedPort[controllerPortNumber - 1] = true;
    randomButtonEvents[controllerPortNumber - 1].push_back(std::unique_ptr<LuaGameCubeButtonProbabilityEvent>(new LuaGCButtonReleaseEvent(probability, buttonName)));
    return 0;
  }

  // Order of arguments is:
  // 1. THIS
  // 2. Port Number
  // 3. Probability
  // 4. Button Name (String)

  // There can be only 5 arguments or 6 arguments. If there are 5, then the following is the format of the remaining args:
  // 5. Magnitude difference (applied in both directions).

  //If there are 6 arguments, then the following is the format of the remaining args:
  // 5. Lower Magnitude difference (subtracted from current value)
  // 6. Upper Magnitude difference (added to current value)
  int addOrSubtractFromCurrentAnalogValueChance(lua_State* luaState)
  {
    luaColonOperatorTypeCheck(luaState, "addOrSubtractFromCurrentAnalogValueChance", "gc_controller:addOrSubtractFromCurrentAnalogValueChance(1, 32, \"cStickX\", 14, 9)");
    s64 controllerPortNumber = getPortNumberHelperFunction(luaState, "addOrSubtractFromCurrentAnalogValueChance");
    double probability = getProbabilityHelperFunction(luaState, "addOrSubtractFromCurrentAnalogValueChance");
    GcButtonName buttonName = parseGCButton(luaL_checkstring(luaState, 4));
    if (buttonName == GcButtonName::L)
      buttonName = GcButtonName::TriggerL;
    if (buttonName == GcButtonName::R)
      buttonName = GcButtonName::TriggerR;

    if (!isAnalogButton(buttonName))
      luaL_error(luaState, "Error: In function gc_controller:addOrSubtractFromCurrentAnalogValueChance(), button name string was not an analog button!");

    s64 maxToSubtract = 0;
    s64 maxToAdd = 0;

    if (lua_gettop(luaState) <= 5)
    {
      maxToAdd = luaL_checkinteger(luaState, 5);
      if (maxToAdd < 1)
        maxToAdd *= -1;
      maxToSubtract = maxToAdd;
    }
    else
    {
      maxToSubtract = luaL_checkinteger(luaState, 5);
      if (maxToSubtract < 1)
        maxToSubtract *= -1;
      maxToAdd = luaL_checkinteger(luaState, 6);
      if (maxToAdd < 1)
        maxToAdd *= -1;
    }

    if (maxToSubtract > 255 || maxToAdd > 255)
      luaL_error(luaState,
                 "Error: In function gc_controller:addOrSubtractFromCurrentAnalogValueChance(), the max amount to "
                 "add or subtract from the current analog value was greater than the maximum value of 255");
    doRandomInputEventsAtSpecifiedPort[controllerPortNumber - 1] = true;
    randomButtonEvents[controllerPortNumber - 1].push_back(std::unique_ptr<LuaGameCubeButtonProbabilityEvent>(new LuaGCAlterAnalogInputFromCurrentValue(
        probability, buttonName, static_cast<u8>(maxToSubtract), static_cast<u8>(maxToAdd))));
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
  int addOrSubtractFromSpecificAnalogValueChance(lua_State* luaState)
  {
    luaColonOperatorTypeCheck(luaState, "addOrSubtractFromSpecificAnalogValueChance", "gc_controller:addOrSubtractFromSpecificAnalogValueChance(1, 32, \"cStickX\", 184, 5, 13)");
    s64 controllerPortNumber =
        getPortNumberHelperFunction(luaState, "addOrSubtractFromSpecificAnalogValueChance");
    double probability =
        getProbabilityHelperFunction(luaState, "addOrSubtractFromSpecificAnalogValueChance");
    GcButtonName buttonName = parseGCButton(luaL_checkstring(luaState, 4));
    if (buttonName == GcButtonName::L)
      buttonName = GcButtonName::TriggerL;
    if (buttonName == GcButtonName::R)
      buttonName = GcButtonName::TriggerR;

    s64 baseAnalogValue = luaL_checkinteger(luaState, 5);
    if (baseAnalogValue < 0 || baseAnalogValue > 255)
      luaL_error(luaState, "Error: in function gc_controller:addOrSubtractFromSpecificAnalogValueChance(), base analog value was outside bounds of 0-255");

    s64 maxToSubtract = 0;
    s64 maxToAdd = 0;

    if (lua_gettop(luaState) <= 6)
    {
      maxToSubtract = maxToAdd = luaL_checkinteger(luaState, 6);
    }
    else
    {
      maxToSubtract = luaL_checkinteger(luaState, 6);
      maxToAdd = luaL_checkinteger(luaState, 7);
    }

    if (maxToSubtract < 0)
      maxToSubtract *= -1;
    if (maxToAdd < 0)
      maxToAdd *= -1;

    if (maxToSubtract > 255 || maxToAdd > 255)
      luaL_error(luaState,
                 "Error: In function gc_controller:addOrSubtractFromSpecificAnalogValueChance(), the amount to add or subtract exceeded the maximum value of 255");

    doRandomInputEventsAtSpecifiedPort[controllerPortNumber - 1] = true;
    randomButtonEvents[controllerPortNumber - 1].push_back(std::unique_ptr<LuaGameCubeButtonProbabilityEvent>(new LuaGCAlterAnalogInputFromFixedValue(
        probability, buttonName, static_cast<u8>(baseAnalogValue), static_cast<u8>(maxToSubtract), static_cast<u8>(maxToAdd))));
    return 0;
  }


  int addButtonComboChance(lua_State* luaState)
  {
    luaColonOperatorTypeCheck(luaState, "addButtonComboChance", "gc_controller:AddButtonComboChance(1, 44, true, buttonTable)");
    s64 controllerPortNumber = getPortNumberHelperFunction(luaState, "addButtonComboChance");
    double probability = getProbabilityHelperFunction(luaState, "addButtonComboChance");
    bool setOtherButtonsToBlank = lua_toboolean(luaState, 4);
    Movie::ControllerState controllerState;
    memset(&controllerState, 0, sizeof(controllerState));
    std::vector<GcButtonName> buttonsList = std::vector<GcButtonName>();
    lua_pushnil(luaState);
    s64 magnitude = 0;

    while (lua_next(luaState, 5) != 0)
    {
      /* uses 'key' (at index -2) and 'value' (at index -1) */
      const char* buttonName = luaL_checkstring(luaState, -2);
      if (buttonName == nullptr || buttonName[0] == '\0')
      {
        luaL_error(luaState,
                   "Error: in addButtonComboChance() function, an empty string was passed for a button name!");
      }

      switch (parseGCButton(buttonName))
      {
      case GcButtonName::A:
        controllerState.A = lua_toboolean(luaState, -1);
        buttonsList.push_back(GcButtonName::A);
        break;

      case GcButtonName::B:
        controllerState.B = lua_toboolean(luaState, -1);
        buttonsList.push_back(GcButtonName::B);
        break;

      case GcButtonName::X:
        controllerState.X = lua_toboolean(luaState, -1);
        buttonsList.push_back(GcButtonName::X);
        break;

      case GcButtonName::Y:
        controllerState.Y = lua_toboolean(luaState, -1);
        buttonsList.push_back(GcButtonName::Y);
        break;

      case GcButtonName::Z:
        controllerState.Z = lua_toboolean(luaState, -1);
        buttonsList.push_back(GcButtonName::Z);
        break;

      case GcButtonName::L:
        controllerState.L = lua_toboolean(luaState, -1);
        buttonsList.push_back(GcButtonName::L);
        break;

      case GcButtonName::R:
        controllerState.R = lua_toboolean(luaState, -1);
        buttonsList.push_back(GcButtonName::R);
        break;

      case GcButtonName::DPadUp:
        controllerState.DPadUp = lua_toboolean(luaState, -1);
        buttonsList.push_back(GcButtonName::DPadUp);
        break;

      case GcButtonName::DPadDown:
        controllerState.DPadDown = lua_toboolean(luaState, -1);
        buttonsList.push_back(GcButtonName::DPadDown);
        break;

      case GcButtonName::DPadLeft:
        controllerState.DPadLeft = lua_toboolean(luaState, -1);
        buttonsList.push_back(GcButtonName::DPadLeft);
        break;

      case GcButtonName::DPadRight:
        controllerState.DPadRight = lua_toboolean(luaState, -1);
        buttonsList.push_back(GcButtonName::DPadRight);
        break;

      case GcButtonName::Start:
        controllerState.Start = lua_toboolean(luaState, -1);
        buttonsList.push_back(GcButtonName::Start);
        break;

      case GcButtonName::Reset:
        controllerState.reset = lua_toboolean(luaState, -1);
        buttonsList.push_back(GcButtonName::Reset);
        break;

      case GcButtonName::AnalogStickX:
        magnitude = luaL_checkinteger(luaState, -1);
        if (magnitude < 0 || magnitude > 255)
          luaL_error(luaState, "Error: analogStickX was outside bounds of 0 and 255!");
        controllerState.AnalogStickX = static_cast<u8>(magnitude);
        buttonsList.push_back(GcButtonName::AnalogStickX);
        break;

      case GcButtonName::AnalogStickY:
        magnitude = luaL_checkinteger(luaState, -1);
        if (magnitude < 0 || magnitude > 255)
          luaL_error(luaState, "Error: analogStickY was outside bounds of 0 and 255!");
        controllerState.AnalogStickY = static_cast<u8>(magnitude);
        buttonsList.push_back(GcButtonName::AnalogStickY);
        break;

      case GcButtonName::CStickX:
        magnitude = luaL_checkinteger(luaState, -1);
        if (magnitude < 0 || magnitude > 255)
          luaL_error(luaState, "Error: cStickX was outside bounds of 0 and 255!");
        controllerState.CStickX = static_cast<u8>(magnitude);
        buttonsList.push_back(GcButtonName::CStickX);
        break;

      case GcButtonName::CStickY:
        magnitude = luaL_checkinteger(luaState, -1);
        if (magnitude < 0 || magnitude > 255)
          luaL_error(luaState, "Error: cStickY was outside bounds of 0 and 255!");
        controllerState.CStickY = static_cast<u8>(magnitude);
        buttonsList.push_back(GcButtonName::CStickY);
        break;

      case GcButtonName::TriggerL:
        magnitude = luaL_checkinteger(luaState, -1);
        if (magnitude < 0 || magnitude > 255)
          luaL_error(luaState, "Error: triggerL was outside bounds of 0 and 255!");
        controllerState.TriggerL = static_cast<u8>(magnitude);
        buttonsList.push_back(GcButtonName::TriggerL);
        break;

      case GcButtonName::TriggerR:
        magnitude = luaL_checkinteger(luaState, -1);
        if (magnitude < 0 || magnitude > 255)
          luaL_error(luaState, "Error: triggerR was outside bounds of 0 and 255!");
        controllerState.TriggerR = static_cast<u8>(magnitude);
        buttonsList.push_back(GcButtonName::TriggerR);
        break;

      default:
        luaL_error(luaState,
                   "Error: Unknown button name passed in as input to addInputs(). Valid button "
                   "names are: A, B, X, Y, Z, L, R, Z, dPadUp, dPadDown, dPadLeft, dPadRight, "
                   "cStickX, cStickY, analogStickX, analogStickY, triggerL, triggerR, RESET, and START");
      }

      lua_pop(luaState, 1);
    }
    doRandomInputEventsAtSpecifiedPort[controllerPortNumber - 1] = true;
    randomButtonEvents[controllerPortNumber - 1].push_back(std::unique_ptr<LuaGameCubeButtonProbabilityEvent>(new LuaGCButtonComboEvent(probability, controllerState, buttonsList, setOtherButtonsToBlank)));
    return 0;
  }

  int addControllerClearChance(lua_State* luaState)
  {
    luaColonOperatorTypeCheck(luaState, "addControllerClearChance",
                              "gc_controller:addControllerClearChance(1, 56)");
    s64 controllerPortNumber = getPortNumberHelperFunction(luaState, "addControllerClearChance");
    double probability = getProbabilityHelperFunction(luaState, "addControllerClearChance");

    doRandomInputEventsAtSpecifiedPort[controllerPortNumber - 1] = true;
    randomButtonEvents[controllerPortNumber - 1].push_back(std::unique_ptr<LuaGameCubeButtonProbabilityEvent>(new LuaGCClearControllerEvent(probability)));
    return 0;

  }

  int getControllerInputs(lua_State* luaState)
  {
    luaColonOperatorTypeCheck(luaState, "getControllerInputs","gc_controller:getControllerInputs(1)");
    s64 controllerPortNumber = getPortNumberHelperFunction(luaState, "getControllerInputs");

    Movie::ControllerState controllerInputs = Movie::GetLuaGCInputsForPort(controllerPortNumber - 1);
    std::string inputDisplayString = Movie::GetInputDisplay();
    inputDisplayString = inputDisplayString;
    bool buttonPressed = false;
    s64 magnitude = 0;
    lua_newtable(luaState);
    GcButtonName allGcButtons[] = { GcButtonName::A, GcButtonName::B, GcButtonName::X, GcButtonName::Y,
                                    GcButtonName::Z, GcButtonName::L, GcButtonName::R, GcButtonName::DPadUp,
                                    GcButtonName::DPadDown, GcButtonName::DPadLeft, GcButtonName::DPadRight, GcButtonName::AnalogStickX,
                                    GcButtonName::AnalogStickY, GcButtonName::CStickX, GcButtonName::CStickY, GcButtonName::TriggerL,
                                    GcButtonName::TriggerR, GcButtonName::Start, GcButtonName::Reset, GcButtonName::Unknown
                                  };

    for (GcButtonName buttonCode : allGcButtons)
    {
      const char* buttonNameString = convertButtonEnumToString(buttonCode);
      lua_pushlstring(luaState, buttonNameString, std::strlen(buttonNameString));

      switch (buttonCode)
      {
      case GcButtonName::A:
        buttonPressed = controllerInputs.A;
        lua_pushboolean(luaState, buttonPressed);
        break;
      case GcButtonName::B:
        buttonPressed = controllerInputs.B;
        lua_pushboolean(luaState, buttonPressed);
        break;
      case GcButtonName::X:
        buttonPressed = controllerInputs.X;
        lua_pushboolean(luaState, buttonPressed);
        break;
      case GcButtonName::Y:
        buttonPressed = controllerInputs.Y;
        lua_pushboolean(luaState, buttonPressed);
        break;
      case GcButtonName::Z:
        buttonPressed = controllerInputs.Z;
        lua_pushboolean(luaState, buttonPressed);
        break;
      case GcButtonName::L:
        buttonPressed = controllerInputs.L;
        lua_pushboolean(luaState, buttonPressed);
        break;
      case GcButtonName::R:
        buttonPressed = controllerInputs.R;
        lua_pushboolean(luaState, buttonPressed);
        break;
      case GcButtonName::Start:
        buttonPressed = controllerInputs.Start;
        lua_pushboolean(luaState, buttonPressed);
        break;
      case GcButtonName::Reset:
        buttonPressed = controllerInputs.reset;
        lua_pushboolean(luaState, buttonPressed);
        break;
      case GcButtonName::DPadUp:
        buttonPressed = controllerInputs.DPadUp;
        lua_pushboolean(luaState, buttonPressed);
        break;
      case GcButtonName::DPadDown:
        buttonPressed = controllerInputs.DPadDown;
        lua_pushboolean(luaState, buttonPressed);
        break;
      case GcButtonName::DPadLeft:
        buttonPressed = controllerInputs.DPadLeft;
        lua_pushboolean(luaState, buttonPressed);
        break;
      case GcButtonName::DPadRight:
        buttonPressed = controllerInputs.DPadRight;
        lua_pushboolean(luaState, buttonPressed);
        break;
      case GcButtonName::AnalogStickX:
        magnitude = controllerInputs.AnalogStickX;
        lua_pushinteger(luaState, magnitude);
        break;
      case GcButtonName::AnalogStickY:
        magnitude = controllerInputs.AnalogStickY;
        lua_pushinteger(luaState, magnitude);
        break;
      case GcButtonName::CStickX:
        magnitude = controllerInputs.CStickX;
        lua_pushinteger(luaState, magnitude);
        break;
      case GcButtonName::CStickY:
        magnitude = controllerInputs.CStickY;
        lua_pushinteger(luaState, magnitude);
        break;
      case GcButtonName::TriggerL:
        magnitude = controllerInputs.TriggerL;
        lua_pushinteger(luaState, magnitude);
        break;
      case GcButtonName::TriggerR:
        magnitude = controllerInputs.TriggerR;
        lua_pushinteger(luaState, magnitude);
        break;
      default:
        luaL_error(
            luaState,
            "An unexpected implementation error occured in gc_controller:getControllerInputs(). "
            "Did you modify the order of the enums in LuaGCButtons.h?");
        break;
      }
      lua_settable(luaState, -3);
    }
    return 1;
  }
  }
