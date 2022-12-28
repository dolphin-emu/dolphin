#include "LuaGameCubeController.h"
#include "Core/Movie.h"

namespace Lua
{
namespace LuaGameCubeController
{

  gc_controller_lua* gc_controller_pointer = NULL;

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
      {nullptr, nullptr}
    };

    luaL_setfuncs(luaState, luaGCControllerFunctions, 0);
    lua_setmetatable(luaState, -2);
    lua_setglobal(luaState, "gc_controller");
  }

  enum BUTTON_NAMES
  {
    A, B, X, Y, Z, L, R, dPadUp, dPadDown, dPadLeft, dPadRight, analogStickX, analogStickY, cStickX, cStickY, triggerL, triggerR, START, RESET, UNKNOWN
  };

  bool isEqualIgnoreCase(const char* string1, const char* string2)
  {
    int index = 0;
    while (string1[index] != '\0')
    {
      if (string1[index] != string2[index])
      {
        char firstChar = string1[index];
        if (firstChar >= 65 && firstChar <= 90)
          firstChar += 32;
        char secondChar = string2[index];
        if (secondChar >= 65 && secondChar <= 90)
          secondChar += 32;
        if (firstChar != secondChar)
          return false;
      }
      ++index;
    }

    return string2[index] == '\0';
  }

  BUTTON_NAMES parseButton(const char* buttonName)
  {
    if (strlen(buttonName) == 1)
    {
      switch (buttonName[0])
      {
      case 'a':
      case 'A':
        return A;

      case 'b':
      case 'B':
        return B;

      case 'x':
      case 'X':
        return X;

      case 'y':
      case 'Y':
        return Y;

      case 'z':
      case 'Z':
        return Z;

      case 'l':
      case 'L':
        return L;

      case 'r':
      case 'R':
        return R;

      default:
        return UNKNOWN;
      }
    }

    switch (buttonName[0])
    {
    case 'd':
    case 'D':
      if (isEqualIgnoreCase(buttonName, "dPadUp"))
        return dPadUp;
      else if (isEqualIgnoreCase(buttonName, "dPadDown"))
        return dPadDown;
      else if (isEqualIgnoreCase(buttonName, "dPadLeft"))
        return dPadLeft;
      else if (isEqualIgnoreCase(buttonName, "dPadRight"))
        return dPadRight;
      else
        return UNKNOWN;

    case 'a':
    case 'A':
      if (isEqualIgnoreCase(buttonName, "analogStickX"))
        return analogStickX;
      else if (isEqualIgnoreCase(buttonName, "analogStickY"))
        return analogStickY;
      else
        return UNKNOWN;

    case 'c':
    case 'C':
      if (isEqualIgnoreCase(buttonName, "cStickX"))
        return cStickX;
      else if (isEqualIgnoreCase(buttonName, "cStickY"))
        return cStickY;
      else
        return UNKNOWN;

    case 't':
    case 'T':
      if (isEqualIgnoreCase(buttonName, "triggerL"))
        return triggerL;
      else if (isEqualIgnoreCase(buttonName, "triggerR"))
        return triggerR;
      else
        return UNKNOWN;

    case 'r':
    case 'R':
        if (isEqualIgnoreCase(buttonName, "reset"))
          return RESET;
        else
          return UNKNOWN;

    case 's':
    case 'S':
      if (isEqualIgnoreCase(buttonName, "start"))
        return START;
      else
        return UNKNOWN;

    default:
      return UNKNOWN;
    }
  }

  //NOTE: In SI.cpp, UpdateDevices() is called to update each device, which moves exactly 8 bytes forward for each controller. Also, it moves in order from controllers 1 to 4.
  int setInputs(lua_State* luaState)
  {
    Movie::ControllerState controllerState = Movie::ControllerState();
    memset(&controllerState, 0, sizeof(Movie::ControllerState));
    controllerState.is_connected = true;

    s64 magnitude = 0;

    if (Movie::IsPlayingInput())
    {
      ;
      //memcpy(&controllerState, &Movie::s_temp_input[Movie::s_currentByte],
           //  sizeof(Movie::ControllerState));
    }

    lua_pushnil(luaState); /* first key */
    while (lua_next(luaState, 2) != 0)
    {
      /* uses 'key' (at index -2) and 'value' (at index -1) */
      const char* buttonName = luaL_checkstring(luaState, -2);
      if (buttonName == NULL || buttonName[0] == '\0')
      {
        luaL_error(luaState, "Error: in setInputs() function, an empty string was passed for a button name!");
      }

      switch (parseButton(buttonName))
      {
      case A:
          controllerState.A = lua_toboolean(luaState, -1);
          break;

      case B:
        controllerState.B = lua_toboolean(luaState, -1);
        break;

      case X:
        controllerState.X = lua_toboolean(luaState, -1);
        break;

      case Y:
        controllerState.Y = lua_toboolean(luaState, -1);
        break;

      case Z:
        controllerState.Z = lua_toboolean(luaState, -1);
        break;

      case L:
        controllerState.L = lua_toboolean(luaState, -1);
        break;

      case R:
        controllerState.R = lua_toboolean(luaState, -1);
        break;

      case dPadUp:
        controllerState.DPadUp = lua_toboolean(luaState, -1);
        break;

      case dPadDown:
        controllerState.DPadDown = lua_toboolean(luaState, -1);
        break;

      case dPadLeft:
        controllerState.DPadLeft = lua_toboolean(luaState, -1);
        break;

      case dPadRight:
        controllerState.DPadRight = lua_toboolean(luaState, -1);
        break;

      case START:
        controllerState.Start = lua_toboolean(luaState, -1);
        break;

      case RESET:
        controllerState.reset = lua_toboolean(luaState, -1);
        break;

      case analogStickX:
        magnitude = luaL_checkinteger(luaState, -1);
        if (magnitude < 0 || magnitude > 255)
          luaL_error(luaState, "Error: analogStickX was outside bounds of 0 and 255!");
        controllerState.AnalogStickX = static_cast<u8>(magnitude);
        break;

      case analogStickY:
        magnitude = luaL_checkinteger(luaState, -1);
        if (magnitude < 0 || magnitude > 255)
          luaL_error(luaState, "Error: analogStickY was outside bounds of 0 and 255!");
        controllerState.AnalogStickY = static_cast<u8>(magnitude);
        break;

      case cStickX:
        magnitude = luaL_checkinteger(luaState, -1);
        if (magnitude < 0 || magnitude > 255)
          luaL_error(luaState, "Error: cStickX was outside bounds of 0 and 255!");
        controllerState.CStickX = static_cast<u8>(magnitude);
        break;

      case cStickY:
        magnitude = luaL_checkinteger(luaState, -1);
        if (magnitude < 0 || magnitude > 255)
          luaL_error(luaState, "Error: cStickY was outside bounds of 0 and 255!");
        controllerState.CStickY = magnitude;
        break;

      default:
        luaL_error(luaState,
                   "Error: Unknown button name passed in as input to setInputs(). Valid button "
                   "names are: A, B, X, Y, Z, L, R, Z, dPadUp, dPadDown, dPadLeft, dPadRight, "
                   "cStickX, cStickY, analogStickX, analogStickY, RESET, and START");
      }

      lua_pop(luaState, 1);
    }

    //Now writing the controller state back out (and checking if we are recording or playing back input).
    //memcpy(&Movie::s_padState, &controllerState, sizeof(Movie::ControllerState));
    if (Movie::IsPlayingInput())
      return 1;  // memcpy(&Movie::s_temp_input[Movie::s_currentByte], &Movie::s_padState,
         // sizeof(Movie::ControllerState));
    return 0;
  }
  }

}
