#include "LuaGameCubeController.h"

namespace Lua
{
namespace LuaGameCubeController
{
class gc_controller_lua
{
public:
  inline gc_controller_lua(){};
};

  gc_controller_lua* gc_controller_pointer = NULL;
  std::array<bool, 4> overwriteControllerAtSpecifiedPort = {};
  std::array<bool, 4> addToControllerAtSpecifiedPort = {};
  std::array<bool, 4> doRandomInputEventsAtSpecifiedPort = {};

  std::array<Movie::ControllerState, 4> newOverwriteControllerInputs = {};
  std::array<Movie::ControllerState, 4> addToControllerInputs = {};
  std::array<std::vector<GC_BUTTON_NAME>, 4> buttonListsForAddToControllerInputs = {};
  std::array<std::vector<std::unique_ptr<LuaGameCubeButtonProbabilityEvent>>, 4> randomButtonEvents = {};

gc_controller_lua* getControllerInstance()
{
  if (gc_controller_pointer == NULL)
    gc_controller_pointer = new gc_controller_lua();
  return gc_controller_pointer;
}

  void InitLuaGameCubeControllerFunctions(lua_State* luaState)
{
    gc_controller_lua** gcControllerLuaPtrPtr = (gc_controller_lua**)lua_newuserdata(luaState, sizeof(gc_controller_lua*));
    *gcControllerLuaPtrPtr = getControllerInstance();
    luaL_newmetatable(luaState, "LuaGCControllerTable");
    lua_pushvalue(luaState, -1);
    lua_setfield(luaState, -2, "__index");

    luaL_Reg luaGCControllerFunctions[] = {
      {"setInputs", setInputs},
      {"addInputs", addInputs},
      {"addButtonFlipChance", addButtonFlipChance},
      {"addButtonPressChance", addButtonPressChance},
      {"addButtonReleaseChance", addButtonReleaseChance},
      {"addOrSubtractFromCurrentAnalogValueChance", addOrSubtractFromCurrentAnalogValueChance},
      {"addOrSubtractFromSpecificAnalogValueChance", addOrSubtractFromSpecificAnalogValueChance},
      {"addButtonComboChance", addButtonComboChance},
      {"addControllerClearChance", addControllerClearChance},
      {"getControllerInputs", getControllerInputs},
      {nullptr, nullptr}
    };

    luaL_setfuncs(luaState, luaGCControllerFunctions, 0);
    lua_setmetatable(luaState, -2);
    lua_setglobal(luaState, "gc_controller");

    for (int i = 0; i < 4; ++i)
    {
      overwriteControllerAtSpecifiedPort[i] = false;
      addToControllerAtSpecifiedPort[i] = false;
      doRandomInputEventsAtSpecifiedPort[i] = false;

      randomButtonEvents[i] = std::vector<std::unique_ptr<LuaGameCubeButtonProbabilityEvent>>();
      buttonListsForAddToControllerInputs[i] = std::vector<GC_BUTTON_NAME>();
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
      if (buttonName == NULL || buttonName[0] == '\0')
      {
        luaL_error(luaState, "Error: in setInputs() function, an empty string was passed for a button name!");
      }

      switch (parseGCButton(buttonName))
      {
      case A:
        newOverwriteControllerInputs[controllerPortNumber - 1].A = lua_toboolean(luaState, -1);
          break;

      case B:
        newOverwriteControllerInputs[controllerPortNumber - 1].B = lua_toboolean(luaState, -1);
        break;

      case X:
        newOverwriteControllerInputs[controllerPortNumber - 1].X = lua_toboolean(luaState, -1);
        break;

      case Y:
        newOverwriteControllerInputs[controllerPortNumber - 1].Y = lua_toboolean(luaState, -1);
        break;

      case Z:
        newOverwriteControllerInputs[controllerPortNumber - 1].Z = lua_toboolean(luaState, -1);
        break;

      case L:
        newOverwriteControllerInputs[controllerPortNumber - 1].L = lua_toboolean(luaState, -1);
        break;

      case R:
        newOverwriteControllerInputs[controllerPortNumber - 1].R = lua_toboolean(luaState, -1);
        break;

      case dPadUp:
        newOverwriteControllerInputs[controllerPortNumber - 1].DPadUp = lua_toboolean(luaState, -1);
        break;

      case dPadDown:
        newOverwriteControllerInputs[controllerPortNumber - 1].DPadDown = lua_toboolean(luaState, -1);
        break;

      case dPadLeft:
        newOverwriteControllerInputs[controllerPortNumber - 1].DPadLeft = lua_toboolean(luaState, -1);
        break;

      case dPadRight:
        newOverwriteControllerInputs[controllerPortNumber - 1].DPadRight = lua_toboolean(luaState, -1);
        break;

      case START:
        newOverwriteControllerInputs[controllerPortNumber - 1].Start = lua_toboolean(luaState, -1);
        break;

      case RESET:
        newOverwriteControllerInputs[controllerPortNumber - 1].reset = lua_toboolean(luaState, -1);
        break;

      case analogStickX:
        magnitude = luaL_checkinteger(luaState, -1);
        if (magnitude < 0 || magnitude > 255)
          luaL_error(luaState, "Error: analogStickX was outside bounds of 0 and 255!");
        newOverwriteControllerInputs[controllerPortNumber - 1].AnalogStickX = static_cast<u8>(magnitude);
        break;

      case analogStickY:
        magnitude = luaL_checkinteger(luaState, -1);
        if (magnitude < 0 || magnitude > 255)
          luaL_error(luaState, "Error: analogStickY was outside bounds of 0 and 255!");
        newOverwriteControllerInputs[controllerPortNumber - 1].AnalogStickY = static_cast<u8>(magnitude);
        break;

      case cStickX:
        magnitude = luaL_checkinteger(luaState, -1);
        if (magnitude < 0 || magnitude > 255)
          luaL_error(luaState, "Error: cStickX was outside bounds of 0 and 255!");
        newOverwriteControllerInputs[controllerPortNumber - 1].CStickX = static_cast<u8>(magnitude);
        break;

      case cStickY:
        magnitude = luaL_checkinteger(luaState, -1);
        if (magnitude < 0 || magnitude > 255)
          luaL_error(luaState, "Error: cStickY was outside bounds of 0 and 255!");
        newOverwriteControllerInputs[controllerPortNumber - 1].CStickY = magnitude;
        break;

      case triggerL:
        magnitude = luaL_checkinteger(luaState, -1);
        if (magnitude < 0 || magnitude > 255)
          luaL_error(luaState, "Error: triggerL was outside bounds of 0 and 255!");
        newOverwriteControllerInputs[controllerPortNumber - 1].TriggerL = magnitude;
        break;

      case triggerR:
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
      if (buttonName == NULL || buttonName[0] == '\0')
      {
        luaL_error(luaState,
                   "Error: in addInputs() function, an empty string was passed for a button name!");
      }

      switch (parseGCButton(buttonName))
      {
      case A:
        addToControllerInputs[controllerPortNumber - 1].A = lua_toboolean(luaState, -1);
        buttonListsForAddToControllerInputs[controllerPortNumber - 1].push_back(A);
        break;

      case B:
        addToControllerInputs[controllerPortNumber - 1].B = lua_toboolean(luaState, -1);
        buttonListsForAddToControllerInputs[controllerPortNumber - 1].push_back(B);
        break;

      case X:
        addToControllerInputs[controllerPortNumber - 1].X = lua_toboolean(luaState, -1);
        buttonListsForAddToControllerInputs[controllerPortNumber - 1].push_back(X);
        break;

      case Y:
        addToControllerInputs[controllerPortNumber - 1].Y = lua_toboolean(luaState, -1);
        buttonListsForAddToControllerInputs[controllerPortNumber - 1].push_back(Y);
        break;

      case Z:
        addToControllerInputs[controllerPortNumber - 1].Z = lua_toboolean(luaState, -1);
        buttonListsForAddToControllerInputs[controllerPortNumber - 1].push_back(Z);
        break;

      case L:
        addToControllerInputs[controllerPortNumber - 1].L = lua_toboolean(luaState, -1);
        buttonListsForAddToControllerInputs[controllerPortNumber - 1].push_back(L);
        break;

      case R:
        addToControllerInputs[controllerPortNumber - 1].R = lua_toboolean(luaState, -1);
        buttonListsForAddToControllerInputs[controllerPortNumber - 1].push_back(R);
        break;

      case dPadUp:
        addToControllerInputs[controllerPortNumber - 1].DPadUp = lua_toboolean(luaState, -1);
        buttonListsForAddToControllerInputs[controllerPortNumber - 1].push_back(dPadUp);
        break;

      case dPadDown:
        addToControllerInputs[controllerPortNumber - 1].DPadDown = lua_toboolean(luaState, -1);
        buttonListsForAddToControllerInputs[controllerPortNumber - 1].push_back(dPadDown);
        break;

      case dPadLeft:
        addToControllerInputs[controllerPortNumber - 1].DPadLeft = lua_toboolean(luaState, -1);
        buttonListsForAddToControllerInputs[controllerPortNumber - 1].push_back(dPadLeft);
        break;

      case dPadRight:
        addToControllerInputs[controllerPortNumber - 1].DPadRight = lua_toboolean(luaState, -1);
        buttonListsForAddToControllerInputs[controllerPortNumber - 1].push_back(dPadRight);
        break;

      case START:
        addToControllerInputs[controllerPortNumber - 1].Start = lua_toboolean(luaState, -1);
        buttonListsForAddToControllerInputs[controllerPortNumber - 1].push_back(START);
        break;

      case RESET:
        addToControllerInputs[controllerPortNumber - 1].reset = lua_toboolean(luaState, -1);
        break;

      case analogStickX:
        magnitude = luaL_checkinteger(luaState, -1);
        if (magnitude < 0 || magnitude > 255)
          luaL_error(luaState, "Error: analogStickX was outside bounds of 0 and 255!");
        addToControllerInputs[controllerPortNumber - 1].AnalogStickX = static_cast<u8>(magnitude);
        buttonListsForAddToControllerInputs[controllerPortNumber - 1].push_back(analogStickX);
        break;

      case analogStickY:
        magnitude = luaL_checkinteger(luaState, -1);
        if (magnitude < 0 || magnitude > 255)
          luaL_error(luaState, "Error: analogStickY was outside bounds of 0 and 255!");
        addToControllerInputs[controllerPortNumber - 1].AnalogStickY = static_cast<u8>(magnitude);
        buttonListsForAddToControllerInputs[controllerPortNumber - 1].push_back(analogStickY);
        break;

      case cStickX:
        magnitude = luaL_checkinteger(luaState, -1);
        if (magnitude < 0 || magnitude > 255)
          luaL_error(luaState, "Error: cStickX was outside bounds of 0 and 255!");
        addToControllerInputs[controllerPortNumber - 1].CStickX = static_cast<u8>(magnitude);
        buttonListsForAddToControllerInputs[controllerPortNumber - 1].push_back(cStickX);
        break;

      case cStickY:
        magnitude = luaL_checkinteger(luaState, -1);
        if (magnitude < 0 || magnitude > 255)
          luaL_error(luaState, "Error: cStickY was outside bounds of 0 and 255!");
        addToControllerInputs[controllerPortNumber - 1].CStickY = magnitude;
        buttonListsForAddToControllerInputs[controllerPortNumber - 1].push_back(cStickY);
        break;

      case triggerL:
        magnitude = luaL_checkinteger(luaState, -1);
        if (magnitude < 0 || magnitude > 255)
          luaL_error(luaState, "Error: triggerL was outside bounds of 0 and 255!");
        addToControllerInputs[controllerPortNumber - 1].TriggerL = magnitude;
        buttonListsForAddToControllerInputs[controllerPortNumber - 1].push_back(triggerL);
        break;

      case triggerR:
        magnitude = luaL_checkinteger(luaState, -1);
        if (magnitude < 0 || magnitude > 255)
          luaL_error(luaState, "Error: triggerR was outside bounds of 0 and 255!");
        addToControllerInputs[controllerPortNumber - 1].TriggerR = magnitude;
        buttonListsForAddToControllerInputs[controllerPortNumber - 1].push_back(triggerR);
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
    GC_BUTTON_NAME buttonName = parseGCButton(luaL_checkstring(luaState, 4));

    if (buttonName == triggerL)
      buttonName = L;
    if (buttonName == triggerR)
      buttonName = R;

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
    GC_BUTTON_NAME buttonName = parseGCButton(luaL_checkstring(luaState, 4));
    if (buttonName == triggerL)
      buttonName = L;
    if (buttonName == triggerR)
      buttonName = R;
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
    GC_BUTTON_NAME buttonName = parseGCButton(luaL_checkstring(luaState, 4));
    if (buttonName == triggerL)
      buttonName = L;
    if (buttonName == triggerR)
      buttonName = R;
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
    GC_BUTTON_NAME buttonName = parseGCButton(luaL_checkstring(luaState, 4));
    if (buttonName == L)
      buttonName = triggerL;
    if (buttonName == R)
      buttonName = triggerR;

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
    GC_BUTTON_NAME buttonName = parseGCButton(luaL_checkstring(luaState, 4));
    if (buttonName == L)
      buttonName = triggerL;
    if (buttonName == R)
      buttonName = triggerR;

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
    std::vector<GC_BUTTON_NAME> buttonsList = std::vector<GC_BUTTON_NAME>();
    lua_pushnil(luaState);
    s64 magnitude = 0;

    while (lua_next(luaState, 5) != 0)
    {
      /* uses 'key' (at index -2) and 'value' (at index -1) */
      const char* buttonName = luaL_checkstring(luaState, -2);
      if (buttonName == NULL || buttonName[0] == '\0')
      {
        luaL_error(luaState,
                   "Error: in addButtonComboChance() function, an empty string was passed for a button name!");
      }

      switch (parseGCButton(buttonName))
      {
      case A:
        controllerState.A = lua_toboolean(luaState, -1);
        buttonsList.push_back(A);
        break;

      case B:
        controllerState.B = lua_toboolean(luaState, -1);
        buttonsList.push_back(B);
        break;

      case X:
        controllerState.X = lua_toboolean(luaState, -1);
        buttonsList.push_back(X);
        break;

      case Y:
        controllerState.Y = lua_toboolean(luaState, -1);
        buttonsList.push_back(Y);
        break;

      case Z:
        controllerState.Z = lua_toboolean(luaState, -1);
        buttonsList.push_back(Z);
        break;

      case L:
        controllerState.L = lua_toboolean(luaState, -1);
        buttonsList.push_back(L);
        break;

      case R:
        controllerState.R = lua_toboolean(luaState, -1);
        buttonsList.push_back(R);
        break;

      case dPadUp:
        controllerState.DPadUp = lua_toboolean(luaState, -1);
        buttonsList.push_back(dPadUp);
        break;

      case dPadDown:
        controllerState.DPadDown = lua_toboolean(luaState, -1);
        buttonsList.push_back(dPadDown);
        break;

      case dPadLeft:
        controllerState.DPadLeft = lua_toboolean(luaState, -1);
        buttonsList.push_back(dPadLeft);
        break;

      case dPadRight:
        controllerState.DPadRight = lua_toboolean(luaState, -1);
        buttonsList.push_back(dPadRight);
        break;

      case START:
        controllerState.Start = lua_toboolean(luaState, -1);
        buttonsList.push_back(START);
        break;

      case RESET:
        controllerState.reset = lua_toboolean(luaState, -1);
        buttonsList.push_back(RESET);
        break;

      case analogStickX:
        magnitude = luaL_checkinteger(luaState, -1);
        if (magnitude < 0 || magnitude > 255)
          luaL_error(luaState, "Error: analogStickX was outside bounds of 0 and 255!");
        controllerState.AnalogStickX = static_cast<u8>(magnitude);
        buttonsList.push_back(analogStickX);
        break;

      case analogStickY:
        magnitude = luaL_checkinteger(luaState, -1);
        if (magnitude < 0 || magnitude > 255)
          luaL_error(luaState, "Error: analogStickY was outside bounds of 0 and 255!");
        controllerState.AnalogStickY = static_cast<u8>(magnitude);
        buttonsList.push_back(analogStickY);
        break;

      case cStickX:
        magnitude = luaL_checkinteger(luaState, -1);
        if (magnitude < 0 || magnitude > 255)
          luaL_error(luaState, "Error: cStickX was outside bounds of 0 and 255!");
        controllerState.CStickX = static_cast<u8>(magnitude);
        buttonsList.push_back(cStickX);
        break;

      case cStickY:
        magnitude = luaL_checkinteger(luaState, -1);
        if (magnitude < 0 || magnitude > 255)
          luaL_error(luaState, "Error: cStickY was outside bounds of 0 and 255!");
        controllerState.CStickY = static_cast<u8>(magnitude);
        buttonsList.push_back(cStickY);
        break;

      case triggerL:
        magnitude = luaL_checkinteger(luaState, -1);
        if (magnitude < 0 || magnitude > 255)
          luaL_error(luaState, "Error: triggerL was outside bounds of 0 and 255!");
        controllerState.TriggerL = static_cast<u8>(magnitude);
        buttonsList.push_back(triggerL);
        break;

      case triggerR:
        magnitude = luaL_checkinteger(luaState, -1);
        if (magnitude < 0 || magnitude > 255)
          luaL_error(luaState, "Error: triggerR was outside bounds of 0 and 255!");
        controllerState.TriggerR = static_cast<u8>(magnitude);
        buttonsList.push_back(triggerR);
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
    for (int buttonCode = A; buttonCode != UNKNOWN; ++buttonCode)
    {
      const char* buttonNameString = convertButtonEnumToString(static_cast<GC_BUTTON_NAME>(buttonCode));
      if (static_cast<GC_BUTTON_NAME>(buttonCode) == RESET)
        continue;
      lua_pushlstring(luaState, buttonNameString, std::strlen(buttonNameString));

      switch (static_cast<GC_BUTTON_NAME>(buttonCode))
      {
      case A:
        buttonPressed = controllerInputs.A;
        lua_pushboolean(luaState, buttonPressed);
        break;
      case B:
        buttonPressed = controllerInputs.B;
        lua_pushboolean(luaState, buttonPressed);
        break;
      case X:
        buttonPressed = controllerInputs.X;
        lua_pushboolean(luaState, buttonPressed);
        break;
      case Y:
        buttonPressed = controllerInputs.Y;
        lua_pushboolean(luaState, buttonPressed);
        break;
      case Z:
        buttonPressed = controllerInputs.Z;
        lua_pushboolean(luaState, buttonPressed);
        break;
      case L:
        buttonPressed = controllerInputs.L;
        lua_pushboolean(luaState, buttonPressed);
        break;
      case R:
        buttonPressed = controllerInputs.R;
        lua_pushboolean(luaState, buttonPressed);
        break;
      case START:
        buttonPressed = controllerInputs.Start;
        lua_pushboolean(luaState, buttonPressed);
        break;
      case dPadUp:
        buttonPressed = controllerInputs.DPadUp;
        lua_pushboolean(luaState, buttonPressed);
        break;
      case dPadDown:
        buttonPressed = controllerInputs.DPadDown;
        lua_pushboolean(luaState, buttonPressed);
        break;
      case dPadLeft:
        buttonPressed = controllerInputs.DPadLeft;
        lua_pushboolean(luaState, buttonPressed);
        break;
      case dPadRight:
        buttonPressed = controllerInputs.DPadRight;
        lua_pushboolean(luaState, buttonPressed);
        break;
      case analogStickX:
        magnitude = controllerInputs.AnalogStickX;
        lua_pushinteger(luaState, magnitude);
        break;
      case analogStickY:
        magnitude = controllerInputs.AnalogStickY;
        lua_pushinteger(luaState, magnitude);
        break;
      case cStickX:
        magnitude = controllerInputs.CStickX;
        lua_pushinteger(luaState, magnitude);
        break;
      case cStickY:
        magnitude = controllerInputs.CStickY;
        lua_pushinteger(luaState, magnitude);
        break;
      case triggerL:
        magnitude = controllerInputs.TriggerL;
        lua_pushinteger(luaState, magnitude);
        break;
      case triggerR:
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
  }

